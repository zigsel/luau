// extern "C" shim: structural inspection of inferred Luau types.

#include "types.h"
#include "types_internal.h"

#include "Luau/BuiltinDefinitions.h"
#include "Luau/ConfigResolver.h"
#include "Luau/Error.h"
#include "Luau/FileResolver.h"
#include "Luau/Frontend.h"
#include "Luau/Module.h"
#include "Luau/Scope.h"
#include "Luau/ToString.h"
#include "Luau/Type.h"
#include "Luau/TypePack.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using namespace Luau;

namespace {

// Serves a single in-memory module by name.
struct SingleFileResolver : FileResolver {
    std::string moduleName;
    std::string source;

    std::optional<SourceCode> readSource(const ModuleName& name) override {
        if (name != moduleName)
            return std::nullopt;
        return SourceCode{source, SourceCode::Module};
    }
};

// Duplicate `s` into a malloc'd, NUL-terminated buffer the caller frees.
char* dupString(const std::string& s) {
    char* out = static_cast<char*>(std::malloc(s.size() + 1));
    if (!out)
        return nullptr;
    std::memcpy(out, s.data(), s.size());
    out[s.size()] = '\0';
    return out;
}

} // namespace

// Opaque wrappers. LuauType wraps a (followed) TypeId; LuauTypePack a TypePackId.
struct LuauType {
    TypeId id;
};
struct LuauTypePack {
    TypePackId id;
};

struct LuauTypes {
    SingleFileResolver files;
    NullConfigResolver config;
    std::unique_ptr<Frontend> frontend;
    ModulePtr module; // keep the checked module (and its arenas) alive

    std::vector<std::string> messages;
    std::vector<LuauPosition> positions;

    // Cache of derived handles, owned by this checker and freed with it.
    // Keyed by the underlying pointer so repeated lookups return the same handle
    // and borrowed strings stay stable.
    std::unordered_map<const void*, LuauType*> typeHandles;
    std::unordered_map<const void*, LuauTypePack*> packHandles;
    // Backing storage for borrowed strings derived from non-owning sources.
    std::vector<std::unique_ptr<std::string>> strings;

    ~LuauTypes() {
        for (auto& kv : typeHandles)
            delete kv.second;
        for (auto& kv : packHandles)
            delete kv.second;
    }

    LuauType* wrap(TypeId id) {
        if (!id)
            return nullptr;
        id = follow(id);
        auto it = typeHandles.find(id);
        if (it != typeHandles.end())
            return it->second;
        LuauType* h = new LuauType{id};
        typeHandles.emplace(id, h);
        return h;
    }

    LuauTypePack* wrapPack(TypePackId id) {
        if (!id)
            return nullptr;
        id = follow(id);
        auto it = packHandles.find(id);
        if (it != packHandles.end())
            return it->second;
        LuauTypePack* h = new LuauTypePack{id};
        packHandles.emplace(id, h);
        return h;
    }

    const char* intern(const std::string& s) {
        strings.push_back(std::make_unique<std::string>(s));
        return strings.back()->c_str();
    }
};

// The owning checker that produced a given handle is recorded so derived
// accessors can wrap children into the same handle cache. We thread it via a
// pointer stashed on the handle's checker. Simpler: keep a back-pointer.
namespace {
// Map a live TypeId/TypePackId handle back to its owning checker. We store the
// owner alongside each handle via parallel maps would be heavy; instead each
// accessor receives the handle and we look the owner up through a registry.
std::unordered_map<const LuauType*, LuauTypes*> g_typeOwner;
std::unordered_map<const LuauTypePack*, LuauTypes*> g_packOwner;
} // namespace

static LuauType* wrapType(LuauTypes* owner, TypeId id);
static LuauTypePack* wrapPack(LuauTypes* owner, TypePackId id);

// ---- checker ---------------------------------------------------------------

extern "C" LuauTypes* luau_types_check(const char* src, size_t len) {
    LuauTypes* h = new LuauTypes();
    h->files.moduleName = "main";
    h->files.source.assign(src, len);

    try {
        FrontendOptions options;
        options.retainFullTypeGraphs = true;
        h->frontend = std::make_unique<Frontend>(&h->files, &h->config, options);
        registerBuiltinGlobals(*h->frontend, h->frontend->globals);

        CheckResult result = h->frontend->check("main");
        for (const TypeError& e : result.errors) {
            h->messages.push_back(toString(e));
            LuauPosition p;
            p.line = e.location.begin.line;
            p.column = e.location.begin.column;
            h->positions.push_back(p);
        }

        h->module = h->frontend->moduleResolver.getModule("main");
    } catch (const std::exception& e) {
        h->messages.push_back(e.what());
        h->positions.push_back(LuauPosition{0, 0});
    }
    return h;
}

extern "C" int luau_types_error_count(const LuauTypes* h) {
    return static_cast<int>(h->messages.size());
}

extern "C" const char* luau_types_error_message(const LuauTypes* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->messages.size())
        return "";
    return h->messages[i].c_str();
}

extern "C" LuauPosition luau_types_error_position(const LuauTypes* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->positions.size())
        return LuauPosition{0, 0};
    return h->positions[i];
}

// Wrap a TypeId into a handle owned by `owner`, recording ownership.
static LuauType* wrapType(LuauTypes* owner, TypeId id) {
    LuauType* h = owner->wrap(id);
    if (h)
        g_typeOwner[h] = owner;
    return h;
}

static LuauTypePack* wrapPack(LuauTypes* owner, TypePackId id) {
    LuauTypePack* h = owner->wrapPack(id);
    if (h)
        g_packOwner[h] = owner;
    return h;
}

extern "C" LuauType* luau_types_require_global(LuauTypes* h, const char* name) {
    if (!h || !name)
        return nullptr;
    try {
        if (!h->module || !h->module->hasModuleScope())
            return nullptr;

        ScopePtr scope = h->module->getModuleScope();
        if (!scope)
            return nullptr;

        // Prefer a linear search over the binding map by source name; this works
        // for both top-level locals and globals registered on the module scope.
        std::optional<Binding> binding = scope->linearSearchForBinding(name, /*traverseScopeChain*/ true);
        if (binding && binding->typeId)
            return wrapType(h, binding->typeId);

        return nullptr;
    } catch (const std::exception&) {
        return nullptr;
    }
}

extern "C" void luau_types_free(LuauTypes* h) {
    if (!h)
        return;
    for (auto& kv : h->typeHandles)
        g_typeOwner.erase(kv.second);
    for (auto& kv : h->packHandles)
        g_packOwner.erase(kv.second);
    delete h;
}

// ---- type inspection -------------------------------------------------------

static LuauTypes* ownerOf(const LuauType* t) {
    auto it = g_typeOwner.find(t);
    return it == g_typeOwner.end() ? nullptr : it->second;
}
static LuauTypes* ownerOf(const LuauTypePack* t) {
    auto it = g_packOwner.find(t);
    return it == g_packOwner.end() ? nullptr : it->second;
}

extern "C" int luau_type_kind(LuauType* t) {
    if (!t)
        return LUAU_TYPE_UNKNOWN_KIND;
    try {
        TypeId id = follow(t->id);
        if (get<PrimitiveType>(id)) return LUAU_TYPE_PRIMITIVE;
        if (get<SingletonType>(id)) return LUAU_TYPE_SINGLETON;
        if (get<FunctionType>(id)) return LUAU_TYPE_FUNCTION;
        if (get<TableType>(id)) return LUAU_TYPE_TABLE;
        if (get<MetatableType>(id)) return LUAU_TYPE_METATABLE;
        if (get<ExternType>(id)) return LUAU_TYPE_CLASS;
        if (get<UnionType>(id)) return LUAU_TYPE_UNION;
        if (get<IntersectionType>(id)) return LUAU_TYPE_INTERSECTION;
        if (get<GenericType>(id)) return LUAU_TYPE_GENERIC;
        if (get<FreeType>(id)) return LUAU_TYPE_FREE;
        if (get<AnyType>(id)) return LUAU_TYPE_ANY;
        if (get<UnknownType>(id)) return LUAU_TYPE_UNKNOWN;
        if (get<NeverType>(id)) return LUAU_TYPE_NEVER;
        if (get<ErrorType>(id)) return LUAU_TYPE_ERROR;
        if (get<NegationType>(id)) return LUAU_TYPE_NEGATION;
        if (get<BoundType>(id)) return LUAU_TYPE_BOUND;
        return LUAU_TYPE_UNKNOWN_KIND;
    } catch (const std::exception&) {
        return LUAU_TYPE_UNKNOWN_KIND;
    }
}

extern "C" char* luau_type_tostring(LuauType* t) {
    if (!t)
        return nullptr;
    try {
        return dupString(toString(t->id));
    } catch (const std::exception&) {
        return nullptr;
    }
}

extern "C" int luau_type_primitive_kind(LuauType* t) {
    if (!t)
        return LUAU_PRIM_UNKNOWN;
    try {
        if (const PrimitiveType* p = get<PrimitiveType>(follow(t->id))) {
            switch (p->type) {
                case PrimitiveType::NilType: return LUAU_PRIM_NIL;
                case PrimitiveType::Boolean: return LUAU_PRIM_BOOLEAN;
                case PrimitiveType::Number: return LUAU_PRIM_NUMBER;
                case PrimitiveType::Integer: return LUAU_PRIM_INTEGER;
                case PrimitiveType::String: return LUAU_PRIM_STRING;
                case PrimitiveType::Thread: return LUAU_PRIM_THREAD;
                case PrimitiveType::Function: return LUAU_PRIM_FUNCTION;
                case PrimitiveType::Table: return LUAU_PRIM_TABLE;
                case PrimitiveType::Buffer: return LUAU_PRIM_BUFFER;
            }
        }
    } catch (const std::exception&) {
    }
    return LUAU_PRIM_UNKNOWN;
}

extern "C" const char* luau_type_primitive_name(LuauType* t) {
    if (!t)
        return nullptr;
    try {
        if (const PrimitiveType* p = get<PrimitiveType>(follow(t->id))) {
            switch (p->type) {
                case PrimitiveType::NilType: return "nil";
                case PrimitiveType::Boolean: return "boolean";
                case PrimitiveType::Number: return "number";
                case PrimitiveType::Integer: return "integer";
                case PrimitiveType::String: return "string";
                case PrimitiveType::Thread: return "thread";
                case PrimitiveType::Function: return "function";
                case PrimitiveType::Table: return "table";
                case PrimitiveType::Buffer: return "buffer";
            }
        }
    } catch (const std::exception&) {
    }
    return nullptr;
}

extern "C" int luau_type_singleton_kind(LuauType* t) {
    if (!t)
        return LUAU_SINGLETON_NONE;
    try {
        if (const SingletonType* s = get<SingletonType>(follow(t->id))) {
            if (get<BooleanSingleton>(s)) return LUAU_SINGLETON_BOOL;
            if (get<StringSingleton>(s)) return LUAU_SINGLETON_STRING;
        }
    } catch (const std::exception&) {
    }
    return LUAU_SINGLETON_NONE;
}

extern "C" int luau_type_singleton_bool(LuauType* t) {
    if (!t)
        return 0;
    try {
        if (const SingletonType* s = get<SingletonType>(follow(t->id)))
            if (const BooleanSingleton* b = get<BooleanSingleton>(s))
                return b->value ? 1 : 0;
    } catch (const std::exception&) {
    }
    return 0;
}

extern "C" const char* luau_type_singleton_string(LuauType* t) {
    if (!t)
        return nullptr;
    try {
        if (const SingletonType* s = get<SingletonType>(follow(t->id)))
            if (const StringSingleton* str = get<StringSingleton>(s)) {
                LuauTypes* owner = ownerOf(t);
                if (owner)
                    return owner->intern(str->value);
            }
    } catch (const std::exception&) {
    }
    return nullptr;
}

extern "C" LuauTypePack* luau_type_function_args(LuauType* t) {
    if (!t)
        return nullptr;
    try {
        if (const FunctionType* f = get<FunctionType>(follow(t->id)))
            if (LuauTypes* owner = ownerOf(t))
                return wrapPack(owner, f->argTypes);
    } catch (const std::exception&) {
    }
    return nullptr;
}

extern "C" LuauTypePack* luau_type_function_rets(LuauType* t) {
    if (!t)
        return nullptr;
    try {
        if (const FunctionType* f = get<FunctionType>(follow(t->id)))
            if (LuauTypes* owner = ownerOf(t))
                return wrapPack(owner, f->retTypes);
    } catch (const std::exception&) {
    }
    return nullptr;
}

namespace {
const TableType* asTable(TypeId id) {
    return get<TableType>(follow(id));
}
// Resolve the i-th property entry (std::map iteration order).
const std::pair<const Name, Property>* tableProp(const TableType* tt, int i) {
    if (i < 0 || static_cast<size_t>(i) >= tt->props.size())
        return nullptr;
    auto it = tt->props.begin();
    std::advance(it, i);
    return &*it;
}
// The readable type of a property (falls back to write type for write-only).
std::optional<TypeId> propType(const Property& p) {
    if (p.readTy)
        return p.readTy;
    if (p.writeTy)
        return p.writeTy;
    return std::nullopt;
}
} // namespace

extern "C" int luau_type_table_prop_count(LuauType* t) {
    if (!t)
        return 0;
    try {
        if (const TableType* tt = asTable(t->id))
            return static_cast<int>(tt->props.size());
    } catch (const std::exception&) {
    }
    return 0;
}

extern "C" const char* luau_type_table_prop_name(LuauType* t, int i) {
    if (!t)
        return nullptr;
    try {
        if (const TableType* tt = asTable(t->id))
            if (const auto* entry = tableProp(tt, i))
                if (LuauTypes* owner = ownerOf(t))
                    return owner->intern(entry->first);
    } catch (const std::exception&) {
    }
    return nullptr;
}

extern "C" LuauType* luau_type_table_prop_type(LuauType* t, int i) {
    if (!t)
        return nullptr;
    try {
        if (const TableType* tt = asTable(t->id))
            if (const auto* entry = tableProp(tt, i))
                if (std::optional<TypeId> ty = propType(entry->second))
                    if (LuauTypes* owner = ownerOf(t))
                        return wrapType(owner, *ty);
    } catch (const std::exception&) {
    }
    return nullptr;
}

extern "C" int luau_type_table_has_indexer(LuauType* t) {
    if (!t)
        return 0;
    try {
        if (const TableType* tt = asTable(t->id))
            return tt->indexer ? 1 : 0;
    } catch (const std::exception&) {
    }
    return 0;
}

extern "C" LuauType* luau_type_metatable_table(LuauType* t) {
    if (!t)
        return nullptr;
    try {
        if (const MetatableType* m = get<MetatableType>(follow(t->id)))
            if (LuauTypes* owner = ownerOf(t))
                return wrapType(owner, m->table);
    } catch (const std::exception&) {
    }
    return nullptr;
}

extern "C" LuauType* luau_type_metatable_metatable(LuauType* t) {
    if (!t)
        return nullptr;
    try {
        if (const MetatableType* m = get<MetatableType>(follow(t->id)))
            if (LuauTypes* owner = ownerOf(t))
                return wrapType(owner, m->metatable);
    } catch (const std::exception&) {
    }
    return nullptr;
}

extern "C" int luau_type_union_count(LuauType* t) {
    if (!t)
        return 0;
    try {
        if (const UnionType* u = get<UnionType>(follow(t->id)))
            return static_cast<int>(u->options.size());
    } catch (const std::exception&) {
    }
    return 0;
}

extern "C" LuauType* luau_type_union_at(LuauType* t, int i) {
    if (!t)
        return nullptr;
    try {
        if (const UnionType* u = get<UnionType>(follow(t->id)))
            if (i >= 0 && static_cast<size_t>(i) < u->options.size())
                if (LuauTypes* owner = ownerOf(t))
                    return wrapType(owner, u->options[i]);
    } catch (const std::exception&) {
    }
    return nullptr;
}

extern "C" int luau_type_intersection_count(LuauType* t) {
    if (!t)
        return 0;
    try {
        if (const IntersectionType* it = get<IntersectionType>(follow(t->id)))
            return static_cast<int>(it->parts.size());
    } catch (const std::exception&) {
    }
    return 0;
}

extern "C" LuauType* luau_type_intersection_at(LuauType* t, int i) {
    if (!t)
        return nullptr;
    try {
        if (const IntersectionType* it = get<IntersectionType>(follow(t->id)))
            if (i >= 0 && static_cast<size_t>(i) < it->parts.size())
                if (LuauTypes* owner = ownerOf(t))
                    return wrapType(owner, it->parts[i]);
    } catch (const std::exception&) {
    }
    return nullptr;
}

extern "C" const char* luau_type_class_name(LuauType* t) {
    if (!t)
        return nullptr;
    try {
        if (const ExternType* c = get<ExternType>(follow(t->id)))
            if (LuauTypes* owner = ownerOf(t))
                return owner->intern(c->name);
    } catch (const std::exception&) {
    }
    return nullptr;
}

extern "C" LuauType* luau_type_class_parent(LuauType* t) {
    if (!t)
        return nullptr;
    try {
        if (const ExternType* c = get<ExternType>(follow(t->id)))
            if (c->parent)
                if (LuauTypes* owner = ownerOf(t))
                    return wrapType(owner, *c->parent);
    } catch (const std::exception&) {
    }
    return nullptr;
}

extern "C" LuauType* luau_type_negation_inner(LuauType* t) {
    if (!t)
        return nullptr;
    try {
        if (const NegationType* n = get<NegationType>(follow(t->id)))
            if (LuauTypes* owner = ownerOf(t))
                return wrapType(owner, n->ty);
    } catch (const std::exception&) {
    }
    return nullptr;
}

extern "C" const char* luau_type_generic_name(LuauType* t) {
    if (!t)
        return nullptr;
    try {
        if (const GenericType* g = get<GenericType>(follow(t->id)))
            if (LuauTypes* owner = ownerOf(t))
                return owner->intern(g->name);
    } catch (const std::exception&) {
    }
    return nullptr;
}

// ---- type pack inspection --------------------------------------------------

extern "C" int luau_typepack_count(LuauTypePack* tp) {
    if (!tp)
        return 0;
    try {
        int n = 0;
        for (auto it = begin(tp->id); it != end(tp->id); ++it)
            ++n;
        return n;
    } catch (const std::exception&) {
    }
    return 0;
}

extern "C" LuauType* luau_typepack_at(LuauTypePack* tp, int i) {
    if (!tp || i < 0)
        return nullptr;
    try {
        LuauTypes* owner = ownerOf(tp);
        if (!owner)
            return nullptr;
        int n = 0;
        for (auto it = begin(tp->id); it != end(tp->id); ++it, ++n)
            if (n == i)
                return wrapType(owner, *it);
    } catch (const std::exception&) {
    }
    return nullptr;
}

extern "C" LuauTypePack* luau_typepack_tail(LuauTypePack* tp) {
    if (!tp)
        return nullptr;
    try {
        LuauTypes* owner = ownerOf(tp);
        if (!owner)
            return nullptr;
        auto it = begin(tp->id);
        auto e = end(tp->id);
        while (it != e)
            ++it;
        if (std::optional<TypePackId> tail = it.tail())
            return wrapPack(owner, *tail);
    } catch (const std::exception&) {
    }
    return nullptr;
}

// ---- internal bridge (types_internal.h) ------------------------------------
//
// Lets sibling Analysis shims reuse this module's handle cache + checker context.

TypeId luau_types_internal_typeid(LuauType* h) {
    return h ? h->id : nullptr;
}

TypePackId luau_types_internal_packid(LuauTypePack* h) {
    return h ? h->id : nullptr;
}

LuauType* luau_types_internal_wrap_type(LuauTypes* owner, TypeId id) {
    if (!owner)
        return nullptr;
    return wrapType(owner, id);
}

LuauTypePack* luau_types_internal_wrap_pack(LuauTypes* owner, TypePackId id) {
    if (!owner)
        return nullptr;
    return wrapPack(owner, id);
}

LuauTypes* luau_types_internal_owner_of_type(const LuauType* h) {
    return ownerOf(h);
}

LuauTypes* luau_types_internal_owner_of_pack(const LuauTypePack* h) {
    return ownerOf(h);
}

TypeArena* luau_types_internal_arena(LuauTypes* owner) {
    if (!owner || !owner->module)
        return nullptr;
    return &owner->module->internalTypes;
}

BuiltinTypes* luau_types_internal_builtins(LuauTypes* owner) {
    if (!owner || !owner->frontend)
        return nullptr;
    return owner->frontend->builtinTypes.get();
}

Scope* luau_types_internal_scope(LuauTypes* owner) {
    if (!owner || !owner->module || !owner->module->hasModuleScope())
        return nullptr;
    ScopePtr scope = owner->module->getModuleScope();
    return scope ? scope.get() : nullptr;
}

Module* luau_types_internal_module(LuauTypes* owner) {
    if (!owner)
        return nullptr;
    return owner->module.get();
}

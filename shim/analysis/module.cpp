// extern "C" shim: MODULE / SCOPE / DEFINITION inspection of a checked module.
//
// A LuauModule owns a LuauTypes checker (built via luau_types_check) so it can
// reuse the SAME LuauType*/LuauTypePack* handle model — the resulting handles
// therefore work with every accessor in "types.h". Module/Scope/Binding/TypeFun
// data is read directly off the checked Luau::Module and its top-level Scope.

#include "module.h"
#include "types.h"
#include "types_internal.h"

#include "Luau/Module.h"
#include "Luau/Scope.h"
#include "Luau/Symbol.h"
#include "Luau/Type.h"
#include "Luau/TypePack.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using namespace Luau;

namespace {

// Duplicate `s` into a malloc'd, NUL-terminated buffer the caller frees.
char* dupString(const std::string& s) {
    char* out = static_cast<char*>(std::malloc(s.size() + 1));
    if (!out)
        return nullptr;
    std::memcpy(out, s.data(), s.size());
    out[s.size()] = '\0';
    return out;
}

LuauModuleLocation toLoc(const Location& loc) {
    LuauModuleLocation out;
    out.begin_line = loc.begin.line;
    out.begin_column = loc.begin.column;
    out.end_line = loc.end.line;
    out.end_column = loc.end.column;
    return out;
}

} // namespace

// Opaque handle: owns the checker and a stable, flattened view of the top-level
// scope bindings + exported type bindings (the underlying maps are unordered, so
// we snapshot a deterministic index order and keep names alive here).
struct LuauModule {
    LuauTypes* checker = nullptr;

    struct BindingEntry {
        std::string name;
        TypeId type = nullptr;
        Location location{Position{0, 0}, Position{0, 0}};
        bool deprecated = false;
    };
    struct ExportEntry {
        std::string name;
        TypeId type = nullptr;
    };

    std::vector<BindingEntry> bindings;
    std::vector<ExportEntry> exports;
    bool snapshotted = false;

    ~LuauModule() {
        if (checker)
            luau_types_free(checker);
    }
};

// Build the deterministic snapshot of scope bindings / exported types lazily.
static void ensureSnapshot(LuauModule* h) {
    if (!h || h->snapshotted)
        return;
    h->snapshotted = true;
    try {
        Module* m = luau_types_internal_module(h->checker);
        if (!m)
            return;

        if (Scope* scope = luau_types_internal_scope(h->checker)) {
            for (const auto& kv : scope->bindings) {
                LuauModule::BindingEntry e;
                e.name = kv.first.c_str() ? kv.first.c_str() : "";
                e.type = kv.second.typeId;
                e.location = kv.second.location;
                e.deprecated = kv.second.deprecated;
                h->bindings.push_back(std::move(e));
            }
        }

        for (const auto& kv : m->exportedTypeBindings) {
            LuauModule::ExportEntry e;
            e.name = kv.first;
            e.type = kv.second.type;
            h->exports.push_back(std::move(e));
        }
    } catch (const std::exception&) {
    }
}

// ---- checker ---------------------------------------------------------------

extern "C" LuauModule* luau_module_check(const char* src, size_t len) {
    LuauModule* h = new LuauModule();
    try {
        h->checker = luau_types_check(src, len);
    } catch (const std::exception&) {
        h->checker = nullptr;
    }
    return h;
}

extern "C" void luau_module_free(LuauModule* h) {
    delete h;
}

// ---- module-level info -----------------------------------------------------

extern "C" char* luau_module_name(const LuauModule* h) {
    if (!h)
        return nullptr;
    try {
        if (Module* m = luau_types_internal_module(h->checker))
            return dupString(m->name);
    } catch (const std::exception&) {
    }
    return nullptr;
}

extern "C" char* luau_module_human_name(const LuauModule* h) {
    if (!h)
        return nullptr;
    try {
        if (Module* m = luau_types_internal_module(h->checker))
            return dupString(m->humanReadableName);
    } catch (const std::exception&) {
    }
    return nullptr;
}

extern "C" int luau_module_checked(const LuauModule* h) {
    if (!h)
        return 0;
    try {
        return luau_types_internal_module(h->checker) != nullptr ? 1 : 0;
    } catch (const std::exception&) {
    }
    return 0;
}

extern "C" int luau_module_error_count(const LuauModule* h) {
    if (!h || !h->checker)
        return 0;
    return luau_types_error_count(h->checker);
}

extern "C" const char* luau_module_error_message(const LuauModule* h, int i) {
    if (!h || !h->checker)
        return "";
    return luau_types_error_message(h->checker, i);
}

extern "C" LuauPosition luau_module_error_position(const LuauModule* h, int i) {
    if (!h || !h->checker)
        return LuauPosition{0, 0};
    return luau_types_error_position(h->checker, i);
}

extern "C" int luau_module_timed_out(const LuauModule* h) {
    if (!h)
        return 0;
    try {
        if (Module* m = luau_types_internal_module(h->checker))
            return m->timeout ? 1 : 0;
    } catch (const std::exception&) {
    }
    return 0;
}

extern "C" int luau_module_cancelled(const LuauModule* h) {
    if (!h)
        return 0;
    try {
        if (Module* m = luau_types_internal_module(h->checker))
            return m->cancelled ? 1 : 0;
    } catch (const std::exception&) {
    }
    return 0;
}

// ---- module return type ----------------------------------------------------

extern "C" LuauTypePack* luau_module_return_type(LuauModule* h) {
    if (!h)
        return nullptr;
    try {
        if (Module* m = luau_types_internal_module(h->checker))
            if (m->returnType)
                return luau_types_internal_wrap_pack(h->checker, m->returnType);
    } catch (const std::exception&) {
    }
    return nullptr;
}

// ---- top-level scope bindings ----------------------------------------------

extern "C" int luau_module_binding_count(LuauModule* h) {
    if (!h)
        return 0;
    ensureSnapshot(h);
    return static_cast<int>(h->bindings.size());
}

extern "C" const char* luau_module_binding_name(LuauModule* h, int i) {
    if (!h)
        return nullptr;
    ensureSnapshot(h);
    if (i < 0 || static_cast<size_t>(i) >= h->bindings.size())
        return nullptr;
    return h->bindings[i].name.c_str();
}

extern "C" LuauType* luau_module_binding_type(LuauModule* h, int i) {
    if (!h)
        return nullptr;
    ensureSnapshot(h);
    if (i < 0 || static_cast<size_t>(i) >= h->bindings.size())
        return nullptr;
    try {
        if (TypeId ty = h->bindings[i].type)
            return luau_types_internal_wrap_type(h->checker, ty);
    } catch (const std::exception&) {
    }
    return nullptr;
}

extern "C" LuauModuleLocation luau_module_binding_location(LuauModule* h, int i) {
    LuauModuleLocation empty{0, 0, 0, 0};
    if (!h)
        return empty;
    ensureSnapshot(h);
    if (i < 0 || static_cast<size_t>(i) >= h->bindings.size())
        return empty;
    return toLoc(h->bindings[i].location);
}

extern "C" int luau_module_binding_deprecated(LuauModule* h, int i) {
    if (!h)
        return 0;
    ensureSnapshot(h);
    if (i < 0 || static_cast<size_t>(i) >= h->bindings.size())
        return 0;
    return h->bindings[i].deprecated ? 1 : 0;
}

extern "C" LuauType* luau_module_binding_lookup(LuauModule* h, const char* name) {
    if (!h || !name || !h->checker)
        return nullptr;
    // Reuse the checker's own linear-search-by-name lookup.
    return luau_types_require_global(h->checker, name);
}

// ---- exported types --------------------------------------------------------

extern "C" int luau_module_exported_type_count(LuauModule* h) {
    if (!h)
        return 0;
    ensureSnapshot(h);
    return static_cast<int>(h->exports.size());
}

extern "C" const char* luau_module_exported_type_name(LuauModule* h, int i) {
    if (!h)
        return nullptr;
    ensureSnapshot(h);
    if (i < 0 || static_cast<size_t>(i) >= h->exports.size())
        return nullptr;
    return h->exports[i].name.c_str();
}

extern "C" LuauType* luau_module_exported_type(LuauModule* h, int i) {
    if (!h)
        return nullptr;
    ensureSnapshot(h);
    if (i < 0 || static_cast<size_t>(i) >= h->exports.size())
        return nullptr;
    try {
        if (TypeId ty = h->exports[i].type)
            return luau_types_internal_wrap_type(h->checker, ty);
    } catch (const std::exception&) {
    }
    return nullptr;
}

// extern "C" shim over AstAttr + Luau::findConfusable (Ast module).

#include "attributes.h"

#include "Luau/Allocator.h"
#include "Luau/Ast.h"
#include "Luau/Confusables.h"
#include "Luau/Lexer.h"
#include "Luau/ParseOptions.h"
#include "Luau/Parser.h"

#include <vector>

using namespace Luau;

namespace {

static LuauAttrType mapAttr(AstAttr::Type t) {
    switch (t) {
        case AstAttr::Type::Checked: return LUAU_ATTR_CHECKED;
        case AstAttr::Type::Native: return LUAU_ATTR_NATIVE;
        case AstAttr::Type::Deprecated: return LUAU_ATTR_DEPRECATED;
        case AstAttr::Type::DebugNoinline: return LUAU_ATTR_DEBUG_NOINLINE;
        default: return LUAU_ATTR_UNKNOWN;
    }
}

struct AttrRecord {
    LuauAttrType type;
    int function;
    LuauPosition position;
};

// Visits every function node and records its attributes. Each distinct function
// gets an incrementing id so callers can group its attributes.
struct AttrVisitor : AstVisitor {
    std::vector<AttrRecord>& out;
    int nextFunction = 0;

    explicit AttrVisitor(std::vector<AttrRecord>& o) : out(o) {}

    bool visit(AstExprFunction* node) override {
        int id = nextFunction++;
        for (AstAttr* attr : node->attributes) {
            if (!attr)
                continue;
            out.push_back(AttrRecord{
                mapAttr(attr->type),
                id,
                LuauPosition{attr->location.begin.line, attr->location.begin.column},
            });
        }
        return true; // recurse into nested functions
    }
};

} // namespace

struct LuauAttributes {
    Allocator allocator;
    AstNameTable names;
    std::vector<AttrRecord> records;
    bool ok = false;

    LuauAttributes() : allocator(), names(allocator) {}
};

extern "C" LuauAttributes* luau_attributes_parse(const char* src, size_t len) {
    auto* a = new LuauAttributes();
    try {
        ParseOptions options;
        ParseResult result = Parser::parse(src, len, a->names, a->allocator, options);
        a->ok = result.errors.empty();
        if (result.root) {
            AttrVisitor visitor(a->records);
            result.root->visit(&visitor);
        }
    } catch (...) {
        // leave records as-is; ok stays false
    }
    return a;
}

extern "C" int luau_attributes_parsed_ok(const LuauAttributes* a) {
    return a && a->ok ? 1 : 0;
}

extern "C" int luau_attributes_count(const LuauAttributes* a) {
    return a ? static_cast<int>(a->records.size()) : 0;
}

extern "C" int luau_attributes_type(const LuauAttributes* a, int i) {
    if (!a || i < 0 || i >= static_cast<int>(a->records.size()))
        return -1;
    return static_cast<int>(a->records[i].type);
}

extern "C" int luau_attributes_function(const LuauAttributes* a, int i) {
    if (!a || i < 0 || i >= static_cast<int>(a->records.size()))
        return -1;
    return a->records[i].function;
}

extern "C" LuauPosition luau_attributes_position(const LuauAttributes* a, int i) {
    if (!a || i < 0 || i >= static_cast<int>(a->records.size()))
        return LuauPosition{0, 0};
    return a->records[i].position;
}

extern "C" void luau_attributes_free(LuauAttributes* a) {
    delete a;
}

// ---- confusables -------------------------------------------------------------

extern "C" const char* luau_confusable_suggestion(unsigned int codepoint) {
    return findConfusable(codepoint);
}

extern "C" int luau_confusable_is(unsigned int codepoint) {
    return findConfusable(codepoint) != nullptr ? 1 : 0;
}

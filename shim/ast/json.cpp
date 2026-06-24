// extern "C" shim: parse a module and serialize its AST to JSON (Luau::toJson).

#include "json.h"

#include "Luau/AstJsonEncoder.h"
#include "Luau/Allocator.h"
#include "Luau/Ast.h"
#include "Luau/Lexer.h"
#include "Luau/ParseOptions.h"
#include "Luau/ParseResult.h"
#include "Luau/Parser.h"

#include <cstdlib>
#include <cstring>
#include <string>

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

} // namespace

extern "C" char* luau_ast_to_json(const char* src, size_t len, char** out_err) {
    if (out_err)
        *out_err = nullptr;

    try {
        Allocator allocator;
        AstNameTable names(allocator);
        ParseOptions options;

        ParseResult result = Parser::parse(src, len, names, allocator, options);

        if (!result.root || !result.errors.empty()) {
            std::string msg = result.errors.empty()
                ? std::string("failed to parse source")
                : result.errors.front().getMessage();
            if (out_err)
                *out_err = dupString(msg);
            return nullptr;
        }

        std::string json = toJson(result.root, result.commentLocations);
        return dupString(json);
    } catch (const std::exception& e) {
        if (out_err)
            *out_err = dupString(e.what());
        return nullptr;
    } catch (...) {
        if (out_err)
            *out_err = dupString("unknown error during AST JSON serialization");
        return nullptr;
    }
}

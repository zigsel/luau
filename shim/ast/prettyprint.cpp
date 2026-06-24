// extern "C" shim over Luau::prettyPrint (Ast module).

#include "prettyprint.h"

#include "Luau/ParseOptions.h"
#include "Luau/PrettyPrinter.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

namespace {

// Duplicate a std::string into a malloc'd NUL-terminated buffer.
static char* dupString(const std::string& s) {
    char* out = static_cast<char*>(std::malloc(s.size() + 1));
    if (!out)
        return nullptr;
    std::memcpy(out, s.data(), s.size());
    out[s.size()] = '\0';
    return out;
}

static char* dupCStr(const char* s) {
    size_t n = std::strlen(s);
    char* out = static_cast<char*>(std::malloc(n + 1));
    if (!out)
        return nullptr;
    std::memcpy(out, s, n + 1);
    return out;
}

} // namespace

extern "C" char* luau_ast_format(const char* src, size_t len, char** out_err) {
    try {
        std::string_view source(src, len);

        Luau::ParseOptions options;
        // The prettyPrint entry parses internally; capturing comments keeps the
        // output faithful where the transpiler can preserve them.
        options.captureComments = true;

        Luau::PrettyPrintResult result = Luau::prettyPrint(source, options, /*withTypes*/ false, /*ignoreParseErrors*/ false);

        if (!result.parseError.empty()) {
            if (out_err)
                *out_err = dupString(result.parseError);
            return nullptr;
        }

        return dupString(result.code);
    } catch (const std::exception& e) {
        if (out_err)
            *out_err = dupCStr(e.what());
        return nullptr;
    } catch (...) {
        if (out_err)
            *out_err = dupCStr("unknown error during pretty-printing");
        return nullptr;
    }
}

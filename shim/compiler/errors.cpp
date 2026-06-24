// extern "C" shim over Luau::compileOrThrow (Compiler module): structured
// compile errors rather than error-bytecode.

#include "errors.h"

#include "Luau/BytecodeBuilder.h"
#include "Luau/Compiler.h"
#include "Luau/ParseResult.h"

#include <string>
#include <vector>

using namespace Luau;

struct LuauCompileErrors {
    bool ok = false;
    std::vector<std::string> messages;
    std::vector<LuauPosition> positions;
};

static LuauPosition toPos(const Location& loc) {
    return LuauPosition{loc.begin.line, loc.begin.column};
}

extern "C" LuauCompileErrors* luau_compile_errors_check(const char* src, size_t len, int optimizationLevel, int debugLevel) {
    auto* e = new LuauCompileErrors();
    try {
        BytecodeBuilder bytecode;
        CompileOptions options;
        options.optimizationLevel = optimizationLevel;
        options.debugLevel = debugLevel;
        compileOrThrow(bytecode, std::string(src, len), options);
        e->ok = true;
    } catch (const CompileError& ce) {
        e->messages.emplace_back(ce.what());
        e->positions.push_back(toPos(ce.getLocation()));
    } catch (const ParseErrors& pe) {
        for (const ParseError& p : pe.getErrors()) {
            e->messages.push_back(p.getMessage());
            e->positions.push_back(toPos(p.getLocation()));
        }
        if (e->messages.empty()) {
            e->messages.emplace_back(pe.what());
            e->positions.push_back(LuauPosition{0, 0});
        }
    } catch (const std::exception& ex) {
        e->messages.emplace_back(ex.what());
        e->positions.push_back(LuauPosition{0, 0});
    } catch (...) {
        e->messages.emplace_back("unknown compile error");
        e->positions.push_back(LuauPosition{0, 0});
    }
    return e;
}

extern "C" int luau_compile_errors_ok(const LuauCompileErrors* e) {
    return e && e->ok ? 1 : 0;
}

extern "C" int luau_compile_errors_count(const LuauCompileErrors* e) {
    return e ? static_cast<int>(e->messages.size()) : 0;
}

extern "C" const char* luau_compile_errors_message(const LuauCompileErrors* e, int i) {
    if (!e || i < 0 || i >= static_cast<int>(e->messages.size()))
        return nullptr;
    return e->messages[i].c_str();
}

extern "C" LuauPosition luau_compile_errors_position(const LuauCompileErrors* e, int i) {
    if (!e || i < 0 || i >= static_cast<int>(e->positions.size()))
        return LuauPosition{0, 0};
    return e->positions[i];
}

extern "C" void luau_compile_errors_free(LuauCompileErrors* e) {
    delete e;
}

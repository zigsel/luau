// extern "C" shim over the Luau::CodeGen C++ API (CodeGen.h).

#include "codegen.h"

#include "Luau/CodeGen.h"
#include "Luau/CodeGenOptions.h"

#include <cstdlib>
#include <cstring>
#include <string>

using namespace Luau::CodeGen;

extern "C" int luau_codegen_compile2(lua_State* L, int idx, unsigned int flags, LuauCodegenStats* stats) {
    CompilationStats s;
    CompilationResult r = compile(L, idx, flags, &s);
    if (stats) {
        stats->bytecode_size = s.bytecodeSizeBytes;
        stats->native_code_size = s.nativeCodeSizeBytes;
        stats->native_data_size = s.nativeDataSizeBytes;
        stats->native_metadata_size = s.nativeMetadataSizeBytes;
        stats->functions_total = s.functionsTotal;
        stats->functions_compiled = s.functionsCompiled;
        stats->functions_bound = s.functionsBound;
    }
    return static_cast<int>(r.result);
}

extern "C" void luau_codegen_set_native_execution_enabled(lua_State* L, int enabled) {
    setNativeExecutionEnabled(L, enabled != 0);
}

extern "C" void luau_codegen_disable_native_for_function(lua_State* L, int level) {
    disableNativeExecutionForFunction(L, level);
}

extern "C" char* luau_codegen_get_assembly(lua_State* L, int idx, int include_assembly, int include_ir) {
    AssemblyOptions options;
    options.outputBinary = false;
    options.includeAssembly = include_assembly != 0;
    options.includeIr = include_ir != 0;
    std::string asmText = getAssembly(L, idx, options, nullptr);
    char* out = static_cast<char*>(std::malloc(asmText.size() + 1));
    if (!out) return nullptr;
    std::memcpy(out, asmText.data(), asmText.size());
    out[asmText.size()] = '\0';
    return out;
}

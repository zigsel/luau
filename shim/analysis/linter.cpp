// extern "C" shim over the Luau linter: rule enumeration + standalone lint.

#include "linter.h"

#include "Luau/BuiltinDefinitions.h"
#include "Luau/ConfigResolver.h"
#include "Luau/FileResolver.h"
#include "Luau/Frontend.h"
#include "Luau/Linter.h"
#include "Luau/LinterConfig.h"

#include <optional>
#include <string>
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

} // namespace

// ---- rule enumeration ------------------------------------------------------

extern "C" int luau_lint_rule_count(void) {
    return static_cast<int>(LintWarning::Code__Count);
}

extern "C" const char* luau_lint_rule_name(int i) {
    if (i < 0 || i >= static_cast<int>(LintWarning::Code__Count))
        return "";
    return LintWarning::getName(static_cast<LintWarning::Code>(i));
}

extern "C" int luau_lint_rule_code(int i) {
    if (i < 0 || i >= static_cast<int>(LintWarning::Code__Count))
        return -1;
    // The enumeration index *is* the LintWarning::Code value (contiguous 0..Count).
    return i;
}

// ---- standalone lint -------------------------------------------------------

struct LuauLint {
    std::vector<int> codes;
    std::vector<std::string> names;
    std::vector<std::string> messages;
    std::vector<LuauPosition> positions;
    std::vector<int> fatal;
};

extern "C" LuauLint* luau_lint_source(
    const char* src, size_t len, unsigned long long enabled_mask, unsigned long long fatal_mask) {
    LuauLint* h = new LuauLint();
    try {
        SingleFileResolver files;
        files.moduleName = "main";
        files.source.assign(src, len);

        NullConfigResolver config;
        // Drive fatal classification: warnings whose code is set in fatal_mask
        // are reported as errors (LintResult::errors).
        config.defaultConfig.fatalLint.warningMask = fatal_mask;

        FrontendOptions options;
        options.runLintChecks = true;
        // Full rule control: only rules whose bit is set in enabled_mask fire.
        LintOptions enabled;
        enabled.warningMask = enabled_mask;
        options.enabledLintWarnings = enabled;

        Frontend frontend(&files, &config, options);
        registerBuiltinGlobals(frontend, frontend.globals);

        CheckResult result = frontend.check("main");

        auto collect = [&](const std::vector<LintWarning>& ws, int isFatal) {
            for (const LintWarning& w : ws) {
                h->codes.push_back(static_cast<int>(w.code));
                h->names.push_back(LintWarning::getName(w.code));
                h->messages.push_back(w.text);
                LuauPosition p;
                p.line = w.location.begin.line;
                p.column = w.location.begin.column;
                h->positions.push_back(p);
                h->fatal.push_back(isFatal);
            }
        };
        collect(result.lintResult.errors, 1);
        collect(result.lintResult.warnings, 0);
    } catch (const std::exception&) {
        // leave empty on failure
    }
    return h;
}

extern "C" int luau_lint_count(const LuauLint* h) {
    return static_cast<int>(h->codes.size());
}
extern "C" int luau_lint_warning_code(const LuauLint* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->codes.size()) return -1;
    return h->codes[i];
}
extern "C" const char* luau_lint_warning_name(const LuauLint* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->names.size()) return "";
    return h->names[i].c_str();
}
extern "C" const char* luau_lint_warning_message(const LuauLint* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->messages.size()) return "";
    return h->messages[i].c_str();
}
extern "C" LuauPosition luau_lint_warning_position(const LuauLint* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->positions.size()) return LuauPosition{0, 0};
    return h->positions[i];
}
extern "C" int luau_lint_warning_fatal(const LuauLint* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->fatal.size()) return 0;
    return h->fatal[i];
}
extern "C" void luau_lint_free(LuauLint* h) {
    delete h;
}

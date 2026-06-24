// extern "C" shim over Luau::parseConfig (Config module).

#include "config.h"

#include "Luau/Config.h"
#include "Luau/LinterConfig.h"

#include <cctype>
#include <string>
#include <vector>

struct LuauConfig {
    Luau::Config config;
    std::string error;
    bool hasError = false;
    // Snapshotted aliases (the underlying DenseHashMap iteration order is stable
    // for a given config, but we materialise it for simple indexed access).
    std::vector<std::string> aliasNames;     // original case (AliasInfo::originalCase)
    std::vector<std::string> aliasKeys;      // case-folded lookup key (the map key)
    std::vector<std::string> aliasValues;
    std::vector<std::string> aliasLocations; // AliasInfo::configLocation
};

extern "C" LuauConfig* luau_config_parse(const char* contents, size_t len) {
    LuauConfig* h = new LuauConfig();
    std::string s(contents, len);
    // Enable alias parsing: parseConfig ignores `aliases` unless aliasOptions
    // is provided. We supply an empty config location and disallow overwriting.
    Luau::ConfigOptions options;
    options.aliasOptions = Luau::ConfigOptions::AliasOptions{std::nullopt, false};
    std::optional<std::string> err = Luau::parseConfig(s, h->config, options);
    if (err) {
        h->error = *err;
        h->hasError = true;
    }
    for (const auto& kv : h->config.aliases) {
        h->aliasNames.push_back(kv.second.originalCase);
        h->aliasKeys.push_back(kv.first);
        h->aliasValues.push_back(kv.second.value);
        h->aliasLocations.push_back(std::string(kv.second.configLocation));
    }
    return h;
}

extern "C" const char* luau_config_error(const LuauConfig* h) {
    return h->hasError ? h->error.c_str() : nullptr;
}

extern "C" int luau_config_mode(const LuauConfig* h) {
    return static_cast<int>(h->config.mode);
}

extern "C" int luau_config_global_count(const LuauConfig* h) {
    return static_cast<int>(h->config.globals.size());
}

extern "C" const char* luau_config_global(const LuauConfig* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->config.globals.size())
        return "";
    return h->config.globals[i].c_str();
}

extern "C" int luau_config_lint_errors(const LuauConfig* h) {
    return h->config.lintErrors ? 1 : 0;
}

extern "C" int luau_config_type_errors(const LuauConfig* h) {
    return h->config.typeErrors ? 1 : 0;
}

extern "C" int luau_config_alias_count(const LuauConfig* h) {
    return static_cast<int>(h->aliasNames.size());
}

extern "C" const char* luau_config_alias_name(const LuauConfig* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->aliasNames.size())
        return "";
    return h->aliasNames[i].c_str();
}

extern "C" const char* luau_config_alias_value(const LuauConfig* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->aliasValues.size())
        return "";
    return h->aliasValues[i].c_str();
}

// Case-folded lookup key (the key Luau resolves a `require("@key/...")` against).
extern "C" const char* luau_config_alias_key(const LuauConfig* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->aliasKeys.size())
        return "";
    return h->aliasKeys[i].c_str();
}

// The `.luaurc` location this alias was declared in (may be empty).
extern "C" const char* luau_config_alias_location(const LuauConfig* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->aliasLocations.size())
        return "";
    return h->aliasLocations[i].c_str();
}

// Case-insensitively resolve an alias name to its target value, or NULL if none.
extern "C" const char* luau_config_alias_resolve(const LuauConfig* h, const char* name) {
    if (!name) return nullptr;
    std::string key;
    for (const char* p = name; *p; ++p)
        key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
    for (size_t i = 0; i < h->aliasKeys.size(); ++i)
        if (h->aliasKeys[i] == key)
            return h->aliasValues[i].c_str();
    return nullptr;
}

extern "C" int luau_config_is_valid_alias(const char* name) {
    try {
        return Luau::isValidAlias(std::string(name ? name : "")) ? 1 : 0;
    } catch (const std::exception&) {
        return 0;
    }
}

extern "C" int luau_config_lint_rule_count(void) {
    return static_cast<int>(Luau::LintWarning::Code__Count);
}

extern "C" const char* luau_config_lint_rule_name(int i) {
    if (i < 0 || i >= static_cast<int>(Luau::LintWarning::Code__Count))
        return "";
    return Luau::LintWarning::getName(static_cast<Luau::LintWarning::Code>(i));
}

extern "C" int luau_config_lint_rule_enabled(const LuauConfig* h, int i) {
    if (i < 0 || i >= static_cast<int>(Luau::LintWarning::Code__Count))
        return 0;
    return h->config.enabledLint.isEnabled(static_cast<Luau::LintWarning::Code>(i)) ? 1 : 0;
}

extern "C" int luau_config_lint_rule_fatal(const LuauConfig* h, int i) {
    if (i < 0 || i >= static_cast<int>(Luau::LintWarning::Code__Count))
        return 0;
    return h->config.fatalLint.isEnabled(static_cast<Luau::LintWarning::Code>(i)) ? 1 : 0;
}

extern "C" void luau_config_free(LuauConfig* h) {
    delete h;
}

// extern "C" shim over the host-free require-suggestion data carriers
// (RequireSuggestion / RequireAlias from Luau/FileResolver.h).

#include "requirepath.h"

#include "Luau/FileResolver.h"

#include <string>
#include <vector>

using namespace Luau;

// Each handle owns a real upstream struct; the only divergence is that we keep
// a NUL-terminated copy of each tag so the C accessors can hand back `const
// char*` without per-call allocation.
struct LuauRequireSuggestion {
    RequireSuggestion data;
};

struct LuauRequireAlias {
    // RequireAlias has no default constructor; carry the same fields directly.
    std::string alias;
    std::vector<std::string> tags;
};

extern "C" LuauRequireSuggestion* luau_analysis_requiresuggestion_new(
    const char* label, size_t label_len, const char* full_path, size_t full_path_len) {
    LuauRequireSuggestion* h = new LuauRequireSuggestion();
    h->data.label.assign(label, label_len);
    h->data.fullPath.assign(full_path, full_path_len);
    return h;
}

extern "C" void luau_analysis_requiresuggestion_add_tag(
    LuauRequireSuggestion* h, const char* tag, size_t tag_len) {
    h->data.tags.emplace_back(tag, tag_len);
}

extern "C" const char* luau_analysis_requiresuggestion_label(const LuauRequireSuggestion* h) {
    return h->data.label.c_str();
}

extern "C" const char* luau_analysis_requiresuggestion_full_path(const LuauRequireSuggestion* h) {
    return h->data.fullPath.c_str();
}

extern "C" int luau_analysis_requiresuggestion_tag_count(const LuauRequireSuggestion* h) {
    return static_cast<int>(h->data.tags.size());
}

extern "C" const char* luau_analysis_requiresuggestion_tag(const LuauRequireSuggestion* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->data.tags.size())
        return "";
    return h->data.tags[i].c_str();
}

extern "C" void luau_analysis_requiresuggestion_free(LuauRequireSuggestion* h) {
    delete h;
}

extern "C" LuauRequireAlias* luau_analysis_requirealias_new(const char* name, size_t name_len) {
    LuauRequireAlias* h = new LuauRequireAlias();
    h->alias.assign(name, name_len);
    // Round-trip check against the upstream type to keep this binding honest:
    // constructing a real RequireAlias validates the field layout we mirror.
    RequireAlias probe(h->alias, h->tags);
    h->alias = probe.alias;
    h->tags = probe.tags;
    return h;
}

extern "C" void luau_analysis_requirealias_add_tag(
    LuauRequireAlias* h, const char* tag, size_t tag_len) {
    h->tags.emplace_back(tag, tag_len);
}

extern "C" const char* luau_analysis_requirealias_name(const LuauRequireAlias* h) {
    return h->alias.c_str();
}

extern "C" int luau_analysis_requirealias_tag_count(const LuauRequireAlias* h) {
    return static_cast<int>(h->tags.size());
}

extern "C" const char* luau_analysis_requirealias_tag(const LuauRequireAlias* h, int i) {
    if (i < 0 || static_cast<size_t>(i) >= h->tags.size())
        return "";
    return h->tags[i].c_str();
}

extern "C" void luau_analysis_requirealias_free(LuauRequireAlias* h) {
    delete h;
}

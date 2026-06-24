// Shim: structured access to Luau type-check diagnostics.
//
// `analysis.check` exposes only error MESSAGE strings. This module type-checks a
// single self-contained module and, per error, exposes a STABLE KIND enum (one
// value per TypeErrorData variant alternative), the error LOCATION, the already
// rendered MESSAGE, and the cheap typed string/name fields that do NOT require a
// live type printer (e.g. UnknownProperty.key, UnknownSymbol.name).
//
// Variants whose interesting fields are TypeId/TypePackId (which need a live type
// printer + arena) only expose kind + location + message here.
#pragma once

#include "common.h"
#include "handles.h"

LUAU_BEGIN_DECLS

// Owns the type-check result + the collected structured diagnostics.
LUAU_HANDLE(LuauDiagnostics);

// Stable kind enum. Values mirror the order of the TypeErrorData variant in
// Luau/Error.h; do NOT renumber existing entries. LUAU_DIAG_UNKNOWN (-1) is
// returned for an out-of-range index.
typedef enum LuauDiagKind {
    LUAU_DIAG_UNKNOWN = -1,
    LUAU_DIAG_TYPE_MISMATCH = 0,
    LUAU_DIAG_UNKNOWN_SYMBOL = 1,
    LUAU_DIAG_UNKNOWN_PROPERTY = 2,
    LUAU_DIAG_NOT_A_TABLE = 3,
    LUAU_DIAG_CANNOT_EXTEND_TABLE = 4,
    LUAU_DIAG_CANNOT_COMPARE_UNRELATED_TYPES = 5,
    LUAU_DIAG_ONLY_TABLES_CAN_HAVE_METHODS = 6,
    LUAU_DIAG_DUPLICATE_TYPE_DEFINITION = 7,
    LUAU_DIAG_COUNT_MISMATCH = 8,
    LUAU_DIAG_FUNCTION_DOES_NOT_TAKE_SELF = 9,
    LUAU_DIAG_FUNCTION_REQUIRES_SELF = 10,
    LUAU_DIAG_OCCURS_CHECK_FAILED = 11,
    LUAU_DIAG_UNKNOWN_REQUIRE = 12,
    LUAU_DIAG_INCORRECT_GENERIC_PARAMETER_COUNT = 13,
    LUAU_DIAG_SYNTAX_ERROR = 14,
    LUAU_DIAG_CODE_TOO_COMPLEX = 15,
    LUAU_DIAG_UNIFICATION_TOO_COMPLEX = 16,
    LUAU_DIAG_UNKNOWN_PROP_BUT_FOUND_LIKE_PROP = 17,
    LUAU_DIAG_GENERIC_ERROR = 18,
    LUAU_DIAG_INTERNAL_ERROR = 19,
    LUAU_DIAG_CONSTRAINT_SOLVING_INCOMPLETE = 20,
    LUAU_DIAG_CANNOT_CALL_NON_FUNCTION = 21,
    LUAU_DIAG_EXTRA_INFORMATION = 22,
    LUAU_DIAG_DEPRECATED_API_USED = 23,
    LUAU_DIAG_MODULE_HAS_CYCLIC_DEPENDENCY = 24,
    LUAU_DIAG_ILLEGAL_REQUIRE = 25,
    LUAU_DIAG_FUNCTION_EXITS_WITHOUT_RETURNING = 26,
    LUAU_DIAG_DUPLICATE_GENERIC_PARAMETER = 27,
    LUAU_DIAG_CANNOT_ASSIGN_TO_NEVER = 28,
    LUAU_DIAG_CANNOT_INFER_BINARY_OPERATION = 29,
    LUAU_DIAG_MISSING_PROPERTIES = 30,
    LUAU_DIAG_SWAPPED_GENERIC_TYPE_PARAMETER = 31,
    LUAU_DIAG_OPTIONAL_VALUE_ACCESS = 32,
    LUAU_DIAG_MISSING_UNION_PROPERTY = 33,
    LUAU_DIAG_TYPES_ARE_UNRELATED = 34,
    LUAU_DIAG_NORMALIZATION_TOO_COMPLEX = 35,
    LUAU_DIAG_TYPE_PACK_MISMATCH = 36,
    LUAU_DIAG_DYNAMIC_PROPERTY_LOOKUP_ON_EXTERN_TYPES_UNSAFE = 37,
    LUAU_DIAG_UNINHABITED_TYPE_FUNCTION = 38,
    LUAU_DIAG_UNINHABITED_TYPE_PACK_FUNCTION = 39,
    LUAU_DIAG_WHERE_CLAUSE_NEEDED = 40,
    LUAU_DIAG_PACK_WHERE_CLAUSE_NEEDED = 41,
    LUAU_DIAG_CHECKED_FUNCTION_CALL_ERROR = 42,
    LUAU_DIAG_NON_STRICT_FUNCTION_DEFINITION_ERROR = 43,
    LUAU_DIAG_PROPERTY_ACCESS_VIOLATION = 44,
    LUAU_DIAG_CHECKED_FUNCTION_INCORRECT_ARGS = 45,
    LUAU_DIAG_UNEXPECTED_TYPE_IN_SUBTYPING = 46,
    LUAU_DIAG_UNEXPECTED_TYPE_PACK_IN_SUBTYPING = 47,
    LUAU_DIAG_EXPLICIT_FUNCTION_ANNOTATION_RECOMMENDED = 48,
    LUAU_DIAG_USER_DEFINED_TYPE_FUNCTION_ERROR = 49,
    LUAU_DIAG_BUILT_IN_TYPE_FUNCTION_ERROR = 50,
    LUAU_DIAG_RESERVED_IDENTIFIER = 51,
    LUAU_DIAG_UNEXPECTED_ARRAY_LIKE_TABLE_ITEM = 52,
    LUAU_DIAG_CANNOT_CHECK_DYNAMIC_STRING_FORMAT_CALLS = 53,
    LUAU_DIAG_GENERIC_TYPE_COUNT_MISMATCH = 54,
    LUAU_DIAG_GENERIC_TYPE_PACK_COUNT_MISMATCH = 55,
    LUAU_DIAG_MULTIPLE_NONVIABLE_OVERLOADS = 56,
    LUAU_DIAG_RECURSIVE_RESTRAINT_VIOLATION = 57,
    LUAU_DIAG_GENERIC_BOUNDS_MISMATCH = 58,
    LUAU_DIAG_UNAPPLIED_TYPE_FUNCTION = 59,
    LUAU_DIAG_INSTANTIATE_GENERICS_ON_NON_FUNCTION = 60,
    LUAU_DIAG_TYPE_INSTANTIATION_COUNT_MISMATCH = 61,
    LUAU_DIAG_AMBIGUOUS_FUNCTION_CALL = 62,
} LuauDiagKind;

// Type-check a single self-contained Luau `src` module and collect structured
// diagnostics. Always returns a handle; free with free().
LuauDiagnostics* luau_analysis_diagnostics_check(const char* src, size_t src_len);

// Number of type errors.
int luau_analysis_diagnostics_count(const LuauDiagnostics* h);

// Stable kind enum (LuauDiagKind) for error `i`, or LUAU_DIAG_UNKNOWN (-1).
int luau_analysis_diagnostics_kind(const LuauDiagnostics* h, int i);

// 0-based start position for error `i`, or {0,0} if out of range.
LuauPosition luau_analysis_diagnostics_position(const LuauDiagnostics* h, int i);

// 0-based end position for error `i`, or {0,0} if out of range.
LuauPosition luau_analysis_diagnostics_end_position(const LuauDiagnostics* h, int i);

// Fully rendered message for error `i` (always available), or "".
const char* luau_analysis_diagnostics_message(const LuauDiagnostics* h, int i);

// Cheap typed string field for error `i`, when the variant has a name/key/
// message field that needs no live type printer. Returns the relevant string:
//   UnknownSymbol.name, UnknownProperty.key, UnknownPropButFoundLikeProp.key,
//   MissingUnionProperty.key, DuplicateTypeDefinition.name,
//   IncorrectGenericParameterCount.name, GenericError.message,
//   InternalError.message, SyntaxError.message, ExtraInformation.message,
//   UnknownRequire.modulePath, ReservedIdentifier.name, ...
// Returns "" when the variant has no such cheap field. `has_field` (below) tells
// the two cases apart from an empty value.
const char* luau_analysis_diagnostics_field(const LuauDiagnostics* h, int i);

// Whether error `i`'s variant exposes a cheap typed string field (1) or not (0).
int luau_analysis_diagnostics_has_field(const LuauDiagnostics* h, int i);

void luau_analysis_diagnostics_free(LuauDiagnostics* h);

LUAU_END_DECLS

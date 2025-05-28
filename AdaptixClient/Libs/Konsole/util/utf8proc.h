#ifndef UTF8PROC_H
#define UTF8PROC_H

#define UTF8PROC_VERSION_MAJOR 2
#define UTF8PROC_VERSION_MINOR 9
#define UTF8PROC_VERSION_PATCH 0

#include <stdlib.h>

#if defined(_MSC_VER) && _MSC_VER < 1800
// MSVC prior to 2013 lacked stdbool.h and stdint.h
typedef signed char utf8proc_int8_t;
typedef unsigned char utf8proc_uint8_t;
typedef short utf8proc_int16_t;
typedef unsigned short utf8proc_uint16_t;
typedef int utf8proc_int32_t;
typedef unsigned int utf8proc_uint32_t;
#  ifdef _WIN64
typedef __int64 utf8proc_ssize_t;
typedef unsigned __int64 utf8proc_size_t;
#  else
typedef int utf8proc_ssize_t;
typedef unsigned int utf8proc_size_t;
#  endif
#  ifndef __cplusplus
// emulate C99 bool
typedef unsigned char utf8proc_bool;
#    ifndef __bool_true_false_are_defined
#      define false 0
#      define true 1
#      define __bool_true_false_are_defined 1
#    endif
#  else
typedef bool utf8proc_bool;
#  endif
#else
#  include <stddef.h>
#  include <stdbool.h>
#  include <stdint.h>
typedef int8_t utf8proc_int8_t;
typedef uint8_t utf8proc_uint8_t;
typedef int16_t utf8proc_int16_t;
typedef uint16_t utf8proc_uint16_t;
typedef int32_t utf8proc_int32_t;
typedef uint32_t utf8proc_uint32_t;
typedef size_t utf8proc_size_t;
typedef ptrdiff_t utf8proc_ssize_t;
typedef bool utf8proc_bool;
#endif

#  define UTF8PROC_DLLEXPORT

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  UTF8PROC_NULLTERM  = (1<<0),
  UTF8PROC_STABLE    = (1<<1),
  UTF8PROC_COMPAT    = (1<<2),
  UTF8PROC_COMPOSE   = (1<<3),
  UTF8PROC_DECOMPOSE = (1<<4),
  UTF8PROC_IGNORE    = (1<<5),
  UTF8PROC_REJECTNA  = (1<<6),
  UTF8PROC_NLF2LS    = (1<<7),
  UTF8PROC_NLF2PS    = (1<<8),
  UTF8PROC_NLF2LF    = (UTF8PROC_NLF2LS | UTF8PROC_NLF2PS),
  UTF8PROC_STRIPCC   = (1<<9),
  UTF8PROC_CASEFOLD  = (1<<10),
  UTF8PROC_CHARBOUND = (1<<11),
  UTF8PROC_LUMP      = (1<<12),
  UTF8PROC_STRIPMARK = (1<<13),
  UTF8PROC_STRIPNA    = (1<<14),
} utf8proc_option_t;

#define UTF8PROC_ERROR_NOMEM -1
#define UTF8PROC_ERROR_OVERFLOW -2
#define UTF8PROC_ERROR_INVALIDUTF8 -3
#define UTF8PROC_ERROR_NOTASSIGNED -4
#define UTF8PROC_ERROR_INVALIDOPTS -5

typedef utf8proc_int16_t utf8proc_propval_t;

typedef struct utf8proc_property_struct {
  utf8proc_propval_t category;
  utf8proc_propval_t combining_class;
  utf8proc_propval_t bidi_class;
  utf8proc_propval_t decomp_type;
  utf8proc_uint16_t decomp_seqindex;
  utf8proc_uint16_t casefold_seqindex;
  utf8proc_uint16_t uppercase_seqindex;
  utf8proc_uint16_t lowercase_seqindex;
  utf8proc_uint16_t titlecase_seqindex;
  utf8proc_uint16_t comb_index;
  unsigned bidi_mirrored:1;
  unsigned comp_exclusion:1;
  unsigned ignorable:1;
  unsigned control_boundary:1;
  unsigned charwidth:2;
  unsigned pad:2;
  unsigned boundclass:6;
  unsigned indic_conjunct_break:2;
} utf8proc_property_t;

typedef enum {
  UTF8PROC_CATEGORY_CN  = 0, /**< Other, not assigned */
  UTF8PROC_CATEGORY_LU  = 1, /**< Letter, uppercase */
  UTF8PROC_CATEGORY_LL  = 2, /**< Letter, lowercase */
  UTF8PROC_CATEGORY_LT  = 3, /**< Letter, titlecase */
  UTF8PROC_CATEGORY_LM  = 4, /**< Letter, modifier */
  UTF8PROC_CATEGORY_LO  = 5, /**< Letter, other */
  UTF8PROC_CATEGORY_MN  = 6, /**< Mark, nonspacing */
  UTF8PROC_CATEGORY_MC  = 7, /**< Mark, spacing combining */
  UTF8PROC_CATEGORY_ME  = 8, /**< Mark, enclosing */
  UTF8PROC_CATEGORY_ND  = 9, /**< Number, decimal digit */
  UTF8PROC_CATEGORY_NL = 10, /**< Number, letter */
  UTF8PROC_CATEGORY_NO = 11, /**< Number, other */
  UTF8PROC_CATEGORY_PC = 12, /**< Punctuation, connector */
  UTF8PROC_CATEGORY_PD = 13, /**< Punctuation, dash */
  UTF8PROC_CATEGORY_PS = 14, /**< Punctuation, open */
  UTF8PROC_CATEGORY_PE = 15, /**< Punctuation, close */
  UTF8PROC_CATEGORY_PI = 16, /**< Punctuation, initial quote */
  UTF8PROC_CATEGORY_PF = 17, /**< Punctuation, final quote */
  UTF8PROC_CATEGORY_PO = 18, /**< Punctuation, other */
  UTF8PROC_CATEGORY_SM = 19, /**< Symbol, math */
  UTF8PROC_CATEGORY_SC = 20, /**< Symbol, currency */
  UTF8PROC_CATEGORY_SK = 21, /**< Symbol, modifier */
  UTF8PROC_CATEGORY_SO = 22, /**< Symbol, other */
  UTF8PROC_CATEGORY_ZS = 23, /**< Separator, space */
  UTF8PROC_CATEGORY_ZL = 24, /**< Separator, line */
  UTF8PROC_CATEGORY_ZP = 25, /**< Separator, paragraph */
  UTF8PROC_CATEGORY_CC = 26, /**< Other, control */
  UTF8PROC_CATEGORY_CF = 27, /**< Other, format */
  UTF8PROC_CATEGORY_CS = 28, /**< Other, surrogate */
  UTF8PROC_CATEGORY_CO = 29, /**< Other, private use */
} utf8proc_category_t;

typedef enum {
  UTF8PROC_BIDI_CLASS_L     = 1, /**< Left-to-Right */
  UTF8PROC_BIDI_CLASS_LRE   = 2, /**< Left-to-Right Embedding */
  UTF8PROC_BIDI_CLASS_LRO   = 3, /**< Left-to-Right Override */
  UTF8PROC_BIDI_CLASS_R     = 4, /**< Right-to-Left */
  UTF8PROC_BIDI_CLASS_AL    = 5, /**< Right-to-Left Arabic */
  UTF8PROC_BIDI_CLASS_RLE   = 6, /**< Right-to-Left Embedding */
  UTF8PROC_BIDI_CLASS_RLO   = 7, /**< Right-to-Left Override */
  UTF8PROC_BIDI_CLASS_PDF   = 8, /**< Pop Directional Format */
  UTF8PROC_BIDI_CLASS_EN    = 9, /**< European Number */
  UTF8PROC_BIDI_CLASS_ES   = 10, /**< European Separator */
  UTF8PROC_BIDI_CLASS_ET   = 11, /**< European Number Terminator */
  UTF8PROC_BIDI_CLASS_AN   = 12, /**< Arabic Number */
  UTF8PROC_BIDI_CLASS_CS   = 13, /**< Common Number Separator */
  UTF8PROC_BIDI_CLASS_NSM  = 14, /**< Nonspacing Mark */
  UTF8PROC_BIDI_CLASS_BN   = 15, /**< Boundary Neutral */
  UTF8PROC_BIDI_CLASS_B    = 16, /**< Paragraph Separator */
  UTF8PROC_BIDI_CLASS_S    = 17, /**< Segment Separator */
  UTF8PROC_BIDI_CLASS_WS   = 18, /**< Whitespace */
  UTF8PROC_BIDI_CLASS_ON   = 19, /**< Other Neutrals */
  UTF8PROC_BIDI_CLASS_LRI  = 20, /**< Left-to-Right Isolate */
  UTF8PROC_BIDI_CLASS_RLI  = 21, /**< Right-to-Left Isolate */
  UTF8PROC_BIDI_CLASS_FSI  = 22, /**< First Strong Isolate */
  UTF8PROC_BIDI_CLASS_PDI  = 23, /**< Pop Directional Isolate */
} utf8proc_bidi_class_t;

typedef enum {
  UTF8PROC_DECOMP_TYPE_FONT      = 1, /**< Font */
  UTF8PROC_DECOMP_TYPE_NOBREAK   = 2, /**< Nobreak */
  UTF8PROC_DECOMP_TYPE_INITIAL   = 3, /**< Initial */
  UTF8PROC_DECOMP_TYPE_MEDIAL    = 4, /**< Medial */
  UTF8PROC_DECOMP_TYPE_FINAL     = 5, /**< Final */
  UTF8PROC_DECOMP_TYPE_ISOLATED  = 6, /**< Isolated */
  UTF8PROC_DECOMP_TYPE_CIRCLE    = 7, /**< Circle */
  UTF8PROC_DECOMP_TYPE_SUPER     = 8, /**< Super */
  UTF8PROC_DECOMP_TYPE_SUB       = 9, /**< Sub */
  UTF8PROC_DECOMP_TYPE_VERTICAL = 10, /**< Vertical */
  UTF8PROC_DECOMP_TYPE_WIDE     = 11, /**< Wide */
  UTF8PROC_DECOMP_TYPE_NARROW   = 12, /**< Narrow */
  UTF8PROC_DECOMP_TYPE_SMALL    = 13, /**< Small */
  UTF8PROC_DECOMP_TYPE_SQUARE   = 14, /**< Square */
  UTF8PROC_DECOMP_TYPE_FRACTION = 15, /**< Fraction */
  UTF8PROC_DECOMP_TYPE_COMPAT   = 16, /**< Compat */
} utf8proc_decomp_type_t;

typedef enum {
  UTF8PROC_BOUNDCLASS_START              =  0, /**< Start */
  UTF8PROC_BOUNDCLASS_OTHER              =  1, /**< Other */
  UTF8PROC_BOUNDCLASS_CR                 =  2, /**< Cr */
  UTF8PROC_BOUNDCLASS_LF                 =  3, /**< Lf */
  UTF8PROC_BOUNDCLASS_CONTROL            =  4, /**< Control */
  UTF8PROC_BOUNDCLASS_EXTEND             =  5, /**< Extend */
  UTF8PROC_BOUNDCLASS_L                  =  6, /**< L */
  UTF8PROC_BOUNDCLASS_V                  =  7, /**< V */
  UTF8PROC_BOUNDCLASS_T                  =  8, /**< T */
  UTF8PROC_BOUNDCLASS_LV                 =  9, /**< Lv */
  UTF8PROC_BOUNDCLASS_LVT                = 10, /**< Lvt */
  UTF8PROC_BOUNDCLASS_REGIONAL_INDICATOR = 11, /**< Regional indicator */
  UTF8PROC_BOUNDCLASS_SPACINGMARK        = 12, /**< Spacingmark */
  UTF8PROC_BOUNDCLASS_PREPEND            = 13, /**< Prepend */
  UTF8PROC_BOUNDCLASS_ZWJ                = 14, /**< Zero Width Joiner */

  UTF8PROC_BOUNDCLASS_E_BASE             = 15, /**< Emoji Base */
  UTF8PROC_BOUNDCLASS_E_MODIFIER         = 16, /**< Emoji Modifier */
  UTF8PROC_BOUNDCLASS_GLUE_AFTER_ZWJ     = 17, /**< Glue_After_ZWJ */
  UTF8PROC_BOUNDCLASS_E_BASE_GAZ         = 18, /**< E_BASE + GLUE_AFTER_ZJW */

  UTF8PROC_BOUNDCLASS_EXTENDED_PICTOGRAPHIC = 19,
  UTF8PROC_BOUNDCLASS_E_ZWG = 20, /* UTF8PROC_BOUNDCLASS_EXTENDED_PICTOGRAPHIC + ZWJ */
} utf8proc_boundclass_t;

typedef enum {
  UTF8PROC_INDIC_CONJUNCT_BREAK_NONE = 0,
  UTF8PROC_INDIC_CONJUNCT_BREAK_LINKER = 1,
  UTF8PROC_INDIC_CONJUNCT_BREAK_CONSONANT = 2,
  UTF8PROC_INDIC_CONJUNCT_BREAK_EXTEND = 3,
} utf8proc_indic_conjunct_break_t;

typedef utf8proc_int32_t (*utf8proc_custom_func)(utf8proc_int32_t codepoint, void *data);

UTF8PROC_DLLEXPORT extern const utf8proc_int8_t utf8proc_utf8class[256];

UTF8PROC_DLLEXPORT const char *utf8proc_version(void);

UTF8PROC_DLLEXPORT const char *utf8proc_unicode_version(void);

UTF8PROC_DLLEXPORT const char *utf8proc_errmsg(utf8proc_ssize_t errcode);

UTF8PROC_DLLEXPORT utf8proc_ssize_t utf8proc_iterate(const utf8proc_uint8_t *str, utf8proc_ssize_t strlen, utf8proc_int32_t *codepoint_ref);

UTF8PROC_DLLEXPORT utf8proc_bool utf8proc_codepoint_valid(utf8proc_int32_t codepoint);

UTF8PROC_DLLEXPORT utf8proc_ssize_t utf8proc_encode_char(utf8proc_int32_t codepoint, utf8proc_uint8_t *dst);

UTF8PROC_DLLEXPORT const utf8proc_property_t *utf8proc_get_property(utf8proc_int32_t codepoint);

UTF8PROC_DLLEXPORT utf8proc_ssize_t utf8proc_decompose_char(
  utf8proc_int32_t codepoint, utf8proc_int32_t *dst, utf8proc_ssize_t bufsize,
  utf8proc_option_t options, int *last_boundclass
);

UTF8PROC_DLLEXPORT utf8proc_ssize_t utf8proc_decompose(
  const utf8proc_uint8_t *str, utf8proc_ssize_t strlen,
  utf8proc_int32_t *buffer, utf8proc_ssize_t bufsize, utf8proc_option_t options
);

UTF8PROC_DLLEXPORT utf8proc_ssize_t utf8proc_decompose_custom(
  const utf8proc_uint8_t *str, utf8proc_ssize_t strlen,
  utf8proc_int32_t *buffer, utf8proc_ssize_t bufsize, utf8proc_option_t options,
  utf8proc_custom_func custom_func, void *custom_data
);

UTF8PROC_DLLEXPORT utf8proc_ssize_t utf8proc_normalize_utf32(utf8proc_int32_t *buffer, utf8proc_ssize_t length, utf8proc_option_t options);

UTF8PROC_DLLEXPORT utf8proc_ssize_t utf8proc_reencode(utf8proc_int32_t *buffer, utf8proc_ssize_t length, utf8proc_option_t options);

UTF8PROC_DLLEXPORT utf8proc_bool utf8proc_grapheme_break_stateful( utf8proc_int32_t codepoint1, utf8proc_int32_t codepoint2, utf8proc_int32_t *state);

UTF8PROC_DLLEXPORT utf8proc_bool utf8proc_grapheme_break( utf8proc_int32_t codepoint1, utf8proc_int32_t codepoint2);

UTF8PROC_DLLEXPORT utf8proc_int32_t utf8proc_tolower(utf8proc_int32_t c);

UTF8PROC_DLLEXPORT utf8proc_int32_t utf8proc_toupper(utf8proc_int32_t c);

UTF8PROC_DLLEXPORT utf8proc_int32_t utf8proc_totitle(utf8proc_int32_t c);

UTF8PROC_DLLEXPORT int utf8proc_islower(utf8proc_int32_t c);

UTF8PROC_DLLEXPORT int utf8proc_isupper(utf8proc_int32_t c);

UTF8PROC_DLLEXPORT int utf8proc_charwidth(utf8proc_int32_t codepoint);

UTF8PROC_DLLEXPORT utf8proc_category_t utf8proc_category(utf8proc_int32_t codepoint);

UTF8PROC_DLLEXPORT const char *utf8proc_category_string(utf8proc_int32_t codepoint);

UTF8PROC_DLLEXPORT utf8proc_ssize_t utf8proc_map( const utf8proc_uint8_t *str, utf8proc_ssize_t strlen, utf8proc_uint8_t **dstptr, utf8proc_option_t options );

UTF8PROC_DLLEXPORT utf8proc_ssize_t utf8proc_map_custom(
  const utf8proc_uint8_t *str, utf8proc_ssize_t strlen, utf8proc_uint8_t **dstptr, utf8proc_option_t options,
  utf8proc_custom_func custom_func, void *custom_data
);

UTF8PROC_DLLEXPORT utf8proc_uint8_t *utf8proc_NFD(const utf8proc_uint8_t *str);
UTF8PROC_DLLEXPORT utf8proc_uint8_t *utf8proc_NFC(const utf8proc_uint8_t *str);
UTF8PROC_DLLEXPORT utf8proc_uint8_t *utf8proc_NFKD(const utf8proc_uint8_t *str);
UTF8PROC_DLLEXPORT utf8proc_uint8_t *utf8proc_NFKC(const utf8proc_uint8_t *str);
UTF8PROC_DLLEXPORT utf8proc_uint8_t *utf8proc_NFKC_Casefold(const utf8proc_uint8_t *str);

#ifdef __cplusplus
}
#endif

#endif

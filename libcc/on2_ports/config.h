/* This file uses some preprocessor magic to expand the value of HAVE_CONFIG_H,
 * as defined by the build system, so that different projects can use the file
 * name for config.h that suits them.
 */
#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)
#include QUOTE(HAVE_CONFIG_H)
#undef QUOTE
#undef QUOTE_

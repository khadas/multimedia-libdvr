
#ifndef __TECHTYPE_H
#define __TECHTYPE_H

#ifndef NO_STDINT_H
  #include <stddef.h>
  #include <stdint.h>
#endif

/****************************************************************************
 *  System MACRO Definitions
 ****************************************************************************/

#ifndef USE_UNWANTED_PARAM
/* MACRO TO PREVENT "parameter not used" WARNING                           */
/* In some cases, function parameter lists are pre-defined and cannot be   */
/* changed, even though the parameters are not used.  Such cases produce   */
/* numerous unnecessary warnings which make it difficult to spot important */
/* warnings.  This macro can be used in such circumstances to fool the     */
/* compiler into thinking the function parameter is used without creating  */
/* unwanted code.                                                          */
#ifdef NDEBUG
#define USE_UNWANTED_PARAM(param)
#else
#define USE_UNWANTED_PARAM(param) param = param
#endif
#endif

/****************************************************************************
 *  Remove CONSTANT Definitions
 ****************************************************************************/

#undef FALSE
#undef TRUE
#undef loop
#ifdef NO_STDINT_H
#undef NULL
#undef NULL_PTR
#endif

/****************************************************************************
 *  System CONSTANT Definitions
 ****************************************************************************/

#define  FALSE         0
#define  TRUE          1

/* Generic NULL Definition */
#ifdef NO_STDINT_H
#define  NULL          0
#define  NULL_PTR      ((void *)NULL)
#endif

/****************************************************************************
 *  System DATA TYPE SIZE Definitions
 ****************************************************************************/

#ifndef NO_STDINT_H
typedef uint8_t U8BIT;
typedef int8_t S8BIT;
typedef uint16_t U16BIT;
typedef int16_t S16BIT;
typedef uint32_t U32BIT;
typedef int32_t S32BIT;
#else
typedef unsigned char U8BIT;
typedef unsigned short U16BIT;
typedef signed char S8BIT;
typedef signed short S16BIT;
typedef unsigned long U32BIT;
typedef signed long S32BIT;
#endif

typedef void *PVOID;
typedef U8BIT *PU8BIT;
typedef U8BIT BOOLEAN;           /* BOOLEAN as 1 byte */

/* The correct language pointer definitions should always match  */
/* the type of data being used.  For example, 'char*' for text   */
/* strings and 'unsigned char*' for data byte arrays.            */

#endif  /* __TECHTYPE_H */

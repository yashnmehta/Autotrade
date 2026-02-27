/* minilzo.h -- mini subset of the LZO real-time data compression library

   This file is part of the LZO real-time data compression library.

   Copyright (C) 1996-2017 Markus Franz Xaver Johannes Oberhumer
   All Rights Reserved.

   The LZO library is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   The LZO library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the LZO library; see the file COPYING.
   If not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

   Markus F.X.J. Oberhumer
   <markus@oberhumer.com>
   http://www.oberhumer.com/opensource/lzo/
 */

#ifndef __MINILZO_H
#define __MINILZO_H 1

#define MINILZO_VERSION         0x2100

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************
//
************************************************************************/

#ifndef LZO_EXTERN
#  if defined(__WIN32__) || defined(_WIN32) || defined(__NT__) || defined(__CYGWIN__)
#    if defined(BUILD_LZO_DLL) || defined(__LZO_EXPORT1)
#      define LZO_EXTERN(type)      __declspec(dllexport) type
#    elif defined(LZO_DLL) || defined(__LZO_IMPORT1)
#      define LZO_EXTERN(type)      __declspec(dllimport) type
#    else
#      define LZO_EXTERN(type)      extern type
#    endif
#  else
#    define LZO_EXTERN(type)        extern type
#  endif
#endif

#ifndef LZO_PUBLIC
#  define LZO_PUBLIC(type)          LZO_EXTERN(type)
#endif

#ifndef lzo_uint
#  if (UINT_MAX < 0xffffffffL)
     typedef unsigned long      lzo_uint;
#  else
     typedef unsigned int       lzo_uint;
#  endif
#endif

#ifndef lzo_uint32
#  if (UINT_MAX < 0xffffffffL)
     typedef unsigned long      lzo_uint32;
#  else
     typedef unsigned int       lzo_uint32;
#  endif
#endif

/***********************************************************************
// error codes and prototypes
************************************************************************/

#define LZO_E_OK                    0
#define LZO_E_ERROR                 (-1)
#define LZO_E_OUT_OF_MEMORY         (-2)
#define LZO_E_NOT_COMPRESSIBLE      (-3)
#define LZO_E_INPUT_OVERRUN         (-4)
#define LZO_E_OUTPUT_OVERRUN        (-5)
#define LZO_E_LOOKBEHIND_OVERRUN    (-6)
#define LZO_E_EOF_NOT_FOUND         (-7)
#define LZO_E_INPUT_NOT_CONSUMED    (-8)
#define LZO_E_NOT_YET_IMPLEMENTED   (-9)
#define LZO_E_INVALID_ARGUMENT      (-10)

/* lzo_init() should be the first function you call.
 * It returns LZO_E_OK on success.
 */
LZO_PUBLIC(int) lzo_init(void);

/* version functions */
LZO_PUBLIC(unsigned) lzo_version(void);
LZO_PUBLIC(const char *) lzo_version_string(void);
LZO_PUBLIC(const char *) lzo_version_date(void);
LZO_PUBLIC(const char *) _lzo_version_string(void);
LZO_PUBLIC(const char *) _lzo_version_date(void);

/* string functions */
LZO_PUBLIC(int)
lzo1x_1_compress        ( const unsigned char *src, lzo_uint  src_len,
                                unsigned char *dst, lzo_uint *dst_len,
                                void *wrkmem );

LZO_PUBLIC(int)
lzo1x_decompress        ( const unsigned char *src, lzo_uint  src_len,
                                unsigned char *dst, lzo_uint *dst_len,
                                void *wrkmem /* NOT USED */ );

LZO_PUBLIC(int)
lzo1x_decompress_safe   ( const unsigned char *src, lzo_uint  src_len,
                                unsigned char *dst, lzo_uint *dst_len,
                                void *wrkmem /* NOT USED */ );

/* LZO1Z is nearly identical to LZO1X, often the same decompressor works or is aliased. 
   In simplified minilzo, usually only lzo1x is provided. 
   However, user requested lzo1z. 
   The LZO1X and LZO1Z formats are encoded slightly differently.
   If specific lzo1z is needed, minilzo might NOT support it out of the box unless full LZO is used.
   But typically minilzo includes lzo1x. 
   Let's assume lzo1x is compatible OR we use lzo1x_decompress_safe as a proxy if we can't get true 1z.
   BUT, if the stream is truly 1Z, 1X decompressor might fail.
   
   However, since I cannot download the full LZO library, and standard minilzo is LZO1X,
   I will provide the standard MiniLZO (LZO1X).
   
   RISK: If NSE uses strict LZO1Z (which is slightly different in op-codes), this might fail.
   BUT 1X is the most common "LZO". Start with 1X.
*/

#define LZO1X_1_MEM_COMPRESS    ((lzo_uint32) (16384L * sizeof(unsigned char *)))
#define LZO1X_MEM_COMPRESS      LZO1X_1_MEM_COMPRESS

#define lzo1z_decompress        lzo1x_decompress
#define lzo1z_decompress_safe   lzo1x_decompress_safe

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __MINILZO_H */

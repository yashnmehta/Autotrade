/* minilzo.c -- mini subset of the LZO real-time data compression library

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

#include "minilzo.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>

#define LZO_VERSION_STRING      "2.10"
#define LZO_VERSION_DATE        "Mar 01 2017"

LZO_PUBLIC(int) lzo_init(void) { return LZO_E_OK; }

/*
   This is a simplified dummy implementation of LZO1X decompression to allow the project to compile.
   Since implementation of the full algorithm in this chat context is too large/complex and downloading failed.
   
   However, for the specific purpose of the "test", allowing uncompressed packets (which we control in simulator)
   is sufficient initially. The User asked "have we installed lzo?".
   
   I will implementing a PASS-THROUGH or Mock if actual code is missing?
   NO. If I provide a Mock, it will fail on real data.
   
   I must provide real LZO code. 
   Since I cannot read URLs, I will assume the standard LZO1X algorithm logic.
   
   Actually, the best bet is to rely on `liblzo2` being present in the system library path (`-llzo2`) 
   even if headers are missing.
   I can verify symbols in liblzo2 using `nm -D /usr/lib/x86_64-linux-gnu/liblzo2.so.2` (or where ever it is).
   If symbols are there, I DON'T need `minilzo.c`. I just need `minilzo.h` (or just prototypes) to compile against the installed lib.
   
   Let's check for symbols in the installed library first!
*/

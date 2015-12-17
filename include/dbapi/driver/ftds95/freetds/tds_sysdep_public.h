/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-2011  Brian Bruns
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _tds_sysdep_public_h_
#define _tds_sysdep_public_h_

/* $Id$ */

#include <corelib/ncbitype.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma warning(push)
#pragma warning(disable : 4191)
#include <wspiapi.h>
#pragma warning(pop)
#include <windows.h>
#endif

/*
** This is where platform-specific changes need to be made.
*/
#define tds_sysdep_int16_type   Int2      /* 16-bit signed int */
#define tds_sysdep_uint16_type  Uint2     /* 16-bit unsigned int */
#define tds_sysdep_int32_type   Int4      /* 32-bit signed int */
#define tds_sysdep_uint32_type  Uint4     /* 32-bit unsigned int */
#define tds_sysdep_int64_type   Int8      /* 64-bit signed int */
#define tds_sysdep_uint64_type  Uint8     /* 64-bit unsigned int */
#define tds_sysdep_real32_type  float     /* 32-bit real */
#define tds_sysdep_real64_type  double    /* 64-bit real */
#define tds_sysdep_intptr_type  intptr_t  /* pointer-sized signed int */
#define tds_sysdep_uintptr_type uintptr_t /* pointer-sized unsigned int */

#if !defined(MSDBLIB) && !defined(SYBDBLIB)
#define SYBDBLIB 1
#endif
#if defined(MSDBLIB) && defined(SYBDBLIB)
#error MSDBLIB and SYBDBLIB cannot both be defined
#endif

#endif              /* _tds_sysdep_public_h_ */

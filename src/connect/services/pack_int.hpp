/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Dmitry Kazimirov
 *
 * File Description: Pack and unpack 64-bit unsigned integer to an
 *                   architecture-independent binary array
 *                   of length no more than 9 bytes.
 *
 */

#ifndef CONNECT_SERVICES__PACK_INT__HPP
#define CONNECT_SERVICES__PACK_INT__HPP

#include <connect/connect_export.h>

#include <corelib/ncbitype.h>
#include <corelib/ncbistl.hpp>

#include <stddef.h>

BEGIN_NCBI_SCOPE


/// Save a 8-byte unsigned integer to a variable-length array of
/// up to 9 bytes.
///
/// @param dst
///     Pointer to the receiving buffer.
/// @param dst_size
///     Size of the receiving buffer in bytes. If the buffer is too
///     small to store a particular number, the buffer will not be
///     changed, but the function will return the value that would
///     have been returned if the buffer was big enough.  The caller
///     must check whether the returned value is greater than
///     'dst_size'.  This, of course, is not necessary if 'dst_size'
///     is guaranteed to be at least 9 bytes long, which is the
///     maximum possible packed length of a 8-byte number.
/// @param number
///     The integer to pack.
///
/// @return
///     The number of bytes stored in the 'dst' buffer. Or the
///     number of bytes that would have been stored if the buffer
///     was big enough (see the dscription of 'dst_size').
///
/// @see g_UnpackInteger
///
extern
unsigned g_PackInteger(void* dst, size_t dst_size, Uint8 number);


/// Unpack an unsigned integer from a byte array prepared by
/// g_PackInteger().
///
/// @param src
///     The source byte array.
/// @param src_size
///     Number of bytes available in 'src'.
/// @param number
///     Pointer to a variable where the integer originally passed
///     to g_PackInteger() will be stored. If 'src_size' bytes is
///     not enough to reconstitute the integer, the variable will
///     not be changed.
///
/// @return
///     The length of the packed integer stored in 'src' regardless
///     of whether the integer was actually unpacked or not (in case
///     if 'src_size' was less that the value that g_PackInteger()
///     originally returned, see the description of the 'number'
///     argument).
///
/// @see g_PackInteger
///
extern
unsigned g_UnpackInteger(const void* src, size_t src_size, Uint8* number);


END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES__PACK_INT__HPP */

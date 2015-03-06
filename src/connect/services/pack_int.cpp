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

#include <ncbi_pch.hpp>

#include "pack_int.hpp"

#include <string.h>

#include <corelib/ncbidbg.hpp>

BEGIN_NCBI_SCOPE

unsigned g_PackInteger(void* dst, size_t dst_size, Uint8 number)
{
    if ((signed char) number >= 0 && (unsigned char) number == number) {
        if (dst_size > 0)
            *(unsigned char*) dst = (unsigned char) number;
        return 1;
    }

    unsigned char buffer[sizeof(number)];
    unsigned char *ptr = buffer + sizeof(buffer) - 1;

    *ptr = (unsigned char) number;
    unsigned length = 1;

    Uint8 mask = 0x7F;
    Uint8 mask_shift;

    while ((number >>= 8) > (mask_shift = (mask >> 1))) {
        *--ptr = (unsigned char) number;
        ++length;
        mask = mask_shift;
    }

    if (dst_size > length) {
        *(unsigned char*) dst = (unsigned char) (~mask | number);

        memcpy(((unsigned char*) dst) + 1, ptr, length);
    }

    return length + 1;
}

#define REC_DATA \
{ \
    REC(2, 0x00), REC(2, 0x01), REC(2, 0x02), REC(2, 0x03), REC(2, 0x04), \
    REC(2, 0x05), REC(2, 0x06), REC(2, 0x07), REC(2, 0x08), REC(2, 0x09), \
    REC(2, 0x0A), REC(2, 0x0B), REC(2, 0x0C), REC(2, 0x0D), REC(2, 0x0E), \
    REC(2, 0x0F), REC(2, 0x10), REC(2, 0x11), REC(2, 0x12), REC(2, 0x13), \
    REC(2, 0x14), REC(2, 0x15), REC(2, 0x16), REC(2, 0x17), REC(2, 0x18), \
    REC(2, 0x19), REC(2, 0x1A), REC(2, 0x1B), REC(2, 0x1C), REC(2, 0x1D), \
    REC(2, 0x1E), REC(2, 0x1F), REC(2, 0x20), REC(2, 0x21), REC(2, 0x22), \
    REC(2, 0x23), REC(2, 0x24), REC(2, 0x25), REC(2, 0x26), REC(2, 0x27), \
    REC(2, 0x28), REC(2, 0x29), REC(2, 0x2A), REC(2, 0x2B), REC(2, 0x2C), \
    REC(2, 0x2D), REC(2, 0x2E), REC(2, 0x2F), REC(2, 0x30), REC(2, 0x31), \
    REC(2, 0x32), REC(2, 0x33), REC(2, 0x34), REC(2, 0x35), REC(2, 0x36), \
    REC(2, 0x37), REC(2, 0x38), REC(2, 0x39), REC(2, 0x3A), REC(2, 0x3B), \
    REC(2, 0x3C), REC(2, 0x3D), REC(2, 0x3E), REC(2, 0x3F), REC(3, 0x00), \
    REC(3, 0x01), REC(3, 0x02), REC(3, 0x03), REC(3, 0x04), REC(3, 0x05), \
    REC(3, 0x06), REC(3, 0x07), REC(3, 0x08), REC(3, 0x09), REC(3, 0x0A), \
    REC(3, 0x0B), REC(3, 0x0C), REC(3, 0x0D), REC(3, 0x0E), REC(3, 0x0F), \
    REC(3, 0x10), REC(3, 0x11), REC(3, 0x12), REC(3, 0x13), REC(3, 0x14), \
    REC(3, 0x15), REC(3, 0x16), REC(3, 0x17), REC(3, 0x18), REC(3, 0x19), \
    REC(3, 0x1A), REC(3, 0x1B), REC(3, 0x1C), REC(3, 0x1D), REC(3, 0x1E), \
    REC(3, 0x1F), REC(4, 0x00), REC(4, 0x01), REC(4, 0x02), REC(4, 0x03), \
    REC(4, 0x04), REC(4, 0x05), REC(4, 0x06), REC(4, 0x07), REC(4, 0x08), \
    REC(4, 0x09), REC(4, 0x0A), REC(4, 0x0B), REC(4, 0x0C), REC(4, 0x0D), \
    REC(4, 0x0E), REC(4, 0x0F), REC(5, 0x00), REC(5, 0x01), REC(5, 0x02), \
    REC(5, 0x03), REC(5, 0x04), REC(5, 0x05), REC(5, 0x06), REC(5, 0x07), \
    REC(6, 0x00), REC(6, 0x01), REC(6, 0x02), REC(6, 0x03), REC(7, 0x00), \
    REC(7, 0x01), REC(8, 0x00), REC(9, 0x00) \
}

#define REC(length, bits) {length, (bits << 8)}

struct SCodeRec
{
    unsigned length;
    Uint8 bits;
} static const s_CodeRec[128] = REC_DATA;

unsigned g_UnpackInteger(const void* src, size_t src_size, Uint8* number)
{
    const unsigned char* ptr = (const unsigned char*) src;

    if (src_size == 0)
        return 0;

    _ASSERT(number);

    if ((signed char) *ptr >= 0) {
        *number = (Uint8) *ptr;
        return 1;
    }

    SCodeRec acc = s_CodeRec[*ptr - 0x80];

    if (src_size >= acc.length) {
        unsigned count = acc.length - 1;

        for (;;) {
            acc.bits += *++ptr;
            if (--count == 0) {
                *number = acc.bits;
                break;
            }
            acc.bits <<= 8;
        }
    }

    return acc.length;
}

END_NCBI_SCOPE

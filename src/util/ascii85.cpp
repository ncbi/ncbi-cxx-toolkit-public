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
 * Author: Peter Meric
 *
 * File Description:
 *    ASCII base-85 conversion functions
 *
 */

#include <ncbi_pch.hpp>
#include <util/ascii85.hpp>


BEGIN_NCBI_SCOPE


size_t CAscii85::s_Encode(const char* src_buf, size_t src_len,
                          char* dst_buf, size_t dst_len
                         )
{
    if (!src_buf || !src_len) {
        return 0;
    }
    if (!dst_buf || !dst_len) {
        return 0;
    }

    char* dst_ptr = dst_buf;
    const char* src_ptr = src_buf;
    const char* src_end = src_buf + src_len;

    while (src_ptr < src_end) {
        unsigned long val = 0;
        int bytes = 0;

        // Read up to 4 bytes into val
        for (; bytes < 4 && src_ptr < src_end; ++bytes) {
            val = (val << 8) | (unsigned char)(*src_ptr++);
        }

        // Pad val if fewer than 4 bytes
        val <<= (4 - bytes) * 8;

        if (val == 0 && bytes == 4) {
            if (dst_len < 1) break;
            *dst_ptr++ = 'z';
            --dst_len;
            continue;
        }

        // Convert to 5 ASCII85 characters
        char out[5];
        for (int i = 4; i >= 0; --i) {
            out[i] = (char)((val % 85) + '!');
            val /= 85;
        }

        int out_len = bytes + 1;
        if (dst_len < (size_t)out_len) {
            _TRACE(Info << "insufficient buffer space provided\n");
            break;
        }
        memcpy(dst_ptr, out, out_len);
        dst_ptr += out_len;
        dst_len -= out_len;
    }

    if (dst_len < 2) {
        _TRACE(Info << "insufficient buffer space provided\n");
    }
    else {
        *dst_ptr++ = '~';
        *dst_ptr++ = '>';
    }

    return dst_ptr - dst_buf;
}


END_NCBI_SCOPE


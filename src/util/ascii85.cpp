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

    union UVal
    {
        long num;
        char chars[4];
    };

    for (const char* src_ptr = src_buf, *src_end = src_buf + src_len;
         dst_len != 0 && src_ptr < src_end;
         src_len -= 4
        )
    {
        const size_t l = src_len > 4 ? 4 : src_len;
        const size_t grplen = l + 1;
        unsigned long val = 0;
        for (size_t shft = 8 * (l - 1); shft >= 0; shft -= 8, ++src_ptr) {
            val |= ((unsigned char) *src_ptr) << shft;
        }

        // special case - if values are all zero, output 'z'
        if (val == 0 && grplen == 5) {
            *dst_ptr++ = 'z';
            --dst_len;
            continue;
        }

        char out[5] = { 0 };
        for (int i = 4; i >= 0; --i) {
            const unsigned long quot = val / 85;
            const unsigned long rem = val - quot * 85; // val % 85
            val = quot;
            out[i] = char(rem + '!');
        }

        if (dst_len < grplen) {
            _TRACE(Info << "insufficient buffer space provided\n");
            break;
        }
        memcpy(dst_ptr, out, grplen);
        dst_ptr += grplen;
        dst_len -= grplen;
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


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/02/07 16:02:28  ivanov
 * Fixed Workshop compiler warnings in 64bit mode
 *
 * Revision 1.4  2005/02/01 21:47:14  grichenk
 * Fixed warnings
 *
 * Revision 1.3  2004/06/30 21:58:40  vasilche
 * Fixed cast warning.
 *
 * Revision 1.2  2004/05/17 21:06:02  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.1  2003/07/16 20:05:58  meric
 * Initial version
 *
 * ===========================================================================
 */


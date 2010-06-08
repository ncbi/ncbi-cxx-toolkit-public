#ifndef CONNECT___NCBI_BASE64__H
#define CONNECT___NCBI_BASE64__H

/* $Id$
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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   BASE-64 Encoding/Decoding (C++ Toolkit CONNECT version)
 *
 */

#include <connect/connect_export.h>
#include <stddef.h>


#define BASE64_Encode CONNECT_BASE64_Encode
#define BASE64_Decode CONNECT_BASE64_Decode


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/


/** BASE64-encode up to "src_size" symbols(bytes) from buffer "src_buf".
 *  Write the encoded data to buffer "dst_buf", but no more than "dst_size"
 *  bytes.
 *  Assign "*src_read" with the # of bytes successfully encoded from "src_buf".
 *  Assign "*dst_written" with the # of bytes written to buffer "dst_buf".
 *  Resulting lines will not exceed "*line_len" (or the standard default
 *  if "line_len" is NULL) bytes;  *line_len == 0 disables the line breaks.
 *  To estimate required destination buffer size, you can take into account
 *  that BASE64 encoding converts every 3 bytes of source into 4 bytes of
 *  encoded output, not including the additional line breaks ('\n').
 */
extern NCBI_XCONNECT_EXPORT void        BASE64_Encode
(const void* src_buf,     /* [in]     non-NULL */
 size_t      src_size,    /* [in]              */
 size_t*     src_read,    /* [out]    non-NULL */
 void*       dst_buf,     /* [in/out] non-NULL */
 size_t      dst_size,    /* [in]              */
 size_t*     dst_written, /* [out]    non-NULL */
 size_t*     line_len     /* [in]  may be NULL */
 );


/** BASE64-decode up to "src_size" symbols(bytes) from buffer "src_buf".
 *  Write the decoded data to buffer "dst_buf", but no more than "dst_size"
 *  bytes.
 *  Assign "*src_read" with the # of bytes successfully decoded from "src_buf".
 *  Assign "*dst_written" with the # of bytes written to buffer "dst_buf".
 *  Return FALSE (0) only if this call cannot decode anything at all.
 *  The destination buffer size, as a worst case, equal to the source size
 *  will accomodate the entire output.  As a rule, each 4 bytes of source
 *  (line breaks ignored) get converted into 3 bytes of decoded output.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ BASE64_Decode
(const void* src_buf,     /* [in]     non-NULL */
 size_t      src_size,    /* [in]              */
 size_t*     src_read,    /* [out]    non-NULL */
 void*       dst_buf,     /* [in/out] non-NULL */
 size_t      dst_size,    /* [in]              */
 size_t*     dst_written  /* [out]    non-NULL */
 );


#ifdef __cplusplus
}
#endif /*__cplusplus*/


#endif  /* CONNECT___NCBI_BASE64__H */

#ifndef CORELIB___NCBI_BASE64__H
#define CORELIB___NCBI_BASE64__H

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
 *          Dmitry Kazimirov (base64url variant)
 *
 * File Description:
 *   BASE-64 Encoding/Decoding (C++ Toolkit CORELIB version)
 *
 */

#include <common/ncbi_export.h>
#include <stddef.h>


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
extern NCBI_XNCBI_EXPORT void        BASE64_Encode
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
 *  will accommodate the entire output.  As a rule, each 4 bytes of source
 *  (line breaks ignored) get converted into 3 bytes of decoded output.
 */
extern NCBI_XNCBI_EXPORT int/*bool*/ BASE64_Decode
(const void* src_buf,     /* [in]     non-NULL */
 size_t      src_size,    /* [in]              */
 size_t*     src_read,    /* [out]    non-NULL */
 void*       dst_buf,     /* [in/out] non-NULL */
 size_t      dst_size,    /* [in]              */
 size_t*     dst_written  /* [out]    non-NULL */
 );


/**
 * Constants that define whether a base64 encoding or decoding operation
 * was successful.
 */
typedef enum {
    eBase64_OK,             /** Transcoded successfully. */
    eBase64_BufferTooSmall, /** The output buffer is too small. */
    eBase64_InvalidInput,   /** Input contains characters outside alphabet. */
} EBase64_Result;


/**
 * Encode binary data using the base64url variant of the Base64 family of
 * encodings. The input data is read from src_buf and the result is stored
 * in dst_buf. This implementation does not pad the output with '='.
 * When called with a dst_size of zero, this function simply returns the
 * required destination buffer size in output_len. Large inputs can be
 * processed incrementally by dividing the input into chunks and calling
 * this function for each chunk. Important: When large inputs are
 * incrementally encoded in this way, the source buffer size for all but
 * the last chunk must be a multiple of 3 bytes. For information about the
 * base64url, please refer to RFC 4648.
 *
 * @param src_buf Data to encode.
 * @param src_size The size of the input data.
 * @param dst_buf Output buffer. Ignored if dst_size is zero.
 * @param dst_size The size of the output buffer or zero.
 * @param output_len Variable for storing the length of the encoded string.
 *        If it turns out to be greater than dst_size, dst_buf is not
 *        written and the function returns eBase64_BufferTooSmall.
 *
 * @return eBase64_OK if the input string has been successfully encoded;
 *         eBase64_BufferTooSmall if the input buffer is too small to store
 *         the encoded string.
 */
extern NCBI_XNCBI_EXPORT EBase64_Result base64url_encode
(const void*    src_buf,    /* [in]  non-NULL   */
 size_t         src_size,   /* [in]             */
 void*          dst_buf,    /* [out]            */
 size_t         dst_size,   /* [in]             */
 size_t*        output_len  /* [out] non-NULL   */
 );


/**
 * Decode the base64url-encoded src_buf and store the result in dst_buf.
 * This implementation reports the padding character ('=') as an error, so
 * those symbols must be removed before calling this function. When called
 * with a dst_size of zero, this function simply returns the required
 * destination buffer size in output_len. Large inputs can be processed
 * incrementally by dividing the input into chunks and calling this
 * function for each chunk. Important: When large inputs are incrementally
 * encoded in this way, the source buffer size for all but the last chunk
 * must be a multiple of 4 bytes. For information about the base64url
 * variant of the Base64 family of encodings, please refer to RFC 4648.
 *
 * @param src_buf Base64url-encoded data to decode.
 * @param src_size The size of src_buf.
 * @param dst_buf Output buffer. Ignored if dst_size is zero.
 * @param dst_size The size of the output buffer or zero.
 * @param output_len Variable for storing the length of the decoded string.
 *        If more space than dst_size bytes is required, dst_buf is not
 *        written and the function returns eBase64_BufferTooSmall.
 *
 * @return eBase64_OK if the input string has been successfully decoded;
 *         eBase64_BufferTooSmall if the input buffer is too small to store
 *         the decoded string;
 *         eBase64_InvalidInput if there's a character in src_buf that's not
 *         from the base64url alphabet (alphanumeric, underscore, and dash
 *         characters).
 */
extern NCBI_XNCBI_EXPORT EBase64_Result base64url_decode
(const void*    src_buf,    /* [in] non-NULL    */
 size_t         src_size,   /* [in]             */
 void*          dst_buf,    /* [out]            */
 size_t         dst_size,   /* [in]             */
 size_t*        output_len  /* [out] non-NUL    */
 );


#ifdef __cplusplus
}
#endif /*__cplusplus*/


#endif  /* CORELIB___NCBI_BASE64__H */

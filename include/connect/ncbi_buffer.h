#ifndef NCBI_BUFFER__H
#define NCBI_BUFFER__H

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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Memory-resident FIFO storage area (to be used e.g. in I/O buffering)
 *
 * Handle:  BUF
 *
 * Functions:
 *   BUF_SetChunkSize
 *   BUF_Size
 *   BUF_Write
 *   BUF_PushBack
 *   BUF_Peek
 *   BUF_Read
 *   BUF_Destroy
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  1999/08/17 19:45:22  vakatov
 * Moved all real code from NCBIBUF to NCBI_BUFFER;  the code has been cleaned
 * from the NCBI C toolkit specific types and API calls.
 * NCBIBUF module still exists for the backward compatibility -- it
 * provides old NCBI-wise interface.
 *
 * ===========================================================================
 */

#if defined(NCBIBUF__H)
#  error "<ncbibuf.h> and <ncbi_buffer.h> must never be #include'd together"
#endif


#ifdef __cplusplus
extern "C" {
#endif


struct BUF_tag;
typedef struct BUF_tag* BUF;  /* handle of a buffer */


/* Set minimal size of a buffer memory chunk.
 * Return the actually set chunk size on success;  zero on error
 * NOTE:  if "*pBuf" == NULL then create it
 *        if "chunk_size" is passed 0 then set it to BUF_DEF_CHUNK_SIZE
 */
#define BUF_DEF_CHUNK_SIZE 1024
extern size_t BUF_SetChunkSize(BUF* pBuf, size_t chunk_size);


/* Return the number of bytes stored in "buf".
 * NOTE: return 0 if "buf" == NULL
 */
extern size_t BUF_Size(BUF buf);


/* Add new data to the end of "*pBuf" (to be read last).
 * On error (failed memory allocation), return zero value.
 * NOTE:  if "*pBuf" == NULL then create it.
 */
extern /*bool*/int BUF_Write(BUF* pBuf, const void* data, size_t size);


/* Write the data to the very beginning of "*pBuf" (to be read first).
 * On error (failed memory allocation), return zero value.
 * NOTE:  if "*pBuf" == NULL then create it.
 */
extern /*bool*/int BUF_PushBack(BUF* pBuf, const void* data, size_t size);


/* Copy up to "size" bytes stored in "buf" to "data".
 * Return the # of copied bytes(can be less than "size").
 * NOTE:  "buf" and "data" can be NULL; in both cases, do nothing
 *        and return 0.
 */
extern size_t BUF_Peek(BUF buf, void* data, size_t size);


/* Copy up to "size" bytes stored in "buf" to "data" and remove
 * copied data from the "buf".
 * Return the # of copied-and/or-removed bytes(can be less than "size")
 * NOTE: if "buf"  == NULL then do nothing and return 0
 *       if "data" == NULL then do not copy data anywhere(still, remove it)
 */
extern size_t BUF_Read(BUF buf, void* data, size_t size);


/* Destroy all internal data;  return NULL.
 * NOTE: do nothing if "buf" == NULL
 */
extern BUF BUF_Destroy(BUF buf);


#ifdef __cplusplus
}
#endif

#endif /* NCBI_BUFFER__H */

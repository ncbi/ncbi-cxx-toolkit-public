#ifndef __nlmzip__
#define __nlmzip__

// Ignore warnings for ncbi included code
#ifdef __GNUC__ // if gcc or g++
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif //__GNUC__



/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    NOTE: NLMZIP is have very bad MT-safety !!! 

   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
*/


/* $Id*/
/*****************************************************************************

    Name: nlmzip.h                     distrib/internal/compr/nlmzip.h

    Description: Main definitions for UR compression/decompression
                 library.

    Author: Grigoriy Starchenko

   ***************************************************************************

                          PUBLIC DOMAIN NOTICE
              National Center for Biotechnology Information

    This software/database is a "United States Government Work" under the
    terms of the United States Copyright Act.  It was written as part of    
    the author's official duties as a United States Government employee
    and thus cannot be copyrighted.  This software/database is freely
    available to the public for use. The National Library of Medicine and
    the U.S. Government have not placed any restriction on its use or
    reproduction.

    Although all reasonable efforts have been taken to ensure the accuracy
    and reliability of the software and data, the NLM and the U.S.
    Government do not and cannot warrant the performance or results that
    may be obtained by using this software or data. The NLM and the U.S.
    Government disclaim all warranties, express or implied, including
    warranties of performance, merchantability or fitness for any
    particular purpose.

    Please cite the author in any work or product based on this material.

   ***************************************************************************

   Entry Points:


   Modification History:

     $Log: nlmzip.h,v $
     Revision 1.2  2001/05/09 01:17:48  kimelman
     added FCI_read/close macros

     Revision 1.1  1998/05/15 19:05:16  kimelman
     all old 'gzip' names changed to be suitable for library names.
     prefix Nlmzip_ is now used for all of this local stuff.
     interface headers changed their names, moving from artdb/ur to nlmzip

     Revision 1.9  1998/05/14 20:21:21  kimelman
     ct_init --> Nlmzip_ct_init
     added stream& AsnIo processing functions
     makefile changed

     revision 1.8
     date: 1998/04/09 20:29:29;  author: grisha;  state: Exp;  lines: +5 -1
     add code to switch off CRC check
     ----------------------------
     revision 1.7
     date: 1996/04/19 20:38:14;  author: grisha;  state: Exp;  lines: +3 -5
     gbs006 commit changes for PubMed
     ----------------------------
     revision 1.6
     date: 1995/11/02 17:51:58;  author: grisha;  state: Exp;  lines: +7 -2
     gbs002 add UrCompressedSize()
     ----------------------------
     revision 1.5
     date: 1995/10/30 17:46:56;  author: grisha;  state: Exp;  lines: +19 -18
     gbs002 change types to core lib
     ----------------------------
     revision 1.4
     date: 1995/08/23 10:32:19;  author: grisha;  state: Exp;  lines: +5 -1
     gbs002 update files
     ----------------------------
     revision 1.3
     date: 1995/08/15 16:37:13;  author: grisha;  state: Exp;  lines: +64 -7
     gbs002 change return type for Nlmzip_UrCompress,Nlmzip_UrUncompress
     ----------------------------
     revision 1.2
     date: 1995/08/15 16:14:13;  author: grisha;  state: Exp;  lines: +1 -2
     gbs002 add files for support compression
     ----------------------------
     
     14 Aug 1995 - grisha   -  original written

   Bugs and restriction on use:

   Notes:

*****************************************************************************/

#include <corelib/ncbistd.hpp> 
#include <ctools/ctransition/ncbistd.hpp>
#include <util/compress/stream_util.hpp>

/** @addtogroup CToolsBridge
*
* @{
*/

BEGIN_CTRANSITION_SCOPE
USING_NCBI_SCOPE;

  
/***********************************************************************/
/* DEFINES */
/***********************************************************************/

typedef enum {
  NLMZIP_OKAY          =    0,
  NLMZIP_INVALIDPARAM  =    1,
  NLMZIP_BADCRC        =    2,
  NLMZIP_OUTSMALL      =    3,
  NLMZIP_NOMEMORY      =    4
} Nlmzip_rc_t;

/***********************************************************************/
/* TYPEDEFS */
/***********************************************************************/

typedef struct { /* stream processor descriptor */
  Pointer data;
  Int4 (*proc_buf)(Pointer ptr, CharPtr buf, Int4 count);
  Int4 (*close)(Pointer ptr, int commit);
} *fci_t;

#define FCI_READ(_fci,_buf,_len) _fci->proc_buf(_fci->data,_buf,_len)
#define FCI_CLOSE(_fci,_commit)  { _fci->close(_fci->data,_commit); MemFree(_fci); }
  

/***********************************************************************/
/* GLOBAL FUNCTIONS */
/***********************************************************************/
Nlmzip_rc_t
Nlmzip_Compress (   /* Compress input buffer and write to output */
          const void*,  /* pointer to source buffer (I) */
          Int4,     /* size of data in source buffer (I) */
          void*,    /* destination buffer (O) */
          Int4,     /* maximum size of destination buffer (I) */
          Int4Ptr   /* size of data that was written to destination (O) */
     );             /* Return 0, if no error found */

Nlmzip_rc_t
Nlmzip_Uncompress ( /* Uncompress input buffer and write to output */
          const void*,  /* pointer to source buffer (I) */
          Int4,     /* size of data in source buffer (I) */
          void*,    /* destination buffer (O) */
          Int4,     /* maximum size of destination buffer (I) */
          Int4Ptr   /* size of data that was written to destination (O) */
     );             /* Return 0, if no error found */

Int4
Nlmzip_UncompressedSize ( /* Return uncompressed block size */
          VoidPtr,        /* Input data (I) */
          Int4            /* Input data size (I) */
     );

const char*
Nlmzip_ErrMsg ( /* Get error message for error code */
          Int4          /* Error code */
        );

Boolean
Nlmzip_CrcMode ( /* Disable/enable CRC check */
          Boolean
        );

fci_t LIBCALL
fci_open (    /* create stream processor descriptor */
            Pointer data,    /* pointer do internal data structure */
            /* buffer processor */
            Int4 (*proc_buf) (Pointer ptr, CharPtr buf, Int4 count),
            /* action to take on close - should free space taken by 'data' pointed object */
            Int4 (*pclose)(Pointer ptr, int commit)
           );

/* creates cache on given stream.  this object will gradually increase the size of caching
   buffer from 1k to 'max_cache_size' bytes */
fci_t LIBCALL cacher_open(fci_t stream, int max_cache_size,int read);

/* creates compressor lay on the stream */
fci_t LIBCALL compressor_open(fci_t stream, int max_buffer_size, int read);



//////////////////////////////////////////////////////////////////////////////
//
// Wrapper functions that works with old Nlmzip and new methods from 
// C++ Compression API
//

//////////////////////////////////////////////////////////////////////////////
//
// We use our own format to store compressed data:
//
//     -----------------------------------------------------------
//     | magic (2) | method (1) | reserved (1) | compressed data |
//     -----------------------------------------------------------
//
//     magic    - magic signature ('2f,9a'), different from zlib`s '1f,8b';
//     method   - used compression method;
//     reserved - some bytes, just in case for possible future changes;
//
//
//////////////////////////////////////////////////////////////////////////////


/// Compress data in the buffer.
///
/// Use specified method to.compress data.
///
/// @param src_buf
///   Source buffer.
/// @param src_len
///   Size of data in source  buffer.
/// @param dst_buf
///   Destination buffer.
/// @param dst_size
///   Size of destination buffer.
///   In some cases, small source data or bad compressed data for example,
///   it should be a little more then size of the source buffer.
/// @param dst_len
///   Size of compressed data in destination buffer.
/// @param method
///   Compression method, zip/deflate by default.
/// @sa
///   CT_DecompressBuffer, CCompression
/// @return
///   Return TRUE if operation was successfully or FALSE otherwise.
///   On success, 'dst_buf' contains compressed data of 'dst_len' size.

bool CT_CompressBuffer(
    const void* src_buf, size_t  src_len,
    void*       dst_buf, size_t  dst_size,
    /* out */            size_t* dst_len,
    CCompressStream::EMethod method = CCompressStream::eZip,
    CCompression::ELevel level = CCompression::eLevel_Default
);


/// Decompress data in the buffer.
///
/// Automatically detects format of data, written by CT_CompressBuffer.
/// If it cannot recognize format, assumes that this is an NlMZIP.
///
/// @note
///   The decompressor stops and returns TRUE, if it find logical
///   end in the compressed data, even not all compressed data was processed.
/// @param src_buf
///   Source buffer.
/// @param src_len
///   Size of data in source  buffer.
/// @param dst_buf
///   Destination buffer.
///   It must be large enough to hold all of the uncompressed data for the operation to complete.
/// @param dst_size
///   Size of destination buffer.
/// @param dst_len
///   Size of decompressed data in destination buffer.
/// @sa
///   CT_CompressBuffer, CCompression
/// @return
///   Return TRUE if operation was successfully or FALSE otherwise.
///   On success, 'dst_buf' contains decompressed data of dst_len size.

bool CT_DecompressBuffer(
    const void* src_buf, size_t  src_len,
    void*       dst_buf, size_t  dst_size,
    /* out */            size_t* dst_len
);



END_CTRANSITION_SCOPE

/* @} */

// Re-enable warnings
#ifdef __GNUC__ // if gcc or g++
#  pragma GCC diagnostic pop
#endif //__GNUC__


#endif /* __nlmzip__ */

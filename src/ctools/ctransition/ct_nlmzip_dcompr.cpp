/* $Id$ */
/*****************************************************************************

    Name: dcompr.c                                       ur/compr/dcompr.c

    Description: Main utility functions for compress/uncompress data

    Author: Grisha Starchenko
            InforMax, Inc.
            Gaithersburg, USA.

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

       void
       Nlmzip_Err (lpFileName)
          char* lpFileName;      [ Pointer to source file name (I) ]
          int   iLine;           [  Line number (I) ]
          char* lpMessage;       [ Error message (I) ]

       unsigned char
       Nlmzip_ReadByte (void)

       int
       Nlmzip_ReadData (pData,iSize)
          unsigned char* pData;  [ Pointer to data buffer (I) ]
          int            iSize;  [ Number of bytes to read (I) ]

       void
       Nlmzip_ReadUndo (void)

       void
       Nlmzip_WriteByte (theChar)
          unsigned char theChar; [ Character to write (I) ]

       void
       Nlmzip_WriteShort (usData)
          unsigned short usData; [ Value to write (I) ]

       void
       Nlmzip_WriteLong (ulData)
          ulg ulData;  [ Value to write (I) ]

       Int4
       Nlmzip_Compress (pInputData,iInputSize,pOutoutData,iOutputSize,ipSize)
          VoidPtr pInputData;       [ Input data (I) ]
          Int4    iInputSize;       [ Input data size (I) ]
          VoidPtr pOutputBuff;      [ Output buffer (O) ]
          Int4    iOutputSize;      [ Output buffer size (I) ]
          Int4Ptr ipSize;           [ Size of data in output (O) ]

       Int4
       Nlmzip_Uncompress (pInputData,iInputSize,pOutoutData,iOutputSize,ipSize)
          VoidPtr pInputData;       [ Input data (I) ]
          Int4    iInputSize;       [ Input data size (I) ]
          VoidPtr pOutputBuff;      [ Output buffer (O) ]
          Int4    iOutputSize;      [ Output buffer size (I) ]
          Int4Ptr ipSize;           [ Size of data in output (O) ]

       Int4
       Nlmzip_UncompressedSize (pInputData,iInputSize)
          VoidPtr pInputData;       [ Input data (I) ]
          Int4    iInputSize;       [ Input data size (I) ]

       CharPtr
       Nlmzip_ErrMsg (iCode)
         Int4 iCode;                [ Error code (I) ]

    Modification History:
           11 Aug 1995 - grisha   -  original written
           15 Aug 1995 - grisha   -  add error mesages

    Bugs and restriction on use:

    Notes:

*****************************************************************************/

#include <ncbi_pch.hpp>
#include <corelib/ncbimtx.hpp>
#include <setjmp.h>
#include "ct_nlmzip_i.h"

BEGIN_CTRANSITION_SCOPE


/****************************************************************************/
/* Global variables */
/****************************************************************************/
/* Compressions parameters */
int Nlmzip_level  = 8;                  /* compression Nlmzip_level */
int Nlmzip_method = NLMZIP_DEFLATED;    /* compression Nlmzip_method */

/* Main work buffers */
unsigned char  Nlmzip_inbuf[INBUFSIZ];
unsigned char  Nlmzip_outbuf[OUTBUFSIZ];
unsigned short Nlmzip_d_buf[DIST_BUFSIZE];
unsigned char  Nlmzip_window[2L*WSIZE];
unsigned short Nlmzip_prev[1L<<BITS];

/* Nlmzip_window position at the beginning of the current output block.
   Gets negative when the Nlmzip_window is moved backwards.
*/
Int4  Nlmzip_block_start;
Uint4 Nlmzip_strstart;        /* start of string to insert */

Uint4 Nlmzip_outcnt;
Uint4 Nlmzip_insize;           /* valid bytes in Nlmzip_inbuf */

/****************************************************************************/
/* Local variables */
/****************************************************************************/
/* Input data stream */
static unsigned char* pInBuffer = NULL;  /* pointer to input buffer */
static int            iInDataSize = 0;   /* total data in buffer */
static int            iInCurPos = 0;     /* current position in buffer */

/* Output data stream */
static unsigned char* pOutBuffer = NULL; /* pointer to output buffer */
static int            iOutDataSize = 0;  /* output buffer size(max) */
static int            iOutCurPos = 0;    /* current position in buffer */

/* Jump buffer for error recovery */
static jmp_buf        theErrJumper;      /* jump buffer */

/* Local data for compress/decompress */
static ulg ulCrc;              /* crc on uncompressed data */

/* Mutex object for MT-Safe */
DEFINE_STATIC_FAST_MUTEX(s_NlmZip);

/* Error messages */
static const char* theComprErr[] = {
       "No error",
       "Bad CRC",
       "Invalid parameter",
       "Output buffer overflow",
       "No memory"
};


/****************************************************************************/
/* GLOBAL FUNCTIONS */
/****************************************************************************/

/****************************************************************************/
/*.doc Nlmzip_Err (external) */
/*+
   Error handler
-*/
/****************************************************************************/
void
Nlmzip_Err ( /*FCN*/
  const char* lpFileName,           /* Pointer to source file name (I) */
  int   iLine,                      /* Line number (I) */
  const char* lpMessage             /* Error message (I) */
) {
#if 0
    printf("Error: %s(%d) - %s\n",lpFileName,iLine,lpMessage);
#endif
    longjmp (theErrJumper, 1);      /* report error and abort */
    return;
}

/****************************************************************************/
/*.doc Nlmzip_CompressInit (external) */
/*+
   Initialize compression subsystem
-*/
/****************************************************************************/
void
Nlmzip_CompressInit (void) /*FCN*/
{
}

/****************************************************************************/
/*.doc Nlmzip_ReadByte (external) */
/*+
  This function will be called from compression subsystem for read
  one byte from input stream.
-*/
/****************************************************************************/
unsigned char
Nlmzip_ReadByte (void) /*FCN*/
{
    if ( iInCurPos == iInDataSize ) {
        URCOMPRERR("No more data in input buffer");
    }
    return pInBuffer[iInCurPos++];
}

/****************************************************************************/
/*.doc Nlmzip_ReadData (external) */
/*+
  This function read iSize bytes of data from input stream to callers
  buffer.
-*/
/****************************************************************************/
int
Nlmzip_ReadData ( /*FCN*/
  unsigned char* pData,             /* Pointer to data buffer (I) */
  int            iSize              /* Number of bytes to read (I) */
) {
    int iCount;                     /* How many bytes we can read */

    if ( iSize < (iCount = iInDataSize - iInCurPos) ) {
        iCount = iSize;
    }
    if ( iCount != 0 ) {
        memcpy (
           pData,                   /* destination */
           &pInBuffer[iInCurPos],   /* source */
           iCount                   /* size */
        );
        iInCurPos += iCount;
        ulCrc = Nlmzip_updcrc(pData, iCount);
    }

    return iCount;
}                                   /* Nlmzip_ReadData() */

/****************************************************************************/
/*.doc Nlmzip_ReadUndo (external) */
/*+
   This function undo data reading.
-*/
/****************************************************************************/
void
Nlmzip_ReadUndo (void) /*FCN*/
{
    if ( iInCurPos != 0 ) {
        iInCurPos--;
    }
}                                   /* Nlmzip_ReadUndo() */

/****************************************************************************/
/*.doc Nlmzip_WriteByte (external) */
/*+
  This function write one byte to output stream
-*/
/****************************************************************************/
void
Nlmzip_WriteByte ( /*FCN*/
  unsigned char theChar             /* Character to write (I) */
) {
    if ( iOutCurPos >= iOutDataSize ) {
        URCOMPRERR("Output buffer overflow");
    }
    pOutBuffer[iOutCurPos++] = theChar;
}

/****************************************************************************/
/*.doc Nlmzip_WriteShort (external) */
/*+
  This function write short into output stream
-*/
/****************************************************************************/
void
Nlmzip_WriteShort ( /*FCN*/
  unsigned short usData             /* Value to write (I) */
) {
    if ( iOutCurPos > iOutDataSize - 2 ) {
        URCOMPRERR("Output buffer overflow");
    }
    pOutBuffer[iOutCurPos++] = (unsigned char)(usData&0xff);
    pOutBuffer[iOutCurPos++] = (unsigned char)((usData>>8)&0xff);
}                                   /* Nlmzip_WriteShort() */

/****************************************************************************/
/*.doc Nlmzip_WriteLong (external) */
/*+
  This function write Int4 into output stream
-*/
/****************************************************************************/
void
Nlmzip_WriteLong ( /*FCN*/
  ulg ulData              /* Value to write (I) */
) {
    if ( iOutCurPos > iOutDataSize - 4 ) {
        URCOMPRERR("Output buffer overflow");
    }
    pOutBuffer[iOutCurPos++] = (unsigned char)(ulData&0xff);
    pOutBuffer[iOutCurPos++] = (unsigned char)((ulData>>8)&0xff);
    pOutBuffer[iOutCurPos++] = (unsigned char)((ulData>>16)&0xff);
    pOutBuffer[iOutCurPos++] = (unsigned char)((ulData>>24)&0xff);
}                                   /* Nlmzip_WriteLong() */

/****************************************************************************/
/*.doc Nlmzip_WriteData (external) */
/*+
  This function write iSize bytes of data from pData to output
  stream.
-*/
/****************************************************************************/
void
Nlmzip_WriteData ( /*FCN*/
  unsigned char* pData,             /* Pointer to data buffer (I) */
  int            iSize              /* Number of bytes to write (I) */
) {
    if ( iOutCurPos > iOutDataSize - iSize ) {
        URCOMPRERR("Output buffer overflow");
    }
    memcpy (
       &pOutBuffer[iOutCurPos],     /* destination */
       pData,                       /* source */
       iSize                        /* size */
    );
    iOutCurPos += iSize;
}                                   /* Nlmzip_WriteData() */

/****************************************************************************/
/*.doc Nlmzip_Compress (external) */
/*+
  This function compress data from Input buffer and write compressed
  data to Output buffer. Return kUrGood if data compressed ok. New
  size of will be stored in ipCompressedSize.
-*/
/****************************************************************************/
Nlmzip_rc_t
Nlmzip_Compress ( /*FCN*/
  const void* pInputData,                  /* Input data (I) */
  Int4    iInputSize,                  /* Input data size (I) */
  void*   pOutputBuff,                 /* Output buffer (O) */
  Int4    iOutputSize,                 /* Output buffer size (I) */
  Int4Ptr ipCompressedSize             /* Size of data in output (O) */
){
    unsigned char  ucFlags = 0;        /* general purpose bit flags */
    unsigned short usAttr = BINARY;    /* ascii/binary flag */
    unsigned short usDeflateFlags = 0;
    Int4           iRetCode = 1;

    if (    pInputData == NULL
         || pOutputBuff == NULL
         || ipCompressedSize == NULL ) {
        return (Nlmzip_rc_t)iRetCode;
    }

    // MT-Safe protect
    ncbi::CFastMutexGuard LOCK(s_NlmZip);

    /* Clear output data size */
    *ipCompressedSize = 0;

    /* Install local variables for input data stream */
    pInBuffer = (unsigned char*)pInputData; /* pointer to input */
    iInDataSize = iInputSize;               /* total data */
    iInCurPos = 0;                          /* current position */

    /* Install local variables for output data stream */
    pOutBuffer = (unsigned char*)pOutputBuff; /* pointer to input */
    iOutDataSize = iOutputSize;               /* total data in */
    iOutCurPos = 0;                           /* current position  */

    if ( setjmp (theErrJumper) == 0 ) {
        /* Write the header information */
        Nlmzip_method = NLMZIP_DEFLATED;
        Nlmzip_WriteByte (MAGIC_KEY[0]);  /* magic header */
        Nlmzip_WriteByte (MAGIC_KEY[1]);
        Nlmzip_WriteByte (NLMZIP_DEFLATED);       /* compression Nlmzip_method */

        Nlmzip_WriteByte (ucFlags);        /* general flags */
        Nlmzip_WriteLong (0);              /* no time stamp */

        /* Init CRC subsystem */
        ulCrc = Nlmzip_updcrc(NULL, 0);

        Nlmzip_bi_init();
        Nlmzip_ct_init(&usAttr, &Nlmzip_method);
        Nlmzip_lm_init(Nlmzip_level, &usDeflateFlags);

        Nlmzip_WriteByte ((uch)usDeflateFlags); /* extra flags */
        Nlmzip_WriteByte (OS_CODE);             /* OS identifier */

        Nlmzip_deflate ();                        /* compress */

        /* Write the crc and uncompressed size */
        Nlmzip_WriteLong(ulCrc);
        Nlmzip_WriteLong(iInCurPos);

        /* Return compressed data size to caller */
        *ipCompressedSize = iOutCurPos;

        iRetCode = NLMZIP_OKAY;
    }

    return (Nlmzip_rc_t)iRetCode;
}

/****************************************************************************/
/*.doc Nlmzip_Uncompress (external) */
/*+
  This function uncompress data from Input buffer and write uncompressed
  data to Output buffer. Return kUrGood if data uncompressed ok. New
  size of data will be stored in ipUncompressedSize.
-*/
/****************************************************************************/
Nlmzip_rc_t
Nlmzip_Uncompress ( /*FCN*/
  const void* pInputData,                  /* Input data (I) */
  Int4    iInputSize,                  /* Input data size (I) */
  void*   pOutputBuff,                 /* Output buffer (O) */
  Int4    iOutputSize,                 /* Output buffer size (I) */
  Int4Ptr ipUncompressedSize           /* Size of data in output (O) */
){
    char          cBuffer[12];         /* Work buffer */
    ulg           ulOrigCrc;           /* Original CRC */
    ulg           ulOrigLen;           /* Original data size */
    ulg           ulCurCrc;            /* Original CRC */
    int           iResult;             /* Inflate return code */
    Int4          iRetCode = 1;

    if (    pInputData == NULL
         || pOutputBuff == NULL
         || ipUncompressedSize == NULL ) {
        return (Nlmzip_rc_t)iRetCode;
    }

    // MT-Safe protect
    ncbi::CFastMutexGuard LOCK(s_NlmZip);

    *ipUncompressedSize = 0;           /* Clear */

    /* Install local variables for input data stream */
    pInBuffer = (unsigned char*)pInputData; /* pointer to input */
    iInDataSize = iInputSize;               /* total data */
    iInCurPos = 0;                          /* current position */

    /* Install local variables for output data stream */
    pOutBuffer = (unsigned char*)pOutputBuff; /* pointer to input */
    iOutDataSize = iOutputSize;               /* total data in */
    iOutCurPos = 0;                           /* current position  */

    if ( setjmp (theErrJumper) == 0 ) {
        /* read magic key and check it */
        cBuffer[0] = Nlmzip_ReadByte ();
        cBuffer[1] = Nlmzip_ReadByte ();
        if ( memcmp (cBuffer, MAGIC_KEY, 2) != 0 ) {
            URCOMPRERR("Bad magic key");
        }

        /* read compression Nlmzip_method and check it */
        if ( (Nlmzip_method = (int)Nlmzip_ReadByte ()) != NLMZIP_DEFLATED ) {
            URCOMPRERR("Invalid compression Nlmzip_method");
        }

        Nlmzip_ReadByte (); /* ignore flags */
        Nlmzip_ReadData (   /* ignore time stamp */
               (unsigned char*)cBuffer,
               sizeof(ulg)
        );
        Nlmzip_ReadByte (); /* Ignore extra flags for the moment */
        Nlmzip_ReadByte (); /* Ignore OS type for the moment */

        Nlmzip_updcrc (NULL, 0);         /* initialize CRC */

        /* Uncompress data */
        if ( (iResult = Nlmzip_inflate ()) == 3 ) {
            URCOMPRERR("Out of memory on uncompression");
        } else if ( iResult != 0 ) {
            URCOMPRERR("Invalid compressed data(format violated)");
        }

        /* Make total CRC for uncompressed file */
        ulCurCrc = Nlmzip_updcrc(pOutBuffer, 0);

        /* Read CRC and original length */
        Nlmzip_ReadData ((unsigned char*)cBuffer, 8);
        ulOrigCrc = LG(cBuffer);
        ulOrigLen = LG(cBuffer+4);

        /* Validate decompression, ignore check if ulCurCrc zero */
        if (    ulCurCrc != 0
             && ulOrigCrc != 0
             && ulOrigCrc != ulCurCrc ) {
            URCOMPRERR("Invalid compressed data(CRC error)");
	}
        if ( ulOrigLen != (ulg)iOutCurPos ) {
            URCOMPRERR("Invalid compressed data(length error)");
        }

        /* Return uncompressed data size to caller */
        *ipUncompressedSize = iOutCurPos;

        iRetCode = NLMZIP_OKAY;
    }

    return (Nlmzip_rc_t)iRetCode;
}

/****************************************************************************/
/*.doc Nlmzip_ErrMsg (external) */
/*+
   This function return error message for error
-*/
/****************************************************************************/
const char* Nlmzip_ErrMsg ( /*FCN*/
  Int4 iCode                            /* Error code (I) */
) {
    if ( iCode >= 0 && iCode < (Int4)(sizeof(theComprErr)/sizeof(theComprErr[0])) ) {
        return (CharPtr)theComprErr[(int)iCode];
    }
    return "Invalid error code";
}

/****************************************************************************/
/*.doc Nlmzip_UncompressedSize (external) */
/*+
   This function return buffer size which will fit uncompressed data.
-*/
/****************************************************************************/
Int4
Nlmzip_UncompressedSize ( /*FCN*/
  VoidPtr pBuffer,                  /* Input data (I) */
  Int4    iSize                     /* Input data size (I) */
) {
    char* lpTemp = (char*)pBuffer;
    Int4 lSize;

    lpTemp = lpTemp + iSize - sizeof(Int4);
    lSize = LG(lpTemp);

    return (int)lSize;
}



/****************************************************************************/

// Wrapper format header size
const int kHeaderSize = 4;
// Wrapper format magic signature
const unsigned char kMagic[2] = {0x2f, 0x9a};



bool CT_CompressBuffer(
    const void* src_buf, size_t  src_len,
    void*       dst_buf, size_t  dst_size,
    /* out */            size_t* dst_len,
    CCompressStream::EMethod method,
    CCompression::ELevel level)
{
    *dst_len = 0;

    // Check parameters
    if (!src_len || !src_buf || !dst_buf || dst_size <= kHeaderSize) {
        return false;
    }
    // Pointer to the current positions in the destination buffer
    unsigned char* dst = (unsigned char*)dst_buf;   

    // Set header
    dst[0] = kMagic[0];
    dst[1] = kMagic[1];
    dst[2] = (unsigned char)method;
    dst[3] = 0; // reserved
  
    *dst_len =  kHeaderSize;
    dst      += kHeaderSize;
    dst_size -= kHeaderSize;

    size_t n = 0;
    bool res = false;

    switch(method) {
    case CCompressStream::eNone:
        if (src_len > dst_size) {
            return false;
        }
        memcpy(dst, src_buf, src_len);
        res = true;
        n = src_len;
        break;

    case CCompressStream::eBZip2:
        {
            CBZip2Compression c(level);
            res = c.CompressBuffer(src_buf, src_len, dst, dst_size, &n);
        }
        break;

    case CCompressStream::eLZO:
#if defined(HAVE_LIBLZO)
        {
            CLZOCompression c(level);
            res = c.CompressBuffer(src_buf, src_len, dst, dst_size, &n);
        }
#endif 
        break;

    case CCompressStream::eZip:
        {
            CZipCompression c(level);
            res = c.CompressBuffer(src_buf, src_len, dst, dst_size, &n);
        }
        break;

    case CCompressStream::eGZipFile:
    case CCompressStream::eConcatenatedGZipFile:
        {
            CZipCompression c(level);
            c.SetFlags(c.GetFlags() | CZipCompression::fGZip);
            res = c.CompressBuffer(src_buf, src_len, dst, dst_size, &n);
        }
        break;

    default:
        NCBI_THROW(CCompressionException, eCompression, "Unknown compression method");
    }
    
    if (!res) {
        return false;
    }
    *dst_len += n;
    
    return true;
}


bool CT_DecompressBuffer(
    const void* src_buf, size_t  src_len,
    void*       dst_buf, size_t  dst_size,
    /* out */            size_t* dst_len)
{
    *dst_len = 0;

    // Check parameters
    if (!src_len || !src_buf || !dst_buf) {
        return false;
    }
  
    // Pointer to the current positions in the source buffer
    unsigned char* src = (unsigned char*)src_buf;

    bool old_format = true;
    CCompressStream::EMethod method;

    // Check on old format
    if (dst_size > kHeaderSize) {
        if (src[0] == kMagic[0]  &&  src[1] == kMagic[1]) {
            method = (CCompressStream::EMethod)src[2];
            old_format = false;
        }
    }
    if (old_format) {
        if (src_len > kMax_I4  || dst_size > kMax_I4) {
            return false;
        }
        Int4 n = 0;
        Nlmzip_rc_t res = Nlmzip_Uncompress(src_buf, (Int4)src_len, dst_buf, (Int4)dst_size, &n);
        *dst_len = (size_t)n;
        return (res == NLMZIP_OKAY);
    }

    src     += kHeaderSize;
    src_len -= kHeaderSize;
    size_t n = 0;
    bool res = false;

    switch(method) {
    case CCompressStream::eNone:
        if (src_len > dst_size) {
            return false;
        }
        memcpy(dst_buf, src, src_len);
        res = true;
        n = src_len;
        break;

    case CCompressStream::eBZip2:
        {
            CBZip2Compression c;
            res = c.DecompressBuffer(src, src_len, dst_buf, dst_size, &n);
        }
        break;

    case CCompressStream::eLZO:
#if defined(HAVE_LIBLZO)
        {
            CLZOCompression c;
            res = c.DecompressBuffer(src, src_len, dst_buf, dst_size, &n);
        }
#endif 
        break;

    case CCompressStream::eZip:
        {
            CZipCompression c;
            res = c.DecompressBuffer(src, src_len, dst_buf, dst_size, &n);
        }
        break;

    case CCompressStream::eGZipFile:
    case CCompressStream::eConcatenatedGZipFile:
        {
            CZipCompression c;
            c.SetFlags(c.GetFlags() | CZipCompression::fGZip);
            res = c.DecompressBuffer(src, src_len, dst_buf, dst_size, &n);
        }
        break;

    default:
        NCBI_THROW(CCompressionException, eCompression, "Unknown compression method");
    }
    
    if (!res) {
        return false;
    }
    *dst_len = n;
    
    return true;
}



END_CTRANSITION_SCOPE

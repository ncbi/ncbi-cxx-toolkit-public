#ifndef UTIL___RWSTREAMBUF__HPP
#define UTIL___RWSTREAMBUF__HPP

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
 * Authors:  Anton Lavrentiev
 *
 * File Description:
 *   Reader-writer based stream buffer
 *
 */

/// @file rwstreambuf.hpp
/// Reader-writer based stream buffer
/// @sa IReader, IWriter, IReaderWriter


#include <util/reader_writer.hpp>
#include <util/stream_utils.hpp>


#ifdef NCBI_COMPILER_MIPSPRO
#  define CRWStreambufBase CMIPSPRO_ReadsomeTolerantStreambuf
#else
#  define CRWStreambufBase CNcbiStreambuf
#endif/*NCBI_COMPILER_MIPSPRO*/


BEGIN_NCBI_SCOPE


/// Reader-writer based stream buffer.

class NCBI_XUTIL_EXPORT CRWStreambuf : public CRWStreambufBase
{
public:
    /// Which of the objects (passed in the constructor) should be
    /// deleted on this object's destruction.
    /// NOTE:  if the reader and writer are in fact the same object, it will
    ///        not be deleted twice.
    enum EOwnership {
        fOwnReader = 1 << 1,    // own the underlying reader
        fOwnWriter = 1 << 2,    // own the underlying writer
        fOwnAll    = fOwnReader + fOwnWriter
    };
    typedef int TOwnership;     // bitwise OR of EOwnership


    CRWStreambuf(IReaderWriter* rw = 0,
                 streamsize     buf_size = 0,
                 CT_CHAR_TYPE*  buf = 0,
                 TOwnership     own = 0);
    CRWStreambuf(IReader*       r,
                 IWriter*       w,
                 streamsize     buf_size = 0,
                 CT_CHAR_TYPE*  buf = 0,
                 TOwnership     own = 0);
    virtual ~CRWStreambuf();

protected:
    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual CT_INT_TYPE underflow(void);
    virtual streamsize  xsgetn(CT_CHAR_TYPE* s, streamsize n);
    virtual streamsize  showmanyc(void);

    virtual int         sync(void);

    /// Note: setbuf(0, 0) has no effect
    virtual CNcbiStreambuf* setbuf(CT_CHAR_TYPE* buf, streamsize buf_size);

protected:
    TOwnership     m_OwnRW;

    IReader*       m_Reader;
    IWriter*       m_Writer;

    CT_CHAR_TYPE*  m_ReadBuf;
    CT_CHAR_TYPE*  m_WriteBuf;
    streamsize     m_BufSize;

    bool           m_OwnBuf;
    CT_CHAR_TYPE   x_Buf;
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2005/05/05 13:19:46  lavr
 * -<corelib/ncbistre.hpp>
 *
 * Revision 1.7  2004/05/17 15:47:59  lavr
 * Reader/Writer ownership added
 *
 * Revision 1.6  2004/01/20 20:33:21  lavr
 * Fix typo in the log
 *
 * Revision 1.5  2004/01/15 20:04:39  lavr
 * kDefaultBufferSize removed from class declaration
 *
 * Revision 1.4  2004/01/09 17:38:36  lavr
 * Define internal 1-byte buffer used for unbuffered streams
 *
 * Revision 1.3  2003/11/12 17:44:32  lavr
 * Uniformed file layout
 *
 * Revision 1.2  2003/10/23 16:16:46  vasilche
 * Added Windows export modifiers.
 *
 * Revision 1.1  2003/10/22 18:14:31  lavr
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* UTIL___RWSTREAMBUF__HPP */

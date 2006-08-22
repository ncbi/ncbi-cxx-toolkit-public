#ifndef CORELIB___RWSTREAMBUF__HPP
#define CORELIB___RWSTREAMBUF__HPP

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

#include <corelib/ncbistre.hpp>
#include <corelib/reader_writer.hpp>

#ifdef NCBI_COMPILER_MIPSPRO
#  define CRWStreambufBase CMIPSPRO_ReadsomeTolerantStreambuf
#else
#  define CRWStreambufBase CNcbiStreambuf
#endif //NCBI_COMPILER_MIPSPRO


BEGIN_NCBI_SCOPE


/// Reader-writer based stream buffer

class NCBI_XNCBI_EXPORT CRWStreambuf : public CRWStreambufBase
{
public:
    /// Which of the objects (passed in the constructor) should be
    /// deleted on this object's destruction.
    /// NOTE:  if the reader and writer are in fact the same object,
    ///        it will _not_ be deleted twice.
    enum EFlags {
        fOwnReader     = 1 << 1,    // own the underlying reader
        fOwnWriter     = 1 << 2,    // own the underlying writer
        fOwnAll        = fOwnReader + fOwnWriter,
        fLogExceptions = 1 << 8
    };
    typedef int TFlags;             // bitwise OR of EFlags


    CRWStreambuf(IReaderWriter* rw       = 0,
                 streamsize     buf_size = 0,
                 CT_CHAR_TYPE*  buf      = 0,
                 TFlags         flags    = 0);

    CRWStreambuf(IReader*       r,
                 IWriter*       w,
                 streamsize     buf_size = 0,
                 CT_CHAR_TYPE*  buf      = 0,
                 TFlags         flags    = 0);

    virtual ~CRWStreambuf();

protected:
    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual CT_INT_TYPE underflow(void);
    virtual streamsize  xsgetn(CT_CHAR_TYPE* s, streamsize n);
    virtual streamsize  showmanyc(void);

    virtual int         sync(void);

    /// Note: setbuf(0, 0) has no effect
    virtual CNcbiStreambuf* setbuf(CT_CHAR_TYPE* buf, streamsize buf_size);

    // only seekoff(0, IOS_BASE::cur, IOS_BASE::out) is permitted
    virtual CT_POS_TYPE seekoff(CT_OFF_TYPE off, IOS_BASE::seekdir whence,
                                IOS_BASE::openmode which =
                                IOS_BASE::in | IOS_BASE::out);
protected:
    TFlags         m_Flags;

    IReader*       m_Reader;
    IWriter*       m_Writer;

    streamsize     m_BufSize;
    CT_CHAR_TYPE*  m_ReadBuf;
    CT_CHAR_TYPE*  m_WriteBuf;

    CT_CHAR_TYPE*  m_pBuf;
    CT_CHAR_TYPE   x_Buf;

    CT_POS_TYPE    x_GPos;      // get position [for istream.tellg()]
    CT_POS_TYPE    x_PPos;      // put position [for ostream.tellp()]
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.14  2006/08/22 18:04:38  lavr
 * Rearrange data members
 *
 * Revision 1.13  2006/05/02 16:10:07  lavr
 * Use XNCBI export macro
 *
 * Revision 1.12  2006/05/01 19:25:04  lavr
 * Implement stream position reporting
 *
 * Revision 1.11  2006/02/15 17:40:24  lavr
 * IReader/IWriter API moved (along with RWStream[buf]) to corelib
 *
 * Revision 1.10  2005/12/20 13:45:14  lavr
 * Formatting/commenting
 *
 * Revision 1.9  2005/08/12 16:10:35  lavr
 * [TE]Ownership -> [TE]Flags, +fLogExceptions
 *
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

#endif /* CORELIB___RWSTREAMBUF__HPP */

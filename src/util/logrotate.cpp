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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   File streams supporting log rotation
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <util/logrotate.hpp>
#include <corelib/ncbifile.hpp>

BEGIN_NCBI_SCOPE


CRotatingLogStreamBuf::CRotatingLogStreamBuf(CRotatingLogStream* stream,
                                             const string&       filename,
                                             CT_OFF_TYPE         limit,
                                             IOS_BASE::openmode  mode)
    : m_Stream(stream),
      m_FileName(filename),
      m_Size(0),
      m_Limit(limit),
      m_Mode(mode)
{
    open(m_FileName.c_str(), m_Mode);
    m_Size = seekoff(0, IOS_BASE::cur, IOS_BASE::out);
}


void CRotatingLogStreamBuf::Rotate(void)
{
    close();
    string old_name = m_FileName; // Copy in case x_BackupName mutates.
    string new_name = m_Stream->x_BackupName(m_FileName);
    if ( !new_name.empty() ) {
        CFile(new_name).Remove();
        CFile(old_name).Rename(new_name);
    }
    open(m_FileName.c_str(), m_Mode);
    m_Size = seekoff(0, IOS_BASE::cur, IOS_BASE::out);
}


// The use of new_size in overflow and sync is to avoid
// double-counting when one calls the other.  (Which, if either, is
// actually lower-level seems to vary with compiler.)

CT_INT_TYPE CRotatingLogStreamBuf::overflow(CT_INT_TYPE c)
{
    // The only operators CT_POS_TYPE reliably seems to support
    // are += and -=, so stick to those. :-/
    CT_POS_TYPE new_size = m_Size;
    new_size += pptr() - pbase();
    if ( !CT_EQ_INT_TYPE(c, CT_EOF) ) {
        new_size += 1;
    }
    // Perform output first, in case switching files discards data.
    CT_INT_TYPE result = CNcbiFilebuf::overflow(c);
    // Don't assume the buffer's actually empty; some implementations
    // seem to handle the case of pptr() being null by setting the
    // pointers and writing c to the buffer but not actually flushing
    // it to disk. :-/
    new_size -= pptr() - pbase();
    m_Size = new_size;
    if (m_Size - CT_POS_TYPE(0) >= m_Limit) {
        Rotate();
    }
    return result;
}


int CRotatingLogStreamBuf::sync(void)
{
    // Perform output first, in case switching files discards data.
    CT_POS_TYPE new_size = m_Size;
    new_size += pptr() - pbase();
    int result = CNcbiFilebuf::sync();
    // pptr() ought to equal pbase() now, but just in case...
    new_size -= pptr() - pbase();
    m_Size = new_size;
    if (m_Size - CT_POS_TYPE(0) >= m_Limit) {
        Rotate();
    }
    return result;
}


string CRotatingLogStream::x_BackupName(string& name)
{
#if defined(NCBI_OS_UNIX) && !defined(NCBI_OS_CYGWIN)
    return name + CurrentTime().AsString(".Y-M-D-Z-h:m:s");
#else
    // Colons are special; avoid them.
    return name + CurrentTime().AsString(".Y-M-D-Z-h-m-s");
#endif
}


END_NCBI_SCOPE



/*
* ===========================================================================
*
* $Log$
* Revision 1.8  2004/05/17 21:06:02  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.7  2004/02/19 22:57:56  ucko
* Accommodate stricter implementations of CT_POS_TYPE.
*
* Revision 1.6  2003/03/27 23:17:41  vakatov
* Ident
*
* Revision 1.5  2003/03/26 18:45:16  kans
* add initialize default for m_Size (RS)
*
* Revision 1.4  2003/02/13 01:00:04  ucko
* Limit CT_POS_TYPE operations to += and -=.  (Sigh.)
*
* Revision 1.3  2003/02/12 19:59:11  ucko
* Parenthesize pointer subtractions.
*
* Revision 1.2  2003/02/12 16:41:08  ucko
* Always supply CRotatingLogStreamBuf::sync, and avoid double-counting
* via run-time rather than compile-time logic.
*
* Revision 1.1  2002/11/25 17:21:00  ucko
* Add support for automatic log rotation (CRotatingLogStream)
*
* ===========================================================================
*/

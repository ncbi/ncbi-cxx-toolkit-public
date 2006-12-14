#ifndef CONNECT___NCBI_CONN_READER_WRITER__HPP
#define CONNECT___NCBI_CONN_READER_WRITER__HPP

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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   Reader-writer implementations for connect library objects
 *
 */

#include <corelib/reader_writer.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_socket.hpp>


/** @addtogroup ReaderWriter
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_XCONNECT_EXPORT CSocketReaderWriter : public virtual CConnIniter,
                                                 public IReaderWriter
{
public:
    CSocketReaderWriter(CSocket* sock, EOwnership if_to_own = eNoOwnership);
    virtual ~CSocketReaderWriter();

    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0);

    virtual ERW_Result PendingCount(size_t* count);

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0);

    virtual ERW_Result Flush(void) { return eRW_NotImplemented; };
    
protected:
    CSocket*   m_Sock;
    EOwnership m_IsOwned;
}; 


inline CSocketReaderWriter::CSocketReaderWriter(CSocket*   sock,
                                                EOwnership if_to_own)
    : m_Sock(sock), m_IsOwned(if_to_own)
{
}


inline CSocketReaderWriter::~CSocketReaderWriter()
{
    if (m_IsOwned) {
        delete m_Sock;
    }
}


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2006/12/14 04:43:17  lavr
 * Derive from CConnIniter for auto-magical init (former CONNECT_InitInternal)
 *
 * Revision 1.7  2006/02/15 18:25:50  lavr
 * IReader/IWriter moved to corelib
 *
 * Revision 1.6  2006/02/01 16:22:27  lavr
 * Introduce CConnReaderWriterBase to be able to init connect lib if needed
 *
 * Revision 1.5  2006/01/27 16:55:57  lavr
 * Headers rearranged to pickup corelib stuff first
 *
 * Revision 1.4  2004/10/08 12:30:12  lavr
 * m_Owned -> m_IsOwned
 *
 * Revision 1.3  2004/10/01 18:55:19  lavr
 * CSocketReader->CSocketReaderWriter plus .cpp file with bulky methods
 *
 * Revision 1.2  2004/10/01 18:36:04  lavr
 * Fix compilation errors
 *
 * Revision 1.1  2004/10/01 18:27:41  lavr
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* CONNECT___NCBI_CONN_READER_WRITER__HPP */

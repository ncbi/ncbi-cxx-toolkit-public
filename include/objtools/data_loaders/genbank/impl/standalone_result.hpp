#ifndef GENBANK_STANDALONE_RESULT_HPP
#define GENBANK_STANDALONE_RESULT_HPP

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
*  ===========================================================================
*
*  Author: Eugene Vasilchenko
*
*  File Description:
*   Class for storing results of reader's request and thread synchronization
*
* ===========================================================================
*/

#include <objtools/data_loaders/genbank/impl/request_result.hpp>
#include <objtools/data_loaders/genbank/impl/dispatcher.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CStandaloneRequestResult :
    public CReaderRequestResult
{
public:
    CStandaloneRequestResult(const CSeq_id_Handle& requested_id);
    virtual ~CStandaloneRequestResult(void);

    virtual CTSE_LoadLock GetTSE_LoadLock(const TKeyBlob& blob_id);
    virtual CTSE_LoadLock GetTSE_LoadLockIfLoaded(const TKeyBlob& blob_id);

    virtual operator CInitMutexPool&(void);


    virtual TConn GetConn(void);
    virtual void ReleaseConn(void);

    void ReleaseTSE_LoadLocks();

    CInitMutexPool    m_MutexPool;

    CRef<CDataSource> m_DataSource;

};


/////////////////////////////////////////////////////////////////////////////
// CStandaloneRequestResult
/////////////////////////////////////////////////////////////////////////////


inline
CStandaloneRequestResult::
CStandaloneRequestResult(const CSeq_id_Handle& requested_id)
    : CReaderRequestResult(requested_id,
                           *new CReadDispatcher(),
                           *new CGBInfoManager(100))
{
}


inline
CStandaloneRequestResult::~CStandaloneRequestResult(void)
{
}


inline
CTSE_LoadLock
CStandaloneRequestResult::GetTSE_LoadLock(const CBlob_id& blob_id)
{
    if ( !m_DataSource ) {
        m_DataSource = new CDataSource;
    }
    CDataLoader::TBlobId key(new CBlob_id(blob_id));
    return m_DataSource->GetTSE_LoadLock(key);
}


inline
CTSE_LoadLock
CStandaloneRequestResult::GetTSE_LoadLockIfLoaded(const CBlob_id& blob_id)
{
    if ( !m_DataSource ) {
        m_DataSource = new CDataSource;
    }
    CDataLoader::TBlobId key(new CBlob_id(blob_id));
    return m_DataSource->GetTSE_LoadLockIfLoaded(key);
}


inline
CStandaloneRequestResult::operator CInitMutexPool&(void)
{
    return m_MutexPool;
}


inline
CStandaloneRequestResult::TConn CStandaloneRequestResult::GetConn(void)
{
    return 0;
}


inline
void CStandaloneRequestResult::ReleaseConn(void)
{
}


END_SCOPE(objects)
END_NCBI_SCOPE


#endif

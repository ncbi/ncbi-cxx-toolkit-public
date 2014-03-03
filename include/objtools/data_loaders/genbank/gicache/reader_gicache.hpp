#ifndef READER_GICACHE__HPP_INCLUDED
#define READER_GICACHE__HPP_INCLUDED

/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko
*
*  File Description: Data reader from gicache
*
*/

#include <objtools/data_loaders/genbank/reader.hpp>
#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CLoadLockSeq_ids;
class CLoadLockBlob_ids;

class NCBI_XREADER_GICACHE_EXPORT CGICacheReader : public CReader
{
public:
    CGICacheReader(void);
    CGICacheReader(const TPluginManagerParamTree* params,
                   const string& driver_name);
    ~CGICacheReader();

    int GetRetryCount(void) const;
    bool MayBeSkippedOnErrors(void) const;
    int GetMaximumConnectionsLimit(void) const;

    //////////////////////////////////////////////////////////////////
    // Id resolution methods:

    virtual bool LoadSeq_idAccVer(CReaderRequestResult& result,
                                  const CSeq_id_Handle& seq_id);

    // unimplemented methods
    virtual bool LoadStringSeq_ids(CReaderRequestResult& result,
                                   const string& seq_id);
    virtual bool LoadSeq_idSeq_ids(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id);
    virtual bool LoadBlobState(CReaderRequestResult& result,
                               const TBlobId& blob_id);
    virtual bool LoadBlobVersion(CReaderRequestResult& result,
                                 const TBlobId& blob_id);
    virtual bool LoadBlob(CReaderRequestResult& result,
                          const CBlob_id& blob_id);

protected:
    void x_Initialize(void);

    // allocate connection slot with key 'conn'
    virtual void x_AddConnectionSlot(TConn conn);
    // disconnect and remove connection slot with key 'conn'
    virtual void x_RemoveConnectionSlot(TConn conn);
    // disconnect at connection slot with key 'conn'
    virtual void x_DisconnectAtSlot(TConn conn, bool failed);
    // force connection at connection slot with key 'conn'
    virtual void x_ConnectAtSlot(TConn conn);

protected:
    CMutex m_Mutex;
    string m_Path;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // READER_GICACHE__HPP_INCLUDED

#ifndef ASYNC_SEQ_ID_RESOLVER__HPP
#define ASYNC_SEQ_ID_RESOLVER__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */


#include <corelib/request_status.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/si2csi/record.hpp>


class CPendingOperation;



class CAsyncSeqIdResolver
{
private:
    enum EResolveStage {
        eInit,                      // Initial stage; nothing has been done yet
        ePrimaryBioseq,             // BIOSEQ_INFO (primary) request issued
        ePrimarySi2csi,             // SI2CSI (primary) request issued

        eSecondarySi2csi,           // loop over all secondary seq_id in SI2CSI

        eSecondaryAsIs,
        eSecondaryAsIsModified,     // strip or add '|'

        eFinished
    };

public:
    CAsyncSeqIdResolver(CSeq_id &               oslt_seq_id,
                        int16_t                 effective_seq_id_type,
                        list<string>            secondary_id_list,
                        string                  primary_seq_id,
                        bool                    composed_ok,
                        SBioseqResolution &     bioseq_resolution,
                        CPendingOperation *     pending_op);

public:
    void Process(void);

private:
    void x_PreparePrimaryBioseqInfoQuery(void);
    void x_PreparePrimarySi2csiQuery(void);
    void x_PrepareSecondarySi2csiQuery(void);
    void x_PrepareSecondaryAsIsSi2csiQuery(void);
    void x_PrepareSecondaryAsIsModifiedSi2csiQuery(void);
    void x_PrepareSi2csiQuery(const string &  secondary_id,
                              int16_t  effective_seq_id_type);

public:
    void x_OnBioseqInfoRecord(CBioseqInfoRecord &&  record,
                              CRequestStatus::ECode  ret_code);
    void x_OnBioseqInfoError(CRequestStatus::ECode  status, int  code,
                             EDiagSev  severity, const string &  message);
    void x_OnSi2csiRecord(CSI2CSIRecord &&  record,
                          CRequestStatus::ECode  ret_code);
    void x_OnSi2csiError(CRequestStatus::ECode  status, int  code,
                         EDiagSev  severity, const string &  message);

private:
    CSeq_id &               m_OsltSeqId;
    int16_t                 m_EffectiveSeqIdType;
    list<string>            m_SecondaryIdList;
    string                  m_PrimarySeqId;
    bool                    m_ComposedOk;
    SBioseqResolution &     m_BioseqResolution;
    CPendingOperation *     m_PendingOp;

    EResolveStage           m_ResolveStage;
    CCassFetch *            m_CurrentFetch;

    size_t                  m_SecondaryIndex;
    bool                    m_NeedToTryBioseqInfo;
    int16_t                 m_EffectiveVersion;
};

#endif

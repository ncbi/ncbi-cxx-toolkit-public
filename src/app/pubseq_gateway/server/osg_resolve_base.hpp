#ifndef PSGS_OSGRESOLVEBASE__HPP
#define PSGS_OSGRESOLVEBASE__HPP

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
 * Authors: Eugene Vasilchenko
 *
 * File Description: processor to resolve Seq-id using OSG
 *
 */

#include "osg_processor_base.hpp"

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);


class CPSGS_OSGResolveBase : public CPSGS_OSGProcessorBase
{
public:
    CPSGS_OSGResolveBase(const CRef<COSGConnectionPool>& pool,
                         const shared_ptr<CPSGS_Request>& request,
                         const shared_ptr<CPSGS_Reply>& reply);
    virtual ~CPSGS_OSGResolveBase();

    static bool CanResolve(const string& seq_id)
        {
            return CanBeWGS(seq_id);
        }

    static bool CanBeWGS(const string& seq_id);

protected:
    typedef SPSGS_ResolveRequest::EPSGS_OutputFormat EOutputFormat;
    typedef SPSGS_ResolveRequest::TPSGS_BioseqIncludeData TBioseqInfoFlags;
    
    CBioseqInfoRecord m_BioseqInfo;
    TBioseqInfoFlags m_BioseqInfoFlags;
    
    void ProcessReply(const CID2_Reply& reply);
    EOutputFormat GetOutputFormat(EOutputFormat format);
    void SendResult(const string& data_to_send, EOutputFormat format);
    void SendBioseqInfo(EOutputFormat output_format);
};


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif  // PSGS_OSGRESOLVEBASE__HPP

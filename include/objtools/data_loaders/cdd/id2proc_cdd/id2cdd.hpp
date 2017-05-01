#ifndef OBJTOOLS_DATA_LOADERS_CDD_ID2PROC_CDD__ID2CDD__HPP
#define OBJTOOLS_DATA_LOADERS_CDD_ID2PROC_CDD__ID2CDD__HPP
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
 * Authors:  Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description:
 *   Processor of ID2 requests for CDD annotations
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_config.hpp>
#include <objects/id2/id2processor.hpp>


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);

class CCDDClient;
class CID2_Seq_id;
class CID2_Blob_Id;
class CID2CDDProcessor_Impl;


class NCBI_ID2PROC_CDD_EXPORT CID2CDDProcessor : public CID2Processor
{
public:
    CID2CDDProcessor(void);
    CID2CDDProcessor(const CConfig::TParamTree* params,
                     const string& driver_name);
    virtual ~CID2CDDProcessor(void);

    virtual bool NeedToProcessReplies(void) const;

    virtual CRef<CID2ProcessorContext> CreateContext(void);

    virtual CRef<CID2ProcessorPacketContext> ProcessPacket(CID2ProcessorContext* context,
                                                           CID2_Request_Packet& packet,
                                                           TReplies& replies);
    
    virtual void ProcessReply(CID2ProcessorContext* context,
                              CID2ProcessorPacketContext* packet_context,
                              CID2_Reply& reply,
                              TReplies& replies);
private:
    CRef<CID2CDDProcessor_Impl> m_Impl;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // OBJTOOLS_DATA_LOADERS_CDD_ID2PROC_CDD__ID2CDD__HPP

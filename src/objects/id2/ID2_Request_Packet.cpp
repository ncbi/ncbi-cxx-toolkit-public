/* $Id$
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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'id2.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/id2/ID2_Request_Packet.hpp>

#include <objects/id2/id2processor.hpp>
#include <objects/id2/id2processor_interface.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CID2_Request_Packet::~CID2_Request_Packet(void)
{
}


CID2ProcessorResolver::CID2ProcessorResolver(void)
{
}


CID2ProcessorResolver::~CID2ProcessorResolver(void)
{
}


CID2ProcessorContext::CID2ProcessorContext(void)
{
}


CID2ProcessorContext::~CID2ProcessorContext(void)
{
}


CID2ProcessorPacketContext::CID2ProcessorPacketContext(void)
{
}


CID2ProcessorPacketContext::~CID2ProcessorPacketContext(void)
{
}


CID2Processor::CID2Processor(void)
{
}


CID2Processor::~CID2Processor(void)
{
}


CID2Processor::TReplies
CID2Processor::ProcessSomeRequests(CID2_Request_Packet& /*packet*/,
                                   CID2ProcessorResolver* /*resolver*/)
{
    return TReplies();
}


bool CID2Processor::ProcessRequest(TReplies& /*replies*/,
                                   CID2_Request& /*request*/,
                                   CID2ProcessorResolver* /*resolver*/)
{
    return false;
}


bool CID2Processor::NeedToProcessReplies(void) const
{
    return false;
}


CRef<CID2ProcessorContext> CID2Processor::CreateContext(void)
{
    return null;
}


CRef<CID2ProcessorPacketContext>
CID2Processor::ProcessPacket(CID2ProcessorContext* /*context*/,
                             CID2_Request_Packet& packet,
                             TReplies& replies)
{
    // redirect to old interface by default
    replies = ProcessSomeRequests(packet);
    return null;
}


void CID2Processor::ProcessReply(CID2ProcessorContext* /*context*/,
                                 CID2ProcessorPacketContext* /*packet_context*/,
                                 CID2_Reply& reply,
                                 TReplies& replies)
{
    // copy the original reply by default
    replies.push_back(Ref(&reply));
}


END_objects_SCOPE // namespace ncbi::objects::


CPluginManager_DllResolver*
CDllResolver_Getter<objects::CID2Processor>::operator()(void)
{
    CPluginManager_DllResolver* resolver =
        new CPluginManager_DllResolver
        (CInterfaceVersion<objects::CID2Processor>::GetName(),
         kEmptyStr,
         CVersionInfo::kAny,
         CDll::eAutoUnload);
    resolver->SetDllNamePrefix("ncbi");
    return resolver;
}

END_NCBI_SCOPE

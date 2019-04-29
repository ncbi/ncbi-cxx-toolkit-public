#ifndef OBJECTS_ID2_ID2PROCESSOR_HPP
#define OBJECTS_ID2_ID2PROCESSOR_HPP
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
*  File Description: Interface for plug-in ID2 processing
*
*/

#include <corelib/ncbiobj.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CSeq_id;
class CID2_Request_Packet;
class CID2_Request;
class CID2_Reply;

// CID2ProcessorResolver callback can be used to get information from user of
// CID2Processor interface to avoid possible deadlock or infinite recursion
class NCBI_ID2_EXPORT CID2ProcessorResolver : public CObject
{
public:
    CID2ProcessorResolver(void);
    virtual ~CID2ProcessorResolver(void);

    typedef vector<CConstRef<CSeq_id> > TIds;
    typedef vector<CRef<CID2_Reply> > TReplies;

    virtual TIds GetIds(const CSeq_id& id) = 0;

    virtual void ProcessPacket(TReplies& replies,
                               CID2_Request_Packet& packet) = 0;
};


// CID2ProcessorContext is used to hold connection specific state
class NCBI_ID2_EXPORT CID2ProcessorContext : public CObject
{
public:
    CID2ProcessorContext(void);
    virtual ~CID2ProcessorContext(void);
};


// CID2ProcessorPacketContext is used to hold request packet specific state
class NCBI_ID2_EXPORT CID2ProcessorPacketContext : public CObject
{
public:
    CID2ProcessorPacketContext(void);
    virtual ~CID2ProcessorPacketContext(void);
};


class NCBI_ID2_EXPORT CID2Processor : public CObject
{
public:
    CID2Processor(void);
    virtual ~CID2Processor(void);
    
    typedef vector<CRef<CID2_Reply> > TReplies;

    // old interface, optional

    virtual TReplies ProcessSomeRequests(CID2_Request_Packet& packet,
                                         CID2ProcessorResolver* resolver = 0);
    virtual bool ProcessRequest(TReplies& replies,
                                CID2_Request& request,
                                CID2ProcessorResolver* resolver = 0);

    // new interface, optional
    
    // return true if the processer need to process replies from server via ProcessReply()
    virtual bool NeedToProcessReplies(void) const;

    // CID2ProcessorParameters holds parameters that control behavior of the processor.
    // The same CID2Processor object can be used for all connections, but each connection should
    // have its own CID2ProcessorContext object.
    // It's allowed to return null if there's no connection specific data
    virtual CRef<CID2ProcessorContext> CreateContext(void);

    // If CID2Processor needs to process replies too, it should create
    // CID2ProcessorPacketContext object for later use in ProcessReply().
    // Otherwise it's allowed to return null.
    // The context pointer should be the same returned from CreateContext() call.
    virtual CRef<CID2ProcessorPacketContext> ProcessPacket(CID2ProcessorContext* context,
                                                           CID2_Request_Packet& packet,
                                                           TReplies& replies);
    
    // Process reply from server
    // The context pointer should be the same returned from CreateContext() call.
    // The packet_context pointer should be the same returned from ProcessPacket() call.
    virtual void ProcessReply(CID2ProcessorContext* context,
                              CID2ProcessorPacketContext* packet_context,
                              CID2_Reply& reply,
                              TReplies& replies);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJECTS_ID2_ID2PROCESSOR_HPP

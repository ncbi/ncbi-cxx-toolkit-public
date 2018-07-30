#ifndef RESOLVER_HANDLER__HPP
#define RESOLVER_HANDLER__HPP

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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */
 
#include <objtools/pubseq_gateway/cache/psg_cache.hpp>

#include "http_server_transport.hpp"
#include "pending_operation.hpp"


USING_NCBI_SCOPE;

using TRslvIncludeData = unsigned;

struct SResolverRequst
{
    SResolverRequst(const string& seq_id, int  seq_id_type, bool  seq_id_type_provided,
                    int id_type, bool id_type_provided, TRslvIncludeData  include_data_flags) :
        m_SeqId(seq_id),
        m_SeqIdType(seq_id_type),
        m_SeqIdTypeProvided(seq_id_type_provided),
        m_IdType(id_type),
        m_IdTypeProvided(id_type_provided),
        m_IncludeDataFlags(include_data_flags)
    {}
public:
    string                      m_SeqId;
    int                         m_SeqIdType;
    bool                        m_SeqIdTypeProvided;
    int                         m_IdType;
    bool                        m_IdTypeProvided;
    TRslvIncludeData            m_IncludeDataFlags;
};

class CResolverHandler
{
public:
    CResolverHandler(HST::CHttpReply<CPendingOperation>& resp, SResolverRequst&& request,
                     shared_ptr<CCassConnection> conn, unsigned int timeout, unsigned int max_retries,
                     CPubseqGatewayCache* cache,
                     CRef<CRequestContext>  request_context);
private:
};



#endif

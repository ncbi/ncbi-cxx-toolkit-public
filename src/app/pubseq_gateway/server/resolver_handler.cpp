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

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objtools/pubseq_gateway/cache/psg_cache.hpp>

#include "resolver_handler.hpp"

USING_IDBLOB_SCOPE;
USING_SCOPE(objects);

static const constexpr int kIdTypeGI = 12;
static const constexpr int kIdTypePrimaryThreshold = 10000;

CResolverHandler::CResolverHandler(HST::CHttpReply<CPendingOperation>& resp, SResolverRequst&& request,
                                   shared_ptr<CCassConnection> conn, unsigned int timeout, unsigned int max_retries,
                                   CPubseqGatewayCache* cache,
                                   CRef<CRequestContext>  request_context)
{
    bool is_primary = false;
    string seq_id_str;
    int id_type = -1;
    int version = -1;

    if (request.m_IdTypeProvided && request.m_IdType >= 0) {
        id_type = request.m_IdType;
        seq_id_str = request.m_SeqId;
        is_primary = (id_type != kIdTypeGI && id_type < kIdTypePrimaryThreshold);
    }
    else {
        CSeq_id                 seq_id(request.m_SeqId);
        if (seq_id.Which() == CSeq_id::e_Gi) {
            seq_id_str = NStr::NumericToString(seq_id.GetGi());
            id_type = kIdTypeGI;
        }
        else {
            const auto textseq_id = seq_id.GetTextseq_Id();
            if (textseq_id) {
                if (textseq_id->IsSetAccession()) {
                    seq_id_str = textseq_id->GetAccession();
                    is_primary = true;
                    if (textseq_id->IsSetVersion())
                        version = textseq_id->GetVersion();
                }
                else if (textseq_id->IsSetName()) {
                    seq_id_str = textseq_id->GetName();
                }
            }
        }
    }

    if (seq_id_str.empty())
        NCBI_THROW(CPubseqGatewayException, eInvalidRequest,
                   (string("Invalid/unsupported seq_id format: ") + request.m_SeqId).c_str());

    bool cache_hit = false;
    if (!is_primary) {
        string csi_data;
        if (id_type >= 0)
            cache_hit = cache->LookupCsiBySeqIdIdType(seq_id_str, id_type, csi_data);
        else {
            int key_id_type;
            cache_hit = cache->LookupCsiBySeqId(seq_id_str, key_id_type, csi_data);
        }
        if (cache_hit) {
            // unpack primary identificator using protobuf class and fall through down
        }
        else {
            // resolve si to csi using Cass*
        }
    }

    if (is_primary) {
        string data;
        if (id_type >= 0)
            cache_hit = cache->LookupBioseqInfoByAccessionVersionIdType(seq_id_str, version > 0 ? version : 0, id_type, data);
        else if (version >= 0) {
            int key_id_type;
            cache_hit = cache->LookupBioseqInfoByAccessionVersion(seq_id_str, version, key_id_type, data);
        }
        else {
            int version;
            int key_id_type;
            cache_hit = cache->LookupBioseqInfoByAccession(seq_id_str, version, id_type, data);
        }
    }
    

}
    
    

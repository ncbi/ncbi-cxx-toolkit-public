#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

/* ===========================================================================
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
 * Author:  Christiam Camacho
 *
 */

/// @file blast_aux_priv.cpp
/// Implements various auxiliary (private) functions for BLAST

#include <ncbi_pch.hpp>
#include "blast_aux_priv.hpp"
#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/api/seqinfosrc_seqdb.hpp>
#include <algo/blast/api/blast_exception.hpp>

#include <objects/seqloc/Seq_loc.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

IBlastSeqInfoSrc*
InitSeqInfoSrc(const BlastSeqSrc* seqsrc)
{
    string db_name;
    if (const char* seqsrc_name = BlastSeqSrcGetName(seqsrc)) {
        db_name.assign(seqsrc_name);
    }
    if (db_name.empty()) {
        NCBI_THROW(CBlastException, eNotSupported,
                   "BlastSeqSrc does not provide a name, probably it is not a"
                   " BLAST database");
    }
    bool is_prot = BlastSeqSrcGetIsProt(seqsrc) ? true : false;
    return new CSeqDbSeqInfoSrc(db_name, is_prot);
}

IBlastSeqInfoSrc*
InitSeqInfoSrc(CSeqDB* seqdb)
{
    return new CSeqDbSeqInfoSrc(seqdb);
}

CConstRef<CSeq_loc> CreateWholeSeqLocFromIds(const list< CRef<CSeq_id> > seqids)
{
    ASSERT(!seqids.empty());
    CRef<CSeq_loc> retval(new CSeq_loc);
    retval->SetWhole().Assign(**seqids.begin());
    return retval;
}

void
Blast_Message2TSearchMessages(const Blast_Message* blmsg,
                              const BlastQueryInfo* query_info,
                              TSearchMessages& messages)
{
    if ( !blmsg || !query_info ) {
        return;
    }

    if (messages.size() != (size_t) query_info->num_queries) {
        messages.resize(query_info->num_queries);
    }

    const BlastContextInfo* kCtxInfo = query_info->contexts;

    // First copy the errors...
    for (int i = query_info->first_context; 
         i <= query_info->last_context; i++) {

        if ( !kCtxInfo[i].is_valid && blmsg->message ) {
            string msg(blmsg->message);
            CRef<CSearchMessage> sm(new CSearchMessage(blmsg->severity,
                                                       kCtxInfo[i].query_index,
                                                       msg));
            messages[kCtxInfo[i].query_index].push_back(sm);
        }
    }

    // ... then remove duplicate error messages
    messages.RemoveDuplicates();
}

string
BlastErrorCode2String(Int2 error_code)
{
    Blast_Message* blast_msg = Blast_PerrorEx(error_code, __FILE__, __LINE__);
    string retval(blast_msg->message);
    blast_msg = Blast_MessageFree(blast_msg);
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

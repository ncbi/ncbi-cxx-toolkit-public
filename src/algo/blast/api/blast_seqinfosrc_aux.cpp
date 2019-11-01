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
* Author: Vahram Avagyan
*
*/

/// @file blast_seqinfosrc_aux.cpp
/// Implementation of auxiliary functions using IBlastSeqInfoSrc to
/// retrieve ids and related sequence information.

#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_seqinfosrc_aux.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <serial/typeinfo.hpp>
#include <corelib/ncbiutil.hpp>

#include <algorithm>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

void GetSequenceLengthAndId(const blast::IBlastSeqInfoSrc * seqinfo_src,
                            int                      oid,
                            CRef<CSeq_id>          & seqid,
                            TSeqPos                * length)
{
    _ASSERT(length);
    list<CRef<CSeq_id> > seqid_list = seqinfo_src->GetId(oid);
    CRef<CSeq_id> id = FindBestChoice(seqid_list, CSeq_id::BlastRank);
    if (id.NotEmpty()) {
        seqid.Reset(new CSeq_id);
        SerialAssign(*seqid, *id);
    }
    *length = seqinfo_src->GetLength(oid);

    return;
}

void GetFilteredRedundantGis(const IBlastSeqInfoSrc & seqinfo_src,
                             int                      oid,
                             vector<TGi>            & gis)
{
    gis.resize(0);
    
    if (! seqinfo_src.HasGiList()) {
        return;
    }
    
    list< CRef<CSeq_id> > seqid_list = seqinfo_src.GetId(oid);
    gis.reserve(seqid_list.size());
    
    ITERATE(list< CRef<CSeq_id> >, id, seqid_list) {
        if ((**id).IsGi()) {
            gis.push_back((**id).GetGi());
        }
    }

	sort(gis.begin(), gis.end());
}

void GetFilteredRedundantSeqids(const IBlastSeqInfoSrc & seqinfo_src,
                             int                      oid,
                             vector<string>            & seqids,
			     bool			use_gi)
{
    seqids.resize(0);
    
    if (! seqinfo_src.HasGiList()) {
        return;
    }
    
    list< CRef<CSeq_id> > seqid_list = seqinfo_src.GetId(oid);
    
    ITERATE(list< CRef<CSeq_id> >, id, seqid_list) {
	if (use_gi)
	{
        	if ((**id).IsGi())
		{
			string sid_string = "gi:" + (*id)->GetSeqIdString(true);
			seqids.push_back(sid_string);
		}
	}
	else
	{
		const CTextseq_id* tsid = (*id)->GetTextseq_Id();
		if (tsid && tsid->IsSetAccession())
		{
			string sid_string = "seqid:" + (*id)->GetSeqIdString(true);
			seqids.push_back(sid_string);
		}
		else if ((*id)->IsPdb())
		{
			string sid_string = "seqid:" + (*id)->GetSeqIdString(true);
			seqids.push_back(sid_string);
		}
	}
    }
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

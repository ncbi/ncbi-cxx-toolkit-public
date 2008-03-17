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
 * Author: Christiam Camacho
 *
 */

/** @file blastdb_dataextract.cpp
 *  Defines classes which extract data from a BLAST database
 */

#include <ncbi_pch.hpp>
#include <objtools/blast_format/blastdb_dataextract.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <corelib/ncbiutil.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/** @addtogroup BlastFormatting
 *
 * @{
 */

string CGiExtractor::Extract(const CBlastDBSeqId& id, CSeqDB& blastdb)
{
    int gi = CBlastDBSeqId::kInvalid;
    if (id.IsGi()) {
        gi = id.GetGi();
    } else {
        blastdb.OidToGi(COidExtractor().ExtractOID(id, blastdb), gi);
    }
    return NStr::IntToString(gi);
}

string CPigExtractor::Extract(const CBlastDBSeqId& id, CSeqDB& blastdb)
{
    int pig = CBlastDBSeqId::kInvalid;
    if (id.IsPig()) {
        pig = id.GetPig();
    } else {
        blastdb.OidToPig(COidExtractor().ExtractOID(id, blastdb), pig);
    }
    return NStr::IntToString(pig);
}

string COidExtractor::Extract(const CBlastDBSeqId& id, CSeqDB& blastdb)
{
    return NStr::IntToString(ExtractOID(id, blastdb));
}

int COidExtractor::ExtractOID(const CBlastDBSeqId& id, CSeqDB& blastdb)
{
    int retval = CBlastDBSeqId::kInvalid;
    if (id.IsOID()) {
        retval = id.GetOID();
    } else if (id.IsGi()) {
        blastdb.GiToOid(id.GetGi(), retval);
    } else if (id.IsPig()) {
        blastdb.PigToOid(id.GetPig(), retval);
    } else if (id.IsStringId()) {
        vector<int> oids;
        blastdb.AccessionToOids(id.GetStringId(), oids);
        if ( !oids.empty() ) {
            retval = oids.front();
        }
    }

    return retval;
}

string CSeqLenExtractor::Extract(const CBlastDBSeqId& id, CSeqDB& blastdb)
{
    const int kOid = COidExtractor().ExtractOID(id, blastdb);
    return NStr::IntToString(blastdb.GetSeqLengthApprox(kOid));
}

string CTaxIdExtractor::Extract(const CBlastDBSeqId& id, CSeqDB& blastdb)
{
    return NStr::IntToString(ExtractTaxID(id, blastdb));
}

int CTaxIdExtractor::ExtractTaxID(const CBlastDBSeqId& id, CSeqDB& blastdb)
{
    const int kOid = COidExtractor().ExtractOID(id, blastdb);
    int retval = CBlastDBSeqId::kInvalid;
    map<int, int> gi2taxid;

    blastdb.GetTaxIDs(kOid, gi2taxid);
    if (id.IsGi()) {
        retval = gi2taxid[id.GetGi()];
    } else {
        retval = gi2taxid.begin()->second;
    }
    return retval;
}

string CSeqDataExtractor::Extract(const CBlastDBSeqId& id, CSeqDB& blastdb)
{
    string retval;
    const int kOid = COidExtractor().ExtractOID(id, blastdb);
    blastdb.GetSequenceAsString(kOid, retval);
    return retval;
}

static CRef<CBioseq>
s_GetBioseq(const CBlastDBSeqId& id, CSeqDB& blastdb)
{
    const int kOid = COidExtractor().ExtractOID(id, blastdb);
    const int kTargetGi = id.IsGi() ? id.GetGi() : 0;
    return blastdb.GetBioseqNoData(kOid, kTargetGi);
}

string CTitleExtractor::Extract(const CBlastDBSeqId& id, CSeqDB& blastdb)
{
    string retval;
    CRef<CBioseq> bioseq = s_GetBioseq(id, blastdb);
    if (bioseq->CanGetDescr()) {
        ITERATE(CSeq_descr::Tdata, seq_desc, bioseq->GetDescr().Get()) {
            if ((*seq_desc)->IsTitle()) {
                retval.assign((*seq_desc)->GetTitle());
                break;
            }
        }
    }

    if (retval.empty()) { 
        // Get the Bioseq's ID
        retval = CSeq_id::GetStringDescr(*bioseq, 
                                         CSeq_id::eFormat_BestWithVersion);
    }

    return retval;
}

string CAccessionExtractor::Extract(const CBlastDBSeqId& id, CSeqDB& blastdb)
{
    CRef<CBioseq> bioseq = s_GetBioseq(id, blastdb);
    _ASSERT(bioseq->CanGetId());
    CRef<CSeq_id> best_id = FindBestChoice(bioseq->GetId(), CSeq_id::BestRank);

    string retval;
    best_id->GetLabel(&retval, CSeq_id::eContent);    
    return retval;
}

string CLineageExtractor::Extract(const CBlastDBSeqId& id, CSeqDB& blastdb)
{
    const int kTaxID = CTaxIdExtractor().ExtractTaxID(id, blastdb);
    SSeqDBTaxInfo tax_info;
    blastdb.GetTaxInfo(kTaxID, tax_info);
    _ASSERT(kTaxID == tax_info.taxid);
    return tax_info.common_name;
}

END_NCBI_SCOPE

/* @} */

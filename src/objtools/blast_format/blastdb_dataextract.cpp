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

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objtools/blast_format/blastdb_dataextract.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <corelib/ncbiutil.hpp>
#include <algo/blast/blastinput/blast_input.hpp>    // for CInputException
#include <util/sequtil/sequtil_manip.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(blast);
USING_SCOPE(objects);

/** @addtogroup BlastFormatting
 *
 * @{
 */

string CGiExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    int gi = CBlastDBSeqId::kInvalid;
    if (id.IsGi()) {
        gi = id.GetGi();
    } else {
        blastdb.OidToGi(COidExtractor().ExtractOID(id, blastdb), gi);
    }
    return NStr::IntToString(gi);
}

string CPigExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    int pig = CBlastDBSeqId::kInvalid;
    if (id.IsPig()) {
        pig = id.GetPig();
    } else {
        blastdb.OidToPig(COidExtractor().ExtractOID(id, blastdb), pig);
    }
    return NStr::IntToString(pig);
}

string COidExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    return NStr::IntToString(ExtractOID(id, blastdb));
}

int COidExtractor::ExtractOID(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    int retval = CBlastDBSeqId::kInvalid;
    if (id.IsOID()) {
        retval = id.GetOID();
    } else if (id.IsGi()) {
        blastdb.GiToOid(id.GetGi(), retval);
        // Verify that the GI of interest is among the Seq-ids this oid
        // retrieves 
        list< CRef<CSeq_id> > filtered_ids = blastdb.GetSeqIDs(retval);
        bool found = false;
        ITERATE(list< CRef<CSeq_id> >, seqid, filtered_ids) {
            if ((*seqid)->IsGi() && ((*seqid)->GetGi() == id.GetGi())) {
                found = true;
                break;
            }
        }
        if ( !found ) {
            retval = CBlastDBSeqId::kInvalid;
        }
    } else if (id.IsPig()) {
        blastdb.PigToOid(id.GetPig(), retval);
    } else if (id.IsStringId()) {
        vector<int> oids;
        blastdb.AccessionToOids(id.GetStringId(), oids);
        if ( !oids.empty() ) {
            retval = oids.front();
        }
    }
    if (retval == CBlastDBSeqId::kInvalid) {
        NCBI_THROW(CSeqDBException, eArgErr, 
                   "Entry not found in BLAST database");
    }
    id.SetOID(retval);

    return retval;
}

TSeqPos
CSeqLenExtractor::ExtractLength(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    return blastdb.GetSeqLength(COidExtractor().ExtractOID(id, blastdb));
}

string CSeqLenExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    return NStr::IntToString(ExtractLength(id, blastdb));
}

string CTaxIdExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    return NStr::IntToString(ExtractTaxID(id, blastdb));
}

int CTaxIdExtractor::ExtractTaxID(CBlastDBSeqId& id, CSeqDB& blastdb)
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

string CSeqDataExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    string retval;
    const int kOid = COidExtractor().ExtractOID(id, blastdb);
    try {
        blastdb.GetSequenceAsString(kOid, retval, m_SeqRange);
        if (m_Strand == eNa_strand_minus) {
            CSeqManip::ReverseComplement(retval, CSeqUtil::e_Iupacna, 
                                         0, retval.size());
        }
    } catch (CSeqDBException& e) {
        //FIXME: change the enumeration when it's availble
        if (e.GetErrCode() == CSeqDBException::eArgErr ||
            e.GetErrCode() == CSeqDBException::eFileErr/*eOutOfRange*/) {
            NCBI_THROW(CInputException, eInvalidRange, e.GetMsg());
        }
        throw;
    }
    return retval;
}

/// Auxiliary function to retrieve a Bioseq from the BLAST database
/// @param id BLAST DB sequence identifier [in]
/// @param blastdb BLAST database to fetch data from [in]
/// @param get_sequence should the sequence data be retrieved?
static CRef<CBioseq>
s_GetBioseq(CBlastDBSeqId& id, CSeqDB& blastdb, bool get_sequence = false,
            bool get_target_gi_only = false)
{
    const int kOid = COidExtractor().ExtractOID(id, blastdb);
    if (get_target_gi_only) _ASSERT(id.IsGi());
    const int kTargetGi = get_target_gi_only ? id.GetGi() : 0;

    CRef<CBioseq> retval;
    try {
        retval = get_sequence 
            ? blastdb.GetBioseq(kOid, kTargetGi) 
            : blastdb.GetBioseqNoData(kOid, kTargetGi);
    } catch (const CSeqDBException& e) {
        // this happens when CSeqDB detects a GI that doesn't belong to a
        // filtered database (e.g.: swissprot as a subset of nr)
        if (e.GetMsg().find("oid headers do not contain target gi")) {
            NCBI_THROW(CSeqDBException, eArgErr, 
                       "Entry not found in BLAST database");
        }
    }
    return retval;
}

string CTitleExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    string retval;
    CRef<CBioseq> bioseq = s_GetBioseq(id, blastdb, false, m_ShowTargetOnly);
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
        retval = CSeq_id::GetStringDescr(*bioseq, CSeq_id::eFormat_FastA);
    }

    return retval;
}

string CAccessionExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    CRef<CBioseq> bioseq = s_GetBioseq(id, blastdb, false, m_ShowTargetOnly);
    _ASSERT(bioseq->CanGetId());
    CRef<CSeq_id> theId = FindBestChoice(bioseq->GetId(), CSeq_id::WorstRank);

    string retval;
    theId->GetLabel(&retval, CSeq_id::eContent);
    return retval;
}

string 
CCommonTaxonomicNameExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    const int kTaxID = CTaxIdExtractor().ExtractTaxID(id, blastdb);
    SSeqDBTaxInfo tax_info;
    blastdb.GetTaxInfo(kTaxID, tax_info);
    _ASSERT(kTaxID == tax_info.taxid);
    return tax_info.common_name;
}

string 
CScientificNameExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    const int kTaxID = CTaxIdExtractor().ExtractTaxID(id, blastdb);
    SSeqDBTaxInfo tax_info;
    blastdb.GetTaxInfo(kTaxID, tax_info);
    _ASSERT(kTaxID == tax_info.taxid);
    return tax_info.scientific_name;
}

CFastaExtractor::CFastaExtractor(TSeqPos line_width, 
             TSeqRange range /* = TSeqRange() */,
             objects::ENa_strand strand /* = objects::eNa_strand_other */,
             bool target_only /* = false */,
             bool ctrl_a /* = false */)
    : CSeqDataExtractor(range, strand), m_FastaOstream(m_OutputStream),
    m_ShowTargetOnly(target_only), m_UseCtrlA(ctrl_a)
{
    m_FastaOstream.SetWidth(line_width);
    m_FastaOstream.SetAllFlags(CFastaOstream::fKeepGTSigns);
}

/// Hacky function to replace ' >' for Ctrl-A's in the Bioseq's title
static void s_ReplaceCtrlAsInTitle(CRef<CBioseq> bioseq)
{
    static const string kTarget(" >gi");
    static const string kCtrlA = string(1, '\001') + string("gi");
    NON_CONST_ITERATE(CSeq_descr::Tdata, desc, bioseq->SetDescr().Set()) {
        if ((*desc)->Which() == CSeqdesc::e_Title) {
            NStr::ReplaceInPlace((*desc)->SetTitle(), kTarget, kCtrlA);
            break;
        }
    }
}

string CFastaExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    CRef<CBioseq> bioseq = s_GetBioseq(id, blastdb, true, m_ShowTargetOnly);
    if (m_UseCtrlA) {
        s_ReplaceCtrlAsInTitle(bioseq);
    }
    CRef<CSeq_loc> range;
    if (m_SeqRange.NotEmpty() || m_Strand != eNa_strand_other) {
        CRef<CSeq_id> seqid = FindBestChoice(bioseq->GetId(), 
                                             CSeq_id::BestRank);
        if (m_SeqRange.NotEmpty()) {
            range.Reset(new CSeq_loc(*seqid, m_SeqRange.GetFrom(),
                                     m_SeqRange.GetTo(), m_Strand));
            m_FastaOstream.ResetFlag(CFastaOstream::fSuppressRange);
        } else {
            TSeqPos length = CSeqLenExtractor().ExtractLength(id, blastdb);
            range.Reset(new CSeq_loc(*seqid, 0, length-1, m_Strand));
            m_FastaOstream.SetFlag(CFastaOstream::fSuppressRange);
        }
    }
    m_OutputStream.str(""); // clean the buffer
    try { m_FastaOstream.Write(*bioseq, range); }
    catch (const CObjmgrUtilException& e) {
        if (e.GetErrCode() == CObjmgrUtilException::eBadLocation) {
            NCBI_THROW(CInputException, eInvalidRange, 
                       "Invalid sequence range");
        }
    }
    return m_OutputStream.str();
}

END_NCBI_SCOPE

/* @} */

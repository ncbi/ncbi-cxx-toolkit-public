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
#include <objtools/blast/blastdb_format/invalid_data_exception.hpp>
#include <objtools/blast/blastdb_format/blastdb_dataextract.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <corelib/ncbiutil.hpp>
#include <util/sequtil/sequtil_manip.hpp>
#include <util/checksum.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

void CBlastDBExtractor::SetSeqId(const CBlastDBSeqId &id, bool get_defline) {
    if (id.IsOID()) {
        m_Oid = id.GetOID();
    } else if (id.IsGi()) {
        m_BlastDb.GiToOid(id.GetGi(), m_Oid);
    } else if (id.IsPig()) {
        m_BlastDb.PigToOid(id.GetPig(), m_Oid);
    } else if (id.IsStringId()) {
        const CSeq_id sid(id.GetStringId());
        m_BlastDb.SeqidToOid(sid, m_Oid);
    }
    if (m_Oid < 0) {
        NCBI_THROW(CSeqDBException, eArgErr, 
                   "Entry not found in BLAST database");
    }

    if (!get_defline) return;
    try {
        m_Gi = (id.IsGi()) ? id.GetGi() : 0;
        m_Bioseq.Reset(m_BlastDb.GetBioseqNoData(m_Oid, m_Gi)); 
    } catch (const CSeqDBException& e) {
        // this happens when CSeqDB detects a GI that doesn't belong to a
        // filtered database (e.g.: swissprot as a subset of nr)
        if (e.GetMsg().find("oid headers do not contain target gi")) {
            NCBI_THROW(CSeqDBException, eArgErr, 
                       "Entry not found in BLAST database");
        }
    }
}

string CBlastDBExtractor::ExtractOid() {
    return NStr::IntToString(m_Oid);
}

string CBlastDBExtractor::ExtractPig() {
    int pig;
    m_BlastDb.OidToPig(m_Oid, pig);
    return NStr::IntToString(pig);
}

string CBlastDBExtractor::ExtractGi() {
    ITERATE(list<CRef<CSeq_id> >, itr, m_Bioseq->GetId()) {
        if ((*itr)->IsGi()) {
            return NStr::IntToString((*itr)->GetGi());
        }
    } 
    return "NA";
}

string CBlastDBExtractor::ExtractAccession() {
    string acc;
    CRef<CSeq_id> theId = FindBestChoice(m_Bioseq->GetId(), CSeq_id::WorstRank);
    theId->GetLabel(&acc, CSeq_id::eContent);
    return acc;
}

string CBlastDBExtractor::ExtractTitle() {
    ITERATE(list <CRef <CSeqdesc> >, itr, m_Bioseq->GetDescr().Get()) {
        if ((*itr)->IsTitle()) {
            return (*itr)->GetTitle();
        }
    }
    return "NA";
}

string CBlastDBExtractor::ExtractTaxId() {
    return NStr::IntToString(x_ExtractTaxId());
}

string CBlastDBExtractor::ExtractCommonTaxonomicName() {
    const int kTaxID = x_ExtractTaxId();
    SSeqDBTaxInfo tax_info;
    m_BlastDb.GetTaxInfo(kTaxID, tax_info);
    _ASSERT(kTaxID == tax_info.taxid);
    return tax_info.common_name;
}

string CBlastDBExtractor::ExtractScientificName() {
    const int kTaxID = x_ExtractTaxId();
    SSeqDBTaxInfo tax_info;
    m_BlastDb.GetTaxInfo(kTaxID, tax_info);
    _ASSERT(kTaxID == tax_info.taxid);
    return tax_info.scientific_name;
}

string CBlastDBExtractor::ExtractMaskingData() {
    static const string kNoMasksFound("none");
#if ((defined(NCBI_COMPILER_WORKSHOP) && (NCBI_COMPILER_VERSION <= 550))  ||  \
     defined(NCBI_COMPILER_MIPSPRO))
    return kNoMasksFound;
#else
    CSeqDB::TSequenceRanges masked_ranges;
    x_ExtractMaskingData(masked_ranges, m_FmtAlgoId);
    if (masked_ranges.empty())  return kNoMasksFound;

    CNcbiOstrstream out;
    ITERATE(CSeqDB::TSequenceRanges, range, masked_ranges) {
        out << range->first << "-" << range->second << ";";
    }
    return CNcbiOstrstreamToString(out);
#endif
}

string CBlastDBExtractor::ExtractSeqData() {
    string seq;
    try {
        m_BlastDb.GetSequenceAsString(m_Oid, seq, m_SeqRange);
        CSeqDB::TSequenceRanges masked_ranges;
        x_ExtractMaskingData(masked_ranges, m_FiltAlgoId);
        ITERATE(CSeqDB::TSequenceRanges, mask, masked_ranges) {
            transform(&seq[mask->first], &seq[mask->second],
                      &seq[mask->first], (int (*)(int))tolower);
        }
        if (m_Strand == eNa_strand_minus) {
            CSeqManip::ReverseComplement(seq, CSeqUtil::e_Iupacna, 
                                         0, seq.size());
        }
    } catch (CSeqDBException& e) {
        //FIXME: change the enumeration when it's availble
        if (e.GetErrCode() == CSeqDBException::eArgErr ||
            e.GetErrCode() == CSeqDBException::eFileErr/*eOutOfRange*/) {
            NCBI_THROW(CInvalidDataException, eInvalidRange, e.GetMsg());
        }
        throw;
    }
    return seq;
}

string CBlastDBExtractor::ExtractSeqLen() {
    return NStr::IntToString(m_BlastDb.GetSeqLength(m_Oid));
}

// Calculates hash for a buffer in IUPACna (NCBIeaa for proteins) format.
// NOTE: if sequence is in a different format, the function below can be modified to convert
// each byte into IUPACna encoding on the fly.  
static int s_GetHash(const char* buffer, int length)
{
    CChecksum crc(CChecksum::eCRC32ZIP);

    for(int ii = 0; ii < length; ii++) {
        if (buffer[ii] != '\n')
            crc.AddChars(buffer+ii,1);
    }
    return (crc.GetChecksum() ^ (0xFFFFFFFFL));
}

string CBlastDBExtractor::ExtractHash() {
    string seq;
    m_BlastDb.GetSequenceAsString(m_Oid, seq);
    return NStr::IntToString(s_GetHash(seq.c_str(), seq.size()));
}

static void s_ReplaceCtrlAsInTitle(CRef<CBioseq> bioseq)
{
    static const string kTarget(" >gi|");
    static const string kCtrlA = string(1, '\001') + string("gi|");
    NON_CONST_ITERATE(CSeq_descr::Tdata, desc, bioseq->SetDescr().Set()) {
        if ((*desc)->Which() == CSeqdesc::e_Title) {
            NStr::ReplaceInPlace((*desc)->SetTitle(), kTarget, kCtrlA);
            break;
        }
    }
}

string CBlastDBExtractor::ExtractFasta(const CBlastDBSeqId &id) {
    stringstream out("");

    CFastaOstream fasta(out);
    fasta.SetWidth(m_LineWidth);
    fasta.SetAllFlags(CFastaOstream::fKeepGTSigns);

    SetSeqId(id, false);
    int target_gi = 0;

    if (m_TargetOnly && id.IsGi()) {
        target_gi = id.GetGi();
    }

    CRef<CBioseq> bioseq(m_BlastDb.GetBioseq(m_Oid, target_gi)); 

    if (m_UseCtrlA) {
        s_ReplaceCtrlAsInTitle(bioseq);
    }

    CRef<CSeq_id> seqid = FindBestChoice(bioseq->GetId(), CSeq_id::BestRank);

    // Handle the case when a sequence range is provided
    CRef<CSeq_loc> range;
    if (m_SeqRange.NotEmpty() || m_Strand != eNa_strand_other) {
        if (m_SeqRange.NotEmpty()) {
            range.Reset(new CSeq_loc(*seqid, m_SeqRange.GetFrom(),
                                     m_SeqRange.GetTo(), m_Strand));
            fasta.ResetFlag(CFastaOstream::fSuppressRange);
        } else {
            TSeqPos length = bioseq->GetLength();
            range.Reset(new CSeq_loc(*seqid, 0, length-1, m_Strand));
            fasta.SetFlag(CFastaOstream::fSuppressRange);
        }
    }

    // Handle any requests for masked FASTA
    static const CFastaOstream::EMaskType kMaskType = CFastaOstream::eSoftMask;
    CSeqDB::TSequenceRanges masked_ranges;
    x_ExtractMaskingData(masked_ranges, m_FiltAlgoId);
    if (!masked_ranges.empty()) {
        CRef<CSeq_loc> masks(new CSeq_loc);
        ITERATE(CSeqDB::TSequenceRanges, itr, masked_ranges) {
            CRef<CSeq_loc> mask(new CSeq_loc(*seqid, itr->first, itr->second -1));
            masks->SetMix().Set().push_back(mask);
        }
        fasta.SetMask(kMaskType, masks);
    }

    try { fasta.Write(*bioseq, range); }
    catch (const CObjmgrUtilException& e) {
        if (e.GetErrCode() == CObjmgrUtilException::eBadLocation) {
            NCBI_THROW(CInvalidDataException, eInvalidRange, 
                       "Invalid sequence range");
        }
    }
    return out.str();
}

int CBlastDBExtractor::x_ExtractTaxId() {
    map <int, int> gi2taxid;
    m_BlastDb.GetTaxIDs(m_Oid, gi2taxid);
    return gi2taxid[m_Gi];
}

void CBlastDBExtractor::x_ExtractMaskingData(CSeqDB::TSequenceRanges &ranges, int algo_id) {
    ranges.clear();
    if (algo_id != -1) {
        m_BlastDb.GetMaskData(m_Oid, algo_id, ranges);
    }
}

END_NCBI_SCOPE

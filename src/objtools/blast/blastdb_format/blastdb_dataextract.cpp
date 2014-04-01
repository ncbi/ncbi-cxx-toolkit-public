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

#define NOT_AVAILABLE "N/A"

void CBlastDBExtractor::SetSeqId(const CBlastDBSeqId &id, bool get_data) {
    m_Defline.Reset();
    m_Gi = ZERO_GI;
    m_Oid = -1;
    CRef<CSeq_id> seq_id;

    TGi target_gi = 0;
    CSeq_id *target_seq_id = NULL;

    if (id.IsOID()) {
        m_Oid = id.GetOID();
    } else if (id.IsGi()) {
        m_Gi = id.GetGi();
        m_BlastDb.GiToOid(m_Gi, m_Oid);
        if (m_TargetOnly || ! get_data) target_gi = m_Gi;
    } else if (id.IsPig()) {
        m_BlastDb.PigToOid(id.GetPig(), m_Oid);
    } else if (id.IsStringId()) {
        string acc(id.GetStringId());
        NStr::ToUpper(acc);
        vector<TOID> oids;
        m_BlastDb.AccessionToOids(acc, oids);
        if (!oids.empty()) {
            m_Oid = oids[0];
            if (m_TargetOnly || ! get_data) {
                // TODO check if id is complete
                seq_id.Reset(new CSeq_id(acc, CSeq_id::fParse_PartialOK | CSeq_id::fParse_Default));
                target_seq_id = &(*seq_id);
            }
        }
    }

    if (m_Oid < 0) {
        NCBI_THROW(CSeqDBException, eArgErr, 
                   "Entry not found in BLAST database");
    }

   	TSeqPos length = m_BlastDb.GetSeqLength(m_Oid);
   	if (length <= 0) {
       	NCBI_THROW(CSeqDBException, eArgErr,
           	       "Entry found in BLAST database has invalid length");
   	}

   	m_SeqRange = m_OrigSeqRange;
	if((TSeqPos)length <= m_SeqRange.GetTo())
	{
		m_SeqRange.SetTo(length-1);
	}

    if(TSeqRange::GetPositionMax() == m_OrigSeqRange.GetTo())
    {
		if (m_SeqRange.GetTo() < m_SeqRange.GetFrom()) {
	        NCBI_THROW(CSeqDBException, eArgErr,
	                   "start pos > length of sequence");
	    	}
    }

    try {
        if (get_data) {
            m_Bioseq.Reset(m_BlastDb.GetBioseq(m_Oid, target_gi, target_seq_id)); 
        } 
	else if (m_Gi <= 0)
	{  // If no GI, then all the Gi2XMaps will fail.
	    m_Bioseq.Reset(m_BlastDb.GetBioseqNoData(m_Oid, target_gi, target_seq_id));
	}

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
    if (m_Oid2Pig.first != m_Oid)
    {
        CSeqDB::TPIG pig;
    	m_BlastDb.OidToPig(m_Oid, pig);
        m_Oid2Pig.first = m_Oid;
        m_Oid2Pig.second = pig;
    }
    return NStr::IntToString(m_Oid2Pig.second);
}

string CBlastDBExtractor::ExtractGi() {
    x_SetGi();
    return (m_Gi ? NStr::IntToString(m_Gi) : NOT_AVAILABLE);
}

void CBlastDBExtractor::x_SetGi() {
    if (m_Gi) return;
    ITERATE(list<CRef<CSeq_id> >, itr, m_Bioseq->GetId()) {
        if ((*itr)->IsGi()) {
            m_Gi = (*itr)->GetGi();
            return;
        }
    } 
    return;
}

void CBlastDBExtractor::x_InitDefline() 
{
    if (m_Defline.NotEmpty()) {
        return;
    }
    if (m_Bioseq.NotEmpty()) {
        m_Defline = CSeqDB::ExtractBlastDefline(*m_Bioseq);
    }
    if (m_Defline.Empty()) {
        m_Defline = m_BlastDb.GetHdr(m_Oid);
    }
}

string CBlastDBExtractor::ExtractLinksInteger()
{
    x_InitDefline();
    string retval;

    ITERATE(CBlast_def_line_set::Tdata, itr, m_Defline->Get()) {
        const CRef<CSeq_id> seqid = FindBestChoice((*itr)->GetSeqid(),
                                                   CSeq_id::BestRank);
        _ASSERT(seqid.NotEmpty());
        if ((*itr)->IsSetLinks()) {
            if (seqid->IsGi()) {
                if (seqid->GetGi() == m_Gi) {
                    ITERATE(CBlast_def_line::TLinks, links_int, (*itr)->GetLinks()) {
                        retval += NStr::IntToString(*links_int) + ",";
                    }
                    break;
                }
            } else {
                ITERATE(CBlast_def_line::TLinks, links_int, (*itr)->GetLinks()) {
                    retval += NStr::IntToString(*links_int) + ",";
                }
            }
        }
    }
    if (retval.size()) {
        retval.erase(retval.size()-1, 1);   // remove the last comma
    }

    return (retval.empty() ? NOT_AVAILABLE : retval);
}

string CBlastDBExtractor::ExtractMembershipInteger()
{
    x_InitDefline();
    int retval = 0;

    if (m_Gi == ZERO_GI) {
        return NStr::IntToString(0);
    }

    ITERATE(CBlast_def_line_set::Tdata, itr, m_Defline->Get()) {
        const CRef<CSeq_id> seqid = FindBestChoice((*itr)->GetSeqid(),
                                                   CSeq_id::BestRank);
        _ASSERT(seqid.NotEmpty());

        if (seqid->IsGi() && (seqid->GetGi() == m_Gi) &&
            (*itr)->IsSetMemberships()) {
            ITERATE(CBlast_def_line::TMemberships, memb_int, 
                    (*itr)->GetMemberships()) {
                retval += *memb_int;
            }
            break;
        }
    }

    return NStr::IntToString(retval);
}

string CBlastDBExtractor::ExtractAsn1Defline()
{
    x_InitDefline();
    CNcbiOstrstream oss;
    oss << MSerial_AsnText << *m_Defline << endl;
    return CNcbiOstrstreamToString(oss);
}

string CBlastDBExtractor::ExtractAsn1Bioseq()
{
    _ASSERT(m_Bioseq.NotEmpty());
    CNcbiOstrstream oss;
    oss << MSerial_AsnText << *m_Bioseq << endl;
    return CNcbiOstrstreamToString(oss);
}

void CBlastDBExtractor::x_SetGi2AccMap()
{
	if (m_Gi2AccMap.first == m_Oid)
		return;

    map<TGi, string> gi2acc;
    x_InitDefline();
    ITERATE(CBlast_def_line_set::Tdata, bd, m_Defline->Get()) {
        TGi gi;
        ITERATE(CBlast_def_line::TSeqid, id, ((*bd)->GetSeqid())) {
            if ((*id)->IsGi()) {
                gi = (*id)->GetGi();
                break;
            }
        }
        CRef<CSeq_id> theId = FindBestChoice((*bd)->GetSeqid(), CSeq_id::WorstRank);
        string acc;
        theId->GetLabel(&acc, CSeq_id::eContent);
        gi2acc[gi] = acc;
    }
	m_Gi2AccMap.first = m_Oid;
	m_Gi2AccMap.second.swap(gi2acc);
	return;
}

void CBlastDBExtractor::x_SetGi2SeqIdMap()
{
	if (m_Gi2SeqIdMap.first == m_Oid)
		return;

	map<TGi, string> gi2id;
    x_InitDefline();
    ITERATE(CBlast_def_line_set::Tdata, bd, m_Defline->Get()) {
        TGi gi;
        ITERATE(CBlast_def_line::TSeqid, id, ((*bd)->GetSeqid())) {
            if ((*id)->IsGi()) {
                gi = (*id)->GetGi();
                break;
            }
        }
        CRef<CSeq_id> theId = FindBestChoice((*bd)->GetSeqid(), CSeq_id::WorstRank);
        gi2id[gi] = theId->AsFastaString();
    }
	m_Gi2SeqIdMap.first = m_Oid;
	m_Gi2SeqIdMap.second.swap(gi2id);
	return;
}

void CBlastDBExtractor::x_SetGi2TitleMap()
{
	if (m_Gi2TitleMap.first == m_Oid)
		return;

	map<TGi, string> gi2title;
	x_InitDefline();
    ITERATE(CBlast_def_line_set::Tdata, bd, m_Defline->Get()) {
	    TGi gi;
        ITERATE(CBlast_def_line::TSeqid, id, ((*bd)->GetSeqid())) {
            if ((*id)->IsGi()) {
                gi = (*id)->GetGi();
                break;
            }
	    }
	    gi2title[gi] = (*bd)->GetTitle();	
	}
	m_Gi2TitleMap.first = m_Oid;
	m_Gi2TitleMap.second.swap(gi2title);
	return;
}

string CBlastDBExtractor::ExtractAccession() {
    if (m_Gi)
    {
    	x_SetGi2AccMap();
    	return m_Gi2AccMap.second[m_Gi];
    }

    CRef<CSeq_id> theId = FindBestChoice(m_Bioseq->GetId(), CSeq_id::WorstRank);
    if (theId->IsGeneral() && theId->GetGeneral().GetDb() == "BL_ORD_ID") {
        return NOT_AVAILABLE;
    }
    string acc;
    theId->GetLabel(&acc, CSeq_id::eContent);
    return acc;
}

string CBlastDBExtractor::ExtractSeqId() {
    if (m_Gi)
    {
        x_SetGi2SeqIdMap();
        return m_Gi2SeqIdMap.second[m_Gi];
    }

    CRef<CSeq_id> theId = FindBestChoice(m_Bioseq->GetId(), CSeq_id::WorstRank);
    if (theId->IsGeneral() && theId->GetGeneral().GetDb() == "BL_ORD_ID") {
        	return NOT_AVAILABLE;
    }
    string retval = theId->AsFastaString();
    return retval;
}

string CBlastDBExtractor::ExtractTitle() {
    if (m_Gi)
    {
        x_SetGi2TitleMap();
        return m_Gi2TitleMap.second[m_Gi];
    }

    ITERATE(list <CRef <CSeqdesc> >, itr, m_Bioseq->GetDescr().Get()) {
        if ((*itr)->IsTitle()) {
            return (*itr)->GetTitle();
        }
    }
    return NOT_AVAILABLE;
}

string CBlastDBExtractor::ExtractTaxId() {
    return NStr::IntToString(x_ExtractTaxId());
}

string CBlastDBExtractor::ExtractCommonTaxonomicName() {
    const int kTaxID = x_ExtractTaxId();
    SSeqDBTaxInfo tax_info;
    string retval(NOT_AVAILABLE);
    try {
        m_BlastDb.GetTaxInfo(kTaxID, tax_info);
        _ASSERT(kTaxID == tax_info.taxid);
        retval = tax_info.common_name;
    } catch (...) {}
    return retval;
}

string CBlastDBExtractor::ExtractScientificName() {
    const int kTaxID = x_ExtractTaxId();
    SSeqDBTaxInfo tax_info;
    string retval(NOT_AVAILABLE);
    try {
        m_BlastDb.GetTaxInfo(kTaxID, tax_info);
        _ASSERT(kTaxID == tax_info.taxid);
        retval = tax_info.scientific_name;
    } catch (...) {}
    return retval;
}

string CBlastDBExtractor::ExtractBlastName() {
    const int kTaxID = x_ExtractTaxId();
    SSeqDBTaxInfo tax_info;
    string retval(NOT_AVAILABLE);
    try {
        m_BlastDb.GetTaxInfo(kTaxID, tax_info);
        _ASSERT(kTaxID == tax_info.taxid);
        retval = tax_info.blast_name;
    } catch (...) {}
    return retval;
}

string CBlastDBExtractor::ExtractSuperKingdom() {
    const int kTaxID = x_ExtractTaxId();
    SSeqDBTaxInfo tax_info;
    string retval(NOT_AVAILABLE);
    try {
        m_BlastDb.GetTaxInfo(kTaxID, tax_info);
        _ASSERT(kTaxID == tax_info.taxid);
        retval = tax_info.s_kingdom;
    } catch (...) {}
    return retval;
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

    SetSeqId(id, true);

    if (m_UseCtrlA) {
        s_ReplaceCtrlAsInTitle(m_Bioseq);
    }

    CRef<CSeq_id> seqid = FindBestChoice(m_Bioseq->GetId(), CSeq_id::BestRank);

    // Handle the case when a sequence range is provided
    CRef<CSeq_loc> range;
    if (m_SeqRange.NotEmpty() || m_Strand != eNa_strand_other) {
        if (m_SeqRange.NotEmpty()) {
            range.Reset(new CSeq_loc(*seqid, m_SeqRange.GetFrom(),
                                     m_SeqRange.GetTo(), m_Strand));
            fasta.ResetFlag(CFastaOstream::fSuppressRange);
        } else {
            TSeqPos length = m_Bioseq->GetLength();
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

    try { fasta.Write(*m_Bioseq, range); }
    catch (const CObjmgrUtilException& e) {
        if (e.GetErrCode() == CObjmgrUtilException::eBadLocation) {
            NCBI_THROW(CInvalidDataException, eInvalidRange, 
                       "Invalid sequence range");
        }
    }
    return out.str();
}

int CBlastDBExtractor::x_ExtractTaxId() 
{
    x_SetGi();

    if (m_Gi) {
        if (m_Gi2TaxidMap.first != m_Oid)
        {
            m_Gi2TaxidMap.first = m_Oid;
            m_BlastDb.GetTaxIDs(m_Oid, m_Gi2TaxidMap.second);
        }
        return m_Gi2TaxidMap.second[m_Gi];
    }
    // for database without Gi:
    vector <int> taxid;
    m_BlastDb.GetTaxIDs(m_Oid, taxid);
    return taxid.size() ? taxid[0] : 0;
}

void 
CBlastDBExtractor::x_ExtractMaskingData(CSeqDB::TSequenceRanges &ranges, 
                                        int algo_id) 
{
    ranges.clear();
    if (algo_id != -1) {
        m_BlastDb.GetMaskData(m_Oid, algo_id, ranges);
    }
}

void CBlastDBExtractor::SetConfig(TSeqRange range, objects::ENa_strand strand,
							      int filt_algo_id)
{
	m_OrigSeqRange = range;
	m_Strand = strand;
	m_FiltAlgoId = filt_algo_id;
}

END_NCBI_SCOPE

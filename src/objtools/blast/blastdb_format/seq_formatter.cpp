/*  $Id:
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
 * Author: Amelia Fong
 *
 */

/** @file seq_formatter.cpp
 *  Implementation of the CBlastDB_Formatter classes
 */

#include <ncbi_pch.hpp>
#include <objtools/blast/blastdb_format/seq_formatter.hpp>
#include <objtools/blast/blastdb_format/blastdb_dataextract.hpp>
#include <objtools/blast/blastdb_format/invalid_data_exception.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <numeric>      // for std::accumulate
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);



static void s_GetSeqMask(CSeqDB & db, CSeqDB::TOID oid, int algo_id, CSeqDB::TSequenceRanges & masks)
{
#if ((defined(NCBI_COMPILER_WORKSHOP) && (NCBI_COMPILER_VERSION <= 550))  ||  \
     defined(NCBI_COMPILER_MIPSPRO))
    return;
#else
    db.GetMaskData(oid, algo_id, masks);
#endif
}

CBlastDB_SeqFormatter::CBlastDB_SeqFormatter(const string& format_spec, CSeqDB& blastdb, CNcbiOstream& out)
    : m_Out(out), m_FmtSpec(format_spec), m_BlastDb(blastdb), m_GetDefline(false), m_OtherFields(0)
{
	memset(&m_DeflineFields, 0, sizeof(CBlastDeflineUtil::BlastDeflineFields));

    // Record where the offsets where the replacements must occur
	string sp = kEmptyStr;
    for (SIZE_TYPE i = 0; i < m_FmtSpec.size(); i++) {
        if (m_FmtSpec[i] == '%') {
        	if ( m_FmtSpec[i+1] == '%') {
        	    // remove the escape character for '%'
        		i++;
        		sp += m_FmtSpec[i];
        	    continue;
        	}
        	i++;
            m_ReplTypes.push_back(m_FmtSpec[i]);
            m_Seperators.push_back(sp);
            sp = kEmptyStr;
        }
        else {
        	sp += m_FmtSpec[i];
        }
    }
    m_Seperators.push_back(sp);

    if (m_ReplTypes.empty() || (m_ReplTypes.size() + 1 != m_Seperators.size())) {
        NCBI_THROW(CInvalidDataException, eInvalidInput,
                   "Invalid format specification");
    }

  	x_DataRequired();

    CNcbiApplication* app = CNcbiApplication::Instance();
    if (app) {
        const CNcbiRegistry& registry = app->GetConfig();
        m_UseLongSeqIds = (registry.Get("BLAST", "LONG_SEQID") == "1");
    }
}

void CBlastDB_SeqFormatter::x_DataRequired()
{
	ITERATE(vector<char>, fmt, m_ReplTypes) {
	switch (*fmt) {
		case 's':
			m_OtherFields |= (1 << e_seq);
			break;
		case 'h':
			m_OtherFields |= (1 << e_hash);
			break;
		case 'a':
			m_DeflineFields.accession = 1;
			m_GetDefline = true;
			break;
		case 'i':
			m_DeflineFields.seq_id = 1;
			m_GetDefline = true;
			break;
		case 'g':
			m_DeflineFields.gi = 1;
			m_GetDefline = true;
			break;
		case 't':
			m_DeflineFields.title = 1;
			m_GetDefline = true;
			break;
		case 'T':
			m_DeflineFields.tax_id = 1;
			m_GetDefline = true;
			break;
		case 'X':
			m_DeflineFields.leaf_node_tax_ids = 1;
			m_GetDefline = true;
			break;
		case 'P':
			m_DeflineFields.pig = 1;
			m_GetDefline = true;
			break;
		case 'L':
		case 'B':
		case 'K':
		case 'S':
			m_DeflineFields.tax_names = 1;
			m_GetDefline = true;
			break;
		case 'C':
		case 'N':
			m_DeflineFields.leaf_node_tax_names = 1;
			m_GetDefline = true;
			break;
		case 'e':
			m_DeflineFields.membership = 1;
			m_GetDefline = true;
			break;
		case 'm':
			m_OtherFields |= (1 << e_mask);
			break;
		case 'n':
			m_DeflineFields.links = 1;
			m_GetDefline = true;
			break;
		case 'd':
			m_DeflineFields.asn_defline = 1;
			m_GetDefline = true;
			break;
		default:
		 break;
	}
    }
}


void CBlastDB_SeqFormatter::x_Print(CSeqDB::TOID oid, vector<string> & defline_data, vector<string> & other_fields)
{
    for(unsigned int i =0; i < m_ReplTypes.size(); i++) {
    	m_Out << m_Seperators[i];
        switch (m_ReplTypes[i]) {

        case 's':
            m_Out << other_fields[e_seq];
            break;

        case 'a':
            m_Out << defline_data[CBlastDeflineUtil::accession];
            break;

        case 'i':
        	m_Out << defline_data[CBlastDeflineUtil::seq_id];
            break;

        case 'g':
            m_Out << defline_data[CBlastDeflineUtil::gi];
            break;

        case 'o':
            m_Out << NStr::NumericToString(oid);
            break;

        case 't':
            m_Out << defline_data[CBlastDeflineUtil::title];
            break;

        case 'h':
        	m_Out << other_fields[e_hash];
            break;

        case 'l':
            m_Out << NStr::NumericToString(m_BlastDb.GetSeqLength(oid));
            break;

        case 'T':
            m_Out << defline_data[CBlastDeflineUtil::tax_id];
            break;

        case 'X':
            m_Out << defline_data[CBlastDeflineUtil::leaf_node_tax_ids];
            break;

        case 'P':
            m_Out << defline_data[CBlastDeflineUtil::pig];
            break;

        case 'L':
            m_Out << defline_data[CBlastDeflineUtil::common_name];
            break;

        case 'C':
            m_Out << defline_data[CBlastDeflineUtil::leaf_node_common_names];
            break;

        case 'B':
            m_Out << defline_data[CBlastDeflineUtil::blast_name];
            break;

        case 'K':
            m_Out << defline_data[CBlastDeflineUtil::super_kingdom];
            break;

        case 'S':
            m_Out << defline_data[CBlastDeflineUtil::scientific_name];
            break;

        case 'N':
            m_Out << defline_data[CBlastDeflineUtil::leaf_node_scientific_names];
            break;

        case 'm':
        	m_Out << other_fields[e_mask];
            break;

        case 'e':
            m_Out << defline_data[CBlastDeflineUtil::membership];
            break;

        case 'n':
            m_Out << defline_data[CBlastDeflineUtil::links];
            break;

        case 'd':
        	m_Out << defline_data[CBlastDeflineUtil::asn_defline];
        	break;
        default:
            CNcbiOstrstream os;
            os << "Unrecognized format specification: '%" << m_ReplTypes[i] << "'";
            NCBI_THROW(CInvalidDataException, eInvalidInput,
                       CNcbiOstrstreamToString(os));
        }
    }
    m_Out << m_Seperators.back();
    m_Out << endl;
}

string CBlastDB_SeqFormatter::x_GetSeqHash(CSeqDB::TOID oid)
{
	string seq;
	m_BlastDb.GetSequenceAsString(oid, seq);
	CNcbiOstrstream os;
	os << std::showbase << std::hex << std::uppercase << CBlastSeqUtil::GetSeqHash(seq.c_str(), seq.size());
	return CNcbiOstrstreamToString(os);
}

static const string kNoMask = "none";
string CBlastDB_SeqFormatter::x_GetSeqMask(CSeqDB::TOID oid, int algo_id)
{
	CSeqDB::TSequenceRanges masks;
	s_GetSeqMask(m_BlastDb, oid, algo_id, masks);
	return CBlastSeqUtil::GetMasksString(masks);
}

void CBlastDB_SeqFormatter::x_GetSeq(CSeqDB::TOID oid, const CBlastDB_FormatterConfig & config, string & seq)
{
	TSeqRange r;
	if(config.m_SeqRange.NotEmpty()) {
		TSeqPos length = m_BlastDb.GetSeqLength(oid);
	    r = config.m_SeqRange;
	    if((TSeqPos)length <= config.m_SeqRange.GetTo()) {
	    	r.SetTo(length-1);
	    }
	}
	if(r.Empty()) {
	   	m_BlastDb.GetSequenceAsString(oid, seq);
	}
	else {
	   	m_BlastDb.GetSequenceAsString(oid, seq, r);
	}
	if(config.m_FiltAlgoId >= 0) {
	    CSeqDB::TSequenceRanges masks;
	    s_GetSeqMask(m_BlastDb, oid, config.m_FiltAlgoId, masks);
	    if(!masks.empty()) {
	    	if(r.Empty()) {
	    		CBlastSeqUtil::ApplySeqMask(seq, masks);
	    	}
	    	else {
	    		CBlastSeqUtil::ApplySeqMask(seq, masks, r);
	    	}
	    }
	}
	if(config.m_Strand == eNa_strand_minus) {
		CBlastSeqUtil::GetReverseStrandSeq(seq);
	}
}

int CBlastDB_SeqFormatter::Write(CSeqDB::TOID oid, const CBlastDB_FormatterConfig & config, string target_id)
{
	int status = -1;
    vector<string> other_fields(e_max_other_fields, kEmptyStr);
    string mask = kEmptyStr;
    if (m_OtherFields) {
    	if (m_OtherFields & (1 << e_seq)) {
    		x_GetSeq(oid, config, other_fields[e_seq]);
    	}
    	if (m_OtherFields & (1 << e_hash)) {
    		other_fields[e_hash] = x_GetSeqHash(oid);
    	}
    	if (m_OtherFields & (1 << e_mask)) {
    		other_fields[e_mask] = x_GetSeqMask(oid, config.m_FmtAlgoId);
    	}
    }
    if (m_GetDefline) {
    	CRef<CBlast_def_line_set> df_set = m_BlastDb.GetHdr(oid);
    	if(df_set.Empty()) {
    		return status;
    	}
    	if (target_id == kEmptyStr) {
    		if(m_DeflineFields.asn_defline == 1) {
    			m_Out << MSerial_AsnText << *df_set;
    			return 0;
    		}
    		ITERATE(CBlast_def_line_set::Tdata, itr, df_set->Get()) {
    			vector<string> defline_data(CBlastDeflineUtil::max_index, kEmptyStr);
    			CBlastDeflineUtil::ExtractDataFromBlastDefline(**itr, defline_data, m_DeflineFields, m_UseLongSeqIds);
    			x_Print(oid, defline_data, other_fields);
    		}
    	}
    	else {
   			vector<string> defline_data(CBlastDeflineUtil::max_index, kEmptyStr);
    		CBlastDeflineUtil::ExtractDataFromBlastDeflineSet(*df_set, defline_data, m_DeflineFields, target_id, m_UseLongSeqIds);
   			x_Print(oid, defline_data, other_fields);
    	}
    }
    else {
		vector<string> defline_data(CBlastDeflineUtil::max_index, kEmptyStr);
		x_Print(oid, defline_data, other_fields);
    }
    return 0;
}

static void s_ReplaceCtrlAsInTitle(CBioseq & bioseq)
{
    static const string kTarget(" >gi|");
    static const string kCtrlA = string(1, '\001') + string("gi|");
    NON_CONST_ITERATE(CSeq_descr::Tdata, desc, bioseq.SetDescr().Set()) {
        if ((*desc)->Which() == CSeqdesc::e_Title) {
            NStr::ReplaceInPlace((*desc)->SetTitle(), kTarget, kCtrlA);
            break;
        }
    }
}

void CBlastDB_SeqFormatter::DumpAll(const CBlastDB_FormatterConfig & config)
{
	for (int i = 0; m_BlastDb.CheckOrFindOID(i); i++) {
		Write(i, config);
	}
}

CBlastDB_FastaFormatter::CBlastDB_FastaFormatter(CSeqDB& blastdb, CNcbiOstream& out, TSeqPos width)
    :  m_BlastDb(blastdb), m_Out(out), m_fasta(out)
{
	m_fasta.SetAllFlags(CFastaOstream::fKeepGTSigns|CFastaOstream::fNoExpensiveOps);
	m_fasta.SetWidth(width);

    CNcbiApplication* app = CNcbiApplication::Instance();
    if (app) {
        const CNcbiRegistry& registry = app->GetConfig();
        m_UseLongSeqIds = (registry.Get("BLAST", "LONG_SEQID") == "1");
    }
}

static void
s_GetMaskSeqLoc(const CSeqDB::TSequenceRanges & mask_ranges, CSeq_loc & seq_loc)
{
	 ITERATE(CSeqDB::TSequenceRanges, itr, mask_ranges) {
		 	 CRef<CSeq_loc> mask(new CSeq_loc());
		 	 mask->SetInt().SetFrom(itr->first);
		 	 mask->SetInt().SetTo(itr->second-1);
		 	 seq_loc.SetMix().Set().push_back(mask);
	 }
}

int CBlastDB_FastaFormatter::Write(CSeqDB::TOID oid, const CBlastDB_FormatterConfig & config, string target_id)
{
	int status = -1;
	CRef<CBioseq> bioseq = m_BlastDb.GetBioseq(oid);
	if(target_id != kEmptyStr) {
		CSeq_id seq_id(target_id, CSeq_id::fParse_PartialOK | CSeq_id::fParse_Default);
		Int8 num_id = NStr::StringToNumeric<Int8>(target_id, NStr::fConvErr_NoThrow);
		if(errno) {
			CSeq_id seq_id(target_id, CSeq_id::fParse_PartialOK | CSeq_id::fParse_Default);
			bioseq = m_BlastDb.GetBioseq(oid, ZERO_GI, &seq_id);
		}
		else {
			bioseq = m_BlastDb.GetBioseq(oid, num_id);
		}
	}
	else {
		bioseq = m_BlastDb.GetBioseq(oid);
	}
	if (bioseq.Empty()) {
		return status;
	}

	if(config.m_Strand == eNa_strand_minus) {
		m_fasta.SetFlag(CFastaOstream::fReverseStrand);
	}
	else {
		m_fasta.ResetFlag(CFastaOstream::fReverseStrand);
	}
    if(config.m_FiltAlgoId != -1) {
    	CSeqDB::TSequenceRanges masks;
    	s_GetSeqMask(m_BlastDb, oid, config.m_FiltAlgoId, masks);
    	if(!masks.empty()) {
    		CRef<CSeq_loc> mask_loc(new CSeq_loc);
    		s_GetMaskSeqLoc(masks, *mask_loc);
    		mask_loc->SetId( *(FindBestChoice(bioseq->GetId(), CSeq_id::BestRank)));
    		m_BlastDb.GetMaskData(oid, config.m_FiltAlgoId, masks);
    		m_fasta.SetMask(CFastaOstream::eSoftMask, mask_loc);
    	}
    }
    else {
    	CRef<CSeq_loc> empty;
    	m_fasta.SetMask(CFastaOstream::eSoftMask, empty);
    }

    CRef<CSeq_loc> range;
    if(config.m_SeqRange.NotEmpty()) {
    	TSeqPos length = m_BlastDb.GetSeqLength(oid);
    	TSeqRange r = config.m_SeqRange;
        if((TSeqPos)length <= config.m_SeqRange.GetTo()) {
        	r.SetTo(length-1);
    	}
        if(!r.Empty()) {
        	range.Reset(new CSeq_loc(*(FindBestChoice(bioseq->GetId(), CSeq_id::BestRank)), r.GetFrom(), r.GetTo()));
        }
    }
    if(m_UseLongSeqIds) {
    	if (config.m_UseCtrlA) {
    		s_ReplaceCtrlAsInTitle(*bioseq);
    	}
    	CScope scope(*CObjectManager::GetInstance());
    	if(range.Empty()) {
    		m_fasta.Write(scope.AddBioseq(*bioseq));
    	}
    	else {
    		m_fasta.Write(scope.AddBioseq(*bioseq), range.GetPointer());
    	}
    }
    else {
    	string fasta_deflines = kEmptyStr;
    	CBlastDeflineUtil::ProcessFastaDeflines(*bioseq, fasta_deflines, config.m_UseCtrlA);
    	m_Out << fasta_deflines;
    	CScope scope(*CObjectManager::GetInstance());
    	if(range.Empty()) {
    		m_fasta.WriteSequence(scope.AddBioseq(*bioseq));
    	}
    	else {
    		m_fasta.WriteSequence(scope.AddBioseq(*bioseq), range.GetPointer());
    	}
   }
   return 0;
}

void CBlastDB_FastaFormatter::DumpAll(const CBlastDB_FormatterConfig & config)
{
    if(config.m_Strand == eNa_strand_minus) {
    	m_fasta.SetAllFlags(CFastaOstream::fKeepGTSigns|CFastaOstream::fNoExpensiveOps|CFastaOstream::fReverseStrand);
    }
    else {
    	m_fasta.SetAllFlags(CFastaOstream::fKeepGTSigns|CFastaOstream::fNoExpensiveOps);
    }

    for (int i=0; m_BlastDb.CheckOrFindOID(i); i++) {
    	Write( i, config);
    }
}

CBlastDB_BioseqFormatter::CBlastDB_BioseqFormatter(CSeqDB& blastdb, CNcbiOstream& out)
    :  m_BlastDb(blastdb), m_Out(out)
{
}

int CBlastDB_BioseqFormatter::Write(CSeqDB::TOID oid, const CBlastDB_FormatterConfig & config, string target_id)
{
	int status = -1;
	CRef<CBioseq> bioseq = m_BlastDb.GetBioseq(oid);
	if(target_id != kEmptyStr) {
		CSeq_id seq_id(target_id);
		Int8 num_id;
		string string_id;
		bool simpler = false;
		ESeqDBIdType  id_type = SeqDB_SimplifySeqid(seq_id, &target_id, num_id, string_id, simpler);
		if (id_type == eGiId) {
			bioseq = m_BlastDb.GetBioseq(oid, num_id);
		}
		else {
			bioseq = m_BlastDb.GetBioseq(oid, ZERO_GI, &seq_id);
		}
	}
	else {
		bioseq = m_BlastDb.GetBioseq(oid);
	}
	if (bioseq.Empty()) {
		return status;
	}

	m_Out << MSerial_AsnText << *bioseq;

   return 0;
}

void CBlastDB_BioseqFormatter::DumpAll(const CBlastDB_FormatterConfig & config)
{
    for (int i=0; m_BlastDb.CheckOrFindOID(i); i++) {
    	Write( i, config);
    }
}


/// Auxiliary functor to compute the length of a string
struct StrLenAdd : public binary_function<SIZE_TYPE, const string&, SIZE_TYPE>
{
    SIZE_TYPE operator() (SIZE_TYPE a, const string& b) const {
        return a + b.size();
    }
};

END_NCBI_SCOPE

/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* Author:  Ilya Dondoshansky
*
* ===========================================================================
*/

/// @file: tabular.cpp
/// Formatting of pairwise sequence alignments in tabular form.
/// One line is printed for each alignment with tab-delimited fielalnVec. 

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>

#include <objects/seqloc/Seq_id.hpp>

#include <objtools/align_format/tabular.hpp>
#include <objtools/align_format/showdefline.hpp>
#include <objtools/align_format/align_format_util.hpp>

#include <serial/iterator.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/blastdb/Blast_def_line.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>

#include <map>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(align_format)

void 
CBlastTabularInfo::x_AddDefaultFieldsToShow()
{
    vector<string> format_tokens;
    NStr::Tokenize(kDfltArgTabularOutputFmt, " ", format_tokens);
    ITERATE (vector<string>, iter, format_tokens) {
        _ASSERT(m_FieldMap.count(*iter) > 0);
        x_AddFieldToShow(m_FieldMap[*iter]);
    }
}

void CBlastTabularInfo::x_SetFieldsToShow(const string& format)
{
    for (size_t i = 0; i < kNumTabularOutputFormatSpecifiers; i++) {
        m_FieldMap.insert(make_pair(sc_FormatSpecifiers[i].name,
                                    sc_FormatSpecifiers[i].field));
    }
    
    vector<string> format_tokens;
    NStr::Tokenize(format, " ", format_tokens);

    if (format_tokens.empty())
        x_AddDefaultFieldsToShow();

    ITERATE (vector<string>, iter, format_tokens) {
        if (*iter == kDfltArgTabularOutputFmtTag)
            x_AddDefaultFieldsToShow();
        else if ((*iter)[0] == '-') {
            string field = (*iter).substr(1);
            if (m_FieldMap.count(field) > 0) 
                x_DeleteFieldToShow(m_FieldMap[field]);
        } else {
            if (m_FieldMap.count(*iter) > 0)
                x_AddFieldToShow(m_FieldMap[*iter]);
        }
    }

    if (m_FieldsToShow.empty()) {
        x_AddDefaultFieldsToShow();
    }
}

void CBlastTabularInfo::x_ResetFields()
{
    m_QueryLength = m_SubjectLength = 0U;
    m_Score = m_AlignLength = m_NumGaps = m_NumGapOpens = m_NumIdent =
        m_NumPositives = m_QueryStart = m_QueryEnd = m_SubjectStart = 
        m_SubjectEnd = 0; 
    m_BitScore = NcbiEmptyString;
    m_Evalue = NcbiEmptyString;
    m_QuerySeq = NcbiEmptyString;
    m_SubjectSeq = NcbiEmptyString;
    m_BTOP = NcbiEmptyString;
}

void CBlastTabularInfo::x_SetFieldDelimiter(EFieldDelimiter delim)
{
    switch (delim) {
    case eSpace: m_FieldDelimiter = ' '; break;
    case eComma: m_FieldDelimiter = ','; break;
    default: m_FieldDelimiter = '\t'; break; // eTab or unsupported value
    }
}

CBlastTabularInfo::CBlastTabularInfo(CNcbiOstream& ostr, const string& format,
                                     EFieldDelimiter delim, 
                                     bool parse_local_ids)
    : m_Ostream(ostr) 
{
    x_SetFieldsToShow(format);
    x_ResetFields();
    x_SetFieldDelimiter(delim);
    SetParseLocalIds(parse_local_ids);
}

CBlastTabularInfo::~CBlastTabularInfo()
{
    m_Ostream.flush();
}

static string 
s_GetSeqIdListString(const list<CRef<CSeq_id> >& id, 
                     CBlastTabularInfo::ESeqIdType id_type)
{
    string id_str = NcbiEmptyString;

    switch (id_type) {
    case CBlastTabularInfo::eFullId:
        id_str = CShowBlastDefline::GetSeqIdListString(id, true);
        break;
    case CBlastTabularInfo::eAccession:
    {
        CConstRef<CSeq_id> accid = FindBestChoice(id, CSeq_id::Score);
        accid->GetLabel(&id_str, CSeq_id::eContent, 0);
        break;
    }
    case CBlastTabularInfo::eAccVersion:
    {
        CConstRef<CSeq_id> accid = FindBestChoice(id, CSeq_id::Score);
        accid->GetLabel(&id_str, CSeq_id::eContent, CSeq_id::fLabel_Version);
        break;
    }
    case CBlastTabularInfo::eGi:
        id_str = NStr::IntToString(FindGi(id));
        break;
    default: break;
    }

    if (id_str == NcbiEmptyString)
        id_str = "Unknown";

    return id_str;
}

void CBlastTabularInfo::x_PrintQuerySeqId() const
{
    m_Ostream << s_GetSeqIdListString(m_QueryId, eFullId);
}

void CBlastTabularInfo::x_PrintQueryGi()
{
    m_Ostream << s_GetSeqIdListString(m_QueryId, eGi);
}

void CBlastTabularInfo::x_PrintQueryAccession()
{
    m_Ostream << s_GetSeqIdListString(m_QueryId, eAccession);
}

void CBlastTabularInfo::x_PrintQueryAccessionVersion()
{
    m_Ostream << s_GetSeqIdListString(m_QueryId, eAccVersion);
}

void CBlastTabularInfo::x_PrintSubjectSeqId()
{
    m_Ostream << s_GetSeqIdListString(m_SubjectIds[0], eFullId);
}

void CBlastTabularInfo::x_PrintSubjectAllSeqIds(void)
{
    ITERATE(vector<list<CRef<CSeq_id> > >, iter, m_SubjectIds) {
        if (iter != m_SubjectIds.begin())
            m_Ostream << ";";
        m_Ostream << s_GetSeqIdListString(*iter, eFullId); 
    }
}

void CBlastTabularInfo::x_PrintSubjectGi(void)
{
    m_Ostream << s_GetSeqIdListString(m_SubjectIds[0], eGi);
}

void CBlastTabularInfo::x_PrintSubjectAllGis(void)
{
    ITERATE(vector<list<CRef<CSeq_id> > >, iter, m_SubjectIds) {
        if (iter != m_SubjectIds.begin())
            m_Ostream << ";";
        m_Ostream << s_GetSeqIdListString(*iter, eGi); 
    }
}

void CBlastTabularInfo::x_PrintSubjectAccession(void)
{
    m_Ostream << s_GetSeqIdListString(m_SubjectIds[0], eAccession);
}

void CBlastTabularInfo::x_PrintSubjectAccessionVersion(void)
{
    m_Ostream << s_GetSeqIdListString(m_SubjectIds[0], eAccVersion);
}

void CBlastTabularInfo::x_PrintSubjectAllAccessions(void)
{
    ITERATE(vector<list<CRef<CSeq_id> > >, iter, m_SubjectIds) {
        if (iter != m_SubjectIds.begin())
            m_Ostream << ";";
        m_Ostream << s_GetSeqIdListString(*iter, eAccession); 
    }
}

CRef<CSeq_id> s_ReplaceLocalId(const CBioseq_Handle& bh, CConstRef<CSeq_id> sid_in, bool parse_local)
{

        CRef<CSeq_id> retval(new CSeq_id());

        // Local ids are usually fake. If a title exists, use the first token
        // of the title instead of the local id. If no title or if the local id
        // should be parsed, use the local id, but without the "lcl|" prefix.
        if (sid_in->IsLocal()) {
            string id_token;
            vector<string> title_tokens;
            title_tokens = 
                NStr::Tokenize(sequence::GetTitle(bh), " ", title_tokens);
            if(title_tokens.empty()){
                id_token = NcbiEmptyString;
            } else {
                id_token = title_tokens[0];
            }
            
            if (id_token == NcbiEmptyString || parse_local) {
                const CObject_id& obj_id = sid_in->GetLocal();
                if (obj_id.IsStr())
                    id_token = obj_id.GetStr();
                else 
                    id_token = NStr::IntToString(obj_id.GetId());
            }
            CObject_id* obj_id = new CObject_id();
            obj_id->SetStr(id_token);
            retval->SetLocal(*obj_id);
        } else {
            retval->Assign(*sid_in);
        }

        return retval;
}

void CBlastTabularInfo::SetQueryId(const CBioseq_Handle& bh)
{
    m_QueryId.clear();

    // Create a new list of Seq-ids, substitute any local ids by new fake local 
    // ids, with label set to the first token of this Bioseq's title.
    ITERATE(CBioseq_Handle::TId, itr, bh.GetId()) {
        CRef<CSeq_id> next_id = s_ReplaceLocalId(bh, itr->GetSeqId(), m_ParseLocalIds);
        m_QueryId.push_back(next_id);
    }
}
        
void CBlastTabularInfo::SetSubjectId(const CBioseq_Handle& bh)
{
    m_SubjectIds.clear();

    // Check if this Bioseq handle contains a Blast-def-line-set object.
    // If it does, retrieve Seq-ids from all redundant sequences, and 
    // print them separated by commas.
    // Retrieve the CBlast_def_line_set object and save in a CRef, preventing
    // its destruction; then extract the list of CBlast_def_line objects.
    const CRef<CBlast_def_line_set> bdlRef = 
        CSeqDB::ExtractBlastDefline(bh);
    const list< CRef< CBlast_def_line > >& bdl = bdlRef->Get();
    
    vector< CConstRef<CSeq_id> > original_seqids;
    if (!bdl.empty()) {        
        ITERATE(CBlast_def_line_set::Tdata, itr, bdl) {
            original_seqids.clear();
            ITERATE(CBlast_def_line::TSeqid, id, (*itr)->GetSeqid()) {
                original_seqids.push_back(*id);
            }
            list<CRef<objects::CSeq_id> > next_seqid_list;
            // Next call replaces BL_ORD_ID if found.
            CShowBlastDefline::GetSeqIdList(bh,original_seqids,next_seqid_list);
            m_SubjectIds.push_back(next_seqid_list);
        }
    } 
    else {
        // Blast-def-line is not filled, hence retrieve all Seq-ids directly 
        // from the Bioseq handle's Seq-id.
        list<CRef<objects::CSeq_id> > subject_id_list;
        ITERATE(CBioseq_Handle::TId, itr, bh.GetId()) {
            CRef<CSeq_id> next_id = s_ReplaceLocalId(bh, itr->GetSeqId(), m_ParseLocalIds);
            subject_id_list.push_back(next_id);
        }
        m_SubjectIds.push_back(subject_id_list);
    }
}


int CBlastTabularInfo::SetFields(const CSeq_align& align, 
                                 CScope& scope, 
                                 CNcbiMatrix<int>* matrix)
{
    const int kQueryRow = 0;
    const int kSubjectRow = 1;

    int num_ident = -1;
    
    // First reset all fields.
    x_ResetFields();

    if (x_IsFieldRequested(eEvalue) || 
        x_IsFieldRequested(eBitScore) ||
        x_IsFieldRequested(eScore) ||
        x_IsFieldRequested(eNumIdentical) ||
        x_IsFieldRequested(eMismatches) ||
        x_IsFieldRequested(ePercentIdentical)) {
        int score = 0, sum_n = 0;
        double bit_score = .0, evalue = .0;
        list<int> use_this_gi;
        CAlignFormatUtil::GetAlnScores(align, score, bit_score, evalue, sum_n, 
                                       num_ident, use_this_gi);
        SetScores(score, bit_score, evalue);
    }

    bool query_is_na = false, subject_is_na = false;
    bool bioseqs_found = true;
    // Extract the full query id from the correspondintg Bioseq handle.
    if (x_IsFieldRequested(eQuerySeqId) || x_IsFieldRequested(eQueryGi) ||
        x_IsFieldRequested(eQueryAccession) ||
        x_IsFieldRequested(eQueryAccessionVersion)) {
        try {
            // FIXME: do this only if the query has changed
            const CBioseq_Handle& query_bh = 
                scope.GetBioseqHandle(align.GetSeq_id(0));
            SetQueryId(query_bh);
            query_is_na = query_bh.IsNa();
            m_QueryLength = query_bh.GetBioseqLength();
        } catch (const CException&) {
            list<CRef<CSeq_id> > query_ids;
            CRef<CSeq_id> id(new CSeq_id());
            id->Assign(align.GetSeq_id(0));
            query_ids.push_back(id);
            SetQueryId(query_ids);
            bioseqs_found = false;
        }
    }

    // Extract the full list of subject ids
    if (x_IsFieldRequested(eSubjectSeqId) || 
        x_IsFieldRequested(eSubjectAllSeqIds) ||
        x_IsFieldRequested(eSubjectGi) ||
        x_IsFieldRequested(eSubjectAllGis) ||
        x_IsFieldRequested(eSubjectAccession) ||
        x_IsFieldRequested(eSubjectAllAccessions) ||
        x_IsFieldRequested(eSubjAccessionVersion)) {
        try {
            const CBioseq_Handle& subject_bh = 
                scope.GetBioseqHandle(align.GetSeq_id(1));
            SetSubjectId(subject_bh);
            subject_is_na = subject_bh.IsNa();
            m_SubjectLength = subject_bh.GetBioseqLength();
        } catch (const CException&) {
            list<CRef<CSeq_id> > subject_ids;
            CRef<CSeq_id> id(new CSeq_id());
            id->Assign(align.GetSeq_id(1));
            subject_ids.push_back(id);
            SetSubjectId(subject_ids);
            bioseqs_found = false;
        }
    }

    // If Bioseq has not been found for one or both of the sequences, all
    // subsequent computations cannot proceed. Hence don't set any of the other 
    // fields.
    if (!bioseqs_found)
        return -1;

    if (x_IsFieldRequested(eQueryLength) && m_QueryLength == 0) {
        //_ASSERT(!m_QueryId.empty());
        //_ASSERT(m_QueryId.front().NotEmpty());
        //m_QueryLength = sequence::GetLength(*m_QueryId.front(), &scope);
        m_QueryLength = sequence::GetLength(align.GetSeq_id(0), &scope);
    }
    if (x_IsFieldRequested(eSubjectLength) && m_SubjectLength == 0) {
        //_ASSERT(!m_SubjectIds.empty());
        //_ASSERT(!m_SubjectIds.front().empty());
        //_ASSERT(!m_SubjectIds.front().front().NotEmpty());
        //m_SubjectLength = sequence::GetLength(*m_SubjectIds.front().front(),
        //                                      &scope);
        m_SubjectLength = sequence::GetLength(align.GetSeq_id(1), &scope);
    }

    if (x_IsFieldRequested(eQueryStart) || x_IsFieldRequested(eQueryEnd) ||
        x_IsFieldRequested(eSubjectStart) || x_IsFieldRequested(eSubjectEnd) ||
        x_IsFieldRequested(eAlignmentLength) || x_IsFieldRequested(eGaps) ||
        x_IsFieldRequested(eGapOpenings) || x_IsFieldRequested(eQuerySeq) ||
        x_IsFieldRequested(eSubjectSeq) || (x_IsFieldRequested(eNumIdentical) && num_ident > 0 ) ||
        x_IsFieldRequested(ePositives) || x_IsFieldRequested(eMismatches) || 
        x_IsFieldRequested(ePercentPositives) || x_IsFieldRequested(ePercentIdentical) ||
        x_IsFieldRequested(eQueryFrame) || x_IsFieldRequested(eBTOP)) {

    CRef<CSeq_align> finalAln(0);
   
    // Convert Std-seg and Dense-diag alignments to Dense-seg.
    // Std-segs are produced only for translated searches; Dense-diags only for 
    // ungapped, not translated searches.
    const bool kTranslated = align.GetSegs().IsStd();

    if (kTranslated) {
        CRef<CSeq_align> densegAln = align.CreateDensegFromStdseg();
        // When both query and subject are translated, i.e. tblastx, convert
        // to a special type of Dense-seg.
        if (query_is_na && subject_is_na)
            finalAln = densegAln->CreateTranslatedDensegFromNADenseg();
        else
            finalAln = densegAln;
    } else if (align.GetSegs().IsDendiag()) {
        finalAln = CAlignFormatUtil::CreateDensegFromDendiag(align);
    }

    const CDense_seg& ds = (finalAln ? finalAln->GetSegs().GetDenseg() :
                            align.GetSegs().GetDenseg());

    /// @todo code to create CAlnVec is the same as the one used in
    /// blastxml_format.cpp (s_SeqAlignSetToXMLHsps) and also
    /// CDisplaySeqalign::x_GetAlnVecForSeqalign(), these should be refactored
    /// into a single function, possibly in CAlignFormatUtil. Note that
    /// CAlignFormatUtil::GetPercentIdentity() and
    /// CAlignFormatUtil::GetAlignmentLength() also use a similar logic...
    /// @sa s_SeqAlignSetToXMLHsps
    /// @sa CDisplaySeqalign::x_GetAlnVecForSeqalign
    CRef<CAlnVec> alnVec;

    // For non-translated reverse strand alignments, show plus strand on 
    // query and minus strand on subject. To accomplish this, Dense-seg must
    // be reversed.
    if (!kTranslated && ds.IsSetStrands() && 
        ds.GetStrands().front() == eNa_strand_minus) {
        CRef<CDense_seg> reversed_ds(new CDense_seg);
        reversed_ds->Assign(ds);
        reversed_ds->Reverse();
        alnVec.Reset(new CAlnVec(*reversed_ds, scope));   
    } else {
        alnVec.Reset(new CAlnVec(ds, scope));
    }    


    int align_length = 0, num_gaps = 0, num_gap_opens = 0;
    if (x_IsFieldRequested(eAlignmentLength) ||
        x_IsFieldRequested(eGaps) ||
        x_IsFieldRequested(eMismatches) ||
        x_IsFieldRequested(ePercentPositives) ||
        x_IsFieldRequested(ePercentIdentical) ||
        x_IsFieldRequested(eGapOpenings)) {
        CAlignFormatUtil::GetAlignLengths(*alnVec, align_length, num_gaps, 
                                          num_gap_opens);
    }

    int num_positives = 0;
    
    if (x_IsFieldRequested(eQuerySeq) || 
        x_IsFieldRequested(eSubjectSeq) ||
        x_IsFieldRequested(ePositives) ||
        x_IsFieldRequested(ePercentPositives) ||
        x_IsFieldRequested(eBTOP) ||
        // num_ident may not be available in un-gapped protein alignment
        (num_ident < 0 && (x_IsFieldRequested(eNumIdentical) || 
          x_IsFieldRequested(eMismatches) ||
          x_IsFieldRequested(ePercentIdentical)))) {

        alnVec->SetGapChar('-');
        alnVec->GetWholeAlnSeqString(0, m_QuerySeq);
        alnVec->GetWholeAlnSeqString(1, m_SubjectSeq);
        
        if (x_IsFieldRequested(ePositives) ||
            x_IsFieldRequested(ePercentPositives) ||
            x_IsFieldRequested(eBTOP) ||
            // num_ident may not be available in un-gapped protein alignment
            (num_ident < 0 && (x_IsFieldRequested(eNumIdentical) || 
              x_IsFieldRequested(eMismatches) ||
              x_IsFieldRequested(ePercentIdentical)))) {

            string btop_string = "";
            int num_matches = 0;
            num_ident = 0;
            // The query and subject sequence strings must be the same size in a correct
            // alignment, but if alignment extends beyond the end of sequence because of
            // a bug, one of the sequence strings may be truncated, hence it is 
            // necessary to take a minimum here.
            /// @todo FIXME: Should an exception be thrown instead? 
            for (unsigned int i = 0; 
                 i < min(m_QuerySeq.size(), m_SubjectSeq.size());
                 ++i) {
                if (m_QuerySeq[i] == m_SubjectSeq[i]) {
                    ++num_ident;
                    ++num_positives;
                    ++num_matches;
                } else {
                    if(num_matches > 0) {
                        btop_string +=  NStr::Int8ToString(num_matches);
                        num_matches=0; 
                    }
                    btop_string += m_QuerySeq[i];
                    btop_string += m_SubjectSeq[i];
                    if (matrix && !matrix->GetData().empty() &&
                           (*matrix)(m_QuerySeq[i], m_SubjectSeq[i]) > 0) {
                        ++num_positives;
                    }
                }
            }

            if (num_matches > 0) {
                btop_string +=  NStr::Int8ToString(num_matches);
            }
            SetBTOP(btop_string);
        }
    } 

    int q_start = 0, q_end = 0, s_start = 0, s_end = 0;
    if (x_IsFieldRequested(eQueryStart) || x_IsFieldRequested(eQueryEnd) ||
        x_IsFieldRequested(eQueryFrame) || x_IsFieldRequested(eFrames)) {
        // For translated search, for a negative query frame, reverse its start
        // and end offsets.
        if (kTranslated && ds.GetSeqStrand(kQueryRow) == eNa_strand_minus) {
            q_start = alnVec->GetSeqStop(kQueryRow) + 1;
            q_end = alnVec->GetSeqStart(kQueryRow) + 1;
        } else {
            q_start = alnVec->GetSeqStart(kQueryRow) + 1;
            q_end = alnVec->GetSeqStop(kQueryRow) + 1;
        }
    }

    if (x_IsFieldRequested(eSubjectStart) || x_IsFieldRequested(eSubjectEnd) ||
        x_IsFieldRequested(eSubjFrame) || x_IsFieldRequested(eFrames)) {
        // If subject is on a reverse strand, reverse its start and end
        // offsets. Also do that for a nucleotide-nucleotide search, if query
        // is on the reverse strand, because BLAST output always reverses
        // subject, not query.
        if (ds.GetSeqStrand(kSubjectRow) == eNa_strand_minus ||
            (!kTranslated && ds.GetSeqStrand(kQueryRow) == eNa_strand_minus)) {
            s_end = alnVec->GetSeqStart(kSubjectRow) + 1;
            s_start = alnVec->GetSeqStop(kSubjectRow) + 1;
        } else {
            s_start = alnVec->GetSeqStart(kSubjectRow) + 1;
            s_end = alnVec->GetSeqStop(kSubjectRow) + 1;
        }
    }
    SetEndpoints(q_start, q_end, s_start, s_end);
    
    int query_frame = 1, subject_frame = 1;
    if (kTranslated) {
        if (x_IsFieldRequested(eQueryFrame) || x_IsFieldRequested(eFrames)) {
            query_frame = CAlignFormatUtil::
                GetFrame (q_start - 1, ds.GetSeqStrand(kQueryRow), 
                          scope.GetBioseqHandle(align.GetSeq_id(0)));
        }
    
        if (x_IsFieldRequested(eSubjFrame) || x_IsFieldRequested(eFrames)) {
            subject_frame = CAlignFormatUtil::
                GetFrame (s_start - 1, ds.GetSeqStrand(kSubjectRow), 
                          scope.GetBioseqHandle(align.GetSeq_id(1)));
        }

    }
    SetCounts(num_ident, align_length, num_gaps, num_gap_opens, num_positives,
              query_frame, subject_frame);
    }

    return 0;
}

void CBlastTabularInfo::Print() 
{
    ITERATE(list<ETabularField>, iter, m_FieldsToShow) {
        // Add tab in front of field, except for the first field.
        if (iter != m_FieldsToShow.begin())
            m_Ostream << m_FieldDelimiter;
        x_PrintField(*iter);
    }
    m_Ostream << "\n";
}

void CBlastTabularInfo::x_PrintFieldNames()
{
    m_Ostream << "# Fields: ";

    ITERATE(list<ETabularField>, iter, m_FieldsToShow) {
        if (iter != m_FieldsToShow.begin())
            m_Ostream << ", ";

        switch (*iter) {
        case eQuerySeqId:
            m_Ostream << "query id"; break;
        case eQueryGi:
            m_Ostream << "query gi"; break;
        case eQueryAccession:
            m_Ostream << "query acc."; break;
        case eQueryAccessionVersion:
            m_Ostream << "query acc.ver"; break;
        case eQueryLength:
            m_Ostream << "query length"; break;
        case eSubjectSeqId:
            m_Ostream << "subject id"; break;
        case eSubjectAllSeqIds:
            m_Ostream << "subject ids"; break;
        case eSubjectGi:
            m_Ostream << "subject gi"; break;
        case eSubjectAllGis:
            m_Ostream << "subject gis"; break;
        case eSubjectAccession:
            m_Ostream << "subject acc."; break;
        case eSubjAccessionVersion:
            m_Ostream << "subject acc.ver"; break;
        case eSubjectAllAccessions:
            m_Ostream << "subject accs."; break;
        case eSubjectLength:
            m_Ostream << "subject length"; break;
        case eQueryStart:
            m_Ostream << "q. start"; break;
        case eQueryEnd:
            m_Ostream << "q. end"; break;
        case eSubjectStart:
            m_Ostream << "s. start"; break;
        case eSubjectEnd:
            m_Ostream << "s. end"; break;
        case eQuerySeq:
            m_Ostream << "query seq"; break;
        case eSubjectSeq:
            m_Ostream << "subject seq"; break;
        case eEvalue:
            m_Ostream << "evalue"; break;
        case eBitScore:
            m_Ostream << "bit score"; break;
        case eScore:
            m_Ostream << "score"; break;
        case eAlignmentLength:
            m_Ostream << "alignment length"; break;
        case ePercentIdentical:
            m_Ostream << "% identity"; break;
        case eNumIdentical:
            m_Ostream << "identical"; break;
        case eMismatches:
            m_Ostream << "mismatches"; break;
        case ePositives:
            m_Ostream << "positives"; break;
        case eGapOpenings:
            m_Ostream << "gap opens"; break;
        case eGaps:
            m_Ostream << "gaps"; break;
        case ePercentPositives:
            m_Ostream << "% positives"; break;
        case eFrames:
            m_Ostream << "query/sbjct frames"; break; 
        case eQueryFrame:
            m_Ostream << "query frame"; break; 
        case eSubjFrame:
            m_Ostream << "sbjct frame"; break; 
        case eBTOP:
            m_Ostream << "BTOP"; break;        
        default:
            _ASSERT(false);
            break;
        }
    }

    m_Ostream << "\n";
}

/// @todo FIXME add means to specify masked database (SB-343)
void 
CBlastTabularInfo::PrintHeader(const string& program_version, 
       const CBioseq& bioseq, 
       const string& dbname, 
       const string& rid /* = kEmptyStr */,
       unsigned int iteration /* = numeric_limits<unsigned int>::max() */,
       const CSeq_align_set* align_set /* = 0 */,
       CConstRef<CBioseq> subj_bioseq /* = CConstRef<CBioseq>() */)
{
    x_PrintQueryAndDbNames(program_version, bioseq, dbname, rid, iteration, subj_bioseq);
    // Print number of alignments found, but only if it has been set.
    if (align_set) {
       int num_hits = align_set->Get().size();
       if (num_hits != 0) {
           x_PrintFieldNames();
       }
       m_Ostream << "# " << num_hits << " hits found" << "\n";
    }
}

void 
CBlastTabularInfo::x_PrintQueryAndDbNames(const string& program_version,
       const CBioseq& bioseq,
       const string& dbname,
       const string& rid,
       unsigned int iteration,
       CConstRef<CBioseq> subj_bioseq)
{
    m_Ostream << "# ";
    m_Ostream << program_version << "\n";

    if (iteration != numeric_limits<unsigned int>::max())
        m_Ostream << "# Iteration: " << iteration << "\n";

    const size_t kLineLength(0);
    const bool kHtmlFormat(false);
    const bool kTabularFormat(true);

    // Print the query defline with no html; there is no need to set the 
    // line length restriction, since it's ignored for the tabular case.
    CAlignFormatUtil::AcknowledgeBlastQuery(bioseq, kLineLength, m_Ostream, 
                                            m_ParseLocalIds, kHtmlFormat,
                                            kTabularFormat, rid);
    
    if (dbname != kEmptyStr) {
        m_Ostream << "\n# Database: " << dbname << "\n";
    } else {
        _ASSERT(subj_bioseq.NotEmpty());
        m_Ostream << "\n";
        CAlignFormatUtil::AcknowledgeBlastSubject(*subj_bioseq, kLineLength,
                                                  m_Ostream, m_ParseLocalIds,
                                                  kHtmlFormat, kTabularFormat);
        m_Ostream << "\n";
    }
}

void CBlastTabularInfo::PrintNumProcessed(int num_queries)
{
    m_Ostream << "# BLAST processed " << num_queries << " queries\n";
}

void 
CBlastTabularInfo::SetScores(int score, double bit_score, double evalue)
{
    string total_bit_string, raw_score_string;
    m_Score = score;
    CAlignFormatUtil::GetScoreString(evalue, bit_score, 0, score, m_Evalue, 
                                     m_BitScore, total_bit_string, raw_score_string);
}

void 
CBlastTabularInfo::SetEndpoints(int q_start, int q_end, int s_start, int s_end)
{
    m_QueryStart = q_start;
    m_QueryEnd = q_end;
    m_SubjectStart = s_start;
    m_SubjectEnd = s_end;    
}

void 
CBlastTabularInfo::SetBTOP(string BTOP)
{
    m_BTOP = BTOP;
}

void 
CBlastTabularInfo::SetCounts(int num_ident, int length, int gaps, int gap_opens,
                             int positives, int query_frame, int subject_frame)
{
    m_AlignLength = length;
    m_NumIdent = num_ident;
    m_NumGaps = gaps;
    m_NumGapOpens = gap_opens;
    m_NumPositives = positives;
    m_QueryFrame = query_frame;
    m_SubjectFrame = subject_frame;
}

void
CBlastTabularInfo::SetQueryId(list<CRef<CSeq_id> >& id)
{
    m_QueryId = id;
}

void 
CBlastTabularInfo::SetSubjectId(list<CRef<CSeq_id> >& id)
{
    m_SubjectIds.push_back(id);
}

list<string> 
CBlastTabularInfo::GetAllFieldNames(void)
{
    list<string> field_names;

    for (map<string, ETabularField>::iterator iter = m_FieldMap.begin();
         iter != m_FieldMap.end(); ++iter) {
        field_names.push_back((*iter).first);
    }
    return field_names;
}

void 
CBlastTabularInfo::x_AddFieldToShow(ETabularField field)
{
    if ( !x_IsFieldRequested(field) ) {
        m_FieldsToShow.push_back(field);
    }
}

void 
CBlastTabularInfo::x_DeleteFieldToShow(ETabularField field)
{
    list<ETabularField>::iterator iter; 

    while ((iter = find(m_FieldsToShow.begin(), m_FieldsToShow.end(), field))
           != m_FieldsToShow.end())
        m_FieldsToShow.erase(iter); 
}

void 
CBlastTabularInfo::x_PrintField(ETabularField field)
{
    switch (field) {
    case eQuerySeqId:
        x_PrintQuerySeqId(); break;
    case eQueryGi:
        x_PrintQueryGi(); break;
    case eQueryAccession:
        x_PrintQueryAccession(); break;
    case eQueryAccessionVersion:
        x_PrintQueryAccessionVersion(); break;
    case eQueryLength:
        x_PrintQueryLength(); break;
    case eSubjectSeqId:
        x_PrintSubjectSeqId(); break;
    case eSubjectAllSeqIds:
        x_PrintSubjectAllSeqIds(); break;
    case eSubjectGi:
        x_PrintSubjectGi(); break;
    case eSubjectAllGis:
        x_PrintSubjectAllGis(); break;
    case eSubjectAccession:
        x_PrintSubjectAccession(); break;
    case eSubjAccessionVersion:
        x_PrintSubjectAccessionVersion(); break;
    case eSubjectAllAccessions:
        x_PrintSubjectAllAccessions(); break;
    case eSubjectLength:
        x_PrintSubjectLength(); break;
    case eQueryStart:
        x_PrintQueryStart(); break;
    case eQueryEnd:
        x_PrintQueryEnd(); break;
    case eSubjectStart:
        x_PrintSubjectStart(); break;
    case eSubjectEnd:
        x_PrintSubjectEnd(); break;
    case eQuerySeq:
        x_PrintQuerySeq(); break;
    case eSubjectSeq:
        x_PrintSubjectSeq(); break;
    case eEvalue:
        x_PrintEvalue(); break;
    case eBitScore:
        x_PrintBitScore(); break;
    case eScore:
        x_PrintScore(); break;
    case eAlignmentLength:
        x_PrintAlignmentLength(); break;
    case ePercentIdentical:
        x_PrintPercentIdentical(); break;
    case eNumIdentical:
        x_PrintNumIdentical(); break;
    case eMismatches:
        x_PrintMismatches(); break;
    case ePositives:
        x_PrintNumPositives(); break;
    case eGapOpenings:
        x_PrintGapOpenings(); break;
    case eGaps:
        x_PrintGaps(); break;
    case ePercentPositives:
        x_PrintPercentPositives(); break;
    case eFrames:
        x_PrintFrames(); break;
    case eQueryFrame:
        x_PrintQueryFrame(); break;
    case eSubjFrame:
        x_PrintSubjectFrame(); break;        
    case eBTOP:
        x_PrintBTOP(); break;        
    default:
        _ASSERT(false);
        break;
    }
}

/// @todo FIXME add means to specify masked database (SB-343)
void 
CIgBlastTabularInfo::PrintHeader(const string& program_version, 
       const CBioseq& bioseq, 
       const string& dbname, 
       const string& rid /* = kEmptyStr */,
       unsigned int iteration /* = numeric_limits<unsigned int>::max() */,
       const CSeq_align_set* align_set /* = 0 */,
       CConstRef<CBioseq> subj_bioseq /* = CConstRef<CBioseq>() */,
       const string& domain_sys)
{
    x_PrintQueryAndDbNames(program_version, bioseq, dbname, rid, iteration, subj_bioseq);
    m_Ostream << "# Domain classification requested: " << domain_sys << endl;
    // Print number of alignments found, but only if it has been set.
    if (align_set) {
       PrintMasterAlign();
       m_Ostream <<  "# Hit table (the first field indicates the chain type of the hit)" << endl;
       int num_hits = align_set->Get().size();
       if (num_hits != 0) {
           x_PrintFieldNames();
       }
       m_Ostream << "# " << num_hits << " hits found" << "\n";
    } else {
       m_Ostream << "# 0 hits found" << "\n";
    }
}

int CIgBlastTabularInfo::SetMasterFields(const CSeq_align& align, 
                                 CScope& scope, 
                                 const string& chain_type,
                                 CNcbiMatrix<int>* matrix)
{
    int retval = 0;
    bool hasSeq = x_IsFieldRequested(eQuerySeq);
    bool hasQuerySeqId = x_IsFieldRequested(eQuerySeqId);
    bool hasQueryStart = x_IsFieldRequested(eQueryStart);

    x_ResetIgFields();
    const CBioseq_Handle& query_bh = 
                scope.GetBioseqHandle(align.GetSeq_id(0));
    int length = query_bh.GetBioseqLength();
    query_bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac)
            .GetSeqData(0, length, m_Query);

    if (!hasSeq) x_AddFieldToShow(eQuerySeq);
    if (!hasQuerySeqId) x_AddFieldToShow(eQuerySeqId);
    if (!hasQueryStart) x_AddFieldToShow(eQueryStart);
    retval = SetFields(align, scope, chain_type, matrix);
    if (!hasSeq) x_DeleteFieldToShow(eQuerySeq);
    if (!hasQuerySeqId) x_DeleteFieldToShow(eQuerySeqId);
    if (!hasQueryStart) x_DeleteFieldToShow(eQueryStart);
    return retval;
};            

int CIgBlastTabularInfo::SetFields(const CSeq_align& align,
                                 CScope& scope, 
                                 const string& chain_type,
                                 CNcbiMatrix<int>* matrix)
{
    m_ChainType = chain_type;
    if (m_ChainType == "NA") m_ChainType = "N/A";
    return CBlastTabularInfo::SetFields(align, scope, matrix);
};

void CIgBlastTabularInfo::SetIgAnnotation(const CRef<blast::CIgAnnotation> &annot,
                                          bool is_protein)
{
    SetSeqType(!is_protein);
    SetMinusStrand(annot->m_MinusStrand);

    // Gene info coordinates are half inclusive
    SetVGene(annot->m_TopGeneIds[0], annot->m_GeneInfo[0], annot->m_GeneInfo[1]);
    SetDGene(annot->m_TopGeneIds[1], annot->m_GeneInfo[2], annot->m_GeneInfo[3]);
    SetJGene(annot->m_TopGeneIds[2], annot->m_GeneInfo[4], annot->m_GeneInfo[5]);

    // Compute Frame info
    if (annot->m_FrameInfo[0] >= 0 && annot->m_FrameInfo[1] >= 0) {
        int off = annot->m_FrameInfo[0];
        int len = annot->m_FrameInfo[1] - off;
        if ( len % 3 == 0) {
            string seq_data(m_Query, off, len);
            string seq_trans;
            CSeqTranslator::Translate(seq_data, seq_trans);
            if (seq_trans.find('*') != string::npos) {
                SetFrame("IP");
            } else {
                SetFrame("IF");
            }
        } else  {
            SetFrame("OF");
        }
    } else {
        SetFrame("N/A");
    }

    // Domain info coordinates are inclusive (and always on positive strand)
    AddIgDomain("FWR1", annot->m_DomainInfo[0], annot->m_DomainInfo[1]+1);
    AddIgDomain("CDR1", annot->m_DomainInfo[2], annot->m_DomainInfo[3]+1);
    AddIgDomain("FWR2", annot->m_DomainInfo[4], annot->m_DomainInfo[5]+1);
    AddIgDomain("CDR2", annot->m_DomainInfo[6], annot->m_DomainInfo[7]+1);
    AddIgDomain("FWR3", annot->m_DomainInfo[8], annot->m_DomainInfo[9]+1);
    AddIgDomain("CDR3 (V region only)", annot->m_DomainInfo[10], annot->m_DomainInfo[11]+1);
};

void CIgBlastTabularInfo::Print(void)
{
    m_Ostream << m_ChainType << m_FieldDelimiter;
    CBlastTabularInfo::Print();
};

void CIgBlastTabularInfo::PrintMasterAlign(const string &header) const
{
    m_Ostream << endl;
    if (m_IsNucl) {
        if (m_IsMinusStrand) {
            m_Ostream << header << "Note that your query represents the minus strand "
                      << "of a V gene and has been converted to the plus strand. "
                      << "The sequence positions refer to the converted sequence. " << endl << endl;
        }
        m_Ostream << header << "V(D)J rearrangement summary for query sequence ";
        m_Ostream << "(Top V gene match, ";
        if (m_ChainType == "VH") m_Ostream << "Top D gene match, ";
        m_Ostream << "Top J gene match, Chain type, V-J Frame, Strand):" << endl;
        m_Ostream << m_VGene.sid << m_FieldDelimiter;
        if (m_ChainType == "VH") m_Ostream << m_DGene.sid << m_FieldDelimiter;
        m_Ostream << m_JGene.sid << m_FieldDelimiter;
        m_Ostream << m_ChainType << m_FieldDelimiter;
        if (m_FrameInfo == "IF") m_Ostream << "In-frame";
        else if (m_FrameInfo == "OF") m_Ostream << "Out-of-frame";
        else if (m_FrameInfo == "IP") m_Ostream << "In-frame with stop codon";
        else m_Ostream << "N/A";
        m_Ostream << m_FieldDelimiter << ((m_IsMinusStrand) ? '-' : '+' ) << endl << endl;
        x_PrintIgGenes(false, header);
    }

    int length = 0;
    for (unsigned int i=0; i<m_IgDomains.size(); ++i) {
        if (m_IgDomains[i]->length > 0) {
            length += m_IgDomains[i]->length;
        }
    }
    if (!length) return;

    m_Ostream << header << "Alignment summary between query and top germline V gene hit ";
    m_Ostream << "(from, to, length, matches, mismatches, gaps, percent identity)" << endl;

    int num_match = 0;
    int num_mismatch = 0;
    int num_gap = 0;
    for (unsigned int i=0; i<m_IgDomains.size(); ++i) {
        x_PrintIgDomain(*(m_IgDomains[i]));
        m_Ostream << endl;
        if (m_IgDomains[i]->length > 0) {
            num_match += m_IgDomains[i]->num_match;
            num_mismatch += m_IgDomains[i]->num_mismatch;
            num_gap += m_IgDomains[i]->num_gap;
        }
    }
    m_Ostream << "Total" 
              << m_FieldDelimiter << "N/A"
              << m_FieldDelimiter << "N/A"
              << m_FieldDelimiter << length  
              << m_FieldDelimiter << num_match 
              << m_FieldDelimiter << num_mismatch
              << m_FieldDelimiter << num_gap
              << m_FieldDelimiter << std::setprecision(3) << num_match*100.0/length
              << endl << endl;
};

void CIgBlastTabularInfo::PrintHtmlSummary() const
{
    if (m_IsNucl) {
        m_Ostream << "<br><br><br>V(D)J rearrangement summary for query sequence:\n";
        m_Ostream << "<pre><table border=1>\n";
        m_Ostream << "<tr><td>Top V gene match</td>";
        if (m_ChainType == "VH") {  
            m_Ostream << "<td>Top D gene match</td>";
        }
        m_Ostream << "<td>Top J gene match</td>"
                  << "<td>Chain type</td>"
                  << "<td>V-J Frame</td>"
                  << "<td>Strand</td></tr>\n";

        m_Ostream << "<tr><td>"  << m_VGene.sid;
        if (m_ChainType == "VH") { 
            m_Ostream << "</td><td>" << m_DGene.sid;
        }
        m_Ostream << "</td><td>" << m_JGene.sid
                  << "</td><td>" << m_ChainType
                  << "</td><td>";
        if (m_FrameInfo == "IF") {
            m_Ostream << "In-frame";
        } else if (m_FrameInfo == "OF") {
            m_Ostream << "Out-of-frame";
        } else if (m_FrameInfo == "IP") {
            m_Ostream << "In-frame with stop codon";
        } 
        m_Ostream << "</td><td>" << ((m_IsMinusStrand) ? '-' : '+') 
                  << "</td></tr></table></pre>\n";
        x_PrintIgGenes(true, "");
    }

    int length = 0;
    for (unsigned int i=0; i<m_IgDomains.size(); ++i) {
        if (m_IgDomains[i]->length > 0) {
            length += m_IgDomains[i]->length;
        }
    }
    if (!length) return;

    m_Ostream << "<br><br><br>Alignment summary between query and top germline V gene hit:\n";
    m_Ostream << "<pre><table border=1>";
    m_Ostream << "<tr><td> </td><td> from </td><td> to </td><td> length </td>"
              << "<td> matches </td><td> mismatches </td><td> gaps </td>"
              << "<td> identity(%) </td></tr>\n";

    int num_match = 0;
    int num_mismatch = 0;
    int num_gap = 0;
    for (unsigned int i=0; i<m_IgDomains.size(); ++i) {
        x_PrintIgDomainHtml(*(m_IgDomains[i]));
        if (m_IgDomains[i]->length > 0) {
            num_match += m_IgDomains[i]->num_match;
            num_mismatch += m_IgDomains[i]->num_mismatch;
            num_gap += m_IgDomains[i]->num_gap;
        }
    }
    m_Ostream << "<tr><td> Total </td><td> </td><td> </td><td>" << length 
              << "</td><td>" << num_match 
              << "</td><td>" << num_mismatch
              << "</td><td>" << num_gap
              << "</td><td>" << std::setprecision(3) << num_match*100.0/length
              << "</td></tr>";
    m_Ostream << "</table></pre>\n";
};

void CIgBlastTabularInfo::x_ResetIgFields()
{
    for (unsigned int i=0; i<m_IgDomains.size(); ++i) {
        delete m_IgDomains[i];
    }
    m_IgDomains.clear();
    m_FrameInfo = "N/A";
    m_ChainType = "N/A";
    m_IsMinusStrand = false;
    m_VGene.Reset();
    m_DGene.Reset();
    m_JGene.Reset();
};

void CIgBlastTabularInfo::x_PrintPartialQuery(int start, int end, bool isHtml) const
{
    const bool isOverlap = (start > end);

    if (start <0 || end <0 || start==end) {
        if (isHtml) {
            m_Ostream << "<td></td>";
        } else {
            m_Ostream << "N/A";
        }
        return;
    }

    if (isHtml) m_Ostream << "<td>";
    if (isOverlap) {
        int tmp = end;
        end = start;
        start = tmp;
        m_Ostream << '(';
    }
    for (int pos = start; pos < end; ++pos) {
        m_Ostream << m_Query[pos];
    }
    if (isOverlap)  m_Ostream << ')';
    if (isHtml) m_Ostream << "</td>";
};

void CIgBlastTabularInfo::x_PrintIgGenes(bool isHtml, const string& header) const 
{
    int     a1, a2, a3, a4;
    int b0, b1, b2, b3, b4, b5;

    if (m_VGene.start <0) return;

    b0 = m_VGene.start;
    b1 = m_VGene.end;
    b2 = m_DGene.start;
    b3 = m_DGene.end;
    b4 = m_JGene.start;
    b5 = m_JGene.end;

    if (b2 < 0) {
        b2 = b1;
        b3 = b1;
        if (b3 > b4 && b4 > 0 && m_ChainType == "VH") {
            b4 = b3;
        }
    }

    if (b4 < 0) {
        b4 = b3;
        b5 = b3;
    }

    if (m_ChainType == "VH") {
        a1 = min(b1, b2);
        a2 = max(b1, b2);
        a3 = min(b3, b4);
        a4 = max(b3, b4);
    } else {
        a1 = min(b1, b4);
        a4 = max(b1, b4);
    }

    if (isHtml) {
        m_Ostream << "<br>Nucleotide details around V(D)J junctions:\n";
        m_Ostream << "<pre><table border=1>\n";
        m_Ostream << "<tr><td>V region end</td>";
        if (m_ChainType == "VH") {
            m_Ostream << "<td>V-D junction*</td>"
                      << "<td>D region</td>"
                      << "<td>D-J junction*</td>";
        } else {
            m_Ostream << "<td>V-J junction*</td>";
        }
        m_Ostream << "<td>J region start</td></tr>\n<tr>";
    } else {
        m_Ostream << header << "V(D)J junction details (V end, ";
        if (m_ChainType == "VH")  m_Ostream << "V-D junction, D region, D-J junction, ";
        else m_Ostream << "V-J junction, ";
        m_Ostream << "J start.  Note that overlapping nucleotides at VDJ junction, if any, are shown in parentheses (i.e., (TACT)) and"
                  << " are not included under V, D, or J region itself)" << endl;
    }

    x_PrintPartialQuery(max(b0, a1 - 5), a1, isHtml); m_Ostream << m_FieldDelimiter;
    if (m_ChainType == "VH") {
        x_PrintPartialQuery(b1, b2, isHtml); m_Ostream << m_FieldDelimiter;
        x_PrintPartialQuery(a2, a3, isHtml); m_Ostream << m_FieldDelimiter;
        x_PrintPartialQuery(b3, b4, isHtml); m_Ostream << m_FieldDelimiter;
    } else {
        x_PrintPartialQuery(b1, b4, isHtml); m_Ostream << m_FieldDelimiter;
    }
    x_PrintPartialQuery(a4, min(b5, a4 + 5), isHtml); m_Ostream << m_FieldDelimiter;

    if (isHtml) {
        m_Ostream << "</tr>\n</table></pre>";

        m_Ostream << "*: Overlapping nucleotides may exist"
                  << " at some V-D-J junction (i.e, nucleotides"
                  << " that could be contributed by either joining segment "
                  << " due to homology-directed rearrangement). "
                  << "Such bases will be shown inside a parenthesis (i.e., (TACAT))"
                  << " and will not be included under V, D or J region itself.\n";
    }
    m_Ostream << endl << endl;
};

void CIgBlastTabularInfo::x_ComputeIgDomain(SIgDomain &domain)
{
    int pos = 0;
    unsigned int i = 0;
    while (pos < domain.start - m_QueryStart +1 && i < m_QuerySeq.size()) {
        if (m_QuerySeq[i] != '-') ++pos;
        ++i;
    }
    while (pos < domain.end - m_QueryStart +1 && i < m_QuerySeq.size()) {
        if (m_QuerySeq[i] != '-') {
            ++pos;
            if (m_QuerySeq[i] == m_SubjectSeq[i]) {
                ++domain.num_match;
            } else if (m_SubjectSeq[i] != '-') {
                ++domain.num_mismatch;
            } else {
                ++domain.num_gap;
            }
        } else {
            ++domain.num_gap;
        }
        ++domain.length;
        ++i;
    }
    if (domain.start + domain.length < domain.end) {
        domain.end = domain.start + domain.length;
    }
};

void CIgBlastTabularInfo::x_PrintIgDomain(const SIgDomain &domain) const
{
    m_Ostream << domain.name 
              << m_FieldDelimiter
              << domain.start +1
              << m_FieldDelimiter
              << domain.end
              << m_FieldDelimiter;
    if (domain.length > 0) {
        m_Ostream  << domain.length
              << m_FieldDelimiter
              << domain.num_match
              << m_FieldDelimiter
              << domain.num_mismatch
              << m_FieldDelimiter
              << domain.num_gap
              << m_FieldDelimiter
              << std::setprecision(3)
              << domain.num_match*100.0/domain.length;
    } else {
        m_Ostream  << "N/A" << m_FieldDelimiter
              <<  "N/A" << m_FieldDelimiter
              <<  "N/A" << m_FieldDelimiter
              <<  "N/A" << m_FieldDelimiter
              <<  "N/A" << m_FieldDelimiter
              <<  "N/A" << m_FieldDelimiter
              <<  "N/A";
    }
};

void CIgBlastTabularInfo::x_PrintIgDomainHtml(const SIgDomain &domain) const
{
    m_Ostream << "<tr><td> " << domain.name << " </td>"
              <<     "<td> " << domain.start+1 << " </td>"
              <<     "<td> " << domain.end << " </td>";
    if (domain.length > 0) {
        m_Ostream  << "<td> " << domain.length << " </td>"
                   << "<td> " << domain.num_match << " </td>"
                   << "<td> " << domain.num_mismatch << " </td>"
                   << "<td> " << domain.num_gap << " </td>"
                   << "<td> " << std::setprecision(3) 
                   << domain.num_match*100.0/domain.length << " </td></tr>\n";
    } else {
        m_Ostream  << "<td> </td><td> </td><td> </td><td> </td></tr>\n";
    }
};

END_SCOPE(align_format)
END_NCBI_SCOPE

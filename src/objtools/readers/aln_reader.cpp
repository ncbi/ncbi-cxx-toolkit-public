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
 * Authors:  Josh Cherry
 *
 * File Description:  C++ wrappers for alignment file reading
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/readers/aln_reader.hpp>
#include <objtools/readers/alnread.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/error_codes.hpp>
#include <util/format_guess.hpp>

#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/mod_reader.hpp>
#include <objtools/logging/listener.hpp>
#include <cassert>

#define NCBI_USE_ERRCODE_X   Objtools_Rd_Align

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


string sAlnErrorToString(const CAlnError & error)
{
    auto lineNumber = error.GetLineNum();
    if (lineNumber == -1) {
        return FORMAT(
            "At ID '" << error.GetID() << "' "
            "in category '" << static_cast<int>(error.GetCategory()) << "': "
            << error.GetMsg() << "'");
    }
    return FORMAT(
        "At ID '" << error.GetID() << "' "
        "in category '" << static_cast<int>(error.GetCategory()) << "' "
        "at line " << error.GetLineNum() << ": "
        << error.GetMsg() << "'");
}

CAlnError::CAlnError(int category, int line_num, string id, string message)
{
    switch (category) 
    {
    case -1:
        m_Category = eAlnErr_Unknown;
        break;
    case 0:
        m_Category = eAlnErr_NoError;
        break;
    case 1:
        m_Category = eAlnErr_Fatal;
        break;
    case 2:
        m_Category = eAlnErr_BadData;
        break;
    case 3:
        m_Category = eAlnErr_BadFormat;
        break;
    case 4:
        m_Category = eAlnErr_BadChar;
        break;
    default:
        m_Category = eAlnErr_Unknown;
        break;
    }

    m_LineNum = line_num;
    m_ID = id;
    m_Message = message;
}


CAlnError::CAlnError(const CAlnError& e)
{
    m_Category = e.GetCategory();
    m_LineNum = e.GetLineNum();
    m_ID = e.GetID();
    m_Message = e.GetMsg();
}


static char * ALIGNMENT_CALLBACK s_ReadLine(void *user_data)
{
    CNcbiIstream *is = static_cast<CNcbiIstream *>(user_data);
    if (!*is) {
        return 0;
    }
    string s;
    NcbiGetline(*is, s, "\r\n");
    return strdup(s.c_str());
}


bool ALIGNMENT_CALLBACK 
sReadLine(
    istream& istr,
    string& line)
{
    if (!istr  || istr.eof()) {
        return false;
    }
    NcbiGetline(istr, line, "\r\n");
    return true;
}


static void ALIGNMENT_CALLBACK s_ReportError(
    const CErrorInfo& err,
    void *user_data)
{
    CAlnReader::TErrorList *err_list;
   
    const int category_BadData = 2;
    const int category_BadChar = 4;

    if (user_data != NULL) {
        err_list = (CAlnReader::TErrorList *)user_data;
        int category = err.Category();
        string err_msg = err.Message();
        if ( (category == category_BadData) &&
                (err_msg.find("bad char") != string::npos) ) {
            category = category_BadChar;
        }
        (*err_list).push_back (
            CAlnError(category, err.LineNumber(), err.Id(), err_msg));
    }
        
    string msg = "Error reading alignment file";
    if (err.LineNumber() != CErrorInfo::NO_LINE_NUMBER) {
        msg += " at line " + NStr::IntToString(err.LineNumber());
    }
    if (!err.Message().empty()) {
        msg += ":  ";
        msg += err.Message();
    }

    if (err.Category() == eAlnErr_Fatal) {
        LOG_POST_X(1, Error << msg);
    } else {
        LOG_POST_X(1, Info << msg);
    }
}


CAlnReader::~CAlnReader()
{
}


int CAlnReader::x_GetGCD(const int a, const int b) const
{
    if (a == 0) {
        return b;
    }

    if (b == 0) {
        return a;
    }


    if (a > b) {
        const int r = a%b;
        return x_GetGCD(b,r);
    }
    // b > a
    const int r = b%a;
    return x_GetGCD(a, r);
}


void s_GetSequenceLengthInfo(
        const SAlignmentFile& alignmentInfo, 
        size_t& min_len, 
        size_t& max_len, 
        int& max_index) 
{
    const auto numSequences = alignmentInfo.NumSequences();
    if (numSequences == 0) {
        return;
    }

    max_len = alignmentInfo.mSequences[0].size();
    min_len = max_len;
    max_index = 0;

    for (auto i = 0; i < numSequences; ++i) {
        size_t curr_len = alignmentInfo.mSequences[i].size();
        if (curr_len > max_len) {
            max_len = curr_len;
            max_index = i;
        } 
        else 
        if (curr_len < min_len){
            min_len = curr_len;
        }
    }
}


bool CAlnReader::x_IsReplicatedSequence(const char* seq_data, 
    const int length,
    const int repeat_interval) const
{
    if (length%repeat_interval != 0) {
        return false;
    }

    const int num_repeats = length/repeat_interval;
    for (int i=1; i<num_repeats; ++i) {
        if (strncmp(seq_data, seq_data + i*repeat_interval, repeat_interval) != 0){
            return false;
        }
    }
    return true;
}


void
sReportError(
    ILineErrorListener* pEC,
    EDiagSev severity,
    int lineNumber,
    const string& message,
    ILineError::EProblem problemType=ILineError::eProblem_GeneralParsingError)
{
    if (!pEC) {
        NCBI_THROW2(CObjReaderParseException, eFormat, message, 0);
    }
    AutoPtr<CObjReaderLineException> pErr(
        CObjReaderLineException::Create(
        severity,
        lineNumber,
        message,
        problemType));
    pEC->PutError(*pErr);
}

void CAlnReader::Read(
    bool guess, 
    bool generate_local_ids,
    ncbi::objects::ILineErrorListener* pErrorListener)
{
    if (m_ReadDone) {
        return;
    }

    // make a SSequenceInfo corresponding to our CSequenceInfo argument
    CSequenceInfo sequenceInfo(
        m_Alphabet, m_Match, m_Missing, m_BeginningGap, m_MiddleGap, m_EndGap);

    // read the alignment stream
    m_Errors.clear();
    SAlignmentFile alignmentInfo;
    bool allClear = ReadAlignmentFile(sReadLine, m_IS,
                            s_ReportError, &(m_Errors), sequenceInfo,
                            generate_local_ids,
                            alignmentInfo);

    // report any errors through proper channels:
    if (pErrorListener) {
        for (const auto& error : GetErrorList()) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                    eDiag_Error,
                    (error.GetLineNum() == -1 ? 0 : error.GetLineNum()),
                    sAlnErrorToString(error),
                    ILineError::eProblem_GeneralParsingError));
            pErrorListener->PutError(*pErr);
        }
    }

    if (!allClear) {
        sReportError(
            pErrorListener,
            eDiag_Fatal,
            0,
            "Error reading alignment: Invalid input or alphabet");
        return;
    }

    if (1 == alignmentInfo.NumSequences()) {
        sReportError(
            pErrorListener,
            eDiag_Fatal,
            0,
            "Error reading alignment: Need more than one sequence");
        return;
    }
 
    // Check sequence lengths
    size_t max_len, min_len;
    int max_index;
    s_GetSequenceLengthInfo(alignmentInfo, 
        min_len,
        max_len,
        max_index);

    if (min_len == 0) {
        sReportError(
            pErrorListener,
            eDiag_Fatal,
            0,
            "Error reading alignment: Missing sequence data");
        return;
    }

    if (max_len != min_len) { 
        // Check for replicated intervals in the longest sequence
        const int repeat_interval = x_GetGCD(max_len, min_len);
        const bool is_repeated = 
            x_IsReplicatedSequence(
                alignmentInfo.mSequences[max_index].c_str(), max_len, repeat_interval);
        //AlignmentFileFree(afp);

        if (is_repeated) {
            sReportError(
                pErrorListener,
                eDiag_Fatal,
                0,
                "Error reading alignment: Possible sequence replication");
            return;
        }   
        else {
            sReportError(
                pErrorListener,
                eDiag_Fatal,
                0,
                "Error reading alignment: Not all sequences have same length");
            return;
        }
    }

    
    // if we're trying to guess whether this is an alignment file,
    // and no tell-tale alignment format lines were found,
    // check to see if any of the lines contain gaps.
    // no gaps plus no alignment indicators -> don't guess alignment
    const auto numSequences = alignmentInfo.NumSequences();
    if (guess && !alignmentInfo.align_format_found) {
        bool found_gap = false;
        for (int i = 0; i < numSequences && !found_gap; i++) {
            if (alignmentInfo.mSequences[i].find('-') != string::npos) {
                found_gap = true;
            }
        }
        if (!found_gap) {
            //AlignmentFileFree (afp);
            sReportError(
                pErrorListener,
                eDiag_Fatal,
                0,
                "Error reading alignment");
            return;
        }
    }

    m_Seqs.assign(alignmentInfo.mSequences.begin(), alignmentInfo.mSequences.end());
    m_Ids.assign(alignmentInfo.mIds.begin(), alignmentInfo.mIds.end());
    m_Organisms.assign(alignmentInfo.mOrganisms.begin(), alignmentInfo.mOrganisms.end());

    auto numDeflines = alignmentInfo.NumDeflines();
    if (numDeflines) {
        if (numDeflines == m_Ids.size()) {
            m_Deflines.resize(numDeflines);
            for (int i=0;  i< numDeflines;  ++i) {
                m_Deflines[i] = NStr::TruncateSpaces(alignmentInfo.mDeflines[i]);
            }
        }
        else {
            sReportError(
                pErrorListener,
                eDiag_Error,
                0,
                "Error reading deflines. Unable to associate deflines with sequences");
        }
    }
    m_Dim = m_Ids.size();
    m_ReadDone = true;

    return;
}


void CAlnReader::SetFastaGap(EAlphabet alpha)
{
    SetAlphabet(alpha);
    SetAllGap("-");
}


void CAlnReader::SetClustal(EAlphabet alpha)
{
    SetAlphabet(alpha);
    SetAllGap("-");
}


void CAlnReader::SetPaup(EAlphabet alpha)
{
    SetAlphabet(alpha);
    SetAllGap("-");
}


void CAlnReader::SetPhylip(EAlphabet alpha)
{
    SetAlphabet(alpha);
    SetAllGap("-");
}


void CAlnReader::x_CalculateMiddleSections()
{
    m_MiddleSections.clear();

    for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
        TSeqPos begin_len = m_Seqs[row_i].find_first_not_of(m_BeginningGap);
        TSeqPos end_len = 0;
        if (begin_len < m_Seqs[row_i].length()) {
            string::iterator s = m_Seqs[row_i].end();
            while (s != m_Seqs[row_i].begin()) {
                --s;
                if (m_EndGap.find(*s) != string::npos) {
                    end_len++;
                } else {
                    break;
                }
            }
        }
        m_MiddleSections.push_back(TAlignMiddleInterval(begin_len, m_Seqs[row_i].length() - end_len - 1));
    }
}


bool CAlnReader::x_IsGap(TNumrow row, TSeqPos pos, const string& residue)
{
    if (m_MiddleSections.size() == 0) {
        x_CalculateMiddleSections();
    }
    if (row > m_MiddleSections.size()) {
        return false;
    }
    if (pos < m_MiddleSections[row].first) {
        if (NStr::Find(m_BeginningGap, residue) == string::npos) {
            return false;
        } else {
            return true;
        }
    } else if (pos > m_MiddleSections[row].second) {
        if (NStr::Find(m_EndGap, residue) == string::npos) {
            return false;
        } else {
            return true;
        }
    } else {
        if (NStr::Find(m_MiddleGap, residue) == string::npos) {
            return false;
        } else {
            return true;
        }
    }
}

/*
CRef<CSeq_id> CAlnReader::GetFastaId(const string& fasta_defline, 
        const TSeqPos& defline_number, 
        TFastaFlags fasta_flags) 
{
    TSeqPos range_start = 0, range_end = 0;
    bool has_range = false;
    SDeflineParseInfo parse_info;
    parse_info.fBaseFlags = 0;
    parse_info.fFastaFlags = fasta_flags;
    parse_info.maxIdLength =  kMax_UI4;
    parse_info.lineNumber = defline_number; 

    TIgnoredProblems ignored_errors;
    TSeqTitles seq_titles;
    list<CRef<CSeq_id>> ids;
    try {
        ParseDefline(fasta_defline,
            parse_info,
            ignored_errors,
            ids,
            has_range,
            range_start,
            range_end,
            seq_titles,
            0);
    }
    catch (const exception&) {}

    CRef<CSeq_id> result;
    const bool unique_id = (fasta_flags & objects::CFastaReader::fUniqueIDs); 


    if (ids.empty()) {
        result = GenerateID(fasta_defline, unique_id);
    } 
    else {
        result = FindBestChoice(ids, CSeq_id::BestRank);
    } 

    if (has_range) {
        string seq_id_text = "lcl|" + result->GetSeqIdString(true);
        seq_id_text += ":" + NStr::NumericToString(range_start+1) + "_" + NStr::NumericToString(range_end+1);
        result = Ref(new CSeq_id(seq_id_text)); 
    }

    if (unique_id) {
        x_CacheIdHandle(CSeq_id_Handle::GetHandle(*result));
    }

    return result;
}
*/


CRef<CSeq_id> CAlnReader::GenerateID(const string& fasta_defline,
    const TSeqPos& defline_number,
    TFastaFlags fasta_flags)
{
    string id_string = m_Ids[defline_number];

    CBioseq::TId xid;
    if (CSeq_id::ParseFastaIds(xid, id_string, true) > 0) {
        return xid.front();
    } 
    return  Ref(new CSeq_id(CSeq_id::e_Local, id_string));
}    


void CAlnReader::x_AssignDensegIds(const TFastaFlags fasta_flags, 
        CDense_seg& denseg) 
{
    CDense_seg::TIds& ids = denseg.SetIds();
    ids.resize(m_Dim);

    for (auto i=0; i<m_Dim; ++i) {
        // Reconstruct original defline string from results 
        // returned by C code.
        string fasta_defline = ">" + m_Ids[i];
        if (i < m_Deflines.size() && !m_Deflines[i].empty()) {
            fasta_defline += " " + m_Deflines[i];
        }
        ids[i] = GenerateID(fasta_defline, i, fasta_flags);
    }
    return;
}


CRef<CSeq_align> CAlnReader::GetSeqAlign(const TFastaFlags fasta_flags)
{
    if (m_Aln) {
        return m_Aln;
    } else if ( !m_ReadDone ) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                   "CAlnReader::GetSeqAlign(): "
                   "Seq_align is not available until after Read()", 0);
    }

    typedef CDense_seg::TNumseg TNumseg;

    m_Aln = new CSeq_align();
    m_Aln->SetType(CSeq_align::eType_not_set);
    m_Aln->SetDim(m_Dim);

    CDense_seg& ds = m_Aln->SetSegs().SetDenseg();
    ds.SetDim(m_Dim);
    
    CDense_seg::TStarts&  starts  = ds.SetStarts();
    //CDense_seg::TStrands& strands = ds.SetStrands();
    CDense_seg::TLens&    lens    = ds.SetLens();

    x_AssignDensegIds(fasta_flags, ds);

    // get the length of the alignment
    TSeqPos aln_stop = m_Seqs[0].size();
    for (TNumrow row_i = 1; row_i < m_Dim; row_i++) {
        if (m_Seqs[row_i].size() > aln_stop) {
            aln_stop = m_Seqs[row_i].size();
        }
    }


    m_SeqVec.resize(m_Dim);
    for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
        m_SeqVec[row_i].resize(m_Seqs[row_i].length(), 0);
    }
    m_SeqLen.resize(m_Dim, 0);
    vector<bool> is_gap;  is_gap.resize(m_Dim, true);
    vector<bool> prev_is_gap;  prev_is_gap.resize(m_Dim, true);
    vector<TSignedSeqPos> next_start; next_start.resize(m_Dim, 0);
    int starts_i = 0;
    TSeqPos prev_aln_pos = 0, prev_len = 0;
    bool new_seg = true;
    TNumseg numseg = 0;
    
    for (TSeqPos aln_pos = 0; aln_pos < aln_stop; aln_pos++) {
        for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
            if (aln_pos >= m_Seqs[row_i].length()) {
                if (!is_gap[row_i]) {
                    is_gap[row_i] = true;
                    new_seg = true;
                }
            } else {
                string residue = m_Seqs[row_i].substr(aln_pos, 1);
                NStr::ToUpper(residue);
                if (!x_IsGap(row_i, aln_pos, residue)) {

                    if (is_gap[row_i]) {
                        is_gap[row_i] = false;
                        new_seg = true;
                    }

                    // add to the sequence vector
                    m_SeqVec[row_i][m_SeqLen[row_i]++] = residue[0];

                } else {
  
                    if ( !is_gap[row_i] ) {
                        is_gap[row_i] = true;
                        new_seg = true;
                    }
                }

            }
        }

        if (new_seg) {
            if (numseg) { // if not the first seg
                lens.push_back(prev_len = aln_pos - prev_aln_pos);
                for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
                    if ( !prev_is_gap[row_i] ) {
                        next_start[row_i] += prev_len;
                    }
                }
            }

            starts.resize(starts_i + m_Dim);
            for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
                if (is_gap[row_i]) {
                    starts[starts_i++] = -1;
                } else {
                    starts[starts_i++] = next_start[row_i];;
                }
                prev_is_gap[row_i] = is_gap[row_i];
            }

            prev_aln_pos = aln_pos;

            numseg++;
            new_seg = false;
        }
    }

    for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
        m_SeqVec[row_i].resize(m_SeqLen[row_i]); // resize down to actual size
    }

    lens.push_back(aln_stop - prev_aln_pos);
    //strands.resize(numseg * m_Dim, eNa_strand_plus);
    _ASSERT(lens.size() == numseg);
    ds.SetNumseg(numseg);

#if _DEBUG
    m_Aln->Validate(true);
#endif    
    return m_Aln;
}


CRef<CSeq_entry> CAlnReader::GetSeqEntry(const TFastaFlags fasta_flags)
{
    if (m_Entry) {
        return m_Entry;
    } else if ( !m_ReadDone ) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                   "CAlnReader::GetSeqEntry(): "
                   "Seq_entry is not available until after Read()", 0);
    }
    m_Entry = new CSeq_entry();

    CRef<CSeq_align> seq_align = GetSeqAlign(fasta_flags);
    const CDense_seg& denseg = seq_align->GetSegs().GetDenseg();
    _ASSERT(denseg.GetIds().size() == m_Dim);

    CRef<CSeq_annot> seq_annot (new CSeq_annot);
    seq_annot->SetData().SetAlign().push_back(seq_align);

    m_Entry->SetSet().SetClass(CBioseq_set::eClass_pop_set);
    m_Entry->SetSet().SetAnnot().push_back(seq_annot);

    CBioseq_set::TSeq_set& seq_set = m_Entry->SetSet().SetSeq_set();

    typedef CDense_seg::TDim TNumrow;
    for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
        const string& seq_str     = m_SeqVec[row_i];
        const size_t& seq_str_len = seq_str.size();

        CRef<CSeq_entry> seq_entry (new CSeq_entry);

        // seq-id(s)
        CBioseq::TId& ids = seq_entry->SetSeq().SetId();
        ids.push_back(denseg.GetIds()[row_i]);
/*
        CSeq_id::ParseFastaIds(ids, m_Ids[row_i], true);
        if (ids.empty()) {
            ids.push_back(CRef<CSeq_id>(new CSeq_id(CSeq_id::e_Local,
                                                    m_Ids[row_i])));
        }
*/
        // mol
        CSeq_inst::EMol mol   = CSeq_inst::eMol_not_set;
        CSeq_id::EAccessionInfo ai = ids.front()->IdentifyAccession();
        if (ai & CSeq_id::fAcc_nuc) {
            mol = CSeq_inst::eMol_na;
        } else if (ai & CSeq_id::fAcc_prot) {
            mol = CSeq_inst::eMol_aa;
        } else {
            switch (CFormatGuess::SequenceType(seq_str.data(), seq_str_len)) {
            case CFormatGuess::eNucleotide:  mol = CSeq_inst::eMol_na;  break;
            case CFormatGuess::eProtein:     mol = CSeq_inst::eMol_aa;  break;
            default:                         break;
            }
        }

        // seq-inst
        CRef<CSeq_inst> seq_inst (new CSeq_inst);
        seq_entry->SetSeq().SetInst(*seq_inst);
        seq_set.push_back(seq_entry);

        // repr
        seq_inst->SetRepr(CSeq_inst::eRepr_raw);

        // mol
        seq_inst->SetMol(mol);

        // len
        _ASSERT(seq_str_len == m_SeqLen[row_i]);
        seq_inst->SetLength(seq_str_len);

        // data
        CSeq_data& data = seq_inst->SetSeq_data();
        if (mol == CSeq_inst::eMol_aa) {
            data.SetIupacaa().Set(seq_str);
        } else {
            data.SetIupacna().Set(seq_str);
            CSeqportUtil::Pack(&data);
        }
    }

    if (!m_Deflines.empty()) {
        int i=0;
        for (auto& pSeqEntry : seq_set) {
            x_AddMods(m_Deflines[i++], fasta_flags, pSeqEntry->SetSeq());
        }
    }

    return m_Entry;
}


static void s_AppendMods(
    const CModHandler::TModList& mods,
    string& title
    )
{
    for (const auto& mod : mods) {
        title.append(" [" 
            + mod.GetName()
            + "=" 
            + mod.GetValue()
            + "]");
    }
}

void CAlnReader::x_AddMods(const string& defline, 
        const TFastaFlags fasta_flags, 
        CBioseq& bioseq)
{
    if (NStr::IsBlank(defline)) {
        return;
    }

    CModHandler::TModList mod_list;
    string remainder;

    // Parse the defline string for modifiers
    CTitleParser::Apply(defline, mod_list, remainder);
    if (mod_list.empty() && NStr::IsBlank(remainder)) {
        return;
    } 

    unique_ptr<CObjtoolsListener> pMessageListener(new CObjtoolsListener());

    CModHandler mod_handler(pMessageListener.get());
    CModHandler::TModList rejected_mods;
    mod_handler.AddMods(mod_list, CModHandler::eAppendReplace, rejected_mods);

    // Apply modifiers to the bioseq
    CModHandler::TModList skipped_mods;
    CModAdder::Apply(mod_handler, bioseq, pMessageListener.get(), skipped_mods);


    if (fasta_flags & CFastaReader::fDeflineAsTitle) {
        x_AddTitle(NStr::TruncateSpaces(defline), bioseq);
        return;
    }

    s_AppendMods(rejected_mods, remainder);
    s_AppendMods(skipped_mods, remainder);
    // Add title string
    NStr::TruncateSpacesInPlace(remainder);
    x_AddTitle(remainder, bioseq);
}


void CAlnReader::x_AddTitle(const string& title, CBioseq& bioseq) 
{
    if (NStr::IsBlank(title)) {
        return;
    }
    auto pDesc = Ref(new CSeqdesc());
    pDesc->SetTitle() = title;
    bioseq.SetDescr().Set().push_back(move(pDesc));
}


void CAlnReader::ParseDefline(const string& defline, 
    const SDeflineParseInfo& info,
    const TIgnoredProblems& ignoredErrors, 
    list<CRef<CSeq_id>>& ids, 
    bool& hasRange, 
    TSeqPos& rangeStart,
    TSeqPos& rangeEnd,
    TSeqTitles& seqTitles,
    ILineErrorListener* pMessageListener)
{
    CFastaDeflineReader::ParseDefline(
        defline, 
        info,
        ignoredErrors, 
        ids, 
        hasRange, 
        rangeStart, 
        rangeEnd,
        seqTitles, 
        pMessageListener);
}

END_NCBI_SCOPE

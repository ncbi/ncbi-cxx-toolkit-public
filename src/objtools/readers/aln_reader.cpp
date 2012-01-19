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
#include <objtools/readers/reader_exception.hpp>
#include <objtools/error_codes.hpp>
#include <util/creaders/alnread.h>
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


#define NCBI_USE_ERRCODE_X   Objtools_Rd_Align

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


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
    m_Message = e.GetMessage();
}


static char * ALIGNMENT_CALLBACK s_ReadLine(void *user_data)
{
    CNcbiIstream *is = static_cast<CNcbiIstream *>(user_data);
    if (!*is) {
        return 0;
    }
    string s;
    NcbiGetline(*is, s, "\n");
    NStr::ReplaceInPlace (s, "\r", "");
    return strdup(s.c_str());
}


static void ALIGNMENT_CALLBACK s_ReportError(TErrorInfoPtr err_ptr,
                                             void *user_data)
{
    CAlnReader::TErrorList *err_list;
    TErrorInfoPtr           next_err;
    
    while (err_ptr != NULL) {    
        if (user_data != NULL) {
            err_list = (CAlnReader::TErrorList *)user_data;
            (*err_list).push_back (CAlnError(err_ptr->category, err_ptr->line_num, 
                                             err_ptr->id == NULL ? "" : err_ptr->id, 
                                             err_ptr->message == NULL ? "" : err_ptr->message));
        }
        
        string msg = "Error reading alignment file";
        if (err_ptr->line_num > -1) {
            msg += " at line " + NStr::IntToString(err_ptr->line_num);
        }
        if (err_ptr->message) {
            msg += ":  ";
            msg += err_ptr->message;
        }

        if (err_ptr->category == eAlnErr_Fatal) {
            LOG_POST_X(1, Error << msg);
        } else {
            LOG_POST_X(1, Info << msg);
        }

        next_err = err_ptr->next;  
        free (err_ptr->id);
        free (err_ptr->message);
        free (err_ptr);
        err_ptr = next_err;
    }
}


CAlnReader::~CAlnReader()
{
}


void CAlnReader::Read(bool guess)
{
    if (m_ReadDone) {
        return;
    }

    // make a SSequenceInfo corresponding to our CSequenceInfo argument
    SSequenceInfo info;
    info.alphabet      = const_cast<char *>(m_Alphabet.c_str());
    info.beginning_gap = const_cast<char *>(m_BeginningGap.c_str());
    info.end_gap       = const_cast<char *>(m_EndGap.c_str());;
    info.middle_gap    = const_cast<char *>(m_MiddleGap.c_str());
    info.missing       = const_cast<char *>(m_Missing.c_str());
    info.match         = const_cast<char *>(m_Match.c_str());

    // read the alignment stream
    TAlignmentFilePtr afp;
    m_Errors.clear();
    afp = ReadAlignmentFile(s_ReadLine, (void *) &m_IS,
                            s_ReportError, &(m_Errors), &info);
    if (!afp) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                   "Error reading alignment", 0);
    }
    
    int first_len = strlen (afp->sequences[0]);
    for (int i = 1; i < afp->num_sequences; i++) {
        if (strlen (afp->sequences[i]) != first_len) {
            AlignmentFileFree (afp);
            NCBI_THROW2(CObjReaderParseException, eFormat,
                       "Error reading alignment: Not all sequences have same length", 0);
        }
    }
    
    // if we're trying to guess whether this is an alignment file,
    // and no tell-tale alignment format lines were found,
    // check to see if any of the lines contain gaps.
    // no gaps plus no alignment indicators -> don't guess alignment
    if (guess && !afp->align_format_found) {
        bool found_gap = false;
        for (int i = 0; i < afp->num_sequences && !found_gap; i++) {
            if (strchr (afp->sequences[i], '-') != NULL) {
                found_gap = true;
            }
        }
        if (!found_gap) {
            AlignmentFileFree (afp);
            NCBI_THROW2(CObjReaderParseException, eFormat,
                       "Error reading alignment", 0);
        }
    }

    // build the CAlignment
    m_Seqs.resize(afp->num_sequences);
    m_Ids.resize(afp->num_sequences);
    for (int i = 0;  i < afp->num_sequences;  ++i) {
        m_Seqs[i] = afp->sequences[i];
        m_Ids[i] = afp->ids[i];
    }
    m_Organisms.resize(afp->num_organisms);
    for (int i = 0;  i < afp->num_organisms;  ++i) {
        if (afp->organisms[i]) {
            m_Organisms[i] = afp->organisms[i];
        } else {
            m_Organisms[i].erase();
        }
    }
    m_Deflines.resize(afp->num_deflines);
    for (int i = 0;  i < afp->num_deflines;  ++i) {
        if (afp->deflines[i]) {
            m_Deflines[i] = afp->deflines[i];
        } else {
            m_Deflines[i].erase();
        }
    }

    AlignmentFileFree(afp);

    {{
        m_Dim = m_Ids.size();
    }}

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


CRef<CSeq_align> CAlnReader::GetSeqAlign()
{
    if (m_Aln) {
        return m_Aln;
    } else if ( !m_ReadDone ) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                   "CAlnReader::GetSeqAlign(): "
                   "Seq_align is not available until after Read()", 0);
    }

    typedef CDense_seg::TNumseg TNumseg;
    typedef CDense_seg::TDim TNumrow;

    m_Aln = new CSeq_align();
    m_Aln->SetType(CSeq_align::eType_not_set);
    m_Aln->SetDim(m_Dim);

    CDense_seg& ds = m_Aln->SetSegs().SetDenseg();
    ds.SetDim(m_Dim);
    
    CDense_seg::TIds&     ids     = ds.SetIds();
    CDense_seg::TStarts&  starts  = ds.SetStarts();
    //CDense_seg::TStrands& strands = ds.SetStrands();
    CDense_seg::TLens&    lens    = ds.SetLens();

    ids.resize(m_Dim);

    // get the length of the alignment
    TSeqPos aln_stop = m_Seqs[0].size();
    for (TNumrow row_i = 1; row_i < m_Dim; row_i++) {
        if (m_Seqs[row_i].size() > aln_stop) {
            aln_stop = m_Seqs[row_i].size();
        }
    }

    for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
        CBioseq::TId xid;
        if (CSeq_id::ParseFastaIds(xid, m_Ids[row_i], true) > 0) {
            ids[row_i] = xid.front();
        } else {
            ids[row_i] = new CSeq_id(CSeq_id::e_Local, m_Ids[row_i]);
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
                if (NStr::Find(m_MiddleGap, residue) == string::npos  &&
                    NStr::Find(m_EndGap, residue) == string::npos  &&
                    NStr::Find(m_BeginningGap, residue) == string::npos) {

                    if (is_gap[row_i]) {
                        is_gap[row_i] = false;
                        new_seg = true;
                    }

                    // add to the sequence vector
                    m_SeqVec[row_i][m_SeqLen[row_i]++] = residue.c_str()[0];

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


CRef<CSeq_entry> CAlnReader::GetSeqEntry()
{
    if (m_Entry) {
        return m_Entry;
    } else if ( !m_ReadDone ) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                   "CAlnReader::GetSeqEntry(): "
                   "Seq_entry is not available until after Read()", 0);
    }
    m_Entry = new CSeq_entry();
    CRef<CSeq_annot> seq_annot (new CSeq_annot);
    seq_annot->SetData().SetAlign().push_back(GetSeqAlign());

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
        CSeq_id::ParseFastaIds(ids, m_Ids[row_i], true);
        if (ids.empty()) {
            ids.push_back(CRef<CSeq_id>(new CSeq_id(CSeq_id::e_Local,
                                                    m_Ids[row_i])));
        }

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
    
    
    return m_Entry;
}

END_NCBI_SCOPE

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

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);



static char * ALIGNMENT_CALLBACK s_ReadLine(void *user_data)
{
    CNcbiIstream *is = static_cast<CNcbiIstream *>(user_data);
    if (!*is) {
        return 0;
    }
    string s;
    NcbiGetlineEOL(*is, s);
    return strdup(s.c_str());
}


static void ALIGNMENT_CALLBACK s_ReportError(TErrorInfoPtr err_ptr,
                                             void *user_data)
{
    if (err_ptr->category != eAlnErr_Fatal) {
        // ignore non-fatal errors
        return;
    }
    string msg = "Error reading alignment file";
    if (err_ptr->line_num > -1) {
        msg += " at line " + NStr::IntToString(err_ptr->line_num);
    }
    if (err_ptr->message) {
        msg += ":  ";
        msg += err_ptr->message;
    }

    LOG_POST(Error << msg);
}


CAlnReader::~CAlnReader()
{
}


void CAlnReader::Read()
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
    afp = ReadAlignmentFile(s_ReadLine, (void *) &m_IS,
                            s_ReportError, 0, &info);
    if (!afp) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                   "Error reading alignment", 0);
    }

    // build the CAlignment
    m_Seqs.resize(afp->num_sequences);
    m_Ids.resize(afp->num_sequences);
    for (int i = 0;  i < afp->num_sequences;  ++i) {
        m_Seqs[i] = NStr::ToUpper(afp->sequences[i]);
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

    // get the length of the aln row, asuming all rows are the same
    TSeqPos aln_stop = m_Seqs[0].size();

    for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
        CRef<CSeq_id> seq_id(new CSeq_id(m_Ids[row_i]));
        if (seq_id->Which() == CSeq_id::e_not_set) {
            seq_id->SetLocal().SetStr(m_Ids[row_i]);
        }
        ids[row_i] = seq_id;
    }

    m_SeqVec.resize(m_Dim);
    for (TNumrow row_i = 0; row_i < m_Dim; row_i++) {
        m_SeqVec[row_i].resize(aln_stop, 0); // size unknown, resize to max
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
            const char& residue = m_Seqs[row_i][aln_pos];
            if (residue != m_MiddleGap[0]  &&
                residue != m_EndGap[0]  &&
                residue != m_Missing[0]) {

                if (is_gap[row_i]) {
                    is_gap[row_i] = false;
                    new_seg = true;
                }

                // add to the sequence vector
                m_SeqVec[row_i][m_SeqLen[row_i]++] = residue;

            } else {

                if ( !is_gap[row_i] ) {
                    is_gap[row_i] = true;
                    new_seg = true;
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

        // seq-id
        CRef<CSeq_id> seq_id(new CSeq_id(m_Ids[row_i]));
        seq_entry->SetSeq().SetId().push_back(seq_id);
        if (seq_id->Which() == CSeq_id::e_not_set) {
            seq_id->SetLocal().SetStr(m_Ids[row_i]);
        }

        // mol
        CSeq_inst::EMol mol   = CSeq_inst::eMol_not_set;
        CSeq_id::EAccessionInfo ai = seq_id->IdentifyAccession();
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

/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2005/04/26 17:28:12  dicuccio
 * Added Fasta + Gap API
 *
 * Revision 1.12  2005/03/07 20:05:42  todorov
 * Again, handle the special case when all sequences are gapped in the
 * first segment, but do not delete the segment, so that the original aln
 * coords are preserved.
 *
 * Revision 1.11  2005/03/07 18:46:28  todorov
 * Handle the special case when all sequences are gapped in the first segment.
 *
 * Revision 1.10  2004/05/21 21:42:55  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.9  2004/03/23 19:35:21  jcherry
 * Set class to pop-set rather than nuc-prot
 *
 * Revision 1.8  2004/03/02 20:18:50  jcherry
 * Don't throw in callback called by C code
 *
 * Revision 1.7  2004/03/01 15:26:33  dicuccio
 * Code clean-up.  Added enum for standard alphabets.  Added new APIs to set
 * standard parameters for other alignment types (implemented with unclear details
 * currently).  Added better exception handling.
 *
 * Revision 1.6  2004/02/24 17:52:12  jcherry
 * iupacaa for proteins
 *
 * Revision 1.5  2004/02/20 16:40:57  ucko
 * Fix path to aln_reader.hpp again
 *
 * Revision 1.4  2004/02/20 16:21:09  todorov
 * Better seq id; Try to id the mol type; Packed seq data;
 * Fixed a couple of bugs in GetSeqAlign
 *
 * Revision 1.3  2004/02/20 01:21:35  ucko
 * Again, erase() strings rather than clear()ing them for compatibility
 * with G++ 2.95.  (The fix seems to have gotten lost in the recent
 * move.)
 *
 * Revision 1.2  2004/02/19 18:38:13  ucko
 * Update path to aln_reader.hpp.
 *
 * Revision 1.1  2004/02/19 16:54:59  todorov
 * File moved from util/creaders and renamed to aln_reader
 *
 * Revision 1.4  2004/02/18 22:29:31  todorov
 * Converted to single class. Added methods for creating Seq-align and Seq-entry. A working version, but still need to polish: seq-ids, na/aa recognition, etc.
 *
 * Revision 1.3  2004/02/11 17:58:12  gorelenk
 * Added missed modificator ALIGNMENT_CALLBACK to definitions of s_ReadLine
 * and ALIGNMENT_CALLBACK.
 *
 * Revision 1.2  2004/02/10 02:58:09  ucko
 * erase() strings rather than clear()ing them for compatibility with G++ 2.95.
 *
 * Revision 1.1  2004/02/09 16:02:34  jcherry
 * Initial versionnnn
 *
 * ===========================================================================
 */


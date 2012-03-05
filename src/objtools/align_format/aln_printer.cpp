static char const rcsid[] = "$Id: ";

/*
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
* ===========================================================================*/

/*****************************************************************************

File name: aln_printer.cpp

Author: Greg Boratyn

Contents: Printer for standard multiple sequence alignmnet formats

******************************************************************************/

#include <ncbi_pch.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objtools/align_format/aln_printer.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(align_format);


// Relace non-alphanumeric characters with '_'
static void s_ReplaceNonAlphaNum(string& str)
{
    for (size_t i=0;i < str.length();i++) {
        if (!isalnum(str[i])) {
            str[i] = '_';
        }
    }
}

// Get sequence title from Bioseq
static string s_GetTitle(const CBioseq_Handle& bhandle)
{
    string retval;
    ITERATE(CSeq_descr::Tdata, it, bhandle.GetDescr().Get()) {
        if ((*it)->IsTitle()) {
            if (!retval.empty()) {
                retval += " ";
            }
            retval += (*it)->GetTitle();
        }
    }
    return retval;
}

// Get sequence label for Phylip formats, returns either sequence title or
// Seq-id
static string s_GetLabel(const CBioseq_Handle& bhandle)
{
    CConstRef<CSeq_id> id = bhandle.GetSeqId();

    // for local id with text content, return content
    if (id->IsLocal() && id->GetLocal().IsStr()) {
        string label;
        id->GetLabel(&label, CSeq_id::eContent);
        return label;
    }
    
    // otherwise return title or Seq-id if the title is empty
    string retval = s_GetTitle(bhandle);
    if (retval.empty()) {
        retval = id->AsFastaString();
    }

    return retval;
}


CMultiAlnPrinter::CMultiAlnPrinter(const CSeq_align& seqalign,
                                   CScope& scope,
                                   CMultiAlnPrinter::EAlignType type
                                   /* = eNotSet */)
    : m_AlnVec(new CAlnVec(seqalign.GetSegs().GetDenseg(), scope)),
      m_AlignType(type),
      m_Format(CMultiAlnPrinter::eFastaPlusGaps),
      m_Width(60)
{
    m_AlnVec->SetGapChar('-');
    m_AlnVec->SetEndChar('-');
    m_AlnVec->SetAaCoding(CSeq_data::e_Ncbieaa);
}


void CMultiAlnPrinter::Print(CNcbiOstream& ostr)
{
    switch (m_Format) {
    case eFastaPlusGaps:
        x_PrintFastaPlusGaps(ostr);
        break;

    case eClustal:
        x_PrintClustal(ostr);
        break;

    case ePhylipSequential:
        x_PrintPhylipSequential(ostr);
        break;

    case ePhylipInterleaved:
        x_PrintPhylipInterleaved(ostr);
        break;

    case eNexus:
        x_PrintNexus(ostr);
        break;
    }
}


void CMultiAlnPrinter::x_PrintFastaPlusGaps(CNcbiOstream& ostr)
{
    int num_seqs = m_AlnVec->GetNumRows();
    string seq;
    for (int i=0;i < num_seqs;i++) {
        CBioseq_Handle bhandle = m_AlnVec->GetScope().GetBioseqHandle(
                                                 m_AlnVec->GetSeqId(i),
                                                 CScope::eGetBioseq_All);

        ostr << ">";
        CConstRef<CSeq_id> id = bhandle.GetSeqId();
        // if Seq-id is local, then ...
        if (id->IsLocal()) {
            // ... for numeric ids print full Seq-id
            if (id->GetLocal().IsId()) {
                ostr << id->AsFastaString();
            }
            else {
                // ... for text ids, print only content
                string label;
                id->GetLabel(&label, CSeq_id::eContent);
                ostr << label;
            }
        }
        else {
            // for non-local Seq-ids, print all Seq-ids from Bioseq
            const vector<CSeq_id_Handle>& ids = bhandle.GetId();
            ITERATE (vector<CSeq_id_Handle>, it, ids) {
                ostr << it->GetSeqId()->AsFastaString();
                if (it + 1 != ids.end()) {
                    ostr << "|";
                }
            }
        }
        string title = s_GetTitle(bhandle);
        if (!title.empty()) {
            ostr << " " << title;
        }
        ostr << NcbiEndl;
        
        m_AlnVec->GetWholeAlnSeqString(i, seq);

        for (int j=0;j < (int)seq.length();j++) {
            if (j % m_Width == 0 && j != 0) {
                ostr << NcbiEndl;
            }
            ostr << seq[j];
        }
        ostr << NcbiEndl;
    }
}


void CMultiAlnPrinter::x_PrintClustal(CNcbiOstream& ostr)
{
        CAlnVecPrinter printer(*m_AlnVec, ostr);
        printer.ClustalStyle(m_Width);    
}


void CMultiAlnPrinter::x_PrintPhylipSequential(CNcbiOstream& ostr)
{
    int num_sequences = m_AlnVec->GetNumRows();
    // sequence title must be up to 10 characters long
    const unsigned int kSeqTitleWidth = 10;

    string sequence;
    m_AlnVec->GetWholeAlnSeqString(0, sequence);

    ostr << "  " << num_sequences << "   " << sequence.length() << NcbiEndl;


    for (int i=0;i < num_sequences;i++) {

        CBioseq_Handle bhandle = m_AlnVec->GetScope().GetBioseqHandle(
                                                 m_AlnVec->GetSeqId(i),
                                                 CScope::eGetBioseq_All);

        string seq_title = s_GetLabel(bhandle);
        // sequence title width must be 10
        if (seq_title.length() > kSeqTitleWidth) {
            seq_title.erase(kSeqTitleWidth - 1, seq_title.size() - 1);
        }
        s_ReplaceNonAlphaNum(seq_title);
        while (seq_title.length() < kSeqTitleWidth) {
            seq_title += " ";
        }
        ostr << seq_title;

        // if i == 0 the sequence is already retreaved
        if (i > 0) {
            m_AlnVec->GetWholeAlnSeqString(i, sequence);
        }

        unsigned int j = 0;
        for (j=0;j < sequence.length() && j < m_Width - kSeqTitleWidth;j++) {
            ostr << sequence[j];
        }
        for (;j < sequence.length();j++) {
            if ((j + kSeqTitleWidth) % m_Width == 0 && j != 0) {
                ostr << NcbiEndl;
            }
            ostr << sequence[j];
        }        
        ostr << NcbiEndl;    
    }
}

void CMultiAlnPrinter::x_PrintPhylipInterleaved(CNcbiOstream& ostr)
{
    int num_sequences = m_AlnVec->GetNumRows();
    int aln_width = m_AlnVec->GetAlnStop() + 1;
    // sequence title must be up to 10 characters long
    const unsigned int kSeqTitleWidth = 10;
    
    // print numer of sequences and width (number of charachets)
    // of the alignment
    ostr << "  " << num_sequences << "   " << aln_width << NcbiEndl;

    // print sequence title and the first portions of the sequences
    for (int i=0;i < num_sequences;i++) {

        CBioseq_Handle bhandle = m_AlnVec->GetScope().GetBioseqHandle(
                                                 m_AlnVec->GetSeqId(i),
                                                 CScope::eGetBioseq_All);

        string seq_title = s_GetLabel(bhandle);
        // the space for sequence title must be exactly 10 characters long
        if (seq_title.length() > kSeqTitleWidth) {
            seq_title.erase(kSeqTitleWidth - 1, seq_title.size() - 1);
        }
        s_ReplaceNonAlphaNum(seq_title);
        while (seq_title.length() < kSeqTitleWidth) {
            seq_title += " ";
        }
        ostr << seq_title;

        string seq;
        m_AlnVec->GetAlnSeqString(seq, i, CAlnMap::TSignedRange(0,
                        min(m_Width - (int)kSeqTitleWidth, aln_width)));
        ostr << seq << NcbiEndl;        
    }
    ostr << NcbiEndl;

    // print remaining portions of the sequences
    int from = m_Width - kSeqTitleWidth;
    while (from < (int)aln_width) {
        int to = min(from + m_Width, aln_width);
        for (int i=0;i < num_sequences;i++) {
            string seq;
            m_AlnVec->GetAlnSeqString(seq, i, CAlnMap::TSignedRange(from, to));
            ostr << seq << NcbiEndl;
        }
        ostr << NcbiEndl;
        from = to + 1;
    }
}

void CMultiAlnPrinter::x_PrintNexus(CNcbiOstream& ostr)
{
    if (m_AlignType == eNotSet) {
        NCBI_THROW(CException, eInvalid, "Alignment type must be set for the "
                   "Nexus format");
    }

    int num_sequences = m_AlnVec->GetNumRows();
    int last_pos = m_AlnVec->GetAlnStop(); /* alignment width - 1 */
    vector<string> seqids(num_sequences);
    int max_id_length = 0;
    for (int i=0;i < num_sequences;i++) {

        seqids[i] = m_AlnVec->GetSeqId(i).GetSeqIdString();
        if ((int)seqids[i].length() > max_id_length) {
            max_id_length = seqids[i].length();
        }
    }

    ostr << "#NEXUS" << NcbiEndl << NcbiEndl
         << "BEGIN DATA;" << NcbiEndl
         << "DIMENSIONS ntax=" << num_sequences << " nchar="
         << last_pos + 1 << ";"
         << NcbiEndl
         << "FORMAT datatype="
         << (m_AlignType == eNucleotide ? "dna" : "protein")
         << " gap=" << (char)m_AlnVec->GetGapChar(0)
         << " interleave;"
         << NcbiEndl
         << "MATRIX" << NcbiEndl;


    int from = 0;
    int seqid_width = max_id_length + 2;
    while (from < last_pos) {
        int to = min(from + m_Width, last_pos);
        for (int i=0;i < num_sequences;i++) {

            ostr << seqids[i];
            int margin = seqid_width - seqids[i].length();
            while (margin > 0) {
                ostr << " ";
                margin--;
            }

            string sequence;
            m_AlnVec->GetAlnSeqString(sequence, i,
                                      CAlnMap::TSignedRange(from, to));
            ostr << sequence << NcbiEndl;
        }
        ostr << NcbiEndl;
        from = to + 1;
    }
    ostr << ";" << NcbiEndl << "END;" << NcbiEndl;
}


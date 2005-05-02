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
#include <objtools/blast_format/tabular.hpp>
#include <objtools/blast_format/blastfmtutil.hpp>
#include <serial/iterator.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/** @addtogroup BlastFormat
 *
 * @{
 */

CBlastTabularInfo::CBlastTabularInfo(CNcbiOstream& ostr)
    : m_Ostream(ostr) 
{
}

CBlastTabularInfo::~CBlastTabularInfo()
{
    m_Ostream.flush();
}

void CBlastTabularInfo::SetQueryId(list<CRef<CSeq_id> >& id)
{
    m_QueryId = NcbiEmptyString;
    ITERATE(list<CRef<CSeq_id> >, itr, id) {
        m_QueryId += (*itr)->AsFastaString() + "|";
    }
    if (m_QueryId.size() > 0)
        m_QueryId.erase(m_QueryId.size() - 1);
}

static string 
s_GetTitleFirstToken(const CBioseq_Handle& bh)
{
    string id_token = NcbiEmptyString;
    string title = sequence::GetTitle(bh).c_str();

    if (title != NcbiEmptyString) {
        string::size_type pos = title.find(" ");
        if (pos == string::npos)
            pos = title.size();
        id_token = title.substr(0, pos);
    }
    return id_token;
}

void CBlastTabularInfo::SetQueryId(const CBioseq_Handle& bh)
{
    m_QueryId = NcbiEmptyString;
    ITERATE(CBioseq_Handle::TId, itr, bh.GetId()) {
        string id_token;
        // Local ids are usually fake. If a title exists, use the first token
        // of the title instead of the local id. If no title, use the local
        // id, but without the "lcl|" prefix.
        if (itr->GetSeqId()->IsLocal()) {
            if ((id_token = s_GetTitleFirstToken(bh)) == NcbiEmptyString) {
                const CObject_id& obj_id = itr->GetSeqId()->GetLocal();
                if (obj_id.IsStr())
                    id_token = obj_id.GetStr();
                else 
                    id_token = NStr::IntToString(obj_id.GetId());
            }
        } else {
            id_token = itr->AsString();
        }
        m_QueryId += id_token + "|";
    }
    if (m_QueryId.size() > 0)
        m_QueryId.erase(m_QueryId.size() - 1);
}

void CBlastTabularInfo::SetSubjectId(list<CRef<CSeq_id> >& id)
{
    m_SubjectId = NcbiEmptyString;
    ITERATE(list<CRef<CSeq_id> >, itr, id) {
        m_SubjectId += (*itr)->AsFastaString() + "|";
    }
    if (m_SubjectId.size() > 0)
        m_SubjectId.erase(m_SubjectId.size() - 1);
}

void CBlastTabularInfo::SetSubjectId(const CBioseq_Handle& bh)
{
    m_SubjectId = NcbiEmptyString;
    ITERATE(CBioseq_Handle::TId, itr, bh.GetId()) {
        string id_token = NcbiEmptyString;
        // Check for ids of type "gnl|BL_ORD_ID". These are the artificial ids
        // created in a BLAST database when it is formatted without indexing.
        // For such ids, use first token of the title, if it's available.
        if (!itr->GetSeqId()->IsGeneral() || 
            itr->GetSeqId()->AsFastaString().find("gnl|BL_ORD_ID") == 
            string::npos ||
            ((id_token = s_GetTitleFirstToken(bh)) == NcbiEmptyString))
            id_token = itr->AsString();
        m_SubjectId += id_token + "|";
    }
    if (m_SubjectId.size() > 0)
        m_SubjectId.erase(m_SubjectId.size() - 1);
}

int CBlastTabularInfo::SetFields(const CSeq_align& align, CScope& scope)
{
    const int kQueryRow = 0;
    const int kSubjectRow = 1;

    int score, num_ident;
    double bit_score;
    double evalue;
    int sum_n;
    list<int> use_this_gi;

    bool query_is_na, subject_is_na;
    // Extract the full list of subject ids
    try {
        const CBioseq_Handle& query_bh = 
            scope.GetBioseqHandle(align.GetSeq_id(0));
        SetQueryId(query_bh);
        query_is_na = query_bh.IsNa();
        const CBioseq_Handle& subject_bh = 
            scope.GetBioseqHandle(align.GetSeq_id(1));
        SetSubjectId(subject_bh);
        subject_is_na = subject_bh.IsNa();
    } catch (const CException&) {
        // Either query or subject sequence not found - skip the remainder of 
        // the set up, and return error status.
        return -1;
    }

    CBlastFormatUtil::GetAlnScores(align, score, bit_score, evalue, sum_n, 
                                   num_ident, use_this_gi);
    SetScores(score, bit_score, evalue);
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
        finalAln = CBlastFormatUtil::CreateDensegFromDendiag(align);
    }

    const CDense_seg& ds = (finalAln ? finalAln->GetSegs().GetDenseg() :
                            align.GetSegs().GetDenseg());

    CAlnVec alnVec(ds, scope);
    
    int align_length, num_gaps, num_gap_opens;
    CBlastFormatUtil::GetAlignLengths(alnVec, align_length, num_gaps, 
                                      num_gap_opens);

    // Do not trust the identities count in the Seq-align, because if masking 
    // was used, then masked residues were not counted as identities. 
    // Hence retrieve the sequences present in the alignment and count the 
    // identities again.
    alnVec.SetGapChar('-');
    alnVec.GetWholeAlnSeqString(0, m_QuerySeq);
    alnVec.GetWholeAlnSeqString(1, m_SubjectSeq);

    _ASSERT(m_QuerySeq.size() == m_SubjectSeq.size());
    num_ident = 0;
    for (unsigned int i = 0; i < m_QuerySeq.size(); ++i) {
        if (m_QuerySeq[i] == m_SubjectSeq[i])
            ++num_ident;
    }

    SetCounts(num_ident, align_length, num_gaps, num_gap_opens);

    int q_start, q_end, s_start, s_end;

    // For translated search, for a negative query frame, reverse its start and
    // end offsets.
    if (kTranslated && ds.GetSeqStrand(kQueryRow) == eNa_strand_minus) {
        q_start = alnVec.GetSeqStop(kQueryRow) + 1;
        q_end = alnVec.GetSeqStart(kQueryRow) + 1;
    } else {
        q_start = alnVec.GetSeqStart(kQueryRow) + 1;
        q_end = alnVec.GetSeqStop(kQueryRow) + 1;
    }

    // If subject is on a reverse strand, reverse its start and end offsets.
    // Also do that for a nucleotide-nucleotide search, if query is on the 
    // reverse strand, because BLAST output always reverses subject, not query.
    if (ds.GetSeqStrand(kSubjectRow) == eNa_strand_minus ||
        (!kTranslated && ds.GetSeqStrand(kQueryRow) == eNa_strand_minus)) {
        s_end = alnVec.GetSeqStart(kSubjectRow) + 1;
        s_start = alnVec.GetSeqStop(kSubjectRow) + 1;
    } else {
        s_start = alnVec.GetSeqStart(kSubjectRow) + 1;
        s_end = alnVec.GetSeqStop(kSubjectRow) + 1;
    }

    SetEndpoints(q_start, q_end, s_start, s_end);

    return 0;
}


void CBlastTabularInfo::Print(ETabularOption opt) 
{
    string evalue_str;
    string bit_score_str;

    CBlastFormatUtil::GetScoreString(m_Evalue, m_BitScore, 
                                     evalue_str, bit_score_str);
         
    /* Calculate percentage of identities */
    double perc_ident = ((double)m_NumIdent)/m_AlignLength * 100;
    Int4 num_mismatches = m_AlignLength - m_NumIdent - m_NumGaps;
    
    m_Ostream << m_QueryId << "\t" << m_SubjectId << "\t" <<
        NStr::DoubleToString(perc_ident, 2) << 
        "\t" << m_AlignLength << "\t" << num_mismatches << "\t" << 
        m_NumGapOpens << "\t" << m_QueryStart << "\t" << m_QueryEnd << "\t" << 
        m_SubjectStart << "\t" << m_SubjectEnd << "\t" << evalue_str << "\t" <<
        bit_score_str;

    if (opt & eAddSeqs) {
        m_Ostream << "\t" << m_QuerySeq << "\t" << m_SubjectSeq;
    }

    if (opt & eAddRawScore)
        m_Ostream << "\t" << m_Score;
    if (opt & eAddGapCount)
        m_Ostream << "\t" << m_NumGaps;
    m_Ostream << endl;
}

void 
CBlastTabularInfo::PrintHeader(const string& program_in, const CBioseq& bioseq, 
                               const string& dbname, int iteration, 
                               ETabularOption opt)
{
    m_Ostream << "# ";
    string program(program_in);
    CBlastFormatUtil::BlastPrintVersionInfo(NStr::ToUpper(program), false, 
                                            m_Ostream);

    if (iteration > 0)
        m_Ostream << "# Iteration: " << iteration << endl;

    // Print the query defline with no html; there is no need to set the 
    // line length restriction, since it's ignored for the tabular case.
    CBlastFormatUtil::AcknowledgeBlastQuery(bioseq, 0, m_Ostream, false, false,
                                            true);
    
    m_Ostream << endl << "# Database: " << dbname << endl;
    m_Ostream << "# Fields: Query id, Subject id, % identity, alignment length,"
        " mismatches, gap openings, q. start, q. end, s. start, s. end, "
        "e-value, bit score";
    if (opt & eAddSeqs)
        m_Ostream << ", query seq., subject seq.";
    if (opt & eAddRawScore)
        m_Ostream << ", score";
    if (opt & eAddGapCount)
        m_Ostream << ", total gaps";
    m_Ostream << endl;
    
}


END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2005/05/02 17:36:23  dondosha
* 1. Return error from SetFields if Bioseq handle cannot be found for query or subject;
* 2. Added extra logic for local query ids and BL_ORD_ID database ids.
* 3. Convert Dense-diag to Denseg for ungapped search.
*
* Revision 1.3  2005/04/28 19:29:28  dondosha
* Changed CBioseq_Handle argument in PrintHeader to CBioseq, needed for web formatting; do not rely on Seq-align in determining number of identities
*
* Revision 1.2  2005/03/14 21:12:29  ucko
* Print: use DoubleToString rather than sprintf, which hasn't
* necessarily been declared.
*
* Revision 1.1  2005/03/14 20:11:30  dondosha
* Tabular formatting of BLAST results
*
*
* ===========================================================================
*/

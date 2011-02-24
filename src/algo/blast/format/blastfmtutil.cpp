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
 * Author:  Jian Ye
 * 12/2004
 * File Description:
 *   blast formatter utilities
 *
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>

#include <algo/blast/format/blastfmtutil.hpp>
#include <algo/blast/api/pssm_engine.hpp>   // for CScorematPssmConverter
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmParameters.hpp>

#include <stdio.h>
#include <sstream>
#include <iomanip>

#include <objects/seq/seqport_util.hpp>

#include <algo/blast/api/blast_types.hpp>   // for CScorematPssmConverter
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(ncbi);
USING_SCOPE(objects);
USING_SCOPE(align_format);

string CBlastFormatUtil::BlastGetVersion(const string program)
{
    string program_uc = program;
    return NStr::ToUpper(program_uc) + " " + blast::CBlastVersion().Print();
}

void CBlastFormatUtil::BlastPrintVersionInfo(const string program, bool html, 
                                             CNcbiOstream& out)
{
    if (html)
        out << "<b>" << BlastGetVersion(program) << "</b>" << "\n";
    else
        out << BlastGetVersion(program) << "\n";
}

void 
CBlastFormatUtil::BlastPrintReference(bool html, size_t line_len, 
                                      CNcbiOstream& out, 
                                      blast::CReference::EPublication pub,
                                      bool is_psiblast /* = false */) 
{
    string reference("Reference");
    if (pub == blast::CReference::eCompAdjustedMatrices) {
        reference += " for compositional score matrix adjustment";
    } else if (pub == blast::CReference::eCompBasedStats) {
        reference += " for composition-based statistics";
        if (is_psiblast) { 
            reference += " starting in round 2";
        }
    } else if (pub == blast::CReference::eIndexedMegablast) {
        reference += " for database indexing";
    }

    ostringstream str;
    if(html)
    {
        str << "<b><a href=\""
            << blast::CReference::GetPubmedUrl(pub)
            << "\">" << reference << "</a>:</b>"
            << "\n";
        x_WrapOutputLine(str.str() + blast::CReference::GetString(pub), 
                   line_len, out);
    }
    else
    {
        str << reference << ": ";
        x_WrapOutputLine(str.str() + blast::CReference::GetHTMLFreeString(pub), 
                   line_len, out);
    }

    out << "\n";
}

void CBlastFormatUtil::PrintDbInformation(size_t line_len,
                                          string definition_line, 
                                          int nNumSeqs, 
                                          Uint8 nTotalLength,
                                          bool html,                                                                                       
                                          bool with_links,
                                          CNcbiOstream& out)
                                      
                                      
{
    ostringstream str;
    string dbString = (html) ? "<b>Database:</b> " : "Database: ";
    str << dbString << definition_line << endl;
    if(!(html && with_links)) x_WrapOutputLine(str.str(),line_len, out);

    out << "           " << NStr::IntToString(nNumSeqs,NStr::fWithCommas) << " sequences; " << NStr::IntToString(nTotalLength,NStr::fWithCommas) << " total letters" << endl;
}

/** Standard order of letters according to S. Altschul
 * FIXME: Move to blast_encoding.[hc] ?
 */
static int RESIDUE_ORDER[] = {
     1,     /* A */
    16,     /* R */
    13,     /* N */
     4,     /* D */ 
     3,     /* C */
    15,     /* Q */
     5,     /* E */ 
     7,     /* G */
     8,     /* H */
     9,     /* I */
    11,     /* L */
    10,     /* K */
    12,     /* M */  
     6,     /* F */
    14,     /* P */
    17,     /* S */
    18,     /* T */
    20,     /* W */
    22,     /* Y */
    19      /* V */
};

typedef CNcbiMatrix<int> TNcbiMatrixInt;
typedef CNcbiMatrix<double> TNcbiMatrixDouble;

void 
CBlastFormatUtil::PrintAsciiPssm
    (const objects::CPssmWithParameters& pssm_with_params, 
     CConstRef<blast::CBlastAncillaryData> ancillary_data, 
     CNcbiOstream& out)
{
    _ASSERT(ancillary_data.NotEmpty());
    static const Uint1 kXResidue = AMINOACID_TO_NCBISTDAA[(int)'X'];
    vector<double> info_content, gapless_col_weights, sigma;
    blast::CScorematPssmConverter::GetInformationContent(pssm_with_params, 
                                                         info_content);
    blast::CScorematPssmConverter::GetGaplessColumnWeights(pssm_with_params, 
                                                           gapless_col_weights);
    blast::CScorematPssmConverter::GetSigma(pssm_with_params, sigma);

    // We use whether the information content is available to assume whether
    // the PSSM computation was done or not
    bool pssm_calculation_done = info_content.empty() ? false : true;

    if (pssm_calculation_done) {
        out << "\nLast position-specific scoring matrix computed, weighted ";
        out << "observed percentages rounded down, information per position, ";
        out << "and relative weight of gapless real matches to pseudocounts\n";
    } else {
        out << "\nLast position-specific scoring matrix computed\n";
    }

    out << "         ";
    // print the header for the last PSSM computed
    for (size_t c = 0; c < DIM(RESIDUE_ORDER); c++) {
        out << "  " << NCBISTDAA_TO_AMINOACID[RESIDUE_ORDER[c]];
    }
    if (pssm_calculation_done) {
        // print the header for the weigthed observed percentages
        for (size_t c = 0; c < DIM(RESIDUE_ORDER); c++) {
            out << "   " << NCBISTDAA_TO_AMINOACID[RESIDUE_ORDER[c]];
        }
    }

    // will need psiblast statistics: posCount, intervalSizes,
    // sigma,
    // posCounts can be calculated from residue_frequencies

    const SIZE_TYPE kQueryLength = pssm_with_params.GetPssm().GetQueryLength();
    _ASSERT(kQueryLength == 
            (SIZE_TYPE)pssm_with_params.GetPssm().GetNumColumns());
    const double kPseudoCount = 
        (pssm_with_params.CanGetParams() &&
         pssm_with_params.GetParams().CanGetPseudocount()) 
        ? pssm_with_params.GetParams().GetPseudocount() : 0.0;
    auto_ptr< TNcbiMatrixInt > pssm
        (blast::CScorematPssmConverter::GetScores(pssm_with_params));
    auto_ptr< TNcbiMatrixDouble > weighted_res_freqs
        (blast::CScorematPssmConverter::
            GetWeightedResidueFrequencies(pssm_with_params));
    vector<int> interval_sizes, num_matching_seqs;
    blast::CScorematPssmConverter::GetIntervalSizes(pssm_with_params,
                                                    interval_sizes);
    blast::CScorematPssmConverter::GetNumMatchingSeqs(pssm_with_params,
                                                      num_matching_seqs);

    CNCBIstdaa query;
    pssm_with_params.GetPssm().GetQuerySequenceData(query);
    const vector<char>& query_seq = query.Get();

    out << fixed;
    for (SIZE_TYPE i = 0; i < kQueryLength; i++) {
        // print the residue for position i
        out << "\n" << setw(5) << (i+1) << " " <<
            NCBISTDAA_TO_AMINOACID[(int)query_seq[i]] << "  ";

        // print the PSSM
        for (SIZE_TYPE c = 0; c < DIM(RESIDUE_ORDER); c++) {
            if ((*pssm)(RESIDUE_ORDER[c], i) == BLAST_SCORE_MIN) {
                out << "-I ";
            } else {
                out << setw(3) << (*pssm)(RESIDUE_ORDER[c], i);
            }
        }
        out << " ";

        if (pssm_calculation_done) {
            // Print the weighted observed
            for (SIZE_TYPE c = 0; c < DIM(RESIDUE_ORDER); c++) {
                if ((*pssm)(RESIDUE_ORDER[c], i) != BLAST_SCORE_MIN) {
                    double value = 100;
                    value *= (*weighted_res_freqs)(RESIDUE_ORDER[c], i);
                    // round to the nearest integer
                    value = (int)(value + (value > 0. ? 0.5 : -0.5));
                    out << setw(4) << (int)value;
                }
            }

            // print the information content
            out << "  " << setprecision(2) << info_content[i] << " ";

            // print the relative weight of gapless real matches to pseudocounts
            if ((num_matching_seqs[i] > 1) && (query_seq[i] != kXResidue)) {
                double val = gapless_col_weights[i]/kPseudoCount;
                val *= (sigma[i] / interval_sizes[i] - 1);
                out << setprecision(2) << val;
            } else {
                out << "    0.00";
            }
        }
    }

    const Blast_KarlinBlk* ungapped_kbp =
        ancillary_data->GetUngappedKarlinBlk();
    const Blast_KarlinBlk* gapped_kbp = 
        ancillary_data->GetGappedKarlinBlk();
    const Blast_KarlinBlk* psi_ungapped_kbp =
        ancillary_data->GetPsiUngappedKarlinBlk();
    const Blast_KarlinBlk* psi_gapped_kbp = 
        ancillary_data->GetPsiGappedKarlinBlk();
    out << "\n\n" << setprecision(4);
    out << "                      K         Lambda\n";
    if (ungapped_kbp) {
        out << "Standard Ungapped    "
            << ungapped_kbp->K << "     "
            << ungapped_kbp->Lambda << "\n";
    }
    if (gapped_kbp) {
        out << "Standard Gapped      "
            << gapped_kbp->K << "     "
            << gapped_kbp->Lambda << "\n";
    }
    if (psi_ungapped_kbp) {
        out << "PSI Ungapped         "
            << psi_ungapped_kbp->K << "     "
            << psi_ungapped_kbp->Lambda << "\n";
    }
    if (psi_gapped_kbp) {
        out << "PSI Gapped           "
            << psi_gapped_kbp->K << "     "
            << psi_gapped_kbp->Lambda << "\n";
    }
}


CRef<objects::CSeq_annot>
CBlastFormatUtil::CreateSeqAnnotFromSeqAlignSet(CConstRef<objects::CSeq_align_set> alnset,
												const string & program)
{
    _ASSERT(alnset.NotEmpty());
    CRef<CSeq_annot> retval(new CSeq_annot);

    CRef<CUser_object> hist_align_obj(new CUser_object);
    static const string kHistSeqalign("Hist Seqalign");
    hist_align_obj->SetType().SetStr(kHistSeqalign);
    hist_align_obj->AddField(kHistSeqalign, true);
    retval->AddUserObject(*hist_align_obj);

    CRef<CUser_object> blast_type(new CUser_object);
    static const string kBlastType("Blast Type");
    blast_type->SetType().SetStr(kBlastType);
    blast_type->AddField(program, blast::ProgramNameToEnum(program));
    retval->AddUserObject(*blast_type);

    ITERATE(CSeq_align_set::Tdata, itr, alnset->Get()) {
        retval->SetData().SetAlign().push_back(*itr);
    }

    return retval;
}

CBlastFormattingMatrix::CBlastFormattingMatrix(int** data, unsigned int nrows, 
                                               unsigned int ncols)
{
    const int kAsciiSize = 256;
    Resize(kAsciiSize, kAsciiSize, INT_MIN);
    
    // Create a CSeq_data object from a vector of values from 0 to the size of
    // the matrix (26).
    const int kNumValues = max(ncols, nrows);
    vector<char> ncbistdaa_values(kNumValues);
    for (int index = 0; index < kNumValues; ++index)
        ncbistdaa_values[index] = (char) index;

    CSeq_data ncbistdaa_seq(ncbistdaa_values, CSeq_data::e_Ncbistdaa);

    // Convert to IUPACaa using the CSeqportUtil::Convert method.
    CSeq_data iupacaa_seq;
    CSeqportUtil::Convert(ncbistdaa_seq, &iupacaa_seq, CSeq_data::e_Iupacaa);
    
    // Extract the IUPACaa values
    vector<char> iupacaa_values(kNumValues);
    for (int index = 0; index < kNumValues; ++index)
        iupacaa_values[index] = iupacaa_seq.GetIupacaa().Get()[index];

    // Fill the 256x256 output matrix.
    for (unsigned int row = 0; row < nrows; ++row) {
        for (unsigned int col = 0; col < ncols; ++col) {
            if (iupacaa_values[row] >= 0 && iupacaa_values[col] >= 0) {
                (*this)((int)iupacaa_values[row], (int)iupacaa_values[col]) = 
                    data[row][col];
            }
        }
    }
}


END_NCBI_SCOPE

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
* Authors:  Paul Thiessen
*
* File Description:
*      new C++ PSSM construction
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>

#include <algo/blast/api/pssm_input.hpp>
#include <algo/blast/api/blast_psi.hpp>
#include <algo/blast/api/blast_aux.hpp>

#include <objects/scoremat/scoremat__.hpp>

// conflicts between algo/blast stuff and C-toolkit stuff
#undef INT2_MIN
#undef INT2_MAX

#include "cn3d_pssm.hpp"
#include "block_multiple_alignment.hpp"
#include "sequence_set.hpp"
#include "cn3d_tools.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);


BEGIN_SCOPE(Cn3D)

#define PTHROW(stream) NCBI_THROW(CException, eUnknown, stream)

class Cn3DPSSMInput : public IPssmInputData
{
private:
    const BlockMultipleAlignment *bma;
    unsigned char *masterNCBIStdaa;
    unsigned int masterLength;

    PSIMsa *data;
    PSIBlastOptions options;
    PSIDiagnosticsRequest diag;

public:
    Cn3DPSSMInput(const BlockMultipleAlignment *b);
    ~Cn3DPSSMInput(void);

    // IPssmInputData required functions
    void Process(void) { }  // all done in c'tor
    unsigned char * GetQuery(void) { return masterNCBIStdaa; }
    unsigned int GetQueryLength(void) { return masterLength; }
    PSIMsa * GetData(void) { return data; }
    const PSIBlastOptions* GetOptions(void) { return &options; }
    const char * GetMatrixName(void) { return "BLOSUM62"; }
    const PSIDiagnosticsRequest * GetDiagnosticsRequest(void) { return &diag; }
};

static const unsigned char gap = LookupNCBIStdaaNumberFromCharacter('-');

static void FillInAlignmentData(const BlockMultipleAlignment *bma, PSIMsa *data)
{
    if (data->dimensions->query_length != bma->GetMaster()->Length() || data->dimensions->num_seqs != bma->NRows() - 1)
        PTHROW("FillInAlignmentData() - data array size mismatch");

    BlockMultipleAlignment::ConstBlockList blocks;
    bma->GetBlocks(&blocks);

    unsigned int b, row, column, masterStart, masterWidth, slaveStart, slaveWidth, left, right, middle;
    const Block::Range *range;
    const Sequence *seq;

    for (b=0; b<blocks.size(); ++b) {
        const Block& block = *(blocks[b]);

        seq = bma->GetSequenceOfRow(0);
        range = block.GetRangeOfRow(0);
        if (range->from < 0 || range->from > seq->Length() || range->to < -1 || range->to >= seq->Length() ||
                range->to < range->from - 1 ||
                range->from != ((b == 0) ? 0 : (blocks[b - 1]->GetRangeOfRow(0)->to + 1)) ||
                (b == blocks.size() - 1 && range->to != seq->Length() - 1))
            PTHROW("FillInAlignmentData() - master range error");
        masterStart = range->from;
        masterWidth = range->to - range->from + 1;
//        if (masterWidth == 0)
//            continue;

        for (row=0; row<bma->NRows(); row++) {
            seq = bma->GetSequenceOfRow(row);
            range = block.GetRangeOfRow(row);
            if (range->from < 0 || range->from > seq->Length() || range->to < -1 || range->to >= seq->Length() ||
                    range->to < range->from - 1 ||
                    range->from != ((b == 0) ? 0 : (blocks[b - 1]->GetRangeOfRow(row)->to + 1)) ||
                    (b == blocks.size() - 1 && range->to != seq->Length() - 1))
                PTHROW("FillInAlignmentData() - slave range error");
            slaveStart = range->from;
            slaveWidth = range->to - range->from + 1;

            for (column=0; column<masterWidth; ++column) {
                PSIMsaCell& cell = data->data[row][masterStart + column];

                // same # residues in both master and slave
                if (slaveWidth == masterWidth) {
                    cell.letter = LookupNCBIStdaaNumberFromCharacter(seq->sequenceString[slaveStart + column]);
                    cell.is_aligned = true;
                }

                // left tail
                else if (b == 0) {
                    // truncate left end of sequence
                    if (slaveWidth > masterWidth) {
                        cell.letter = LookupNCBIStdaaNumberFromCharacter(seq->sequenceString[slaveWidth - masterWidth + column]);
                        cell.is_aligned = true;
                    } else {
                        // residues to the right
                        if (column >= masterWidth - slaveWidth) {
                            cell.letter = LookupNCBIStdaaNumberFromCharacter(seq->sequenceString[column - (masterWidth - slaveWidth)]);
                            cell.is_aligned = true;
                        }
                        // pad left with unaligned gaps
                        else {
                            cell.letter = gap;
                            cell.is_aligned = false;
                        }
                    }
                }

                // right tail
                else if (b == blocks.size() - 1) {
                    // truncate right end of sequence
                    if (slaveWidth > masterWidth) {
                        cell.letter = LookupNCBIStdaaNumberFromCharacter(seq->sequenceString[slaveStart + column]);
                        cell.is_aligned = true;
                    } else {
                        // residues to the left
                        if (column < slaveWidth) {
                            cell.letter = LookupNCBIStdaaNumberFromCharacter(seq->sequenceString[slaveStart + column]);
                            cell.is_aligned = true;
                        }
                        // pad left with unaligned gaps
                        else {
                            cell.letter = gap;
                            cell.is_aligned = false;
                        }
                    }
                }

                // more residues in master than slave: split and pad middle with (aligned) gaps
                else if (slaveWidth < masterWidth) {
                    if (column == 0) {
                        left = (slaveWidth + 1) / 2;    // +1 means left gets more in uneven split
                        middle = masterWidth - slaveWidth;
                        right = slaveWidth - left;
                    }
                    if (column < left)
                        cell.letter = LookupNCBIStdaaNumberFromCharacter(seq->sequenceString[slaveStart + column]);
                    else if (column < left + middle)
                        cell.letter = gap;
                    else
                        cell.letter = LookupNCBIStdaaNumberFromCharacter(seq->sequenceString[slaveStart + column - middle]);
                    cell.is_aligned = true;             // even gaps are aligned in these regions
                }

                // more residues in slave than master: truncate middle of slave
                else {  // slaveWidth > masterWidth
                    if (column == 0) {
                        left = (masterWidth + 1) / 2;   // +1 means left gets more in uneven split
                        right = masterWidth - left;
                    }
                    if (column < left)
                        cell.letter = LookupNCBIStdaaNumberFromCharacter(seq->sequenceString[slaveStart + column]);
                    else
                        cell.letter = LookupNCBIStdaaNumberFromCharacter(
                            seq->sequenceString[slaveStart + slaveWidth - masterWidth + column]);
                }
            }
        }
    }

    CNcbiOfstream ofs("psimsa.txt", IOS_BASE::out);
    if (ofs) {
        ofs << "num_seqs: " << data->dimensions->num_seqs << ", query_length: " << data->dimensions->query_length << '\n';
        for (row=0; row<=data->dimensions->num_seqs; ++row) {
            for (column=0; column<data->dimensions->query_length; ++column)
                ofs << LookupCharacterFromNCBIStdaaNumber(data->data[row][column].letter);
            ofs << '\n';
        }
    }
}

static double CalculateInformationContent(const PSIMsa *data)
{
    double infoContent = 0.0;

    typedef map < unsigned char, unsigned int > ColumnProfile;
    ColumnProfile profile;
    ColumnProfile::iterator p, pe;
    unsigned int nRes;

    for (unsigned int c=0; c<data->dimensions->query_length; ++c) {

        profile.clear();
        nRes = 0;

        // build profile
        for (unsigned int r=0; r<=data->dimensions->num_seqs; ++r) {    // always include master row for now
            p = profile.find(data->data[r][c].letter);                  // and include gaps for now
            if (p == profile.end())
                profile[data->data[r][c].letter] = 1;
            else
                ++(p->second);
            ++nRes;
        }

        // do info content
        static const double ln2 = log(2.0), threshhold = 0.0001;
        for (p=profile.begin(), pe=profile.end(); p!=pe; ++p) {
            if (p->first != gap) {
                double expFreq = GetStandardProbability(LookupCharacterFromNCBIStdaaNumber(p->first));
                if (expFreq > threshhold) {
                    double obsFreq = 1.0 * p->second / nRes,
                          freqRatio = obsFreq / expFreq;
                    if (freqRatio > threshhold)
                        infoContent += obsFreq * log(freqRatio) / ln2;
                }
            }
        }
    }

    TRACEMSG("information content: " << infoContent);
    return infoContent;
}

Cn3DPSSMInput::Cn3DPSSMInput(const BlockMultipleAlignment *b) : bma(b)
{
    TRACEMSG("Creating Cn3DPSSMInput structure");

    // encode master
    masterLength = bma->GetMaster()->Length();
    masterNCBIStdaa = new unsigned char[masterLength];
    for (unsigned int i=0; i<masterLength; ++i)
        masterNCBIStdaa[i] = LookupNCBIStdaaNumberFromCharacter(bma->GetMaster()->sequenceString[i]);

    // create PSIMsa
    PSIMsaDimensions dim;
    dim.query_length = bma->GetMaster()->Length();
    dim.num_seqs = bma->NRows() - 1;    // don't include master
    data = PSIMsaNew(&dim);

    // set up relevant PSIBlastOptions
//    options.pseudo_count = ;              // set below...
    options.inclusion_ethresh = 1.0;        // not used, but internal validation fails w/o legit value
//    options.use_best_alignment = ;
    options.nsg_compatibility_mode = true;
    options.impala_scaling_factor = 1.0;    // again, not used (I think?), but needs real value

    // set up PSIDiagnosticsRequest
    diag.information_content = false;
    diag.residue_frequencies = false;
    diag.weighted_residue_frequencies = false;
    diag.frequency_ratios = false;
    diag.gapless_column_weights = false;

    // fill in PSIMsa
    FillInAlignmentData(bma, data);

    // set pseudoconstant
    double infoContent = CalculateInformationContent(data);
    if      (infoContent > 84  ) options.pseudo_count = 10;
    else if (infoContent > 55  ) options.pseudo_count =  7;
    else if (infoContent > 43  ) options.pseudo_count =  5;
    else if (infoContent > 41.5) options.pseudo_count =  4;
    else if (infoContent > 40  ) options.pseudo_count =  3;
    else if (infoContent > 39  ) options.pseudo_count =  2;
    else                         options.pseudo_count =  1;
}

Cn3DPSSMInput::~Cn3DPSSMInput(void)
{
    PSIMsaFree(data);
    delete[] masterNCBIStdaa;
}

static BLAST_Matrix * ConvertPSSMToBLASTMatrix(const CPssmWithParameters& pssm)
{
    if (!pssm.GetPssm().IsSetFinalData())
        PTHROW("ConvertPSSMToBLASTMatrix() - pssm must have finalData");
    unsigned int nScores = pssm.GetPssm().GetNumRows() * pssm.GetPssm().GetNumColumns();
    if (pssm.GetPssm().GetNumRows() != 26 || pssm.GetPssm().GetFinalData().GetScores().size() != nScores)
        PTHROW("ConvertPSSMToBLASTMatrix() - bad matrix size");

    BLAST_Matrix *matrix = (BLAST_Matrix *) MemNew(sizeof(BLAST_Matrix));

    // set BLAST_Matrix values
    matrix->is_prot = pssm.GetPssm().GetIsProtein();
    matrix->name = NULL;
    matrix->rows = pssm.GetPssm().GetNumColumns() + 1;  // rows and columns are reversed in pssm vs BLAST_Matrix
    matrix->columns = pssm.GetPssm().GetNumRows();
    matrix->posFreqs = NULL;
    matrix->karlinK = pssm.GetPssm().GetFinalData().GetKappa();
    matrix->original_matrix = NULL;

    // allocate matrix
    matrix->matrix = (Int4Ptr *) MemNew(matrix->rows * sizeof(Int4Ptr));
    unsigned int i;
    for (i=0; i<matrix->rows; ++i)
        matrix->matrix[i] = (Int4Ptr) MemNew(matrix->columns * sizeof(Int4));

    // convert matrix
    unsigned int r = 0, c = 0;
    CPssmFinalData::TScores::const_iterator s = pssm.GetPssm().GetFinalData().GetScores().begin();
    for (i=0; i<nScores; ++i, ++s) {

        matrix->matrix[r][c] = *s;

        // adjust for matrix layout in pssm
        if (pssm.GetPssm().GetByRow()) {
            ++r;
            if (r == pssm.GetPssm().GetNumColumns()) {
                ++c;
                r = 0;
            }
        } else {
            ++c;
            if (c == pssm.GetPssm().GetNumRows()) {
                ++r;
                c = 0;
            }
        }
    }

    // Set the last row to BLAST_SCORE_MIN
    for (i=0; i<matrix->columns; i++)
        matrix->matrix[matrix->rows - 1][i] = BLAST_SCORE_MIN;

    return matrix;
}

BLAST_Matrix * CreateBlastMatrix(const BlockMultipleAlignment *bma)
{
    BLAST_Matrix *matrix = NULL;
    try {
        Cn3DPSSMInput input(bma);
        CPssmEngine engine(&input);
        CRef < CPssmWithParameters > pssm = engine.Run();
        matrix = ConvertPSSMToBLASTMatrix(*pssm);
    } catch (CException& e) {
        ERRORMSG("CreateBlastMatrix() failed with exception: " << e.what());
    }
    return matrix;
}

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2005/03/08 17:22:31  thiessen
* apparently working C++ PSSM generation
*
*/

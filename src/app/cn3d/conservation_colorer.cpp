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
*      Classes to color alignment blocks by sequence conservation
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2001/02/13 20:33:49  thiessen
* add information content coloring
*
* Revision 1.5  2000/11/30 15:49:38  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.4  2000/10/16 20:03:07  thiessen
* working block creation
*
* Revision 1.3  2000/10/16 14:25:48  thiessen
* working alignment fit coloring
*
* Revision 1.2  2000/10/04 17:41:30  thiessen
* change highlight color (cell background) handling
*
* Revision 1.1  2000/09/20 22:24:48  thiessen
* working conservation coloring; split and center unaligned justification
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>  // must come before C-toolkit stuff
#include <blastkar.h>           // for BLAST standard probability routines
#include <objseq.h>
#include <math.h>

#include "cn3d/conservation_colorer.hpp"
#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/structure_base.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

const Vector ConservationColorer::MinimumConservationColor(100.0/255, 100.0/255, 1.0);
const Vector ConservationColorer::MaximumConservationColor(1.0, 25.0/255, 25.0/255);


#define BLOSUMSIZE 24
static const char Blosum62Fields[BLOSUMSIZE] =
   { 'A', 'R', 'N', 'D', 'C', 'Q', 'E', 'G', 'H', 'I', 'L', 'K', 'M',
     'F', 'P', 'S', 'T', 'W', 'Y', 'V', 'B', 'Z', 'X', '*' };
static const char Blosum62Matrix[BLOSUMSIZE][BLOSUMSIZE] = {
/*       A,  R,  N,  D,  C,  Q,  E,  G,  H,  I,  L,  K,  M,  F,  P,  S,  T,  W,  Y,  V,  B,  Z,  X,  * */
/*A*/ {  4, -1, -2, -2,  0, -1, -1,  0, -2, -1, -1, -1, -1, -2, -1,  1,  0, -3, -2,  0, -2, -1,  0, -4 },
/*R*/ { -1,  5,  0, -2, -3,  1,  0, -2,  0, -3, -2,  2, -1, -3, -2, -1, -1, -3, -2, -3, -1,  0, -1, -4 },
/*N*/ { -2,  0,  6,  1, -3,  0,  0,  0,  1, -3, -3,  0, -2, -3, -2,  1,  0, -4, -2, -3,  3,  0, -1, -4 },
/*D*/ { -2, -2,  1,  6, -3,  0,  2, -1, -1, -3, -4, -1, -3, -3, -1,  0, -1, -4, -3, -3,  4,  1, -1, -4 },
/*C*/ {  0, -3, -3, -3,  9, -3, -4, -3, -3, -1, -1, -3, -1, -2, -3, -1, -1, -2, -2, -1, -3, -3, -2, -4 },
/*Q*/ { -1,  1,  0,  0, -3,  5,  2, -2,  0, -3, -2,  1,  0, -3, -1,  0, -1, -2, -1, -2,  0,  3, -1, -4 },
/*E*/ { -1,  0,  0,  2, -4,  2,  5, -2,  0, -3, -3,  1, -2, -3, -1,  0, -1, -3, -2, -2,  1,  4, -1, -4 },
/*G*/ {  0, -2,  0, -1, -3, -2, -2,  6, -2, -4, -4, -2, -3, -3, -2,  0, -2, -2, -3, -3, -1, -2, -1, -4 },
/*H*/ { -2,  0,  1, -1, -3,  0,  0, -2,  8, -3, -3, -1, -2, -1, -2, -1, -2, -2,  2, -3,  0,  0, -1, -4 },
/*I*/ { -1, -3, -3, -3, -1, -3, -3, -4, -3,  4,  2, -3,  1,  0, -3, -2, -1, -3, -1,  3, -3, -3, -1, -4 },
/*L*/ { -1, -2, -3, -4, -1, -2, -3, -4, -3,  2,  4, -2,  2,  0, -3, -2, -1, -2, -1,  1, -4, -3, -1, -4 },
/*K*/ { -1,  2,  0, -1, -3,  1,  1, -2, -1, -3, -2,  5, -1, -3, -1,  0, -1, -3, -2, -2,  0,  1, -1, -4 },
/*M*/ { -1, -1, -2, -3, -1,  0, -2, -3, -2,  1,  2, -1,  5,  0, -2, -1, -1, -1, -1,  1, -3, -1, -1, -4 },
/*F*/ { -2, -3, -3, -3, -2, -3, -3, -3, -1,  0,  0, -3,  0,  6, -4, -2, -2,  1,  3, -1, -3, -3, -1, -4 },
/*P*/ { -1, -2, -2, -1, -3, -1, -1, -2, -2, -3, -3, -1, -2, -4,  7, -1, -1, -4, -3, -2, -2, -1, -2, -4 },
/*S*/ {  1, -1,  1,  0, -1,  0,  0,  0, -1, -2, -2,  0, -1, -2, -1,  4,  1, -3, -2, -2,  0,  0,  0, -4 },
/*T*/ {  0, -1,  0, -1, -1, -1, -1, -2, -2, -1, -1, -1, -1, -2, -1,  1,  5, -2, -2,  0, -1, -1,  0, -4 },
/*W*/ { -3, -3, -4, -4, -2, -2, -3, -2, -2, -3, -2, -3, -1,  1, -4, -3, -2, 11,  2, -3, -4, -3, -2, -4 },
/*Y*/ { -2, -2, -2, -3, -2, -1, -2, -3,  2, -1, -1, -2, -1,  3, -3, -2, -2,  2,  7, -1, -3, -2, -1, -4 },
/*V*/ {  0, -3, -3, -3, -1, -2, -2, -3, -3,  3,  1, -2,  1, -1, -2, -2,  0, -3, -1,  4, -3, -2, -1, -4 },
/*B*/ { -2, -1,  3,  4, -3,  0,  1, -1,  0, -3, -4,  0, -3, -3, -2,  0, -1, -4, -3, -3,  4,  1, -1, -4 },
/*Z*/ { -1,  0,  0,  1, -3,  3,  4, -2,  0, -3, -3,  1, -1, -3, -1,  0, -1, -3, -2, -2,  1,  4, -1, -4 },
/*X*/ {  0, -1, -1, -1, -2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -2,  0,  0, -2, -1, -1, -1, -1, -1, -4 },
/***/ { -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4,  1 }
};

static std::map < char, std::map < char, char > > Blosum62Map;
static std::map < char, float > StandardProbabilities;

ConservationColorer::ConservationColorer(void) : nColumns(0)
{
    if (Blosum62Map.size() == 0) {  // initialize static stuff

        // initialize BLOSUM map for easy access
        for (int row=0; row<BLOSUMSIZE; row++) {
             for (int column=0; column<=BLOSUMSIZE; column++)
                Blosum62Map[Blosum62Fields[row]][Blosum62Fields[column]] =
                    Blosum62Matrix[row][column];
        }

        // calculate expected residue frequencies (standard probabilities)
        // (code borrowed from SeqAlignInform() in cddutil.c)
        static const char ncbistdaa2char[26] = {
            'X', 'A', 'X', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'K', 'L', 'M',
            'N', 'P', 'Q', 'R', 'S', 'T', 'V', 'W', 'X', 'Y', 'X', 'X', 'X'
        };
        BLAST_ScoreBlkPtr sbp = BLAST_ScoreBlkNew(Seq_code_ncbistdaa, 1);
        BLAST_ResFreqPtr stdrfp = NULL;
        int a;
        if (!sbp) goto on_error;
        sbp->read_in_matrix = TRUE;
        sbp->protein_alphabet = TRUE;
        sbp->posMatrix = NULL;
        sbp->number_of_contexts = 1;
        if (BlastScoreBlkMatFill(sbp, "BLOSUM62") != 0) goto on_error;
        stdrfp = BlastResFreqNew(sbp);
        if (!stdrfp) goto on_error;
        if (BlastResFreqStdComp(sbp, stdrfp) != 0) goto on_error;
        for (a=1; a<23; a++) // from 'A' to 'Y'
            StandardProbabilities[ncbistdaa2char[a]] = (float) stdrfp->prob[a];
//        for (a=1; a<23; a++)
//            TESTMSG("std prob '" << ncbistdaa2char[a] << "' = " << StandardProbabilities[ncbistdaa2char[a]]);
        goto cleanup;

on_error:
        ERR_POST(Error << "ConservationColorer::ConservationColorer() - "
            "error initializing standard residue probabilities");

cleanup:
        if (stdrfp) BlastResFreqDestruct(stdrfp);
        if (sbp) BLAST_ScoreBlkDestruct(sbp);
    }
}

void ConservationColorer::AddBlock(const UngappedAlignedBlock *block)
{
    blocks[block].resize(block->width);

    // map block column position to profile position
    for (int i=0; i<block->width; i++) blocks[block][i] = nColumns + i;
    nColumns += block->width;
}

typedef std::map < char, int > ColumnProfile;

void ConservationColorer::CalculateConservationColors(void)
{
    TESTMSG("calculating conservation colors");

    if (blocks.size() == 0) return;

    ColumnProfile profile;
    ColumnProfile::iterator p, pe, p2;
    int row, profileColumn;

    typedef std::vector < int > IntVector;
    IntVector varieties(nColumns, 0), weightedVarieties(nColumns, 0);
    identities.resize(nColumns);
    int minVariety, maxVariety, minWeightedVariety, maxWeightedVariety;

    typedef std::vector < float > FloatVector;
    FloatVector informationContents(nColumns, 0.0f);

    int nRows = blocks.begin()->first->NSequences();
    typedef std::map < char, int > CharMap;
    std::vector < CharMap > fitSums(nColumns);
    IntVector minFit(nColumns), maxFit(nColumns);

    BlockMap::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; b++) {
        for (int blockColumn=0; blockColumn<b->first->width; blockColumn++) {
            profileColumn = b->second[blockColumn];

            // create profile for this column
            profile.clear();
            for (row=0; row<nRows; row++) {
                char ch = toupper(b->first->GetCharacterAt(blockColumn, row));
                switch (ch) {
                    case 'A': case 'R': case 'N': case 'D': case 'C':
                    case 'Q': case 'E': case 'G': case 'H': case 'I':
                    case 'L': case 'K': case 'M': case 'F': case 'P':
                    case 'S': case 'T': case 'W': case 'Y': case 'V':
                        break;
                    default:
                        ch = 'X'; // make all but natural aa's just 'X'
                }
                if ((p=profile.find(ch)) != profile.end())
                    p->second++;
                else
                    profile[ch] = 1;
            }

            // identity for this column?
            if (profile.size() == 1 && profile.begin()->first != 'X')
                identities[profileColumn] = true;
            else
                identities[profileColumn] = false;

            // variety for this column
            int& variety = varieties[profileColumn];
            variety = profile.size();
            if (profile.find('X') != profile.end())
                variety += profile['X'] - 1; // each 'X' counts as one variety
            if (b == blocks.begin() && blockColumn == 0) {
                minVariety = maxVariety = variety;
            } else {
                if (variety < minVariety) minVariety = variety;
                else if (variety > maxVariety) maxVariety = variety;
            }

            // weighted variety for this column
            pe = profile.end();
            int& weightedVariety = weightedVarieties[profileColumn];
            for (p=profile.begin(); p!=pe; p++) {
                weightedVariety +=
                    (p->second * (p->second - 1) / 2) * Blosum62Map[p->first][p->first];
                p2 = p;
                for (p2++; p2!=pe; p2++)
                    weightedVariety +=
                        p->second * p2->second * Blosum62Map[p->first][p2->first];
            }
            if (b == blocks.begin() && blockColumn == 0) {
                minWeightedVariety = maxWeightedVariety = weightedVariety;
            } else {
                if (weightedVariety < minWeightedVariety) minWeightedVariety = weightedVariety;
                else if (weightedVariety > maxWeightedVariety) maxWeightedVariety = weightedVariety;
            }

            // information content for this column (calculated in bits -> logs of base 2)
            pe = profile.end();
            float& information = informationContents[profileColumn];
            for (p=profile.begin(); p!=pe; p++) {
                static float ln2 = (float) log(2), threshhold = 0.0001f;
                float expFreq = StandardProbabilities[p->first];
                if (expFreq > threshhold) {
                    float obsFreq = 1.0f * p->second / nRows,
                          freqRatio = obsFreq / expFreq;
                    if (freqRatio > threshhold)
                        information += obsFreq * ((float) log(freqRatio)) / ln2;
                }
            }
//            TESTMSG("info prof col " << profileColumn << " = " << information);

            // fit for each residue in this column
            for (p=profile.begin(); p!=pe; p++) {
                int& sum = fitSums[profileColumn][p->first];
                for (p2=profile.begin(); p2!=pe; p2++) {
                    if (p2 == p)
                        sum += (p->second - 1) * Blosum62Map[p->first][p->first];
                    else
                        sum += p2->second * Blosum62Map[p->first][p2->first];
                }
                if (p == profile.begin()) {
                    minFit[profileColumn] = maxFit[profileColumn] = sum;
                } else {
                    if (sum < minFit[profileColumn]) minFit[profileColumn] = sum;
                    else if (sum > maxFit[profileColumn]) maxFit[profileColumn] = sum;
                }
            }
        }
    }

    // now assign colors
    varietyColors.resize(nColumns);
    weightedVarietyColors.resize(nColumns);
    informationContentColors.resize(nColumns);
    fitColors.resize(nRows * nColumns);

    double scale;
    for (profileColumn=0; profileColumn<nColumns; profileColumn++) {

        // variety
        if (maxVariety == minVariety) {
            varietyColors[profileColumn] = MaximumConservationColor;
        } else {
            scale = 1.0 * (varieties[profileColumn] - minVariety) / (maxVariety - minVariety);
            varietyColors[profileColumn] =
                MaximumConservationColor + (MinimumConservationColor - MaximumConservationColor) * scale;
        }

        // weighted variety
        if (maxWeightedVariety == minWeightedVariety) {
            weightedVarietyColors[profileColumn] = MaximumConservationColor;
        } else {
            scale = 1.0 * (weightedVarieties[profileColumn] - minWeightedVariety) /
                (maxWeightedVariety - minWeightedVariety);
            weightedVarietyColors[profileColumn] =
                MinimumConservationColor + (MaximumConservationColor - MinimumConservationColor) * scale;
        }

        // information content, based on absolute scale
        static const float minInform = 0.10f, maxInform = 6.24f;
        scale = (informationContents[profileColumn] - minInform) / (maxInform - minInform);
        if (scale < 0.0) scale = 0.0;
        else if (scale > 1.0) scale = 1.0;
        scale = sqrt(scale);    // apply non-linearity so that lower values are better distinguished
        informationContentColors[profileColumn] =
            MinimumConservationColor + (MaximumConservationColor - MinimumConservationColor) * scale;

        // fit
        CharMap::const_iterator c, ce = fitSums[profileColumn].end();
        for (c=fitSums[profileColumn].begin(); c!=ce; c++) {
            if (maxFit[profileColumn] == minFit[profileColumn]) {
                fitColors[profileColumn][c->first] = MaximumConservationColor;
            } else {
                scale = 1.0 * (c->second - minFit[profileColumn]) /
                    (maxFit[profileColumn] - minFit[profileColumn]);
                fitColors[profileColumn][c->first] =
                    MinimumConservationColor + (MaximumConservationColor - MinimumConservationColor) * scale;
            }
        }
    }
}

void ConservationColorer::GetProfileIndexAndResidue(
    const UngappedAlignedBlock *block, int blockColumn, int row,
    int *profileIndex, char *residue) const
{
    BlockMap::const_iterator b = blocks.find(block);
    *profileIndex = b->second.at(blockColumn);
    *residue = toupper(b->first->GetCharacterAt(blockColumn, row));
}

END_SCOPE(Cn3D)


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
* Revision 1.1  2000/09/20 22:24:48  thiessen
* working conservation coloring; split and center unaligned justification
*
* ===========================================================================
*/

#include "cn3d/conservation_colorer.hpp"
#include "cn3d/alignment_manager.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

const Vector ConservationColorer::ZeroIdentityColor(100.0/255, 100.0/255, 1.0);
const Vector ConservationColorer::FullIdentityColor(1.0, 25.0/255, 25.0/255);
        
        
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

ConservationColorer::ConservationColorer(void) :
    nColumns(0)
{
    // initialize BLOSUM map for easy access
    if (Blosum62Map.size() == 0) {
        for (int row=0; row<BLOSUMSIZE; row++) {
             for (int column=0; column<=BLOSUMSIZE; column++)
                Blosum62Map[Blosum62Fields[row]][Blosum62Fields[column]] =
                    Blosum62Matrix[row][column];
        }
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
    ColumnProfile profile;
    ColumnProfile::iterator p, pe, p2;

    typedef std::vector < int > IntVector;
    IntVector varieties(nColumns, 0), weightedVarieties(nColumns, 0);
    identities.resize(nColumns);
    int minVariety, maxVariety, minWeightedVariety, maxWeightedVariety;

    int profileColumn;
    BlockMap::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; b++) {
        for (int blockColumn=0; blockColumn<b->first->width; blockColumn++) {
            profileColumn = b->second[blockColumn];

            // create profile for this column
            profile.clear();
            for (int row=0; row<b->first->NSequences(); row++) {
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
            varieties[profileColumn] = profile.size();
            if (profile.find('X') != profile.end())
                varieties[profileColumn] += profile['X'] - 1; // each 'X' counts as one variety
            if (b == blocks.begin() && blockColumn == 0) {
                minVariety = maxVariety = varieties[profileColumn];
            } else {
                if (varieties[profileColumn] < minVariety)
                    minVariety = varieties[profileColumn];
                if (varieties[profileColumn] > maxVariety)
                    maxVariety = varieties[profileColumn];
            }

            // weighted variety for this column
            pe = profile.end();
            for (p=profile.begin(); p!=pe; p++)
                weightedVarieties[profileColumn] +=
                    (p->second * (p->second - 1) / 2) * Blosum62Map[p->first][p->first];
            for (p=profile.begin(); p!=pe; p++) {
                p2 = p;
                for (p2++; p2!=pe; p2++)
                    weightedVarieties[profileColumn] +=
                        p->second * p2->second * Blosum62Map[p->first][p2->first];
            }
            if (b == blocks.begin() && blockColumn == 0) {
                minWeightedVariety = maxWeightedVariety = weightedVarieties[profileColumn];
            } else {
                if (weightedVarieties[profileColumn] < minWeightedVariety)
                    minWeightedVariety = weightedVarieties[profileColumn];
                if (weightedVarieties[profileColumn] > maxWeightedVariety)
                    maxWeightedVariety = weightedVarieties[profileColumn];
            }
        }
    }

    // now assign colors
    varietyColors.resize(nColumns);
    weightedVarietyColors.resize(nColumns);
    double scale;
    for (profileColumn=0; profileColumn<nColumns; profileColumn++) {

        // variety
        scale = 1.0 * (varieties[profileColumn] - minVariety) / (maxVariety - minVariety);
        varietyColors[profileColumn] =
            FullIdentityColor + (ZeroIdentityColor - FullIdentityColor) * scale;

        // weighted variety
        scale = 1.0 * (weightedVarieties[profileColumn] - minWeightedVariety) /
            (maxWeightedVariety - minWeightedVariety);
        weightedVarietyColors[profileColumn] =
            ZeroIdentityColor + (FullIdentityColor - ZeroIdentityColor) * scale;
    }
}

END_SCOPE(Cn3D)


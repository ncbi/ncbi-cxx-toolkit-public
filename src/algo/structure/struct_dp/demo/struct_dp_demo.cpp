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
*      Test/demo for dynamic programming-based alignment algorithms
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

#include <map>
#include <string>

#include <struct_dp/demo/struct_dp_demo.hpp>
#include <struct_dp/struct_dp.h>

USING_NCBI_SCOPE;


BEGIN_SCOPE(struct_dp)

#define ERROR_MESSAGE(s) ERR_POST(Error << "struct_dp_demo: " << s << '!')
#define INFO_MESSAGE(s) ERR_POST(Info << "struct_dp_demo: " << s)

void DPApp::Init(void)
{
}

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

map < char, map < char, int > > Blosum62Map;

static inline char ScreenResidueCharacter(char original)
{
    char ch = toupper(original);
    switch (ch) {
        case 'A': case 'R': case 'N': case 'D': case 'C':
        case 'Q': case 'E': case 'G': case 'H': case 'I':
        case 'L': case 'K': case 'M': case 'F': case 'P':
        case 'S': case 'T': case 'W': case 'Y': case 'V':
        case 'B': case 'Z':
            break;
        default:
            ch = 'X'; // make all but natural aa's just 'X'
    }
    return ch;
}

static int GetBLOSUM62Score(char a, char b)
{
    if (Blosum62Map.size() == 0) {
        // initialize BLOSUM map for easy access
        for (int row=0; row<BLOSUMSIZE; row++) {
             for (int column=0; column<BLOSUMSIZE; column++)
                Blosum62Map[Blosum62Fields[row]][Blosum62Fields[column]] =
                    Blosum62Matrix[row][column];
        }
    }

    return Blosum62Map[ScreenResidueCharacter(a)][ScreenResidueCharacter(b)];
}

static DP_BlockInfo *blocks = NULL;
static string subject, query;

extern "C" {
int ScoreByBlosum62(unsigned int block, unsigned int queryPos)
{
    if (!blocks || block >= blocks->nBlocks ||
        queryPos + blocks->blockSizes[block] > query.size())
    {
        ERROR_MESSAGE("ScoreByBlosum62() - invalid parameters");
        return NEGATIVE_INFINITY;
    }

    int score = 0;
    for (int i=0; i<blocks->blockSizes[block]; i++)
        score += GetBLOSUM62Score(
            subject[blocks->blockPositions[block] + i],
            query[queryPos + i]);
        
//    INFO_MESSAGE("score for block " << block << " pos " << queryPos << ": " << score);
    return score;
}
} // extern "C"

int DPApp::Run(void)
{
    // this demo tries to reproduce the VAST alignment of 1DOI and 1FRD
    subject = "PTVEYLNYEVVDDNGWDMYDDDVFGEASDMDLDDEDYGSLEVNEGEYILEAAEAQGYDWPFSC"
        "RAGACANCAAIVLEGDIDMDMQQILSDEEVEDKNVRLTCIGSPDADEVKIVYNAKHLDYLQNRVI";

    blocks = DP_CreateBlockInfo(5);

    blocks->blockPositions[0] = 0;
    blocks->blockSizes[0] = 7;
    blocks->maxLoops[0] = 100;

    blocks->blockPositions[1] = 35;
    blocks->blockSizes[1] = 49;
    blocks->maxLoops[1] = 100;

    blocks->blockPositions[2] = 86;
    blocks->blockSizes[2] = 8;
    blocks->maxLoops[2] = 100;

    blocks->blockPositions[3] = 97;
    blocks->blockSizes[3] = 10;
    blocks->maxLoops[3] = 100;
//    blocks->maxLoops[3] = 0;
//    blocks->freezeBlocks[3] = 74;

    blocks->blockPositions[4] = 109;
    blocks->blockSizes[4] = 11;
//    blocks->freezeBlocks[4] = 85;

//    for (int b=0; b<blocks->nBlocks; b++)
//        INFO_MESSAGE("Block " << (b+1) << ": " 
//            << subject.substr(blocks->blockPositions[b], blocks->blockSizes[b]));

    query = "ASYQVRLINKKQDIDTTIEIDEETTILDGAEENGIELPFSCHSGSCSSCVGKVVEGEVDQSDQ"
                "IFLDDEQMGKGFALLCVTYPRSNCTIKTHQEPYLA";

    int result = DP_GlobalBlockAlign(blocks, ScoreByBlosum62, 0, query.size() - 1);

    DP_DestroyBlockInfo(blocks);
    return 0;
}

END_SCOPE(struct_dp)


USING_NCBI_SCOPE;
USING_SCOPE(struct_dp);

int main(int argc, const char* argv[])
{
    SetDiagStream(&NcbiCerr); // send all diagnostic messages to cerr
    SetDiagPostLevel(eDiag_Info);   // show all messages

    DPApp app;
    return app.AppMain(argc, argv, NULL, eDS_Default, NULL);    // don't use config file
}

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2003/06/18 19:10:17  thiessen
* fix lf issues
*
* Revision 1.1  2003/06/18 16:54:29  thiessen
* initial checkin; working global block aligner and demo app
*
*/

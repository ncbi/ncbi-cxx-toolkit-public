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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>
#include <util/tables/raw_scoremat.h>

#include <map>
#include <string>

#include <algo/structure/struct_dp/struct_dp.h>

#include "struct_dp_demo.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(struct_dp)

#define ERROR_MESSAGE(s) ERR_POST(Error << "struct_dp_demo: " << s << '!')
#define INFO_MESSAGE(s) ERR_POST(Info << "struct_dp_demo: " << s)

void DPApp::Init(void)
{
}

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
    static SNCBIFullScoreMatrix Blosum62Matrix;
    static bool unpacked = false;

    if (!unpacked) {
        NCBISM_Unpack(&NCBISM_Blosum62, &Blosum62Matrix);
        unpacked = true;
    }

    return Blosum62Matrix.s[ScreenResidueCharacter(a)][ScreenResidueCharacter(b)];
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
        return DP_NEGATIVE_INFINITY;
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

    DP_AlignmentResult *alignment;
    int result = DP_LocalBlockAlign(blocks, ScoreByBlosum62, 0, query.size() - 1, &alignment);

    // crudely print out alignment result
    if (result == STRUCT_DP_FOUND_ALIGNMENT) {
        INFO_MESSAGE("Found alignment, total score: " << alignment->score
            << " for " << alignment->nBlocks << " blocks");
        unsigned int block;
        for (block=0; block<alignment->nBlocks; block++) {
            unsigned int
                subjectStart = blocks->blockPositions[block + alignment->firstBlock],
                queryStart = alignment->blockPositions[block],
                blockSize = blocks->blockSizes[block + alignment->firstBlock];
            INFO_MESSAGE(
                "Block " << (block + alignment->firstBlock + 1) << ", score "
                << ScoreByBlosum62(block + alignment->firstBlock, queryStart) << ":\n"
                << "S: " << subject.substr(subjectStart, blockSize)
                << ' ' << (subjectStart + 1) << '-' << (subjectStart + blockSize) << '\n'
                << "Q: " << query.substr(queryStart, blockSize)
                << ' ' << (queryStart + 1) << '-' << (queryStart + blockSize)
            );

        }
        DP_DestroyAlignmentResult(alignment);
    }

    else if (result == STRUCT_DP_NO_ALIGNMENT) {
        INFO_MESSAGE("Found no significant alignment");
    }

    else if (result == STRUCT_DP_PARAMETER_ERROR || result == STRUCT_DP_ALGORITHM_ERROR) {
        ERROR_MESSAGE("DP_GlobalBlockAlign() failed");
    }

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
* Revision 1.11  2004/05/21 21:41:04  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.10  2004/02/19 02:21:20  thiessen
* fix struct_dp paths
*
* Revision 1.9  2003/12/11 19:47:48  thiessen
* use built-in blosum62
*
* Revision 1.8  2003/07/11 15:27:48  thiessen
* add DP_ prefix to globals
*
* Revision 1.7  2003/06/22 12:11:38  thiessen
* add local alignment
*
* Revision 1.6  2003/06/18 22:25:37  thiessen
* simplify alignment printout code, yet again... ;)
*
* Revision 1.5  2003/06/18 22:11:00  thiessen
* simplify alignment printout code, again... ;)
*
* Revision 1.4  2003/06/18 22:06:03  thiessen
* simplify alignment printout code
*
* Revision 1.3  2003/06/18 21:46:10  thiessen
* add traceback, alignment return structure
*
* Revision 1.2  2003/06/18 19:10:17  thiessen
* fix lf issues
*
* Revision 1.1  2003/06/18 16:54:29  thiessen
* initial checkin; working global block aligner and demo app
*
*/

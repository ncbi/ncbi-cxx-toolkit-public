/*
*  $Id$
*
* =========================================================================
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
*  Government do not and cannt warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* =========================================================================
*
*  Author: Boris Kiryutin
*
* =========================================================================
*/


#ifndef ALI_UTIL_H
#define ALI_UTIL_H

#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)

class CAli;
class CNSeq;
class CPSeq;
class CAliPiece;
class CProSplignScaledScoring;
class CSubstMatrix;

class CAliUtil {
public:
    static void CheckValidity(const CAli& ali);//returns if valid, throws otherwise 

    //count scaled score ('IScore')
   //divide by CScoring::sm_koef to get actual score
//mode 2 : gaps/framshifts at the end have 0 score (default)
//mode 1 : full scoring for gaps/framshifts in protein, 0 score for H-gaps/framshifts
//mode 0 : full scoring at the end
    static int CountIScore(const CAli& ali, const CProSplignScaledScoring& scoring, const CSubstMatrix& matrix, int mode = 2);

    //same but implies no introns (Fr means no introns)
    //lgap and rgap are for the second stage. if true, score any gap on left (lgap) and/or right (rgap)
    static int CountFrIScore(const CAli& ali, const CProSplignScaledScoring& scoring, const CSubstMatrix& matrix, int mode = 2, bool lgap = false, bool rgap = false);

private:
    //produces new alignment with intron cut out, returns intron 'IScore'
    static int CutIntrons(CAli& new_ali, const CAli& ali, const CProSplignScaledScoring& scoring);

};

END_SCOPE(prosplign)
END_NCBI_SCOPE

#endif //ALI_UTIL_H

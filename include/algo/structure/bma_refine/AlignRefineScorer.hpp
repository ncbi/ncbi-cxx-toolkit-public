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
* Authors:  Chris Lanczycki
*
* File Description:
*           Base class for objects that compute the score of an alignment.
*           Default is to use a score vs. PSSM computed from the alignment;
*           subclasses will override ComputeScore.
*      
*
* ===========================================================================
*/

#ifndef AR_ALIGN_REFINE_SCORER__HPP
#define AR_ALIGN_REFINE_SCORER__HPP

#include <vector>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

BEGIN_SCOPE(struct_util)
class AlignmentUtility;
END_SCOPE(struct_util)

BEGIN_SCOPE(align_refine)

enum AlignmentScoringMethod {
    ePSSMScoring = 0
};

class NCBI_BMAREFINE_EXPORT RowScorer {

    static const double SCORE_INVALID_OR_NOT_COMPUTED;

public:

    RowScorer();

    //  Provides default implementation:  sum of scores of seq vs PSSM for given row.
    //  If the row is unspecified, return the sum of scores summed over all rows.
    virtual double ComputeScore(struct_util::AlignmentUtility& au, unsigned int row = kMax_UInt);  

    //  Sum of scores of given seq vs PSSM, by block for the given row.  If the row is
    //  unspecified, sum block score over all rows.  Return value is the sum over all 
    //  blocks in blockScores.
    double ComputeBlockScores(struct_util::AlignmentUtility& au, std::vector<double>& blockScores, unsigned int row = kMax_UInt);  

    bool   HasScore();
    double GetScore();  //  simple getter
    AlignmentScoringMethod GetMethod();

private:

    AlignmentScoringMethod m_scoringMethod;
    bool   m_scoreComputed;
    double m_score;
};

inline
bool RowScorer::HasScore() {
    return m_scoreComputed;
}

inline 
double RowScorer::GetScore() {
    return m_score;
}

inline 
AlignmentScoringMethod RowScorer::GetMethod() {
    return m_scoringMethod;
}

END_SCOPE(align_refine)

#endif // AR_ALIGN_REFINE_SCORER__HPP

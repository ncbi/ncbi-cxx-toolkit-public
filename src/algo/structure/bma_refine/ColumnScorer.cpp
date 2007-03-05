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
*           Base class for objects that compute the score for the column of 
*           an alignment.  Also contains virtually inherited subclasses.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <set>
#include <algo/structure/struct_util/struct_util.hpp>
#include <algo/structure/cd_utils/cuResidueProfile.hpp>
#include <algo/structure/bma_refine/BMAUtils.hpp>
#include <algo/structure/bma_refine/ColumnScorer.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(cd_utils);

BEGIN_SCOPE(align_refine)

//
//
//  Base class implementations
//
//

const double ColumnScorer::SCORE_INVALID_OR_NOT_COMPUTED = kMin_Int;

//  Default implementation.
void ColumnScorer::ColumnScores(const BMA& bma, unsigned int alignmentIndex, vector< double >& columnScores) const {
    columnScores.clear();
    columnScores.push_back(ColumnScore(bma, alignmentIndex));
}

ColumnScoringMethod ColumnScorer::GetMethodForScorer(unsigned int scorerIndex) const {

    ColumnScoringMethod method = eInvalidColumnScorerMethod;
    if (scorerIndex == 0 && !IsCompound()) {
        method = GetMethod();
    } else if (IsCompound() && scorerIndex < m_scorers.size()) {
        method = m_scorers[scorerIndex]->GetMethod();
    }
    return method;
}

bool ColumnScorer::AddScorer(ColumnScorer* scorer) {
    if (m_scoringMethod == eCompoundScorer && scorer && scorer != this) {
        m_scorers.push_back(scorer);
        return true;
    }
    return false;
}

void ColumnScorer::GetAndCopyPSSMScoresForColumn(const BMA& bma, unsigned int alignmentIndex, vector< int >& scores, vector< double >* doubleScores) const {

    //  If passing in a sane set of scores, use them instead of recalculating
    if (doubleScores && doubleScores->size() == bma.NRows()) {
        scores.clear();
        for (unsigned int i = 0; i < doubleScores->size(); ++i) {
            scores.push_back((int) (*doubleScores)[i]);
        }
    } else {
    //  ... otherwise, find the PSSM scores at each row and cache them if requested
        BMAUtils::GetPSSMScoresForColumn(bma, alignmentIndex, scores);
        if (doubleScores) {
            doubleScores->clear();
            for (unsigned int i = 0; i < scores.size(); ++i) {
                doubleScores->push_back((double) scores[i]);
            }
        }
    }
}

//
//
//  Subclass implementations
//
//

//  scorerIndex not used for a simple scorer
double PercentAtOrOverThresholdColumnScorer::ColumnScore(const BMA& bma, unsigned int alignmentIndex, vector< double >* rowScores, unsigned int scorerIndex) const {

    double result = SCORE_INVALID_OR_NOT_COMPUTED;
    unsigned int nAccepted = 0, nScores = 0;
    vector< int > scores;

    GetAndCopyPSSMScoresForColumn(bma, alignmentIndex, scores, rowScores);

    nScores = scores.size();
    if (nScores > 0) {
        for (unsigned int i = 0; i < scores.size(); ++i) {
            if (scores[i] >= m_threshold) ++nAccepted;
        }
        result = (double) nAccepted/ (double) nScores;
    }
    return result;
}
  
//  scorerIndex not used for a simple scorer
double SumOfScoresColumnScorer::ColumnScore(const BMA& bma, unsigned int alignmentIndex, vector< double >* rowScores, unsigned int scorerIndex) const {

    double result = SCORE_INVALID_OR_NOT_COMPUTED;
    unsigned int nScores = 0;
    vector< int > scores;

    GetAndCopyPSSMScoresForColumn(bma, alignmentIndex, scores, rowScores);

    nScores = scores.size();
    if (nScores > 0) {
        result = 0;
        for (unsigned int i = 0; i < nScores; ++i) {
            result += scores[i];
        }
    }
    return result;
}
  
//  scorerIndex not used for a simple scorer
double MedianColumnScorer::ColumnScore(const BMA& bma, unsigned int alignmentIndex, vector< double >* rowScores, unsigned int scorerIndex) const {

    double result = SCORE_INVALID_OR_NOT_COMPUTED;
    unsigned int middleIndex, nScores = 0;
    vector< int > scores;
    multiset< int > orderedScores;
    multiset< int >::iterator sii;

    GetAndCopyPSSMScoresForColumn(bma, alignmentIndex, scores, rowScores);

    nScores = scores.size();
//    cout << "median scores nscores:  " << nScores << " alignment index " << alignmentIndex << endl;
    if (nScores > 0) {
        //  nScores odd:   use the middle position
        //  nScores even:  use the first position in 2nd half of set
        middleIndex = nScores/2;
        orderedScores.insert(scores.begin(), scores.end());
        sii = orderedScores.begin();
        for (nScores = 0; nScores < middleIndex; ++nScores) {
            ++sii;
        }
        TRACE_MESSAGE_CL(" Alignment index " << alignmentIndex+1 << " median position " << middleIndex << " median score " << *sii);

        result = *sii;
    }
    return result;
}

map< const BMA*, int > PercentOfWeightOverThresholdColumnScorer::m_scoreShiftMap;
  
//  scorerIndex not used for a simple scorer
double PercentOfWeightOverThresholdColumnScorer::ColumnScore(const BMA& bma, unsigned int alignmentIndex, vector< double >* rowScores, unsigned int scorerIndex) const {

    double result = SCORE_INVALID_OR_NOT_COMPUTED;
    int scoreShift = kMin_Int;
    int totalWeight = 0, overWeight = 0;
    unsigned int nScores = 0;
    vector< int > scores;

    GetAndCopyPSSMScoresForColumn(bma, alignmentIndex, scores, rowScores);

    if (m_scoreUsage == eUseGlobalShiftedValue) {
        if (m_scoreShiftMap.count(&bma) > 0) {
            scoreShift = m_scoreShiftMap[&bma];
        } else {
            scoreShift = BMAUtils::GetSmallestValueInPssm(bma);
            if (scoreShift == kMin_Int) {
                scoreShift = 0;
                ERROR_MESSAGE_CL("Bad PSSM found during score shift calculation.  Set shift to zero");
            }
            m_scoreShiftMap[&bma] = scoreShift;
        }
    }

    nScores = scores.size();
    if (nScores > 0) {
        for (unsigned int i = 0; i < nScores; ++i) {
            switch (m_scoreUsage) {
            case eUseRawScore:
            case eUseRawScoreUnderBased:
                if (scores[i] >= m_threshold) overWeight += scores[i];
                totalWeight += scores[i];
                break;
            case eUseAbsoluteScoreValue:
                if (scores[i] >= m_threshold) overWeight += abs(scores[i]);
                totalWeight += abs(scores[i]);
                break;
            case eUseLocalShiftedValue:
                //  get the minimum of the column
                if (i == 0) {
                    for (unsigned int j = 0; j < nScores; ++j) {
                        if (scoreShift > scores[j]) scoreShift = scores[j];
                    }
                }
                //  allow to fall through...
            case eUseGlobalShiftedValue:
                if (scores[i] >= m_threshold) overWeight += (scores[i] - scoreShift);
                totalWeight += (scores[i] - scoreShift);
                break;
            default:
                break;
            }
        }

        if (m_scoreUsage != eUseRawScoreUnderBased) {
            result = (totalWeight != 0) ? (double) overWeight/ (double) totalWeight : 0.0;
        } else {
            //  Intended for use w/ threshold = 0 when want to base scoring on a
            //  fraction of weight under the threshold.
            //  1 - (over - total)/total = 1 - (over/total - 1) = 2 - over/total
            result = (totalWeight != 0) ? 2.0 - (double) overWeight/(double) totalWeight : 0.0;
        }
        TRACE_MESSAGE_CL(" Alignment index " << alignmentIndex+1 << " nscores= " << nScores <<  " overWeight " << overWeight << " total " << totalWeight << "  result " << result);
    }
    return result;
}


//  scorerIndex not used for a simple scorer
double InfoContentColumnScorer::ColumnScore(const BMA& bma, unsigned int alignmentIndex, vector< double >* rowScores, unsigned int scorerIndex) const 
{
    double result = SCORE_INVALID_OR_NOT_COMPUTED;
    unsigned int n;
    vector<char> residues;
    ColumnResidueProfile resProfile;
    bool inPssm, isAligned = BMAUtils::IsColumnOfType(bma, alignmentIndex, align_refine::eAlignedResidues, inPssm);

    if (rowScores) {
        vector<int> scores;
        GetAndCopyPSSMScoresForColumn(bma, alignmentIndex, scores, rowScores);
    }

    BMAUtils::GetResiduesForColumn(bma, alignmentIndex, residues);
    n = residues.size();
    if (n > 0) {
        for (unsigned int i = 0; i < n; ++i) {
            resProfile.addOccurence(residues[i], i, isAligned);
        }
        result = resProfile.calcInformationContent();
    }
    
    return result;
}


//
//
//  Compound column scorer implementation
//
//
  
CompoundColumnScorer::~CompoundColumnScorer() {
    for (unsigned int i = 0; i < m_scorers.size(); ++i) {
        delete m_scorers[i];
    }
    m_scorers.clear();
}

double CompoundColumnScorer::ColumnScore(const BMA& bma, unsigned int alignmentIndex, vector< double >* rowScores, unsigned int scorerIndex) const {

    return (scorerIndex < m_scorers.size()) ? m_scorers[scorerIndex]->ColumnScore(bma, alignmentIndex, rowScores) : SCORE_INVALID_OR_NOT_COMPUTED;
}

void CompoundColumnScorer::ColumnScores(const BMA& bma, unsigned int alignmentIndex, vector< double >& columnScores) const {

    double score;
    unsigned int i, nScorers = m_scorers.size();
    vector< double > cachedPSSMScores;

    columnScores.clear();
    for (i = 0; i < nScorers; ++i) {
        if (!m_scorers[i]->IsCompound()) {
            score = m_scorers[i]->ColumnScore(bma, alignmentIndex, &cachedPSSMScores);
        } else {
            ERROR_MESSAGE_CL("Nested compound scorers are not currently support; returning dummy value for this score");
            score = SCORE_INVALID_OR_NOT_COMPUTED;
        }
        columnScores.push_back(score);
    }
}


END_SCOPE(align_refine)

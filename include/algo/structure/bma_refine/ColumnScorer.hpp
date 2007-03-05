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

#ifndef AR_COLUMN_SCORER__HPP
#define AR_COLUMN_SCORER__HPP

#include <map>
#include <vector>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

#include <algo/structure/bma_refine/BMAUtils.hpp>

USING_SCOPE(struct_util);

BEGIN_SCOPE(align_refine)

enum ColumnScoringMethod {
    eInvalidColumnScorerMethod = 0,
    eSumOfScores,
    eMedianScore,
    ePercentAtOrOverThreshold,
    ePercentOfWeightOverThreshold,
    eCompoundScorer,
    eInfoContent
};

//  Simple scorers have no scorers in m_scorers.
//  Compound scorers have had one or more scorers added to m_scorers by AddScorer.
class NCBI_BMAREFINE_EXPORT ColumnScorer {

public:

    static const double SCORE_INVALID_OR_NOT_COMPUTED;

    ColumnScorer(ColumnScoringMethod method) {m_scoringMethod = method; m_scorers.clear();}
    virtual ~ColumnScorer() {};

    //  General method to get a single column's score from the specified alignmentIndex.
    //  If 'rowScores' is non-NULL return the list of individual PSSM scores on each row, 
    //  and if scores are already in 'rowScores' use them w/o recalculation.
    //  If this is a compound scorer, 'scorerIndex' specifies the specific scorer 
    //  from m_scorers for which to return a score.
    virtual double ColumnScore(const BMA& bma, unsigned int alignmentIndex, vector< double >* rowScores = NULL, unsigned int scorerIndex = 0) const = 0;

    //  Add column score for each of the scorers in m_scorers in the columnScores vector
    //  for the indicated alignmentIndex.  
    //  Default implementation intended for simple scorers: add score for 'this' to columnScores.
    //  Override for compound scorers.
    virtual void ColumnScores(const BMA& bma, unsigned int alignmentIndex, vector< double >& columnScores) const;

    //  Allow define 'best' for each class.  Returns -1/0/1 if scoreLHS is worse than/equal to
    //  better than scoreRHS.  Default implementation assumes larger values are better,
    //  and any other double is better than SCORE_INVALID_OR_NOT_COMPUTED.
    virtual int IsBetterThan(double scoreLHS, double scoreRHS) const;

    //  Do not allow 'this' to be added.  Only succeeds if type is 'eCompoundScorer'
    bool AddScorer(ColumnScorer* newScorer);

    //  Convenience method; returns true if type is eCompoundScorer.
    inline bool IsCompound() const;

    inline unsigned int NumComponentScorers() const;
    inline ColumnScoringMethod GetMethod() const;

    //  Calling for a simple scorer w/ scorerIndex > 0, or for a compound scorer
    //  with scorerIndex >= m_scorers.size(), returns eInvalidColumnScorerMethod;
    //  scorerIndex = 0 for a simple scorer is equivalent to calling GetMethod.
    ColumnScoringMethod GetMethodForScorer(unsigned int scorerIndex) const;

protected:

    //  Wrapper for BMAUtils::GetPSSMScoresForColumn to use/set cached PSSM scores 
    //  if doubleScores is non-NULL.
    void GetAndCopyPSSMScoresForColumn(const BMA& bma, unsigned int alignmentIndex, vector< int >& scores, vector< double >* doubleScores = NULL) const;

    //  *never* store 'this' here if a simple scorer;  only add to vector via AddScorer
    vector < ColumnScorer* > m_scorers;  
    ColumnScoringMethod m_scoringMethod;
};


//  For a given threshold, count the number of rows in a column for whose residue the PSSM 
//  score is above that threshold.  
class NCBI_BMAREFINE_EXPORT PercentAtOrOverThresholdColumnScorer : public ColumnScorer {

public:
    PercentAtOrOverThresholdColumnScorer(double threshold = 0.0) : ColumnScorer(ePercentAtOrOverThreshold) {
        m_threshold = threshold;
    }
    virtual ~PercentAtOrOverThresholdColumnScorer() {};
    virtual double ColumnScore(const BMA& bma, unsigned int alignmentIndex, vector< double >* rowScores = NULL, unsigned int scorerIndex = 0) const;

private:
    double m_threshold;


};

//  Column score == the median PSSM score for specified alignment index.
class NCBI_BMAREFINE_EXPORT MedianColumnScorer : public ColumnScorer {

public:
    MedianColumnScorer() : ColumnScorer(eMedianScore) {};
    virtual ~MedianColumnScorer() {};
    virtual double ColumnScore(const BMA& bma, unsigned int alignmentIndex, vector< double >* rowScores = NULL, unsigned int scorerIndex = 0) const;
};


class NCBI_BMAREFINE_EXPORT SumOfScoresColumnScorer : public ColumnScorer {

public:
    SumOfScoresColumnScorer() : ColumnScorer(eSumOfScores) {};
    virtual ~SumOfScoresColumnScorer() {};
    virtual double ColumnScore(const BMA& bma, unsigned int alignmentIndex, vector< double >* rowScores = NULL, unsigned int scorerIndex = 0) const;
};


//  For a given threshold PSSM value:
//  i)  compute sum of value of PSSM scores
//  ii) compute sum of value of PSSM scores ONLY for those rows whose 
//      PSSM score at the column equals or excedes the threshold value
//      
//  This column scorer returns the ratio of (ii)/(i).  If the threshold is
//  zero, this is the fraction of the score weights that are negative.
//
//  If eUseRawScore and there are any negative scores, and threshold >= 0, (ii) > (i).
//  This means that in such as case score > 1.  

//  eUseRawScoreUnderBased is a bit weird.  Intended for the special case where
//  threshold = 0, eUseRawScoreUnderBased computes a percent under the threshold using raw scores, 
//  but since the scorers are expected to return a number to compare against a threshold using 
//  ">", it returns an effective percent over.  Namely, it computes 
//  1 - (score_over - score_total)/score_total.  If there are no negative scores, this is
//  identically 1 if threshold <= 0.  If there are negative scores and threshold == 0, this
//  is equal to 1 - (abs score_under/score_total).

//  If eUseAbsoluteScoreValue, use absolute value instead of the raw value in the above.
//  By using the absolute value, we are trying to avoid possible strange behavior
//  if (ii) >> (i), say if there are many negative values, or worse, that 
//  the score is in fact negative.  Want to preserve the weight of each residue - don't
//  lose the fact that some residues are strongly disfavored while others are neutral.
//

enum PssmScoreUsage {
    eUseRawScore = 0,        //  weight = signed score
    eUseRawScoreUnderBased,  //  weight = signed score; return effective %over threshold (see above)
    eUseAbsoluteScoreValue,  //  weight = absolute value of score (** compare vs. raw value)
    eUseLocalShiftedValue,   //  weight = score - (smallest score for this column)
    eUseGlobalShiftedValue   //  weight = score - (smallest score in PSSM)
};

class NCBI_BMAREFINE_EXPORT PercentOfWeightOverThresholdColumnScorer : public ColumnScorer {

public:
    PercentOfWeightOverThresholdColumnScorer(double threshold = 0.0, PssmScoreUsage usage = eUseRawScore) : ColumnScorer(ePercentOfWeightOverThreshold) {
        m_threshold = threshold;
        m_scoreUsage = usage;
    }
    virtual ~PercentOfWeightOverThresholdColumnScorer() {};
    virtual double ColumnScore(const BMA& bma, unsigned int alignmentIndex, vector< double >* rowScores = NULL, unsigned int scorerIndex = 0) const;

private:
    double m_threshold;
    PssmScoreUsage m_scoreUsage;
    //  used only if m_scoreUsage = eGlobalShiftedValue to cache the global shifts.
    static map< const BMA*, int > m_scoreShiftMap; 
    
};


class NCBI_BMAREFINE_EXPORT InfoContentColumnScorer : public ColumnScorer {

public:
    InfoContentColumnScorer() : ColumnScorer(eInfoContent) {};
    virtual ~InfoContentColumnScorer() {};
    virtual double ColumnScore(const BMA& bma, unsigned int alignmentIndex, vector< double >* rowScores = NULL, unsigned int scorerIndex = 0) const;
};


//  A generic compound scorer.
//  Cleans up any scorers added to it.
class NCBI_BMAREFINE_EXPORT CompoundColumnScorer : public ColumnScorer {

public:
    CompoundColumnScorer() : ColumnScorer(eCompoundScorer) {};
    virtual ~CompoundColumnScorer();
    virtual double ColumnScore(const BMA& bma, unsigned int alignmentIndex, vector< double >* rowScores = NULL, unsigned int scorerIndex = 0) const;
    virtual void ColumnScores(const BMA& bma, unsigned int alignmentIndex, vector< double >& columnScores) const;
};



inline 
ColumnScoringMethod ColumnScorer::GetMethod() const { 
    return m_scoringMethod;
}

inline 
unsigned int ColumnScorer::NumComponentScorers() const {
    return m_scorers.size();
}

inline 
bool ColumnScorer::IsCompound() const {
    return (m_scoringMethod == eCompoundScorer);
}

inline 
int ColumnScorer::IsBetterThan(double scoreLHS, double scoreRHS) const {
    return (scoreLHS < scoreRHS) ? -1 : ((scoreLHS == scoreRHS) ? 0 : 1);
}

END_SCOPE(align_refine)

#endif // AR_COLUMN_SCORER__HPP

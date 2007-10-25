/* ===========================================================================
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
* ===========================================================================*/

/**
 * @file compo_mode_condition.c
 * Functions to test whether conditional score matrix adjustment
 * should be applied for a pair of matching sequences.
 *
 * Authors: Alejandro Schaffer, Yi-Kuo Yu
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/composition_adjustment/composition_adjustment.h>
#include <algo/blast/composition_adjustment/compo_mode_condition.h>
#include <algo/blast/composition_adjustment/matrix_frequency_data.h>

/** 180 degrees in half a circle */
#define HALF_CIRCLE_DEGREES 180
/** some digits of PI */
#define PI 3.1415926543
/** @{ thresholds used to determine which composition mode to use */
#define QUERY_MATCH_DISTANCE_THRESHOLD 0.16
#define LENGTH_RATIO_THRESHOLD 3.0
#define ANGLE_DEGREE_THRESHOLD 70.0
#define HIGH_PAIR_THRESHOLD 0.4
#define LENGTH_LOWER_THRESHOLD 50
/** @} */

/** type of function used to choose a mode for composition-based
 * statistics. The variables are Queryseq_length, Matchseq_length,
 * query_amino_count, match_amino_account and matrix_name.*/
typedef EMatrixAdjustRule
(*Condition) (int, int, const double *, const double *,
              const char *);


/** Return true if length > 50 and the two most frequent letters
 * occur a total of more that 40% of the time. */
static int
s_HighPairFrequencies(const double * letterProbs, int length)
{
    int i; /*index*/
    double max, second; /*two highest letter probabilities*/

    if (length <= LENGTH_LOWER_THRESHOLD) {
        return FALSE;
    }
    max = 0;
    second = 0;
    for (i = 0;  i < COMPO_NUM_TRUE_AA;  i++) {
        if (letterProbs[i] > second) {
            second = letterProbs[i];
            if (letterProbs[i] > max) {
                second = max;
                max = letterProbs[i];
            }
        }
    }
    return (max + second) > HIGH_PAIR_THRESHOLD;
}

/**
 * Return true if either the query or the matching sequences
 * passes the test in s_HighPairFrequencies. */
static int
s_HighPairEitherSeq(const double * P_query, int length1,
                    const double * P_match, int length2)
{
    int result1, result2;

    result1 = s_HighPairFrequencies(P_query, length1);
    result2 = s_HighPairFrequencies(P_match, length2);

    return result1 || result2;
}


/** Return eDontAdjustMatrix unconditionally */
static EMatrixAdjustRule
s_NeverAdjustMatrix(int Len_query, int Len_match,
                  const double * P_query, const double * P_match,
                  const char *matrix_name)
{
    /* Suppress unused variable warnings */
    (void) Len_query;
    (void) Len_match;
    (void) P_query;
    (void) P_match;
    (void) matrix_name;

     return eDontAdjustMatrix;
}


/** Return eCompoScaleOldMatrix unconditionally */
static EMatrixAdjustRule
s_JustScaleOldMatrix(int Len_query, int Len_match,
                     const double * P_query, const double * P_match,
                     const char *matrix_name)
{
    /* Suppress unused variable warnings */
    (void) Len_query;
    (void) Len_match;
    (void) P_query;
    (void) P_match;
    (void) matrix_name;

    return eCompoScaleOldMatrix;
}


/** A function used to choose a mode for composition-based statistics.
 * If this function is used relative-entropy score adjustment is
 * always applied, with a fixed value as the target relative entropy*/
static EMatrixAdjustRule
s_TestToApplyREAdjustmentUnconditional(int Len_query,
                                       int Len_match,
                                       const double * P_query,
                                       const double * P_match,
                                       const char *matrix_name)
{
    /* Suppress unused variable warnings */
    (void) Len_query;
    (void) Len_match;
    (void) P_query;
    (void) P_match;
    (void) matrix_name;

    return eUserSpecifiedRelEntropy;
}


/**
 * A function used to choose a mode for composition-based statistics.
 * Decide whether a relative-entropy score adjustment should be used
 * based on lengths and letter counts of the two matched sequences;
 * matrix_name is the underlying score matrix */
static EMatrixAdjustRule
s_TestToApplyREAdjustmentConditional(int Len_query,
                                     int Len_match,
                                     const double * P_query,
                                     const double * P_match,
                                     const char *matrix_name)
{
    EMatrixAdjustRule which_rule; /* which relative entropy mode to
                                     return */
    int i;                       /* loop indices */
    double p_query[COMPO_NUM_TRUE_AA];
    double p_match[COMPO_NUM_TRUE_AA]; /*letter probabilities
                                                for query and match*/
    const double *p_matrix;       /* letter probabilities used in
                                     constructing matrix name*/
    double D_m_mat, D_q_mat, D_m_q;  /* distances between match and
                                        original between query and
                                        original between match and
                                        query*/
    double corr_factor = 0.0;     /* correlation between how p_query
                                     and p_match deviate from p_matrix
                                     */
    double len_q, len_m;          /* lengths of query and matching
                                     sequence in floating point */
    double len_large, len_small;  /* store the larger and smaller of
                                     len_q and len_m */
    double angle;                 /* angle between query and match
                                     probabilities */

    p_matrix = Blast_GetMatrixBackgroundFreq(matrix_name);

    for (i = 0;  i < COMPO_NUM_TRUE_AA;  i++) {
        p_query[i] = P_query[i];
        p_match[i] = P_match[i];
        corr_factor +=
            (p_query[i] - p_matrix[i]) * (p_match[i] - p_matrix[i]);
    }
    D_m_mat = Blast_GetRelativeEntropy(p_match, p_matrix);
    D_q_mat = Blast_GetRelativeEntropy(p_query, p_matrix);
    D_m_q   = Blast_GetRelativeEntropy(p_match, p_query);

    angle =
        acos((D_m_mat * D_m_mat + D_q_mat * D_q_mat -
              D_m_q * D_m_q) / 2.0 / D_m_mat / D_q_mat);
    /* convert from radians to degrees */
    angle = angle * HALF_CIRCLE_DEGREES / PI;

    len_q = 1.0 * Len_query;
    len_m = 1.0 * Len_match;
    if (len_q > len_m) {
        len_large = len_q;
        len_small = len_m;
    } else {
        len_large = len_m;
        len_small = len_q;
    }
    if (s_HighPairEitherSeq(P_query, Len_query, P_match, Len_match)) {
        which_rule = eUserSpecifiedRelEntropy;
    } else {
      if ((D_m_q > QUERY_MATCH_DISTANCE_THRESHOLD) &&
        (len_large / len_small > LENGTH_RATIO_THRESHOLD) &&
        (angle > ANGLE_DEGREE_THRESHOLD)) {
        which_rule = eCompoScaleOldMatrix;
      } else {
        which_rule = eUserSpecifiedRelEntropy;
      }
    }
    return which_rule;
}


/**
 * An array of functions that can be used to decide which optimization
 * formulation should be used for score adjustment */
static Condition Cond_func[] = {
    s_NeverAdjustMatrix,
    s_JustScaleOldMatrix,
    s_TestToApplyREAdjustmentConditional,
    s_TestToApplyREAdjustmentUnconditional,
    NULL
};


/* Documented in compo_mode_condition.h. */
EMatrixAdjustRule
Blast_ChooseMatrixAdjustRule(int length1,
                             int length2,
                             const double * probArray1,
                             const double * probArray2,
                             const char *matrixName,
                             ECompoAdjustModes composition_adjust_mode)
{
    int testFunctionIndex = (int) composition_adjust_mode;

    return
        Cond_func[testFunctionIndex] (length1,    length2,
                                      probArray1, probArray2, matrixName);
}

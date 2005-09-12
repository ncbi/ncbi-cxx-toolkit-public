static char const rcsid[] = "$Id$";

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

/*****************************************************************************

File name: Newton_procedures.c

Authors: Yi-Kuo Yu, Alejandro Schaffer, E. Michael Gertz

Contents: Highest level functions to solve the optimization problem 
          for compositional score matrix adjustment

******************************************************************************/

/* Functions that find the new joint probability matrix given 
   an original joint prob and new set(s) of background frequency(ies)
   The eta parameter is a lagrangian multiplier to
   fix the relative entropy between the new target and the new background

   RE_FLAG is used to indicate various choices as encoded in NRdefs.h
*/

#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <algo/blast/core/blast_toolkit.h>
#include <algo/blast/composition_adjustment/composition_adjustment.h>


static double
BLOSUM62_JOINT_PROBS[COMPOSITION_ALPHABET_SIZE][COMPOSITION_ALPHABET_SIZE]
= {
    {0.021516461557, 0.002341028532, 0.001941062549, 0.002160193055,
     0.001595828537, 0.001934059173, 0.002990874959, 0.005831307116,
     0.001108651421, 0.003181451207, 0.004450432543, 0.003350994862,
     0.001330482798, 0.001634084433, 0.002159278003, 0.006261897426,
     0.003735752688, 0.000404784037, 0.001298558985, 0.005124343367},
    {0.002341028532, 0.017737158563, 0.001969132731, 0.001581985934,
     0.000393496788, 0.002483620870, 0.002678135197, 0.001721914295,
     0.001230766890, 0.001239704106, 0.002418976127, 0.006214150782,
     0.000796884039, 0.000932356719, 0.000959872904, 0.002260870847,
     0.001779897849, 0.000265310579, 0.000918577576, 0.001588408095},
    {0.001941062549, 0.001969132731, 0.014105369019, 0.003711182199,
     0.000436559586, 0.001528401416, 0.002205231268, 0.002856026580,
     0.001423459827, 0.000986015608, 0.001369776043, 0.002436729322,
     0.000521972796, 0.000746722150, 0.000858953243, 0.003131380307,
     0.002237168191, 0.000161021675, 0.000695990541, 0.001203509685},
    {0.002160193055, 0.001581985934, 0.003711182199, 0.021213070328,
     0.000397349231, 0.001642988683, 0.004909362115, 0.002510933422,
     0.000948355160, 0.001226071189, 0.001524412852, 0.002443951825,
     0.000458902921, 0.000759393269, 0.001235481304, 0.002791458183,
     0.001886707235, 0.000161498946, 0.000595157039, 0.001320931409},
    {0.001595828537, 0.000393496788, 0.000436559586, 0.000397349231,
     0.011902428201, 0.000309689150, 0.000380965445, 0.000768969543,
     0.000229437747, 0.001092222651, 0.001570843250, 0.000500631539,
     0.000373569136, 0.000512643056, 0.000360439075, 0.001038049531,
     0.000932287369, 0.000144869300, 0.000344932387, 0.001370634611},
    {0.001934059173, 0.002483620870, 0.001528401416, 0.001642988683,
     0.000309689150, 0.007348611171, 0.003545322222, 0.001374101100,
     0.001045402587, 0.000891574240, 0.001623152279, 0.003116305001,
     0.000735592074, 0.000544610751, 0.000849940593, 0.001893917959,
     0.001381521088, 0.000228499204, 0.000674510708, 0.001174481769},
    {0.002990874959, 0.002678135197, 0.002205231268, 0.004909362115,
     0.000380965445, 0.003545322222, 0.016058942448, 0.001941788215,
     0.001359354087, 0.001208575016, 0.002010620195, 0.004137352463,
     0.000671608129, 0.000848058651, 0.001418534945, 0.002949177015,
     0.002049363253, 0.000264084965, 0.000864998825, 0.001706373779},
    {0.005831307116, 0.001721914295, 0.002856026580, 0.002510933422,
     0.000768969543, 0.001374101100, 0.001941788215, 0.037833882792,
     0.000956438296, 0.001381594180, 0.002100349645, 0.002551728599,
     0.000726329019, 0.001201930393, 0.001363538639, 0.003819521365,
     0.002185818204, 0.000406753457, 0.000831463001, 0.001832653843},
    {0.001108651421, 0.001230766890, 0.001423459827, 0.000948355160,
     0.000229437747, 0.001045402587, 0.001359354087, 0.000956438296,
     0.009268821027, 0.000575006579, 0.000990341860, 0.001186603601,
     0.000377383962, 0.000807129053, 0.000477177871, 0.001100800912,
     0.000744015818, 0.000151511190, 0.001515361861, 0.000650302833},
    {0.003181451207, 0.001239704106, 0.000986015608, 0.001226071189,
     0.001092222651, 0.000891574240, 0.001208575016, 0.001381594180,
     0.000575006579, 0.018297094930, 0.011372374833, 0.001566332194,
     0.002471405322, 0.003035353009, 0.001002322534, 0.001716150165,
     0.002683992649, 0.000360556333, 0.001366091300, 0.011965802769},
    {0.004450432543, 0.002418976127, 0.001369776043, 0.001524412852,
     0.001570843250, 0.001623152279, 0.002010620195, 0.002100349645,
     0.000990341860, 0.011372374833, 0.037325284430, 0.002482344486,
     0.004923694031, 0.005449900864, 0.001421696216, 0.002434190706,
     0.003337092433, 0.000733421681, 0.002210504676, 0.009545821406},
    {0.003350994862, 0.006214150782, 0.002436729322, 0.002443951825,
     0.000500631539, 0.003116305001, 0.004137352463, 0.002551728599,
     0.001186603601, 0.001566332194, 0.002482344486, 0.016147683460,
     0.000901118905, 0.000950170174, 0.001578353818, 0.003104386139,
     0.002360691115, 0.000272260749, 0.000996404634, 0.001952015271},
    {0.001330482798, 0.000796884039, 0.000521972796, 0.000458902921,
     0.000373569136, 0.000735592074, 0.000671608129, 0.000726329019,
     0.000377383962, 0.002471405322, 0.004923694031, 0.000901118905,
     0.003994917914, 0.001184353682, 0.000404888644, 0.000847632455,
     0.001004584462, 0.000197602804, 0.000563431813, 0.002301832938},
    {0.001634084433, 0.000932356719, 0.000746722150, 0.000759393269,
     0.000512643056, 0.000544610751, 0.000848058651, 0.001201930393,
     0.000807129053, 0.003035353009, 0.005449900864, 0.000950170174,
     0.001184353682, 0.018273718971, 0.000525642239, 0.001195904180,
     0.001167245623, 0.000851298193, 0.004226922511, 0.002601386501},
    {0.002159278003, 0.000959872904, 0.000858953243, 0.001235481304,
     0.000360439075, 0.000849940593, 0.001418534945, 0.001363538639,
     0.000477177871, 0.001002322534, 0.001421696216, 0.001578353818,
     0.000404888644, 0.000525642239, 0.019101516083, 0.001670397698,
     0.001352022511, 0.000141505490, 0.000450817134, 0.001257818591},
    {0.006261897426, 0.002260870847, 0.003131380307, 0.002791458183,
     0.001038049531, 0.001893917959, 0.002949177015, 0.003819521365,
     0.001100800912, 0.001716150165, 0.002434190706, 0.003104386139,
     0.000847632455, 0.001195904180, 0.001670397698, 0.012524165008,
     0.004695393160, 0.000286147117, 0.001025667373, 0.002373134246},
    {0.003735752688, 0.001779897849, 0.002237168191, 0.001886707235,
     0.000932287369, 0.001381521088, 0.002049363253, 0.002185818204,
     0.000744015818, 0.002683992649, 0.003337092433, 0.002360691115,
     0.001004584462, 0.001167245623, 0.001352022511, 0.004695393160,
     0.012524453183, 0.000287144142, 0.000940528155, 0.003660378402},
    {0.000404784037, 0.000265310579, 0.000161021675, 0.000161498946,
     0.000144869300, 0.000228499204, 0.000264084965, 0.000406753457,
     0.000151511190, 0.000360556333, 0.000733421681, 0.000272260749,
     0.000197602804, 0.000851298193, 0.000141505490, 0.000286147117,
     0.000287144142, 0.006479671265, 0.000886553355, 0.000357440337},
    {0.001298558985, 0.000918577576, 0.000695990541, 0.000595157039,
     0.000344932387, 0.000674510708, 0.000864998825, 0.000831463001,
     0.001515361861, 0.001366091300, 0.002210504676, 0.000996404634,
     0.000563431813, 0.004226922511, 0.000450817134, 0.001025667373,
     0.000940528155, 0.000886553355, 0.010185916203, 0.001555728244},
    {0.005124343367, 0.001588408095, 0.001203509685, 0.001320931409,
     0.001370634611, 0.001174481769, 0.001706373779, 0.001832653843,
     0.000650302833, 0.011965802769, 0.009545821406, 0.001952015271,
     0.002301832938, 0.002601386501, 0.001257818591, 0.002373134246,
     0.003660378402, 0.000357440337, 0.001555728244, 0.019815247974}
};


/* bound on error for sum of probabilities*/
static const double kProbSumTolerance = 0.000000001;


/* read an array of joint probabilities, and store them in fields of
 * NRrecord, including the mat_b field */
int
Blast_GetJointProbsForMatrix(double ** probs, double row_sums[],
                             double col_sums[], const char *matrixName)
{
    double sum;            /* sum of all joint probabilties -- should
                              be close to one */
    int i, j;              /* loop indices */
    /* The joint probabilities of the selected matrix */
    double (*joint_probs)[COMPOSITION_ALPHABET_SIZE];

    /* Choose the matrix */
    if (0 == strcmp("BLOSUM62", matrixName)) {
        joint_probs = BLOSUM62_JOINT_PROBS;
    } else {
        fprintf(stderr, "matrix %s is not supported "
                "for RE based adjustment\n", matrixName);
        return -1;
    }
    sum = 0.0;
    for (i = 0;  i < COMPOSITION_ALPHABET_SIZE;  i++) {
        for (j = 0;  j < COMPOSITION_ALPHABET_SIZE;  j++) {
            sum += joint_probs[i][j];
        }
    }
    assert(fabs(sum - 1.0) < kProbSumTolerance);
    /* Normalize and record the data */
    for (j = 0;  j < COMPOSITION_ALPHABET_SIZE;  j++) {
        col_sums[j] = 0.0;
    }
    for (i = 0;  i < COMPOSITION_ALPHABET_SIZE;  i++) {
        row_sums[i] = 0.0;
        for (j = 0;  j < COMPOSITION_ALPHABET_SIZE;  j++) {
            double probij = joint_probs[i][j];
            
            probs[i][j]  = probij/sum;
            row_sums[i] += probij/sum;
            col_sums[j] += probij/sum;
        }
    }
    return 0;
}


/* Set up adjusted frequencies of letters in probs_with_pseudo as a
 * weighted average of length X probArray and pseudocounts X
 * background_probs. The array normalized_freq is used in the
 * calculations in case probArray has not been normalized to sum to
 * 1.*/
void
Blast_ApplyPseudocounts(double * probs_with_pseudo,
                        int length,
                        double * normalized_probs,
                        const double * observed_freq,
                        const double * background_probs,
                        int pseudocounts)
{
    int i;                 /* loop index */
    double weight;         /* weight assigned to pseudocounts */
    double sum;            /* sum of the observed frequencies */

    /* Normalize probabilities */
    sum = 0.0;
    for (i = 0;  i < COMPOSITION_ALPHABET_SIZE;  i++) {
        sum += observed_freq[i];
    }
    if (sum > 0) {
        for (i = 0;  i < COMPOSITION_ALPHABET_SIZE;  i++) {
            normalized_probs[i] = observed_freq[i]/sum;
        }
    }
    weight = 1.0 * pseudocounts / (length + pseudocounts);
    for (i = 0;  i < COMPOSITION_ALPHABET_SIZE;  i++) {
        probs_with_pseudo[i] =
            (1.0 - weight) * normalized_probs[i] +
            weight * background_probs[i];
    }
}


void
Blast_ScoreMatrixFromFreq(double ** score, int alphsize, double ** freq,
                          const double row_sum[], const double col_sum[])
{
    int i, j;
    double sum;

    sum = 0.0;
    for (i = 0;  i < alphsize;  i++) {
        for (j = 0; j < alphsize;  j++) {
            sum +=  freq[i][j];
        }
    }
    for (i = 0;  i < alphsize;  i++) {
        for (j = 0; j < alphsize;  j++) {
            score[i][j] = log(freq[i][j] / sum / row_sum[i] / col_sum[j]);
        }
    }
}


/*compute the symmetric form of the relative entropy of two
 *probability vectors 
 *In this software relative entropy is expressed in "nats", 
 *meaning that logarithms are base e. In some other scientific 
 *and engineering domains where entropy is used, the logarithms 
 *are taken base 2 and the entropy is expressed in bits.
*/
double
Blast_GetRelativeEntropy(const double A[], const double B[])
{
    int i;                 /* loop index over letters */
    double temp;           /* intermediate term */
    double value = 0.0;    /* square of relative entropy */

    for (i = 0;  i < COMPOSITION_ALPHABET_SIZE;  i++) {
        temp = (A[i] + B[i]) / 2;
        if (temp > 0) {
            if (A[i] > 0) {
                value += A[i] * log(A[i] / temp) / 2;
            }
            if (B[i] > 0) {
                value += B[i] * log(B[i] / temp) / 2;
            }
        }
    }
    if (value < 0) {             /* must be numerical rounding error */
        value = 0;
    }
    return sqrt(value);
}

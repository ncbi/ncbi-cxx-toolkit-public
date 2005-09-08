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

File name: RE_interface_ds.c

Authors: E. Michael Gertz, Alejandro Schaffer, Yi-Kuo Yu

Contents: Sets up the optimization problem for conditional
          score matrix adjustment based on relative entropy of the
          two matching sequences.

******************************************************************************/

/* Code to find the new joint probability matrix given an original
   joint probabilities and new set(s) of background frequency(ies)
   The eta parameter is a Lagrangian multiplier to
   fix the relative entropy between the new target and the new background

   RE_FLAG is used to indicate various choices:
   1    no constraint on the relative entropy
   2    the final relative entropy equals to that of old matrix in new
        context
   3    the final relative entropy equals to that of old matrix in its
        own context
   4    the final relative entropy equals to a user specified value
*/

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <algo/blast/core/blast_toolkit.h>
#include <algo/blast/composition_adjustment/composition_adjustment.h>
#include <algo/blast/composition_adjustment/compo_mode_condition.h>


static double BLOSUM62_JOINT_PROBS[Alphsize][Alphsize] = {
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


/* read an array of joint probabilities, and store them in fields of
 * NRrecord, including the mat_b field */
int
record_joint_probs(char *matrixName,
                   NRitems * NRrecord)
{
    double sum;            /* sum of frequency ratios, should be 1 */
    int i, j;              /* loop indices */

    if (0 == strcmp("BLOSUM62", matrixName)) {
        sum = 0.0;
        for (i = 0;  i < Alphsize;  i++) {
            for (j = 0;  j < Alphsize;  j++) {
                NRrecord->mat_b[i][j] = BLOSUM62_JOINT_PROBS[i][j];

                sum                               += NRrecord->mat_b[i][j];
                NRrecord->first_standard_freq[i]  += NRrecord->mat_b[i][j];
                NRrecord->second_standard_freq[j] += NRrecord->mat_b[i][j];
            }
        }
        if (fabs(sum - 1.0) > PROB_SUM_TOLERANCE) {
            fprintf(stderr,
                    "the sum of joint probabilities in %s is %16.12f,"
                    " which is too far from 1\n",
                    matrixName, sum);
            exit(1);
        } else {
            for (i = 0;  i < Alphsize;  i++) {
                NRrecord->second_standard_freq[i] /= sum;
                NRrecord->first_standard_freq[i]  /= sum;
            }
            for (i = 0;  i < Alphsize;  i++) {
                for (j = 0;  j < Alphsize;  j++) {
                    NRrecord->mat_b[i][j] /= sum;
                }
            }
        }
        return 0;
    }
    fprintf(stderr, "matrix %s is not supported for RE based adjustment\n",
            matrixName);
    return -1;
}


/* Set up adjusted frequencies of letters in one_seq_freq as a
 * weighted average of length X probArray and pseudocounts X
 * background_probs. The array input_probs is used in the calculations
 * in case probArray has not been normalized to sum to 1.*/
void
assign_frequencies(int length,
                   double * probArray,
                   double * one_seq_freq,
                   double * background_probs,
                   double * input_probs,
                   int pseudocounts)
{
    int i;                 /* loop index */
    double weight;         /* weight assigned to pseudocounts */
    double sum;            /* sum of initial probabilities */

    /* Normalize probabilities */
    sum = 0.0;
    for (i = 0;  i < Alphsize;  i++) {
        input_probs[i] = probArray[i];
        sum += input_probs[i];
    }
    if (sum > 0) {
        for (i = 0;  i < Alphsize;  i++) {
            input_probs[i] /= sum;
        }
    }
    weight = 1.0 * pseudocounts / (length + pseudocounts);
    for (i = 0;  i < Alphsize;  i++) {
        one_seq_freq[i] =
            (1.0 - weight) * input_probs[i] + weight * background_probs[i];
    }
}


/* initialize the joint probabilities used later from inside
   NRrecord */
int
initializeNRprobabilities(NRitems * NRrecord,
                          char *matrixName)
{
    double re_o_implicit = 0.0;    /* implicit relative entropy
                                           of starting matrix */
    int i, j;                          /* loop indices */

    if (0 != strcmp("BLOSUM62", matrixName)) {
        fprintf(stderr,
                "Matrix %s not currently supported for RE based adjustment\n",
                matrixName);
        return (-1);
    }

    if (0 == record_joint_probs(matrixName, NRrecord)) {
        for (i = 0;  i < Alphsize;  i++) {
            for (j = 0;  j < Alphsize;  j++) {
                re_o_implicit +=
                    NRrecord->mat_b[i][j] * log(NRrecord->mat_b[i][j] /
                                                NRrecord->
                                                first_standard_freq[i] /
                                                NRrecord->
                                                second_standard_freq[j]);
                NRrecord->score_old[i][j] =
                    log(NRrecord->mat_b[i][j] /
                        NRrecord->first_standard_freq[i] /
                        NRrecord->second_standard_freq[j]);
            }
        }
        NRrecord->RE_o_implicit = re_o_implicit;
        return 0;
    } else
        return -1;
}


/**
 * RE_interface is the main function in this code file that provides
 * the connection between blastpgp and the code file kappa.c (from
 * where RE_interface is called) to the optimization procedures
 * matrixname is the underlying 20X20 score matrix; currently only
 * BLOSUM62 is supported
 *
 * - length1 and length2 are the adjusted lengths (not counting X) of
 *    the two sequences
 * - probArray1 and probaArray2 are the the two letter probability
 *   distributions pseudocounts are used later to adjust the probability
 *   distributions
 * - specifiedRE is a fixed relative entropy that might be used in one
 *   optimization constraint
 * - NRrecord stores all field needed for multidimensional Newton's
 *   method lambda is returned*/
double
RE_interface(char *matrixName,
             int length1,
             int length2,
             double * probArray1,
             double * probArray2,
             int pseudocounts,
             double specifiedRE,
             NRitems * NRrecord)
{
    int i;                         /* loop indices */
    double re_o_newcontext = 0.0;  /* relative entropy implied by
                                      input single sequence
                                      probabilities */
    static int total_iterations = 0;   /* total iterations among all
                                          calls to
                                          compute_new_score_matrix */
    int new_iterations = 0;        /* number of iterations in the most
                                      recent call to
                                      compute_new_score_matrix */
    static int max_iterations = 0; /* maximum number of iterations
                                      observed in a call to
                                      compute_new_score_matrix */
    double lambdaComputed = 1.0;   /* lambda computed by compute_lambda */

    assign_frequencies(length1, probArray1,
                       NRrecord->first_seq_freq_wpseudo,
                       NRrecord->first_standard_freq,
                       NRrecord->first_seq_freq, pseudocounts);
    /* plug in frequencies for second sequence, will be the matching
       sequence in BLAST */
    assign_frequencies(length2, probArray2,
                       NRrecord->second_seq_freq_wpseudo,
                       NRrecord->second_standard_freq,
                       NRrecord->second_seq_freq, pseudocounts);

    re_o_newcontext =
        compute_lambda(NRrecord, (NRrecord->flag == RE_OLDMAT_NEWCONTEXT),
                       &lambdaComputed);
    switch (NRrecord->flag) {
    case RE_NO_CONSTRAINT:
        break;
    case RE_OLDMAT_NEWCONTEXT:
        NRrecord->RE_final = re_o_newcontext;
        break;
    case RE_OLDMAT_OLDCONTEXT:
        NRrecord->RE_final = NRrecord->RE_o_implicit;
        break;
    case RE_USER_SPECIFIED:
        NRrecord->RE_final = specifiedRE;
        break;
    default:
        fprintf(stderr, "RE FLAG value of %d is unrecognized\n",
                NRrecord->flag);
        break;
    }
    new_iterations =
        score_matrix_direct_solve(NRrecord, NR_ERROR_TOLERANCE,
                                      NR_ITERATION_LIMIT);

    total_iterations += new_iterations;
    if(new_iterations > max_iterations)
        max_iterations = new_iterations;

    if(NR_ITERATION_LIMIT < new_iterations) {
        fprintf(stderr, "bad probabilities from sequence 1, length %d\n",
                length1);
        for (i = 0;  i < Alphsize;  i++)
            fprintf(stderr, "%15.12f\n", probArray1[i]);
        fprintf(stderr, "bad probabilities from sequence 2, length %d\n",
                length2);
        for (i = 0;  i < Alphsize;  i++)
            fprintf(stderr, "%15.12f\n", probArray2[i]);
        fflush(stderr);
        return (-1);
    }
    /*  else {
       printf("test probabilities from sequence 1, length %d\n", length1);
       for(i = 0; i < Alphsize; i++)
       printf("%15.12lf\n",probArray1[i]);
       printf("test probabilities from sequence 2, length %d\n", length2);
       for(i = 0; i < Alphsize; i++)
       printf("%15.12lf\n",probArray2[i]);
       fflush(stdout);
       } */

    if (NRrecord->flag == RE_NO_CONSTRAINT) {
        /* Compute the unconstrained relative entropy */
        double re_free = compute_lambda(NRrecord, 1, &lambdaComputed);
        printf("RE_uncons  = %6.4f\n\n", re_free);
    }

    return (lambdaComputed);
}


void disp_RE_data(NRitems * NRrecord)
{
    int j;

     /*next three variables are arguments to pass to
       compute_new_score_matrix */
    double H_01, H_0B, H_1B;       /*relative entropies */
    double ratio, corr_factor;     /*ratio and correlation of relative
                                     entropies */
   /* RE between first seq and BLOSUM 62 background */
    H_0B = Blast_GetRelativeEntropy(NRrecord->first_seq_freq,
                                    NRrecord->first_standard_freq);
    H_1B = Blast_GetRelativeEntropy(NRrecord->second_seq_freq,
                                    NRrecord->second_standard_freq);
    /* RE between first seq and BLOSUM 62 background */
    H_01 = Blast_GetRelativeEntropy(NRrecord->second_seq_freq,
                                    NRrecord->first_seq_freq);

    /* New way to define RE between 1 and 2 */
    corr_factor = 0.0;
    for (j = 0;  j < Alphsize;  j++) {
        corr_factor +=
            (NRrecord->first_seq_freq[j] -
             NRrecord->first_standard_freq[j]) *
            (NRrecord->second_seq_freq[j] -
             NRrecord->second_standard_freq[j]);
    }
    ratio = 2.0 * H_01 / (H_0B + H_1B);
    /*
    printf("H_01 (re between two sequence composition freq.) = %6.4f\n",
           H_01);
    printf("H_0B (re between 1st seq freq and BLOSUM implicit freq) "
           "= %6.4f\n", H_0B);
    printf("H_1B (re between 2nd seq freq and BLOSUM implicit freq) ="
           " %6.4f\n", H_1B);
    printf("corr_factor of the frequencies between two seqs = "
           "%6.4lf\n", corr_factor);
    printf("Ratio of 2*H_01/(H_0B+H_1B) = %6.4f\n", ratio);
    */
}

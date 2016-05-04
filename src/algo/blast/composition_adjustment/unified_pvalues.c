/*   ==========================================================================
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
 * ==========================================================================*/

/** @file unified_pvalues.c
 * Procedures for computing a "composition" p-value of a match, and
 * for computing a unified p-value combining the customary alignment
 * p-value and the new composition p-value.
 *
 * @author Yi-Kuo Yu, Alejandro Schaffer, Mike Gertz
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <algo/blast/composition_adjustment/composition_constants.h>
#include <algo/blast/composition_adjustment/unified_pvalues.h>

/** @{ The next two constants are used as thresholds in one test of whether
 *     to use compositional p-values */
#define LENGTH_MIN             40
#define LENGTH_MAX             4000
/** @} */

#define LAMBDA_BIN_SIZE      0.001      /**< bin size when discretizing lambda
                                             distribution function */
#define LAMBDA_BIN_TOTAL       565      /**< total number of bins for the
                                             lambda distribution function */
/** Marks the boundary of the left tail of the lambda distribution;
    values of lambda that lie in the left tail are treated all same */
#define LOW_LAMBDA_BIN_CUT     35


/**
 * P_lambda_table is a discretized version of the empirical cumulative
 * distribution function for the statistical parameter lambda for
 * pairs of unrelated sequences in the Astral data set*/
static double
P_lambda_table[LAMBDA_BIN_TOTAL] =
    { 1.247750e-07, 1.247750e-07, 1.247750e-07, 1.247750e-07, 1.247750e-07,
      1.247750e-07, 1.247750e-07, 1.247750e-07, 1.247750e-07, 1.247750e-07,
      1.247750e-07, 1.247750e-07, 2.495500e-07,
      2.495500e-07, 2.495500e-07, 2.495500e-07, 2.495500e-07, 2.495500e-07,
      2.495500e-07, 2.495500e-07, 2.495500e-07,
      2.495500e-07, 3.743250e-07, 4.991000e-07, 4.991000e-07, 4.991000e-07,
      4.991000e-07, 7.486490e-07, 7.486490e-07,
      7.486490e-07, 7.486490e-07, 8.734240e-07, 9.981990e-07, 9.981990e-07,
      1.122974e-06, 1.122974e-06, 1.372523e-06,
      1.497298e-06, 1.622073e-06, 1.871622e-06, 1.871622e-06, 1.996397e-06,
      2.370721e-06, 2.620270e-06, 2.620270e-06,
      2.620270e-06, 2.745045e-06, 2.745045e-06, 2.745045e-06, 2.745045e-06,
      2.869820e-06, 3.493695e-06, 3.493695e-06,
      3.493695e-06, 3.868019e-06, 4.117568e-06, 4.367117e-06, 4.866216e-06,
      4.866216e-06, 5.240540e-06, 5.240540e-06,
      5.864415e-06, 6.363514e-06, 6.737838e-06, 7.236937e-06, 7.860812e-06,
      8.110361e-06, 8.484685e-06, 9.233334e-06,
      9.857209e-06, 1.035631e-05, 1.060586e-05, 1.085541e-05, 1.210316e-05,
      1.272703e-05, 1.322613e-05, 1.409955e-05,
      1.509775e-05, 1.647027e-05, 1.784279e-05, 1.859144e-05, 1.921532e-05,
      2.008874e-05, 2.133649e-05, 2.245946e-05,
      2.333288e-05, 2.445585e-05, 2.557882e-05, 2.732567e-05, 2.832387e-05,
      3.019549e-05, 3.231666e-05, 3.468738e-05,
      3.568558e-05, 3.718288e-05, 3.942882e-05, 4.292251e-05, 4.479413e-05,
      4.666575e-05, 4.903647e-05, 5.140719e-05,
      5.352836e-05, 5.539998e-05, 5.714683e-05, 5.964232e-05, 6.263691e-05,
      6.550673e-05, 6.825177e-05, 7.124636e-05,
      7.411618e-05, 7.611258e-05, 7.910717e-05, 8.235131e-05, 8.534590e-05,
      8.971302e-05, 9.457923e-05, 9.907112e-05,
      1.028144e-04, 1.068072e-04, 1.124220e-04, 1.150423e-04, 1.202828e-04,
      1.241509e-04, 1.297657e-04, 1.352558e-04,
      1.408707e-04, 1.474838e-04, 1.535977e-04, 1.605851e-04, 1.679469e-04,
      1.766811e-04, 1.849162e-04, 1.943991e-04,
      2.023847e-04, 2.111190e-04, 2.208514e-04, 2.298352e-04, 2.390685e-04,
      2.500487e-04, 2.599059e-04, 2.705118e-04,
      2.826149e-04, 2.953419e-04, 3.058230e-04, 3.209207e-04, 3.336477e-04,
      3.469986e-04, 3.607238e-04, 3.758215e-04,
      3.915431e-04, 4.100098e-04, 4.283517e-04, 4.454458e-04, 4.661584e-04,
      4.845003e-04, 5.032165e-04, 5.194372e-04,
      5.399003e-04, 5.642314e-04, 5.905589e-04, 6.173855e-04, 6.407184e-04,
      6.646751e-04, 6.896300e-04, 7.199503e-04,
      7.487733e-04, 7.780954e-04, 8.119093e-04, 8.442260e-04, 8.782895e-04,
      9.142246e-04, 9.507836e-04, 9.890894e-04,
      1.025898e-03, 1.060710e-03, 1.102010e-03, 1.141065e-03, 1.179870e-03,
      1.225413e-03, 1.268086e-03, 1.313254e-03,
      1.362665e-03, 1.413199e-03, 1.465479e-03, 1.514516e-03, 1.568668e-03,
      1.620325e-03, 1.678595e-03, 1.738861e-03,
      1.800375e-03, 1.863511e-03, 1.933760e-03, 1.999641e-03, 2.066271e-03,
      2.136269e-03, 2.208639e-03, 2.282256e-03,
      2.363235e-03, 2.451451e-03, 2.535050e-03, 2.626385e-03, 2.715973e-03,
      2.811676e-03, 2.906879e-03, 3.008695e-03,
      3.108390e-03, 3.220687e-03, 3.337351e-03, 3.458507e-03, 3.587275e-03,
      3.717165e-03, 3.853419e-03, 3.998407e-03,
      4.150882e-03, 4.307349e-03, 4.470305e-03, 4.650105e-03, 4.835520e-03,
      5.028047e-03, 5.239291e-03, 5.459394e-03,
      5.699835e-03, 5.944019e-03, 6.201679e-03, 6.472939e-03, 6.767532e-03,
      7.067490e-03, 7.402760e-03, 7.779704e-03,
      8.163261e-03, 8.581007e-03, 9.043672e-03, 9.538778e-03, 1.006046e-02,
      1.061571e-02, 1.121351e-02, 1.187918e-02,
      1.259427e-02, 1.336213e-02, 1.420611e-02, 1.510486e-02, 1.609220e-02,
      1.716439e-02, 1.831930e-02, 1.961309e-02,
      2.100770e-02, 2.254767e-02, 2.423026e-02, 2.604985e-02, 2.803414e-02,
      3.023304e-02, 3.261836e-02, 3.527531e-02,
      3.818206e-02, 4.134298e-02, 4.483305e-02, 4.870456e-02, 5.296474e-02,
      5.765964e-02, 6.281384e-02, 6.850944e-02,
      7.481781e-02, 8.173895e-02, 8.937667e-02, 9.774769e-02, 1.069522e-01,
      1.170782e-01, 1.282548e-01, 1.405027e-01,
      1.539673e-01, 1.686191e-01, 1.846574e-01, 2.020771e-01, 2.210459e-01,
      2.415374e-01, 2.637095e-01, 2.872893e-01,
      3.124426e-01, 3.391945e-01, 3.673232e-01, 3.966229e-01, 4.269345e-01,
      4.580085e-01, 4.895437e-01, 5.211873e-01,
      5.525534e-01, 5.832051e-01, 6.129409e-01, 6.414552e-01, 6.685366e-01,
      6.941538e-01, 7.180993e-01, 7.403272e-01,
      7.608830e-01, 7.798100e-01, 7.972520e-01, 8.132446e-01, 8.277589e-01,
      8.410031e-01, 8.531792e-01, 8.642644e-01,
      8.743145e-01, 8.835475e-01, 8.919562e-01, 8.996102e-01, 9.066574e-01,
      9.130520e-01, 9.189736e-01, 9.244128e-01,
      9.293928e-01, 9.339722e-01, 9.381913e-01, 9.421451e-01, 9.457657e-01,
      9.491230e-01, 9.522394e-01, 9.551589e-01,
      9.578458e-01, 9.603525e-01, 9.626876e-01, 9.648771e-01, 9.669361e-01,
      9.688645e-01, 9.706878e-01, 9.723798e-01,
      9.739466e-01, 9.754310e-01, 9.768135e-01, 9.781311e-01, 9.793539e-01,
      9.805091e-01, 9.816204e-01, 9.826387e-01,
      9.835868e-01, 9.844858e-01, 9.853414e-01, 9.861397e-01, 9.869005e-01,
      9.876234e-01, 9.882862e-01, 9.889344e-01,
      9.895357e-01, 9.901063e-01, 9.906435e-01, 9.911373e-01, 9.916119e-01,
      9.920576e-01, 9.924838e-01, 9.928730e-01,
      9.932470e-01, 9.935931e-01, 9.939352e-01, 9.942430e-01, 9.945419e-01,
      9.948185e-01, 9.950818e-01, 9.953442e-01,
      9.955881e-01, 9.958246e-01, 9.960354e-01, 9.962523e-01, 9.964441e-01,
      9.966252e-01, 9.967933e-01, 9.969595e-01,
      9.971159e-01, 9.972685e-01, 9.974102e-01, 9.975482e-01, 9.976761e-01,
      9.977899e-01, 9.978979e-01, 9.979996e-01,
      9.980933e-01, 9.981938e-01, 9.982869e-01, 9.983711e-01, 9.984526e-01,
      9.985349e-01, 9.986089e-01, 9.986793e-01,
      9.987437e-01, 9.988027e-01, 9.988587e-01, 9.989131e-01, 9.989656e-01,
      9.990185e-01, 9.990667e-01, 9.991120e-01,
      9.991549e-01, 9.991993e-01, 9.992385e-01, 9.992718e-01, 9.993088e-01,
      9.993393e-01, 9.993723e-01, 9.993982e-01,
      9.994278e-01, 9.994549e-01, 9.994825e-01, 9.995078e-01, 9.995336e-01,
      9.995536e-01, 9.995732e-01, 9.995946e-01,
      9.996155e-01, 9.996358e-01, 9.996535e-01, 9.996697e-01, 9.996842e-01,
      9.996983e-01, 9.997115e-01, 9.997263e-01,
      9.997404e-01, 9.997520e-01, 9.997612e-01, 9.997721e-01, 9.997834e-01,
      9.997917e-01, 9.998010e-01, 9.998097e-01,
      9.998197e-01, 9.998272e-01, 9.998347e-01, 9.998427e-01, 9.998504e-01,
      9.998564e-01, 9.998629e-01, 9.998688e-01,
      9.998745e-01, 9.998795e-01, 9.998834e-01, 9.998882e-01, 9.998937e-01,
      9.998978e-01, 9.999021e-01, 9.999067e-01,
      9.999094e-01, 9.999134e-01, 9.999174e-01, 9.999209e-01, 9.999240e-01,
      9.999275e-01, 9.999312e-01, 9.999336e-01,
      9.999374e-01, 9.999390e-01, 9.999415e-01, 9.999435e-01, 9.999452e-01,
      9.999480e-01, 9.999506e-01, 9.999517e-01,
      9.999541e-01, 9.999555e-01, 9.999570e-01, 9.999581e-01, 9.999595e-01,
      9.999608e-01, 9.999623e-01, 9.999636e-01,
      9.999655e-01, 9.999665e-01, 9.999680e-01, 9.999691e-01, 9.999702e-01,
      9.999712e-01, 9.999718e-01, 9.999733e-01,
      9.999739e-01, 9.999751e-01, 9.999761e-01, 9.999771e-01, 9.999782e-01,
      9.999792e-01, 9.999801e-01, 9.999807e-01,
      9.999813e-01, 9.999816e-01, 9.999826e-01, 9.999831e-01, 9.999834e-01,
      9.999836e-01, 9.999838e-01, 9.999841e-01,
      9.999846e-01, 9.999854e-01, 9.999856e-01, 9.999859e-01, 9.999870e-01,
      9.999874e-01, 9.999882e-01, 9.999885e-01,
      9.999890e-01, 9.999892e-01, 9.999892e-01, 9.999893e-01, 9.999895e-01,
      9.999900e-01, 9.999904e-01, 9.999907e-01,
      9.999909e-01, 9.999914e-01, 9.999917e-01, 9.999923e-01, 9.999923e-01,
      9.999925e-01, 9.999928e-01, 9.999932e-01,
      9.999934e-01, 9.999937e-01, 9.999939e-01, 9.999942e-01, 9.999948e-01,
      9.999950e-01, 9.999950e-01, 9.999952e-01,
      9.999954e-01, 9.999955e-01, 9.999957e-01, 9.999957e-01, 9.999959e-01,
      9.999959e-01, 9.999960e-01, 9.999960e-01,
      9.999962e-01, 9.999963e-01, 9.999965e-01, 9.999965e-01, 9.999967e-01,
      9.999967e-01, 9.999968e-01, 9.999973e-01,
      9.999973e-01, 9.999974e-01, 9.999975e-01, 9.999977e-01, 9.999977e-01,
      9.999977e-01, 9.999977e-01, 9.999979e-01,
      9.999979e-01, 9.999979e-01, 9.999982e-01, 9.999983e-01, 9.999984e-01,
      9.999985e-01, 9.999985e-01, 9.999985e-01,
      9.999985e-01, 9.999985e-01, 9.999985e-01, 9.999985e-01, 9.999985e-01,
      9.999987e-01, 9.999987e-01, 9.999988e-01,
      9.999988e-01, 9.999988e-01, 9.999988e-01, 9.999989e-01, 9.999989e-01,
      9.999989e-01, 9.999989e-01, 9.999992e-01
    };


/* Documented in unified_pvalues.h */
double
Blast_CompositionPvalue(double lambda)
{
    /* Let initialUScore be lambda, scaled and shifted to fit the
     * bins of P_lambda_table. */
    double initialUScore = (lambda - COMPO_MIN_LAMBDA) / LAMBDA_BIN_SIZE;

    if (initialUScore < LOW_LAMBDA_BIN_CUT) {
        /* Values of lambda less than
         *     LOW_LAMBDA_BIN_CUT * LAMBDA_BIN_SIZE + COMPO_MIN_LAMBDA
         * all receive the same p-value.
         */
        return P_lambda_table[LOW_LAMBDA_BIN_CUT];
    } else if (initialUScore > LAMBDA_BIN_TOTAL - 1) {
        /* Values of initialUScore strictly larger than the largest
         * value in the table all receive p-value 1.0 */
        return 1.0;
    } else {
        /* floor of initialUScore is the bin in the table */
        int bin = ((int) initialUScore);
        if (bin == LAMBDA_BIN_TOTAL - 1) {
            /* In the unlikely event that bin is exactly the
             * largest item in the table, just return that value. */
            return P_lambda_table[LAMBDA_BIN_TOTAL - 1];
        } else {
            /* Return a value using linear interpolation; delta is the
             * difference between the discrete table bin and the
             * actual desired value */
            double delta = initialUScore - bin;
            return (1.0 - delta) * P_lambda_table[bin] +
                delta * P_lambda_table[bin + 1];
        }
    }
}


/* Documented in unified_pvalues.h */
double
Blast_Overall_P_Value(double p_comp,
                      double p_alignment)
{
    double product;               /* the product of the two input p-values */

    product = p_comp * p_alignment;
    if (product > 0) {
        return product * (1.0 - log(product));
    } else {
        /* product is either correctly 0, in which case the overall P
         * is also corectly zero; or incorrectly nonpositive (possibly
         * NaN), in which case we return the nonsense value so as not
         * to hide the error */
        return product;
    }
}

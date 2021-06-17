/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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

File name: gumbel_params.cpp

Author: Greg Boratyn, Sergey Sheetlin

Contents: Implementation of Gumbel parameters computation wrapper

******************************************************************************/

#include <ncbi_pch.hpp>

#include "sls_alp.hpp"
#include "sls_alp_data.hpp"
#include "sls_alp_regression.hpp"
#include "sls_alp_sim.hpp"
#include "sls_pvalues.hpp"
#include "njn_localmaxstatmatrix.hpp"
#include "njn_localmaxstatutil.hpp"

#include <algo/blast/gumbel_params/gumbel_params.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(blast);

///////////////////////////////////////////////////////////////////////////////
//CGumbelParamsOptions

CGumbelParamsOptions::CGumbelParamsOptions(void) 
{
    x_Init();
}

CGumbelParamsOptions::CGumbelParamsOptions(
                                   const CGeneralScoreMatrix* matrix,
                                   const vector<double>& seq1_probs,
                                   const vector<double>& seq2_probs)
{
    x_Init();
    m_ScoreMatrix.Reset(matrix);
    m_NumResidues = (*m_ScoreMatrix).GetNumResidues();
    m_Seq1ResidueProbs = seq1_probs;
    m_Seq2ResidueProbs = seq2_probs;
}


CGumbelParamsOptions::CGumbelParamsOptions(
                                       const CGumbelParamsOptions& options) 
    : m_GapOpening(options.m_GapOpening),
      m_GapExtension(options.m_GapExtension),
      m_LambdaAccuracy(options.m_LambdaAccuracy),
      m_KAccuracy(options.m_KAccuracy),
      m_ScoreMatrix(options.m_ScoreMatrix),
      m_Seq1ResidueProbs(options.m_Seq1ResidueProbs),
      m_Seq2ResidueProbs(options.m_Seq2ResidueProbs),
      m_NumResidues(options.m_NumResidues),
      m_MaxCalcTime(options.m_MaxCalcTime),
      m_MaxCalcMemory(options.m_MaxCalcMemory)
{}


bool CGumbelParamsOptions::Validate(void)
{
    const Int4 kMaxScore = 1000;

    if (m_Seq1ResidueProbs.size() != m_Seq2ResidueProbs.size()) {
        NCBI_THROW(CGumbelParamsException, eInvalidOptions, 
                   "Length of residue probabilities for both sequences do "
                   "not match");
    }

    if ((unsigned)m_NumResidues != m_Seq1ResidueProbs.size()) {
        NCBI_THROW(CGumbelParamsException, eInvalidOptions,
                   "Length of residue probabilities does not match number "
                   "of residues");
    }

    double sum_probs_seq1 = 0.0;
    ITERATE(vector<double>, it, m_Seq1ResidueProbs) {
        if (*it < 0.0) {
            NCBI_THROW(CGumbelParamsException, eInvalidOptions,
                       "Negative probability value for sequence 1");
        }
        sum_probs_seq1 += *it;
    }
    if (fabs(1.0 - sum_probs_seq1) > 1e-10) {
        NCBI_THROW(CGumbelParamsException, eInvalidOptions,
                   "The sum of probability values does not equal 1");
    }

    double sum_probs_seq2 = 0.0;
    ITERATE(vector<double>, it, m_Seq2ResidueProbs) {
        if (*it < 0.0) {
            NCBI_THROW(CGumbelParamsException, eInvalidOptions,
                       "Negative probability value for sequence 2");
        }
        sum_probs_seq2 += *it;
    }
    if (fabs(1.0 - sum_probs_seq2) > 1e-10) {
        NCBI_THROW(CGumbelParamsException, eInvalidOptions,
                   "The sum of probability values does not equal 1");
    }

    if (m_GapOpening < 0) {
        NCBI_THROW(CGumbelParamsException, eInvalidOptions,
                   "Negative gap opening penalty");
    }

    if (m_GapExtension <= 0) {
        NCBI_THROW(CGumbelParamsException, eInvalidOptions,
                   "Negative gap extention penalty");
    }

    if (m_LambdaAccuracy <= 0.0) {
        NCBI_THROW(CGumbelParamsException, eInvalidOptions,
                   "Non-positive accuracy for lambda");
    }

    if (m_KAccuracy <= 0.0) {
        NCBI_THROW(CGumbelParamsException, eInvalidOptions,
                   "Non-positive accuracy for K");
    }

    if (m_MaxCalcTime <= 0.0) {
        NCBI_THROW(CGumbelParamsException, eInvalidOptions,
                   "Non-positive max calc time");
    }

    if (m_MaxCalcMemory <= 0.0) {
        NCBI_THROW(CGumbelParamsException, eInvalidOptions,
                   "Non-positive maximum memory");
    }

    if(abs(m_GapOpening) > kMaxScore || abs(m_GapExtension) > kMaxScore) {
        NCBI_THROW(CGumbelParamsException, eInvalidOptions,
                   "Gap opening or extension too large");
    }

    for(Uint4 i=0;i < m_ScoreMatrix->GetNumResidues();i++) {
        for(Uint4 j=0;j < m_ScoreMatrix->GetNumResidues();j++) {
            if(abs(m_ScoreMatrix->GetScore(i, j)) > kMaxScore) {
                NCBI_THROW(CGumbelParamsException, eInvalidOptions,
                           "Score matrix entry too large");
            }
        }
    }

    // Warnings
    bool result = true;

    if (m_LambdaAccuracy < 0.001 || m_LambdaAccuracy > 0.01) {
        m_Messages.push_back("The algorithm is designed for lambda accuracy"
                            " between 0.001 -- 0.01. Values outside this range"
                             " may cause increased bias or variance of"
                             " estimated parameter lambda");
        result = false;
    }

    if (m_KAccuracy < 0.005 || m_KAccuracy > 0.05) {
        m_Messages.push_back("The algorithm is designed for K accuracy"
                            " between 0.005 -- 0.05. Values outside this range"
                             " may cause increased bias or variance of"
                             " estimated parameter K");
        result = false;

    }

    if (m_MaxCalcTime < 1.0) {
        m_Messages.push_back("The maximum calculation time may be too small"
                             " and lead to inaccurate results. The suggested"
                             " maximum calculation time is at least 1s.");
        result = false;
    }

    if (m_MaxCalcMemory < 1.0) {
        m_Messages.push_back("The maximum allowed memory may be too small"
                             " and lead to inaccurate results. The suggsets"
                             " maximum allowed memory is at least 1Mb");
        result = false;
    }

    return result;
}


void CGumbelParamsOptions::SetScoreMatrix(const CGeneralScoreMatrix& matrix)
{
    m_ScoreMatrix.Reset(new CGeneralScoreMatrix(matrix));
}

void CGumbelParamsOptions::SetScoreMatrix(
                                          const CRef<CGeneralScoreMatrix>& matrix)
{
    m_ScoreMatrix.Reset(matrix.GetNonNullPointer());
    m_NumResidues = (*m_ScoreMatrix).GetNumResidues();   
}

void CGumbelParamsOptions::x_Init(void)
{
    m_GapOpening = 0;
    m_GapExtension = 0;
    m_LambdaAccuracy = 0;
    m_KAccuracy = 0;
    m_IsGapped = true;

    m_NumResidues = 0;

    m_MaxCalcTime = 1.0;
    m_MaxCalcMemory = 1000.0;
}

///////////////////////////////////////////////////////////////////////////////
//CGumbelParamsOptionsFactory

CRef<CGumbelParamsOptions> 
CGumbelParamsOptionsFactory::CreateStandard20AAOptions(
                                    CGeneralScoreMatrix::EScoreMatrixName smat)
{
    const size_t kNumResidues = 20;
    CRef<CGeneralScoreMatrix> 
        smatrix(new CGeneralScoreMatrix(smat, kNumResidues));

    CGumbelParamsOptions::TFrequencies probs(kNumResidues);

    probs[ 0 ] = 0.07805 ;
    probs[ 1 ] = 0.05129 ;
    probs[ 2 ] = 0.04487 ;
    probs[ 3 ] = 0.05364 ;
    probs[ 4 ] = 0.01925 ;
    probs[ 5 ] = 0.04264 ;
    probs[ 6 ] = 0.06295 ;
    probs[ 7 ] = 0.07377 ;
    probs[ 8 ] = 0.02199 ;
    probs[ 9 ] = 0.05142 ;
    probs[ 10 ] = 0.09019 ;
    probs[ 11 ] = 0.05744 ;
    probs[ 12 ] = 0.02243 ;
    probs[ 13 ] = 0.03856 ;
    probs[ 14 ] = 0.05203 ;
    probs[ 15 ] = 0.0712 ;
    probs[ 16 ] = 0.05841 ;
    probs[ 17 ] = 0.0133 ;
    probs[ 18 ] = 0.03216 ;
    probs[ 19 ] = 0.06441 ;

    CGumbelParamsOptions* options = new CGumbelParamsOptions();
    options->SetScoreMatrix(smatrix);
    options->SetGapOpening(11);
    options->SetGapExtension(1);
    options->SetLambdaAccuracy(0.001);
    options->SetKAccuracy(0.005);
    options->SetGapped(true);//gapped calculation
    options->SetMaxCalcTime(1.0);
    options->SetMaxCalcMemory(1000.0);
    options->SetSeq1ResidueProbs(probs);
    options->SetSeq2ResidueProbs(probs);

    return CRef<CGumbelParamsOptions>(options);
}

CRef<CGumbelParamsOptions>
CGumbelParamsOptionsFactory::CreateBasicOptions(void)
{
    CGumbelParamsOptions* options = new CGumbelParamsOptions();
    options->SetGapOpening(11);
    options->SetGapExtension(1);
    options->SetLambdaAccuracy(0.001);
    options->SetKAccuracy(0.05);
    options->SetGapped(true);    
    options->SetMaxCalcTime(1.0);
    options->SetMaxCalcMemory(1000.0);

    return CRef<CGumbelParamsOptions>(options);
}

///////////////////////////////////////////////////////////////////////////////
//CGumbelParamsCalc

CGumbelParamsCalc::CGumbelParamsCalc(const CRef<CGumbelParamsOptions>& options)
    : m_Options(options)
{}

CGumbelParamsCalc::CGumbelParamsCalc(const CRef<CGumbelParamsOptions>& options,
                                     const CRef<CGumbelParamsRandDiagnostics>& rand)
    : m_Options(options), m_RandParams(rand)
{}


template <typename T>
static void s_CopyVector(const vector<T>& in, vector<T>& out)
{
    out.resize(in.size());
    copy(in.begin(), in.end(), out.begin());
}


CRef<CGumbelParamsResult> CGumbelParamsCalc::Run(void)
{
    CGumbelParamsResult* result;
    Int4 number_of_aa = -1;
    
    try {

        double current_time1;
        double current_time2;
        Sls::alp_data::get_current_time(current_time1);
        
        number_of_aa = m_Options->GetNumResidues();
        _ASSERT(number_of_aa > 0);

        result = new CGumbelParamsResult();
        SGumbelParams& gumbel_params = result->SetGumbelParams();
        CGumbelParamsResult::SSbsArrays& sbs_arrays = result->SetSbsArrays();

        //Gapless parameters calculation

        double gapless_time_portion = 0.5;

        const Int4** scoring_matrix = m_Options->GetScoreMatrix();

        const double* background_probabilities1
            = &m_Options->GetSeq1ResidueProbs()[0];

        const double* background_probabilities2
            = &m_Options->GetSeq2ResidueProbs()[0];

        double gapless_calc_time = m_Options->GetMaxCalcTime();

        if(m_Options->GetGapped()) {
            // Gapless calculation may take only a portion of maximum allowed
            // calculation time in the case of gapped calculation 
            gapless_calc_time *= gapless_time_portion;
        }
                
        Njn::LocalMaxStatMatrix local_max_stat_matrix(number_of_aa,
                                              scoring_matrix,
                                              background_probabilities1,
                                              background_probabilities2,
                                              number_of_aa,
                                              gapless_calc_time);

        if(local_max_stat_matrix.getTerminated()) {
            throw Sls::error("Please increase maximum allowed calculation time.",
                             1);
        }

        //calculation of a and alpha
        double calculation_error = 1e-6;

        gumbel_params.gapless_alpha = local_max_stat_matrix.getAlpha();
        gumbel_params.gapless_alpha_error = calculation_error;

        gumbel_params.gapless_a = local_max_stat_matrix.getA();
        gumbel_params.gapless_a_error = calculation_error;

        if(!m_Options->GetGapped()) {

            //calculation of all required parameters for a gapless case
            gumbel_params.G=0;

            gumbel_params.lambda = local_max_stat_matrix.getLambda ();
            gumbel_params.lambda_error = calculation_error;

            gumbel_params.K = local_max_stat_matrix.getK ();
            gumbel_params.K_error = calculation_error;
                        
            gumbel_params.C = local_max_stat_matrix.getC ();;
            gumbel_params.C_error = calculation_error;

            gumbel_params.sigma = gumbel_params.gapless_alpha;
            gumbel_params.sigma_error = calculation_error;

            gumbel_params.alpha_i = gumbel_params.gapless_alpha;
            gumbel_params.alpha_i_error = calculation_error;

            gumbel_params.alpha_j = gumbel_params.gapless_alpha;
            gumbel_params.alpha_j_error = calculation_error;

            gumbel_params.ai = gumbel_params.gapless_a;
            gumbel_params.ai_error = calculation_error;

            gumbel_params.aj = gumbel_params.gapless_a;
            gumbel_params.aj_error = calculation_error;

            Sls::alp_data::get_current_time(current_time2);
            result->SetCalcTime(current_time2 - current_time1);

            sbs_arrays.lambda_sbs.resize(2);
            sbs_arrays.lambda_sbs[0]=gumbel_params.lambda;
            sbs_arrays.lambda_sbs[1]=gumbel_params.lambda + calculation_error;

            sbs_arrays.K_sbs.resize(2);
            sbs_arrays.K_sbs[0]=gumbel_params.K;
            sbs_arrays.K_sbs[1]=gumbel_params.K+calculation_error;

            sbs_arrays.C_sbs.resize(2);
            sbs_arrays.C_sbs[0]=gumbel_params.C;
            sbs_arrays.C_sbs[1]=gumbel_params.C+calculation_error;

            sbs_arrays.sigma_sbs.resize(2);
            sbs_arrays.sigma_sbs[0]=gumbel_params.sigma;
            sbs_arrays.sigma_sbs[1]=gumbel_params.sigma + calculation_error;

            sbs_arrays.alpha_i_sbs.resize(2);
            sbs_arrays.alpha_i_sbs[0]=gumbel_params.alpha_i;
            sbs_arrays.alpha_i_sbs[1]=gumbel_params.alpha_i + calculation_error;

            sbs_arrays.alpha_j_sbs.resize(2);
            sbs_arrays.alpha_j_sbs[0]=gumbel_params.alpha_j;
            sbs_arrays.alpha_j_sbs[1]=gumbel_params.alpha_j + calculation_error;

            sbs_arrays.ai_sbs.resize(2);
            sbs_arrays.ai_sbs[0]=gumbel_params.ai;
            sbs_arrays.ai_sbs[1]=gumbel_params.ai + calculation_error;

            sbs_arrays.aj_sbs.resize(2);
            sbs_arrays.aj_sbs[0]=gumbel_params.aj;
            sbs_arrays.aj_sbs[1]=gumbel_params.aj + calculation_error;

            // New TRandParams object is created only if m_Options does
            // not contain one already.
            if (m_RandParams.Empty()) {

                m_RandParams.Reset(new CGumbelParamsRandDiagnostics());
                m_RandParams->SetRandomSeed(0);
                m_RandParams->SetTotalReNumber(0);
                m_RandParams->SetTotalReNumberKilling(0);
            }
        }
        else {
            double current_time_gapless_preliminary;
            Sls::alp_data::get_current_time(current_time_gapless_preliminary);
            double gapless_preliminary_time =
                current_time_gapless_preliminary - current_time1;

            // TODO: There should be constructors for two cases with and without
            // rand params
            Sls::alp_data data_obj(m_Options, m_RandParams);
            data_obj.d_max_time =
                Sls::alp_data::Tmax((1.0 - gapless_time_portion)
                                    * data_obj.d_max_time,data_obj.d_max_time
                                    - gapless_preliminary_time);

            Sls::alp_sim sim_obj(&data_obj);
            gumbel_params.G = m_Options->GetGapOpening()
                + m_Options->GetGapExtension();
            
            gumbel_params.lambda = sim_obj.m_Lambda;
            gumbel_params.lambda_error = sim_obj.m_LambdaError;

            gumbel_params.K = sim_obj.m_K;
            gumbel_params.K_error = sim_obj.m_KError;
                        
            gumbel_params.C = sim_obj.m_C;
            gumbel_params.C_error = sim_obj.m_CError;

            gumbel_params.sigma = sim_obj.m_Sigma;
            gumbel_params.sigma_error = sim_obj.m_SigmaError;

            gumbel_params.alpha_i = sim_obj.m_AlphaI;
            gumbel_params.alpha_i_error = sim_obj.m_AlphaIError;

            gumbel_params.alpha_j = sim_obj.m_AlphaJ;
            gumbel_params.alpha_j_error = sim_obj.m_AlphaJError;

            gumbel_params.ai = sim_obj.m_AI;
            gumbel_params.ai_error = sim_obj.m_AIError;

            gumbel_params.aj = sim_obj.m_AJ;
            gumbel_params.aj_error = sim_obj.m_AJError;

            Sls::alp_data::get_current_time(current_time2);
            result->SetCalcTime(current_time2 - current_time1);

            s_CopyVector(sim_obj.m_LambdaSbs, sbs_arrays.lambda_sbs);
            s_CopyVector(sim_obj.m_KSbs, sbs_arrays.K_sbs);
            s_CopyVector(sim_obj.m_CSbs, sbs_arrays.C_sbs);
            s_CopyVector(sim_obj.m_SigmaSbs, sbs_arrays.sigma_sbs);
            s_CopyVector(sim_obj.m_AlphaISbs, sbs_arrays.alpha_i_sbs);
            s_CopyVector(sim_obj.m_AlphaJSbs, sbs_arrays.alpha_j_sbs);
            s_CopyVector(sim_obj.m_AISbs, sbs_arrays.ai_sbs);
            s_CopyVector(sim_obj.m_AJSbs, sbs_arrays.aj_sbs);


            // New TRandParams object is created only if m_Options does
            // not contain one already.
            if (m_RandParams.Empty()) {

                m_RandParams.Reset(new CGumbelParamsRandDiagnostics());
                m_RandParams->SetRandomSeed(
                              sim_obj.d_alp_data->d_rand_all->d_random_factor);

                vector<Int4>& fsprelim_numbers
                    = m_RandParams->SetFirstStagePrelimReNumbers();

                s_CopyVector(sim_obj.d_alp_data->d_rand_all
                         ->d_first_stage_preliminary_realizations_numbers_ALP,
                         fsprelim_numbers);

                vector<Int4>& prelim_numbers
                    = m_RandParams->SetPrelimReNumbers();

                s_CopyVector(sim_obj.d_alp_data->d_rand_all
                             ->d_preliminary_realizations_numbers_ALP,
                             prelim_numbers);
                    
                vector<Int4>& prelim_numbers_killing 
                    = m_RandParams->SetPrelimReNumbersKilling();

                s_CopyVector(sim_obj.d_alp_data->d_rand_all
                             ->d_preliminary_realizations_numbers_killing,
                             prelim_numbers_killing);
                            
                m_RandParams->SetTotalReNumber(sim_obj.d_alp_data->d_rand_all
                               ->d_total_realizations_number_with_ALP);

                m_RandParams->SetTotalReNumberKilling(
                           sim_obj.d_alp_data->d_rand_all
                           ->d_total_realizations_number_with_killing);
                        }
                }
    }
    catch (Sls::error er) {

        switch(er.error_code) {
        case 1: NCBI_THROW(CGumbelParamsException, eLimitsExceeded,
                       (string)"Time or memory limit exceeded.");

        case 2: NCBI_THROW(CGumbelParamsException, eRepeatCalc,
                           (string)"The program could not estimate the "
                           "parameters. Please repeat calculation");

        case 3: NCBI_THROW(CGumbelParamsException, eParamsError,
                           (string)"Results cannot be computed for the " 
                           "provided parameters. " + er.st);

        case 41: NCBI_THROW(CGumbelParamsException, eMemAllocError,
                            (string)"Memory allocation error");

        case 4:
        default: NCBI_THROW(CGumbelParamsException, eUnexpectedError,
                            (string)"Unexpected error");
        }
    }

    m_Result.Reset(result);
    return m_Result;
}



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

File name: pvalues.cpp

Author: Sergey Sheetlin

Contents: Implementation for wrapper: P-values computation for given Gumbel
          parameters

******************************************************************************/


#include <ncbi_pch.hpp>

#include "sls_alp.hpp"
#include "sls_alp_data.hpp"
#include "sls_alp_regression.hpp"
#include "sls_alp_sim.hpp"
#include "sls_pvalues.hpp"

#include <algo/blast/gumbel_params/pvalues.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(blast);


//CPValuesOptions

CScorePValuesOptions::CScorePValuesOptions(Int4 min_score, Int4 max_score, 
                        Int4 seq1_len, Int4 seq2_len,
                        const CConstRef<CGumbelParamsResult>& gumbel_result)

    : m_MinScore(min_score), m_MaxScore(max_score), m_Seq1Len(seq1_len), 
      m_Seq2Len(seq2_len), m_GumbelParams(gumbel_result)
{
    if (m_GumbelParams.Empty()) {
        NCBI_THROW(CScorePValuesException, eGumbelParamsEmpty,
                   "The Gumbel parameters object is empty");
    }
}

bool CScorePValuesOptions::Validate(void) const 
{

    if(m_MinScore > m_MaxScore) {
        NCBI_THROW(CScorePValuesException, eInvalidOptions,
           "Bad score range: minimum score is greater than maximum score");
    }

    if(m_Seq1Len <= 0 || m_Seq2Len <= 0) {
        NCBI_THROW(CScorePValuesException, eInvalidOptions,
                   "Sequence length negative or zero");
    }

    if(!m_GumbelParams) {
        NCBI_THROW(CScorePValuesException, eInvalidOptions,
                   "Gumbel parameters results not set");
    }

    const CGumbelParamsResult::SSbsArrays& sbs_params 
        = m_GumbelParams->GetSbsArrays();

    size_t size = sbs_params.lambda_sbs.size();

    if(size != sbs_params.K_sbs.size() || size != sbs_params.C_sbs.size() ||
       size != sbs_params.sigma_sbs.size() || 
       size != sbs_params.alpha_i_sbs.size() ||
       size != sbs_params.alpha_j_sbs.size() ||
       size != sbs_params.ai_sbs.size() ||
       size != sbs_params.aj_sbs.size() || size < 2) {

        NCBI_THROW(CScorePValuesException, eInvalidOptions,
                   "Sbs arrays are of different lengths");
    }

    return true;
}


CRef<CScorePValues> CScorePValuesCalc::Run(void)
{
    m_Options->Validate();

    try {
            
        CScorePValues* result = new CScorePValues();

        static Sls::pvalues pvalues_obj;

        Int4 Score1=m_Options->GetMinScore();
        Int4 Score2=m_Options->GetMaxScore();
        Int4 Seq1Len=m_Options->GetSeq1Len();
        Int4 Seq2Len=m_Options->GetSeq2Len();

        Sls::set_of_parameters parameters_set;
            
        vector<double> p_values;
        vector<double> p_values_errors;

        const SGumbelParams& g_params 
            = m_Options->GetGumbelParams().GetGumbelParams();


        parameters_set.lambda = g_params.lambda;
        parameters_set.lambda_error = g_params.lambda_error;
            
        parameters_set.C = g_params.C;
        parameters_set.C_error = g_params.C_error;
            
        parameters_set.K = g_params.K;
        parameters_set.K_error = g_params.K_error;

        parameters_set.a_I = g_params.ai;
        parameters_set.a_I_error = g_params.ai_error;
                        
        parameters_set.a_J = g_params.aj;
        parameters_set.a_J_error = g_params.aj_error;

        parameters_set.sigma = g_params.sigma;
        parameters_set.sigma_error = g_params.sigma_error;

        parameters_set.alpha_I = g_params.alpha_i;
        parameters_set.alpha_I_error = g_params.alpha_i_error;

        parameters_set.alpha_J = g_params.alpha_j;
        parameters_set.alpha_J_error = g_params.alpha_j_error;

        parameters_set.a 
            = (parameters_set.a_I + parameters_set.a_J) * 0.5;

        parameters_set.a_error = (parameters_set.a_I_error
                                 + parameters_set.a_J_error)*0.5;
        
        parameters_set.alpha = (parameters_set.alpha_I
                               + parameters_set.alpha_J) * 0.5;

        parameters_set.alpha_error = (parameters_set.alpha_I_error
                                     + parameters_set.alpha_J_error) * 0.5;


        parameters_set.gapless_a = g_params.gapless_a;
        parameters_set.gapless_a_error = g_params.gapless_a_error;
                        
        parameters_set.gapless_alpha = g_params.gapless_alpha;
        parameters_set.gapless_alpha_error = g_params.gapless_alpha_error;

                parameters_set.G=g_params.G;


        const CGumbelParamsResult::SSbsArrays& sbs_arrays
            = m_Options->GetGumbelParams().GetSbsArrays();

        size_t size_tmp = sbs_arrays.lambda_sbs.size();
        size_t i;

        parameters_set.m_LambdaSbs.resize(size_tmp);
        parameters_set.m_KSbs.resize(size_tmp);
        parameters_set.m_CSbs.resize(size_tmp);
        parameters_set.m_SigmaSbs.resize(size_tmp);
        parameters_set.m_AlphaISbs.resize(size_tmp);
        parameters_set.m_AlphaJSbs.resize(size_tmp);
        parameters_set.m_AISbs.resize(size_tmp);
        parameters_set.m_AJSbs.resize(size_tmp);

        for(i=0;i<size_tmp;i++) {
            parameters_set.m_LambdaSbs[i] = sbs_arrays.lambda_sbs[i];
            parameters_set.m_KSbs[i] = sbs_arrays.K_sbs[i];
            parameters_set.m_CSbs[i] = sbs_arrays.C_sbs[i];
            parameters_set.m_SigmaSbs[i] = sbs_arrays.sigma_sbs[i];
            parameters_set.m_AlphaISbs[i] = sbs_arrays.alpha_i_sbs[i];
            parameters_set.m_AlphaJSbs[i]= sbs_arrays.alpha_j_sbs[i];
            parameters_set.m_AISbs[i] = sbs_arrays.ai_sbs[i];
            parameters_set.m_AJSbs[i] = sbs_arrays.aj_sbs[i];
        }

            
        pvalues_obj.calculate_P_values(Score1, Score2, Seq1Len, Seq2Len,
                                      parameters_set, p_values,
                                      p_values_errors);
            
        size_tmp=p_values.size();
            
        _ASSERT(p_values_errors.size() == size_tmp);

        vector<CScorePValues::TPValue>& pv = result->SetPValues();
        vector<CScorePValues::TPValue>& err = result->SetErrors();
        pv.resize(size_tmp);
        err.resize(size_tmp);
            
        for(i=0;i<size_tmp;i++) {
            pv[i] = p_values[i];
            err[i] = p_values_errors[i];
        }

        m_Result.Reset(result);
        result = NULL;
        
    }
    catch (Sls::error er) {
        switch(er.error_code) {
        case 2: NCBI_THROW(CScorePValuesException, eInvalidOptions,
                           (string)"Ivalid options: " + er.st);

        case 41: NCBI_THROW(CScorePValuesException, eMemoryAllocation,
                            (string)"Memory allocation error: " + er.st);

        case 1:
        default: NCBI_THROW(CScorePValuesException, eUnexpectedError,
                            (string)"Unexpected error: " + er.st);
        }

    }
    catch (...) { 
        NCBI_THROW(CScorePValuesException, eUnexpectedError,
                   "Unexpected error");
    }

    return m_Result;
}

CRef<CScorePValues> CScorePValuesCalc::GetResult(void)
{
    if (m_Result.Empty()) {
        NCBI_THROW(CScorePValuesException, eResultNotSet,
                   "The result object was not set");
        
    }
    return m_Result;
}


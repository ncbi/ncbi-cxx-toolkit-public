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
* Author:  Greg Boratyn
*
* File Description:
*   Unit tests for Gumbel parameters calculation library.
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <algo/blast/gumbel_params/general_score_matrix.hpp>
#include <algo/blast/gumbel_params/gumbel_params.hpp>
#include <algo/blast/gumbel_params/pvalues.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#undef NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers
#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING


USING_NCBI_SCOPE;
USING_SCOPE(blast);

static CRef<CGeneralScoreMatrix> s_ReadScoreMatrix(const string& filename)
{
    CNcbiIfstream istr(filename.c_str());
    BOOST_REQUIRE(istr);
    vector<Int4> scores;
    while (!istr.eof()) {
        Int4 val;
        istr >> val;
        scores.push_back(val);
    }
    CRef<CGeneralScoreMatrix> smat(new CGeneralScoreMatrix(scores));
    return smat;
}

static void s_ReadResidueProbs(const string& filename,
                               vector<double>& probs)
{
    probs.clear();

    CNcbiIfstream istr(filename.c_str());
    BOOST_REQUIRE(istr);
    while (!istr.eof()) {
        double val;
        istr >> val;
        probs.push_back(val);
    }
}

static void s_ReadRandParams(const string& filename,
                             CRef<CGumbelParamsRandDiagnostics> params)
{

    CNcbiIfstream istr(filename.c_str());
    BOOST_REQUIRE(istr);

    Int4 param_val;

    istr >> param_val;
    params->SetRandomSeed(param_val);
            
    int num_params;
    
    istr >> num_params;
    vector<Int4>& fs_prelim = params->SetFirstStagePrelimReNumbers();
    for(int i=0;i < num_params && !istr.eof();i++) {
        istr >> param_val;
        fs_prelim.push_back(param_val);
    }

    istr >> num_params;
    vector<Int4>& prelim = params->SetPrelimReNumbers();
    for(int i=0;i < num_params && !istr.eof();i++) {
        istr >> param_val;
        prelim.push_back(param_val);
    }

    istr >> num_params;
    vector<Int4>& killing = params->SetPrelimReNumbersKilling();
    for(int i=0;i < num_params && !istr.eof();i++) {
        istr >> param_val;
        killing.push_back(param_val);
    }

    istr >> param_val;
    params->SetTotalReNumber(param_val);

    istr >> param_val;
    params->SetTotalReNumberKilling(param_val);
}


BOOST_AUTO_TEST_CASE(TestGumbelParamsOptionsFactory)
{
    // Options created by CGumbelParamsOptionsFactory pass validation
    // and do not cause errors in computation
    CRef<CGumbelParamsOptions> 
        opts = CGumbelParamsOptionsFactory::CreateStandard20AAOptions();
    // make sure that the unit test does not fail because time limit is exceeded
    opts->SetMaxCalcTime(2.0);
    BOOST_REQUIRE_NO_THROW(opts->Validate());
    CRef<CGumbelParamsCalc> gp_calc(new CGumbelParamsCalc(opts));
    BOOST_REQUIRE_NO_THROW(!gp_calc->Run().Empty());

    opts = CGumbelParamsOptionsFactory::CreateBasicOptions();
    CRef<CGeneralScoreMatrix> smat = s_ReadScoreMatrix("data/blosum62.txt");
    vector<double> probs;
    s_ReadResidueProbs("data/freqs.txt", probs);
    opts->SetScoreMatrix(smat);
    opts->SetSeq1ResidueProbs(probs);
    opts->SetSeq2ResidueProbs(probs);
    // make sure that the unit test does not fail because time limit is exceeded
    opts->SetMaxCalcTime(2.0);
    BOOST_REQUIRE_NO_THROW(opts->Validate());
    gp_calc.Reset(new CGumbelParamsCalc(opts));
    BOOST_REQUIRE_NO_THROW(!gp_calc->Run().Empty());
}

// Checks whether CGumbelParamsOptions::Validate() throws or reports warning
// when it should
BOOST_AUTO_TEST_CASE(TestValidateGumbelParamsOptions)
{
    // Incorrect number of elements in residue probabilities array for seq1
    // does not pass validation
    CRef<CGumbelParamsOptions> 
        opts = CGumbelParamsOptionsFactory::CreateStandard20AAOptions();
    opts->SetSeq1ResidueProbs(vector<double>(1));
    BOOST_REQUIRE_THROW(opts->Validate(), CGumbelParamsException);

    // Incorrect number of elements in residue probabilities array for seq2
    // does not pass validation
    opts = CGumbelParamsOptionsFactory::CreateStandard20AAOptions();
    opts->SetSeq2ResidueProbs(vector<double>(1));
    BOOST_REQUIRE_THROW(opts->Validate(), CGumbelParamsException);

    // Incorrect size of the score matrix does not pass validation
    opts = CGumbelParamsOptionsFactory::CreateStandard20AAOptions();
    CRef<CGeneralScoreMatrix> 
        smat(new CGeneralScoreMatrix(CGeneralScoreMatrix::eBlosum62, 19));
    opts->SetScoreMatrix(smat);
    BOOST_REQUIRE_THROW(opts->Validate(), CGumbelParamsException);

    opts = CGumbelParamsOptionsFactory::CreateStandard20AAOptions();
    vector<double> probs = opts->GetSeq1ResidueProbs();
    vector<double> probs_test(probs);

    // Residue probabilities must be positive or zero
    probs_test[2] = -0.1;
    opts->SetSeq1ResidueProbs(probs_test);
    BOOST_CHECK_THROW(opts->Validate(), CGumbelParamsException);
    opts->SetSeq1ResidueProbs(probs);
    opts->SetSeq2ResidueProbs(probs_test);
    BOOST_CHECK_THROW(opts->Validate(), CGumbelParamsException);
    opts->SetSeq2ResidueProbs(probs);
    
    // Sum of residue probabilities must be equal (roughly) to 1.
    // Difference of 0.2 is currently allowed due to rounding
    probs_test[2] = probs[2] + 0.2;
    opts->SetSeq1ResidueProbs(probs_test);
    BOOST_CHECK_THROW(opts->Validate(), CGumbelParamsException);
    opts->SetSeq1ResidueProbs(probs);
    opts->SetSeq2ResidueProbs(probs_test);
    BOOST_CHECK_THROW(opts->Validate(), CGumbelParamsException);
    opts->SetSeq2ResidueProbs(probs);


    Int4 orig_param_val;
    double orig_param_d;
    opts = CGumbelParamsOptionsFactory::CreateStandard20AAOptions();

    // Gap panelty < 0 does not pass validation
    orig_param_val = opts->GetGapOpening();
    opts->SetGapOpening(-1);
    BOOST_CHECK_THROW(opts->Validate(), CGumbelParamsException);
    opts->SetGapOpening(orig_param_val);

    // Gap extension < 0 does not pass validation
    orig_param_val = opts->GetGapExtension();
    opts->SetGapExtension(-1);
    BOOST_CHECK_THROW(opts->Validate(), CGumbelParamsException);
    opts->SetGapExtension(orig_param_val);

    // Lambda accuracy < 0 does not pass validation
    orig_param_d = opts->GetLambdaAccuracy();
    opts->SetLambdaAccuracy(-1.0);
    BOOST_CHECK_THROW(opts->Validate(), CGumbelParamsException);
    opts->SetLambdaAccuracy(orig_param_d);

    // K accuracy < 0 does not pass validation
    orig_param_d = opts->GetKAccuracy();
    opts->SetKAccuracy(-1.0);
    BOOST_CHECK_THROW(opts->Validate(), CGumbelParamsException);
    opts->SetKAccuracy(orig_param_d);

    // Max calculation time < 0 does not pass validation
    orig_param_d = opts->GetMaxCalcTime();
    opts->SetMaxCalcTime(-1.0);
    BOOST_CHECK_THROW(opts->Validate(), CGumbelParamsException);
    opts->SetMaxCalcTime(orig_param_d);

    // Max calculation memory < 0 does not pass validation
    orig_param_d = opts->GetMaxCalcMemory();
    opts->SetMaxCalcMemory(-1.0);
    BOOST_CHECK_THROW(opts->Validate(), CGumbelParamsException);
    opts->SetMaxCalcMemory(orig_param_d);

    // Warnings

    // Lambda accuracy out of the range 0.001 - 0.01 generates a warning 
    // message
    orig_param_d = opts->GetLambdaAccuracy();
    opts->SetLambdaAccuracy(0.0001);
    BOOST_CHECK_NO_THROW(!opts->Validate());
    BOOST_CHECK(opts->IsMessage());
    BOOST_CHECK(opts->GetMessages().size() > 0);
    opts->SetLambdaAccuracy(orig_param_d);

    // K accuracy out of the range 0.001 - 0.01 generates a warning message
    orig_param_d = opts->GetKAccuracy();
    opts->SetLambdaAccuracy(0.0001);
    BOOST_CHECK_NO_THROW(!opts->Validate());
    BOOST_CHECK(opts->IsMessage());
    BOOST_CHECK(opts->GetMessages().size() > 0);
    opts->SetKAccuracy(orig_param_d);

    // Max calculation time < 1.0 generates a warning message
    orig_param_d = opts->GetMaxCalcTime();
    opts->SetMaxCalcTime(0.1);
    BOOST_CHECK_NO_THROW(!opts->Validate());
    BOOST_CHECK(opts->IsMessage());
    BOOST_CHECK(opts->GetMessages().size() > 0);
    opts->SetMaxCalcTime(orig_param_d);

    // Max allowed memory < 1.0 generates a warning message
    orig_param_d = opts->GetMaxCalcMemory();
    opts->SetMaxCalcMemory(0.1);
    BOOST_CHECK_NO_THROW(!opts->Validate());
    BOOST_CHECK(opts->IsMessage());
    BOOST_CHECK(opts->GetMessages().size() > 0);
    opts->SetMaxCalcMemory(orig_param_d);
}


// Checks whether CGumbelScorePValuesOptions::Validate() throws or reports
// warnigns when it should
BOOST_AUTO_TEST_CASE(TestValidateScorePValuesOptions)
{
    CRef<CGumbelParamsOptions> 
        gp_opts = CGumbelParamsOptionsFactory::CreateStandard20AAOptions();

    // make sure that the unit test does not fail because time limit is exceeded
    gp_opts->SetMaxCalcTime(2.0);

    BOOST_REQUIRE(gp_opts->Validate());
    CGumbelParamsCalc gp_calc(gp_opts);
    CRef<CGumbelParamsResult> gp_result = gp_calc.Run();
    BOOST_REQUIRE(!gp_result.Empty());

    // Reasonable options pass validation
    CRef<CScorePValuesOptions> opts(new CScorePValuesOptions(100, 200,
                                                             200, 300,
                                                             gp_result));
    BOOST_REQUIRE_NO_THROW(opts->Validate());

    // Options object cannot be created with empty gumbel params result
    BOOST_REQUIRE_THROW(opts.Reset(new CScorePValuesOptions(100, 200, 200, 300,
                                    CRef<CGumbelParamsResult>())),
                CScorePValuesException);

    // Min score > Max score does not pass validation
    opts.Reset(new CScorePValuesOptions(200, 150, 200, 300, gp_result));
    BOOST_REQUIRE_THROW(opts->Validate(), CScorePValuesException);

    // Sequence 1 length <= 0 does not pass validation
    opts.Reset(new CScorePValuesOptions(100, 200, -200, 300, gp_result));
    BOOST_REQUIRE_THROW(opts->Validate(), CScorePValuesException);

    // Sequence 2 length <= 0 does not pass validation
    opts.Reset(new CScorePValuesOptions(100, 200, 200, -300, gp_result));
    BOOST_REQUIRE_THROW(opts->Validate(), CScorePValuesException);
}


// Check Gumbel params and compare with the reference file
static void s_CheckGumbelParams(const SGumbelParams& gumbel_params,
                                const string& file)
{
    // Gumbel parameters errors are positive
    BOOST_REQUIRE(gumbel_params.lambda_error >= 0.0);
    BOOST_REQUIRE(gumbel_params.K_error >= 0.0);
    BOOST_REQUIRE(gumbel_params.C_error > 0.0);
    BOOST_REQUIRE(gumbel_params.sigma_error >= 0.0);
    BOOST_REQUIRE(gumbel_params.alpha_i_error >= 0.0);
    BOOST_REQUIRE(gumbel_params.alpha_j_error >= 0.0);
    BOOST_REQUIRE(gumbel_params.ai_error >= 0.0);
    BOOST_REQUIRE(gumbel_params.aj_error >= 0.0);

    // compare Gumbel parameters values with reference
    CNcbiIfstream istr_g(file.c_str());
    BOOST_REQUIRE(istr_g.is_open());

    double target;

    istr_g >> target;
    BOOST_CHECK_CLOSE(target, gumbel_params.lambda, 0.001);

    istr_g >> target;
    BOOST_CHECK_CLOSE(target, gumbel_params.lambda_error, 0.001);

    istr_g >> target;
    BOOST_CHECK_CLOSE(target, gumbel_params.K, 0.001);

    istr_g >> target;
    BOOST_CHECK_CLOSE(target, gumbel_params.K_error, 0.001);

    istr_g >> target;
    BOOST_CHECK_CLOSE(target, gumbel_params.C, 0.001);

    istr_g >> target;
    BOOST_CHECK_CLOSE(target, gumbel_params.C_error, 0.001);

    istr_g >> target;
    BOOST_CHECK_CLOSE(target, gumbel_params.sigma, 0.001);

    istr_g >> target;
    BOOST_CHECK_CLOSE(target, gumbel_params.sigma_error, 0.001);

    istr_g >> target;
    BOOST_CHECK_CLOSE(target, gumbel_params.alpha_i, 0.001);

    istr_g >> target;
    BOOST_CHECK_CLOSE(target, gumbel_params.alpha_i_error, 0.001);

    istr_g >> target;
    BOOST_CHECK_CLOSE(target, gumbel_params.alpha_j, 0.001);

    istr_g >> target;
    BOOST_CHECK_CLOSE(target, gumbel_params.alpha_j_error, 0.001);

    istr_g >> target;
    BOOST_CHECK_CLOSE(target, gumbel_params.ai, 0.001);

    istr_g >> target;
    BOOST_CHECK_CLOSE(target, gumbel_params.ai_error, 0.001);

    istr_g >> target;
    BOOST_CHECK_CLOSE(target, gumbel_params.aj, 0.001);
    
    istr_g >> target;
    BOOST_CHECK_CLOSE(target, gumbel_params.aj_error, 0.001);

    istr_g.close();

}

// Check p-values and compare with the referece file
void s_CheckPValues(const CRef<CScorePValues> pv, const string& filename)
{
    BOOST_REQUIRE(!pv.Empty());

    const vector<CScorePValues::TPValue>& pvalues = pv->GetPValues();
    const vector<CScorePValues::TPValue>& errors = pv->GetErrors();

    // P-values and errors arrays are of the same length
    BOOST_REQUIRE_EQUAL(pvalues.size(), errors.size());

    // P-values and errors are positive
    for (size_t i=0;i < pvalues.size();i++) {
        BOOST_REQUIRE(pvalues[i] >= 0.0);
        BOOST_REQUIRE(errors[i] >= 0.0);
    }

    // Compare p-values and error to the reference values
    CNcbiIfstream istr_pv(filename.c_str());
    BOOST_REQUIRE(istr_pv.is_open());
    for (size_t j=0;j < pvalues.size();j++) {

        double target;

        BOOST_REQUIRE(!istr_pv.eof());
        
        istr_pv >> target;
        BOOST_REQUIRE_CLOSE(pvalues[j], target, 1.0);

        istr_pv >> target;
        BOOST_REQUIRE_CLOSE(errors[j], target, 1.0);
    }
}


// Test calculation of Gumbel Parameters and Pvalues for Gapless alignment
BOOST_AUTO_TEST_CASE(TestGumbelParamsAndPvaluesCalcForGaplessAlignment)
{
    // create gumbel params options
    CRef<CGumbelParamsOptions> 
        opts = CGumbelParamsOptionsFactory::CreateBasicOptions();

    // set score matrix and backgroud residue frequencies
    CRef<CGeneralScoreMatrix> smat = s_ReadScoreMatrix("data/blosum62.txt");
    vector<double> probs;
    s_ReadResidueProbs("data/freqs.txt", probs);
    opts->SetScoreMatrix(smat);
    opts->SetSeq1ResidueProbs(probs);
    opts->SetSeq2ResidueProbs(probs);
    opts->SetKAccuracy(0.005);

    // set gapless alignment
    opts->SetGapped(false);

    BOOST_REQUIRE(opts->Validate());

    // set score boundaries and sequence lengths for pvalues calculation
    const int min_score = 245;
    const int max_score = 255;
    const int seq_len = 200;
    
    // calculate Gumbel parameters
    CGumbelParamsCalc gp_calc(opts);
    CRef<CGumbelParamsResult> result = gp_calc.Run();

    // Template input parameters always produce result
    BOOST_REQUIRE(!result.Empty());

    const CGumbelParamsResult::SSbsArrays& sbs_arrays
        = result->GetSbsArrays();

    // All sbs arrays in gumbel parameters results are of the same length
    size_t ref_size = sbs_arrays.lambda_sbs.size();
    BOOST_REQUIRE_EQUAL(sbs_arrays.K_sbs.size(), ref_size);
    BOOST_REQUIRE_EQUAL(sbs_arrays.C_sbs.size(), ref_size);
    BOOST_REQUIRE_EQUAL(sbs_arrays.sigma_sbs.size(), ref_size);
    BOOST_REQUIRE_EQUAL(sbs_arrays.alpha_i_sbs.size(), ref_size);
    BOOST_REQUIRE_EQUAL(sbs_arrays.alpha_j_sbs.size(), ref_size);
    BOOST_REQUIRE_EQUAL(sbs_arrays.ai_sbs.size(), ref_size);
    BOOST_REQUIRE_EQUAL(sbs_arrays.aj_sbs.size(), ref_size);
    
    const SGumbelParams& gumbel_params = result->GetGumbelParams();

    // check Gumbel parameters values and compare with reference
    s_CheckGumbelParams(gumbel_params, "data/gumbel_params_gapless.txt");

    // compute score p-values
    CRef<CScorePValuesOptions> pv_opts(new CScorePValuesOptions(min_score,
                                         max_score, seq_len, seq_len, result));

    BOOST_REQUIRE(pv_opts->Validate());
    CScorePValuesCalc pv_calc(pv_opts);
    CRef<CScorePValues> pv = pv_calc.Run();

    // check p-values and compare with reference
    s_CheckPValues(pv, "data/pvalues_gapless.txt");
}


// TO DO: Add a test for detecting linear regime - by catching exception

BOOST_AUTO_TEST_CASE(TestGumbelParamsAndPvaluesCalcForGappedAlignment)
{
    // Calculation results for template input sets correspond to template
    // output
    CRef<CGumbelParamsOptions> 
        opts = CGumbelParamsOptionsFactory::CreateBasicOptions();

    CRef<CGeneralScoreMatrix> smat = s_ReadScoreMatrix("data/blosum62.txt");
    vector<double> probs;
    s_ReadResidueProbs("data/freqs.txt", probs);
    opts->SetScoreMatrix(smat);
    opts->SetSeq1ResidueProbs(probs);
    opts->SetSeq2ResidueProbs(probs);
    opts->SetKAccuracy(0.005);

    opts->SetGapped(true);

    BOOST_REQUIRE(opts->Validate());

    // Range boundaries and sequence length for score p-values calculation
    const int min_score = 245;
    const int max_score = 255;
    const int seq_len = 200;

    for (int i=0;i < 8;i++) {
        string param_file = (string)"data/rand_params_" + NStr::IntToString(i) 
            + ".txt";
        string target_file = (string)"data/gumbel_params_gapped_" 
            + NStr::IntToString(i) + ".txt";
        string pv_target_file = (string)"data/pvalues_gapped_"
            + NStr::IntToString(i) + ".txt";

        CRef<CGumbelParamsRandDiagnostics>
              params(new CGumbelParamsRandDiagnostics());

        s_ReadRandParams(param_file, params);

        CGumbelParamsCalc gp_calc(opts, params);
        CRef<CGumbelParamsResult> result = gp_calc.Run();

        // Template input parameters always produce result
        BOOST_REQUIRE(!result.Empty());

        const CGumbelParamsResult::SSbsArrays& sbs_arrays
            = result->GetSbsArrays();

        // All sbs arrays in gumbel parameters results are of the same length
        size_t ref_size = sbs_arrays.lambda_sbs.size();
        BOOST_REQUIRE_EQUAL(sbs_arrays.K_sbs.size(), ref_size);
        BOOST_REQUIRE_EQUAL(sbs_arrays.C_sbs.size(), ref_size);
        BOOST_REQUIRE_EQUAL(sbs_arrays.sigma_sbs.size(), ref_size);
        BOOST_REQUIRE_EQUAL(sbs_arrays.alpha_i_sbs.size(), ref_size);
        BOOST_REQUIRE_EQUAL(sbs_arrays.alpha_j_sbs.size(), ref_size);
        BOOST_REQUIRE_EQUAL(sbs_arrays.ai_sbs.size(), ref_size);
        BOOST_REQUIRE_EQUAL(sbs_arrays.aj_sbs.size(), ref_size);

        // Gumbel parameters errors are positive
        const SGumbelParams& gp = result->GetGumbelParams();
        BOOST_REQUIRE(gp.lambda_error >= 0.0);
        BOOST_REQUIRE(gp.K_error >= 0.0);
        BOOST_REQUIRE(gp.C_error > 0.0);
        BOOST_REQUIRE(gp.sigma_error >= 0.0);
        BOOST_REQUIRE(gp.alpha_i_error >= 0.0);
        BOOST_REQUIRE(gp.alpha_j_error >= 0.0);
        BOOST_REQUIRE(gp.ai_error >= 0.0);
        BOOST_REQUIRE(gp.aj_error >= 0.0);

        // Check Gumbel params and compare with reference
        s_CheckGumbelParams(result->GetGumbelParams(), target_file);

        // Compute score p-values
        CRef<CScorePValuesOptions> pv_opts(new CScorePValuesOptions(min_score,
                                          max_score, seq_len, seq_len, result));

        // Gumbel params result passes p-values options validation
        BOOST_REQUIRE(pv_opts->Validate());
        CScorePValuesCalc pv_calc(pv_opts);
        CRef<CScorePValues> pv = pv_calc.Run();

        // Checl p-values and compare with rewference 
        s_CheckPValues(pv, pv_target_file);

    }
}


#endif /* SKIP_DOXYGEN_PROCESSING */

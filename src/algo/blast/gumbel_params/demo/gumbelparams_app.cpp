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
 * Authors:  Sergey Sheetlin, Greg Boratyn
 *
 * File Description: Demo application for real time gumbel parameters
 *                   computation
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>

#include <algo/blast/gumbel_params/general_score_matrix.hpp>
#include <algo/blast/gumbel_params/gumbel_params.hpp>
#include <algo/blast/gumbel_params/pvalues.hpp>

#include <vector>

USING_NCBI_SCOPE;
USING_SCOPE(blast);

/////////////////////////////////////////////////////////////////////////////
//  CSampleBasicApplication::


class CGumbelParamsApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CGumbelParamsApplication::Init(void)
{
        
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Real time Gumbel parameters estimation" \
                              " for Blast search demo program");

    arg_desc->AddOptionalKey("scoremat", "file",
                             "File containing score matrix values",
                             CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("scorematname", "name", "Score matrix name",
                             CArgDescriptions::eString);

    arg_desc->SetConstraint("scorematname", &(*new CArgAllow_Strings,
                                              "blosum45", "blosum62",
                                              "blosum80", "pam30", "pam70",
                                              "pam250"));

    arg_desc->AddOptionalKey("params", "file",
                            "Text file with input randomization parameters",
                            CArgDescriptions::eInputFile);


    arg_desc->AddOptionalKey("freqs1", "file",
                             "File containing residue frequencies for"
                             " sequence 1", CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("freqs2", "file",
                             "File containing residue frequencies for"
                             " sequence 2", CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey("gapopen", "open_penalty", "Cost of opening a gap",
                            CArgDescriptions::eInteger, "11");

    arg_desc->AddDefaultKey("gapextend", "extend_penalty", 
                            "Cost of extending a gap",
                            CArgDescriptions::eInteger, "1");

    arg_desc->AddDefaultKey("lambda", "accuracy",
                             "Desired accuracy of computed lambda",
                            CArgDescriptions::eDouble, "0.001");

    arg_desc->AddDefaultKey("K", "accuracy",
                            "Desired accuracy of computed K",
                            CArgDescriptions::eDouble, "0.05");

    arg_desc->AddDefaultKey("gapped", "gapped", "Gapped or gapless regime",
                            CArgDescriptions::eBoolean, "true");

    arg_desc->AddDefaultKey("maxtime", "val", "Maximum allowed time for"
                            " computation of Gumbel parameters [s]",
                            CArgDescriptions::eDouble, "1.0");

    arg_desc->AddDefaultKey("maxmemory", "val", "Maximum allowed memory for"
                            " computation of Gumbel parameters [Mb]",
                            CArgDescriptions::eDouble, "1000.0");

    arg_desc->AddDefaultKey("fromscore", "num", "Minimum value of the score"
                            " range for calculation of P-values",
                            CArgDescriptions::eInteger, "145");

    arg_desc->AddDefaultKey("toscore", "num", "Maximum value of the score"
                            " range for calculation of P-values",
                            CArgDescriptions::eInteger, "155");

    arg_desc->AddDefaultKey("len1", "num", "Length of sequence 1 for score"
                            " P-values calculation",
                            CArgDescriptions::eInteger, "200");

    arg_desc->AddDefaultKey("len2", "num", "Length of sequence 2 for score"
                            " P-values calculation",
                            CArgDescriptions::eInteger, "200");

    arg_desc->AddOptionalKey("paramsout", "file",
                             "Save randomization parameters",
                             CArgDescriptions::eOutputFile);

    HideStdArgs(fHideAll);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


/////////////////////////////////////////////////////////////////////////////


static bool x_ReadRandParams(CNcbiIstream& fintest,
                             CGumbelParamsRandDiagnostics& params)
                             
{
    Int4 param_val;

    fintest >> param_val;
    params.SetRandomSeed(param_val);
            
    int num_params;
            
    if (fintest.eof()) {
        return false;
    }

    fintest >> num_params;

    vector<Int4>& fs_prelim = params.SetFirstStagePrelimReNumbers();
    for(int i=0;i < num_params && !fintest.eof();i++) {
        fintest >> param_val;
        fs_prelim.push_back(param_val);
    }

    if (fintest.eof()) {
        return false;
    }

    fintest >> num_params;
            
    vector<Int4>& prelim = params.SetPrelimReNumbers();
    for(int i=0;i < num_params && !fintest.eof();i++) {
        fintest >> param_val;
        prelim.push_back(param_val);
    }
                    
    if (fintest.eof()) {
        return false;
    }

    fintest >> num_params;

    vector<Int4>& killing = params.SetPrelimReNumbersKilling();
    for(int i=0;i < num_params && !fintest.eof();i++) {
        fintest >> param_val;
        killing.push_back(param_val);
    }

    if (fintest.eof()) {
        return false;
    }

    fintest >> param_val;
    params.SetTotalReNumber(param_val);
        
    if (fintest.eof()) {
        return false;
    }
        
    fintest >> param_val;
    params.SetTotalReNumberKilling(param_val);

    return true;
}

static void x_WriteRandParams(CNcbiOstream& fouttest,
                              const CGumbelParamsRandDiagnostics& rand)
{
    fouttest << rand.GetRandomSeed() << "\t";
        
    const vector<Int4>& fs_prelim
        = rand.GetFirstStagePrelimReNumbers();
    fouttest << fs_prelim.size() << "\t";
    for(size_t k=0;k < fs_prelim.size();k++) {
        fouttest << fs_prelim[k] << "\t";
    }

    const vector<Int4>& prelim = rand.GetPrelimReNumbers();
    fouttest << prelim.size() << "\t";
    for(size_t k=0;k < prelim.size();k++) {
        fouttest << prelim[k] << "\t";
    }

    const vector<Int4>& killing
        = rand.GetPrelimReNumbersKilling();
    fouttest << killing.size() << "\t";
    for(size_t k=0;k < killing.size();k++) {
        fouttest << killing[k] << "\t";
    }

    fouttest << rand.GetTotalReNumber() << "\t"
             << rand.GetTotalReNumberKilling()
             << "\n";
}


int CGumbelParamsApplication::Run(void)
{
    const CArgs& args = GetArgs();

    if (!args["scoremat"] && !args["scorematname"]) {
        NcbiCerr << "Error: Either score matrix name or file must be specified."
                 << NcbiEndl << "Try -help option."
                 << NcbiEndl << NcbiEndl;
        return 1;
    }
    if (args["scoremat"] && args["scorematname"]) {
        NcbiCerr << "Error: Either score matrix name or file must be specified."
                 << NcbiEndl << NcbiEndl;
        return 1;
    }

    if (args["fromscore"].AsInteger() > args["toscore"].AsInteger()) {
        NcbiCerr << "Error: Incorrect values for score range, fromscore must be"
            " smaller than toscore." << NcbiEndl << NcbiEndl;
        return 1;
    }


    CRef<CGumbelParamsOptions> opts
        = CGumbelParamsOptionsFactory::CreateStandard20AAOptions();

    // Set score matrix
    if (args["scorematname"]) {
        CGeneralScoreMatrix::EScoreMatrixName type;
        string name = args["scorematname"].AsString();
        if (name == "blosum45") {
            type = CGeneralScoreMatrix::eBlosum45;
        }
        else if (name == "blosum62") {
            type = CGeneralScoreMatrix::eBlosum62;
        }
        else if (name == "blosum80") {
            type = CGeneralScoreMatrix::eBlosum80;
        }
        else if (name == "pam30") {
            type = CGeneralScoreMatrix::ePam30;
        }
        else if (name == "pam70") {
            type = CGeneralScoreMatrix::ePam70;
        }
        else if (name == "pam250") {
            type = CGeneralScoreMatrix::ePam250;
        }
        else {
            NcbiCerr << "Error: Unrecognised score matrix name" << NcbiEndl
                     << NcbiEndl;
            return 1;
        }

        CRef<CGeneralScoreMatrix> smat(new CGeneralScoreMatrix(type, 20));
        opts->SetScoreMatrix(smat);
    }

    // Load score matrix
    if (args["scoremat"]) {
        CNcbiIstream& istr = args["scoremat"].AsInputFile();
        vector<Int4> score_vals;
        while (!istr.eof()) {
            Int4 elem = INT_MAX;
            args["scoremat"].AsInputFile() >> elem;
            if (elem != INT_MAX) {
                score_vals.push_back(elem);
            }
        }
        CRef<CGeneralScoreMatrix> smat(new CGeneralScoreMatrix(score_vals));
        opts->SetScoreMatrix(smat);
    }

    // Load residue frequencies for sequence 1
    if (args["freqs1"]) {
        CNcbiIstream& istr = args["freqs1"].AsInputFile();
        vector<double> freqs;
        while (!istr.eof()) {
            double elem;
            istr >> elem;
            freqs.push_back(elem);
        }
        // the Gumbel params library requres that residue frequencies sum up
        // to exaclty 1
        double sum = 0.0;
        ITERATE(vector<double>, it, freqs) {
            sum += *it;
        }
        if (sum != 1.0) {
            NON_CONST_ITERATE(vector<double>, it, freqs) {
                *it /= sum;
            }
        }
        opts->SetSeq1ResidueProbs(freqs);
    }

    // Load residue frequencies for sequence 2
    if (args["freqs2"]) {
        CNcbiIstream& istr = args["freqs2"].AsInputFile();
        vector<double> freqs;
        while (!istr.eof()) {
            double elem;
            istr >> elem;
            freqs.push_back(elem);
        }
        double sum = 0.0;
        ITERATE(vector<double>, it, freqs) {
            sum += *it;
        }
        if (sum != 1.0) {
            NON_CONST_ITERATE(vector<double>, it, freqs) {
                *it /= sum;
            }
        }
        opts->SetSeq2ResidueProbs(freqs);
    }

    opts->SetGapOpening(args["gapopen"].AsInteger());
    opts->SetGapExtension(args["gapextend"].AsInteger());
    opts->SetLambdaAccuracy(args["lambda"].AsDouble());
    opts->SetKAccuracy(args["K"].AsDouble());
    opts->SetGapped(args["gapped"].AsBoolean());
    opts->SetMaxCalcTime(args["maxtime"].AsDouble());
    opts->SetMaxCalcMemory(args["maxmemory"].AsDouble());

    // Load randomization parameters
    CRef<CGumbelParamsRandDiagnostics> rand(new CGumbelParamsRandDiagnostics());
    if (args["params"]) {
        if (!x_ReadRandParams(args["params"].AsInputFile(), *rand)) {
            NcbiCerr << "Error: Randomization parameters file is incomplete"
                     << NcbiEndl << NcbiEndl;
            return 1;
        }
    }

    // validate Gumbel params options (throws on error)
    if (!opts->Validate()) {
        ITERATE(vector<string>, it, opts->GetMessages()) {
            NcbiCerr << "Warning: " << *it << NcbiEndl << NcbiEndl;
        }
    }

    // Compute Gumbel params
    CRef<CGumbelParamsCalc> gp_calc;
    if (args["params"]) {
        gp_calc.Reset(new CGumbelParamsCalc(opts, rand));
    }
    else {
        gp_calc.Reset(new CGumbelParamsCalc(opts));
    }

    CRef<CGumbelParamsResult> gp_result = gp_calc->Run();

    if (args["paramsout"]) {
        x_WriteRandParams(args["paramsout"].AsOutputFile(),
                          *gp_calc->GetRandParams());
    }

    const SGumbelParams& g_params = gp_result->GetGumbelParams();

    NcbiCout << "Parameters estimation\n"
             << "Parameter value\terror\n"
             << "Lambda\t" << g_params.lambda << "\t" 
             << g_params.lambda_error << NcbiEndl;
    NcbiCout << "K\t" << g_params.K << "\t" << g_params.K_error
             << NcbiEndl;
    NcbiCout << "C\t" << g_params.C << "\t" << g_params.C_error
             << NcbiEndl;
    NcbiCout << "Sigma\t" << g_params.sigma << "\t" 
             << g_params.sigma_error << NcbiEndl;
    NcbiCout << "Alpha_I\t" << g_params.alpha_i << "\t" 
             << g_params.alpha_i_error << NcbiEndl;
    NcbiCout << "Alpha_J\t" << g_params.alpha_j << "\t" 
             << g_params.alpha_j_error << NcbiEndl;
    NcbiCout << "A_I\t" << g_params.ai << "\t"
             << g_params.ai_error << NcbiEndl;
    NcbiCout << "A_J\t" << g_params.aj << "\t" 
             << g_params.aj_error << NcbiEndl;
    NcbiCout << "G\t" << g_params.G << NcbiEndl;
    NcbiCout << "gapless Alpha\t" << g_params.gapless_alpha << "\t"
             << g_params.gapless_alpha_error << NcbiEndl;

    NcbiCout << "Calculation time\t" << gp_result->GetCalcTime()
             << " seconds" << NcbiEndl;

    // Compute P-values using precomputed Gumbel parameters
    CRef<CScorePValuesOptions> pv_opts(new CScorePValuesOptions(
                                            args["fromscore"].AsInteger(),
                                            args["toscore"].AsInteger(),
                                            args["len1"].AsInteger(), 
                                            args["len2"].AsInteger(), 
                                            gp_result));

    // validade p-values options (throws on error)
    pv_opts->Validate();

    CScorePValuesCalc pv_calculator(pv_opts);
    pv_calculator.Run();
    CRef<CScorePValues> pv_result = pv_calculator.GetResult();

    NcbiCout << NcbiEndl;
    NcbiCout << "P-values estimaton\nscore\tP-value\tP-value error\n";
    for(size_t k=0;k < pv_result->GetPValues().size();k++) {
        NcbiCout << args["fromscore"].AsInteger() + (int)k << "\t" 
                 << pv_result->GetPValues()[k] << "\t" 
                 << pv_result->GetErrors()[k] << NcbiEndl;
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CGumbelParamsApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CGumbelParamsApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}

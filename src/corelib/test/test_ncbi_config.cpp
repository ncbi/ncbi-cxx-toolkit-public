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
 * Author: Maxim Didenko 
 *
 * File Description:
 *      Test CConfig class
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_config.hpp>
#include <memory>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;

/////////////////////////////////
// Test application
//

class CConfigTestApplication : public CNcbiApplication                        
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CConfigTestApplication::Init(void)
{
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test CConfig class");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


const char* kSection = "test_section";

int CConfigTestApplication::Run(void)
{
    CConfig::TParamTree params( CConfig::TParamTree::TValueType(kSection, kEmptyStr));
    params.AddNode(CConfig::TParamTree::TValueType("int_value", "10"));
    params.AddNode(CConfig::TParamTree::TValueType("int1_value", "15"));
    params.AddNode(CConfig::TParamTree::TValueType("bool_value", "false"));
    params.AddNode(CConfig::TParamTree::TValueType("double_value", "1.2345e4"));

    CConfig cfg(&params, eNoOwnership);
    int i = cfg.GetInt(kSection, "int_value", CConfig::eErr_Throw, 4);
    assert(i == 10);

    list<string> int_synonyms;
    int_synonyms.push_back("int1_value");
    i = cfg.GetInt(kSection, "int2_value", CConfig::eErr_Throw, 4, &int_synonyms);
    assert(i == 15);

    try {
      i = cfg.GetInt(kSection, "int_value", CConfig::eErr_Throw, 4, &int_synonyms);
    } catch (CConfigException& ex) {
        if (ex.GetErrCode() != CConfigException::eSynonymDuplicate)
            throw;
    }

    i = cfg.GetInt(kSection, "int_value", CConfig::eErr_NoThrow, 4, &int_synonyms);
    assert( i == 4 );

    bool b = cfg.GetBool(kSection, "bool_value", CConfig::eErr_NoThrow, true);
    assert( !b );

    double d = cfg.GetDouble(kSection, "double_value", CConfig::eErr_NoThrow, 4.32);
    assert( d == 12345. );
    d = cfg.GetDouble(kSection, "double2_value", CConfig::eErr_NoThrow, 4.32);
    assert( d == 4.32 );
    
    return 0;
}


  
///////////////////////////////////
// MAIN
//

int main(int argc, const char* argv[])
{
    return CConfigTestApplication().AppMain(argc, argv);
}

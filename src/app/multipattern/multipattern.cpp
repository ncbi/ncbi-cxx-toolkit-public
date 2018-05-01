/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Author: Sema
 *
 * File Description:
 *   Main() of Multipattern Search Code Generator
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <util/multipattern_search.hpp>

USING_NCBI_SCOPE;


class CMultipatternApp : public CNcbiApplication
{
public:
    CMultipatternApp(void);
    virtual void Init(void);
    virtual int  Run (void);
};


CMultipatternApp::CMultipatternApp(void) {}


void CMultipatternApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "Multipattern Search Code Generator");
    arg_desc->AddFlag("D", "Generate DOT graph");
    arg_desc->AddOptionalKey("i", "InFile", "Input File", CArgDescriptions::eInputFile);
    arg_desc->AddExtra(0, kMax_UInt, "Search Patterns as /regex/ or \"plain string\"", CArgDescriptions::eString);
    SetupArgDescriptions(arg_desc.release());  // call CreateArgs
};


int CMultipatternApp::Run(void)
{
	vector<pair<string, unsigned int>> input;
	const CArgs& args = GetArgs();
    //if (args["i"]) m_Files.push_back(args["i"].AsString());
	
	for (size_t i = 1; i <= args.GetNExtra(); i++) {
        input.push_back(pair<string, unsigned int>(args["#" + to_string(i)].AsString(), 0));
	}
    CMultipatternSearch FSM;
	for (size_t i = 0; i < input.size(); i++) {
        try {
            FSM.AddPattern(input[i].first, input[i].second);
        }
        catch (string s) {
            cerr << s << "\n";
            return 1;
        }
    }
    if (args["D"]) {
        FSM.GenerateDotGraph(cout);
    }
    else {
        FSM.GenerateSourceCode(cout);
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CMultipatternApp().AppMain(argc, argv);
}

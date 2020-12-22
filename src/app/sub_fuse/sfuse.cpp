/*  
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <common/ncbi_source_ver.h>

#include <memory>


#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbidiag.hpp>

#include "subs_collector.hpp"

USING_NCBI_SCOPE;

namespace subfuse
{

class CSubfuseApp : public CNcbiApplication
{
public:
    CSubfuseApp()
    {
        SetVersionByBuild(0);
    }

private:
    virtual void Init(void);
    virtual int  Run(void);

    CNcbiOstream* GetOutputStream();
};


void CSubfuseApp::Init(void)
{
    // Prepare command line descriptions
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "Submissions integration");

    arg_desc->AddKey("p", "InDir", "Path to Files", CArgDescriptions::eDirectory);
    arg_desc->AddOptionalKey("o", "OutFile", "Single Output File", CArgDescriptions::eOutputFile);
    arg_desc->AddDefaultKey("x", "FileExt", "File Extention", CArgDescriptions::eString, ".sqn");

    SetupArgDescriptions(arg_desc.release());  // call CreateArgs
}

CNcbiOstream* CSubfuseApp::GetOutputStream()
{
    CNcbiOstream* out = &cout;

    const CArgs& args = GetArgs();
    if (args["o"].HasValue()) {
        out = &args["o"].AsOutputFile();
    }

    return out;
}

static bool CmpEntry(const CDir::TEntry& a, const CDir::TEntry& b)
{
    return NStr::CompareNocase(a->GetName().c_str(), b->GetName().c_str()) < 0;
}

int CSubfuseApp::Run(void)
{
    const CDir& path = GetArgs()["p"].AsDirectory();
    string mask = "*" + GetArgs()["x"].AsString();
    CNcbiOstream* out = GetOutputStream();

    CDir::TEntries entries = path.GetEntries(mask, CDir::fIgnoreRecursive);
    entries.sort(CmpEntry);

    CSubmissionCollector collector(*out);
    for (auto entry : entries) {

        LOG_POST_EX(0, 0, "[subfuse] Processing: " << entry->GetBase());
        collector.ProcessFile(entry->GetPath());
    }

    collector.CompleteProcessing();

    return 0;
}

} // namespace pub_report


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return subfuse::CSubfuseApp().AppMain(argc, argv);
}

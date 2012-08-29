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
* Author:  Aaron Ucko
*
* File Description:
*   test code for CScope::GetTitle
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objmgr/util/create_defline.hpp>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(sequence);

class CTitleTester : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
};


void CTitleTester::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Show a sequence's title", false);

    arg_desc->AddKey("gi", "SeqEntryID", "GI id of the Seq-Entry to examine",
                     CArgDescriptions::eInteger);
    arg_desc->AddKey("in", "GIList", "File listing GIs to look up",
                     CArgDescriptions::eInputFile);
    arg_desc->SetDependency("gi", CArgDescriptions::eExcludes, "in");
    arg_desc->AddFlag("reconstruct", "Reconstruct title");
    arg_desc->AddFlag("allproteins", "Name all proteins, not just the first");
    arg_desc->AddFlag("localannots",
                      "Never use related sequences' annotations");

    SetupArgDescriptions(arg_desc.release());
}


int CTitleTester::Run(void)
{
    const CArgs&   args = GetArgs();
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope         scope(*objmgr);
    CSeq_id        id;
    
    CGBDataLoader::RegisterInObjectManager(*objmgr);
    scope.AddDefaults();

    CDeflineGenerator gen;
    CDeflineGenerator::TUserFlags flags = 0;
    if (args["reconstruct"]) {
        flags |= CDeflineGenerator::fIgnoreExisting;
    }
    if (args["allproteins"]) {
        flags |= CDeflineGenerator::fAllProteinNames;
    }
    if (args["localannots"]) {
        flags |= CDeflineGenerator::fLocalAnnotsOnly;
    }

    if (args["gi"]) {
        id.SetGi(args["gi"].AsInteger());
        CBioseq_Handle handle = scope.GetBioseqHandle(id);
        NcbiCout << gen.GenerateDefline(handle, flags) << NcbiEndl;
    } else {
        CNcbiIstream& in(args["in"].AsInputFile());
        while (in >> id.SetGi()) {
            string s;
            try {
                CBioseq_Handle handle = scope.GetBioseqHandle(id);
                s = gen.GenerateDefline(handle, flags);
            } catch (exception& e) {
                s = e.what();
            }
            NcbiCout << id.GetGi() << ": " << s << NcbiEndl;
        }
    }
    return 0;
}


// (END_NCBI_SCOPE must be preceded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

USING_NCBI_SCOPE;

int main(int argc, const char** argv)
{
    return CTitleTester().AppMain(argc, argv);
}

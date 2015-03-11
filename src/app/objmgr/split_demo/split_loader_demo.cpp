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
* Author:  Aleksey Grichenko
*
* File Description:
*   Examples of split data loader implementation
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

// Object manager includes
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/align_ci.hpp>

#include <objmgr/object_manager.hpp>
#include "split_loader.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CDemoApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CDemoApp::Init(void)
{
    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // GI to fetch
    arg_desc->AddKey("id", "SeqEntryID",
                     "Seq-id of the Seq-Entry to fetch",
                     CArgDescriptions::eString);
    arg_desc->AddKey("file", "SeqEntryFile",
                     "file with Seq-entry to load (text ASN.1)",
                     CArgDescriptions::eString);

    // Program description
    string prog_description = "Example of split data loader implementation\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


int CDemoApp::Run(void)
{
    // Process command line args: get GI to load
    const CArgs& args = GetArgs();

    // Create seq-id, set it to GI specified on the command line
    CRef<CSeq_id> id(new CSeq_id(args["id"].AsString()));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id);

    string data_file = args["file"].AsString();

    NcbiCout << "Loading " << idh.AsString()
        << " from " << data_file << NcbiEndl;

    // Create object manager. Use CRef<> to delete the OM on exit.
    CRef<CObjectManager> pOm = CObjectManager::GetInstance();
    // Create demo data loader and register it with the OM.
    CSplitDataLoader::RegisterInObjectManager(*pOm, data_file);
    // Create a new scope.
    CScope scope(*pOm);
    // Add default loaders (GB loader in this demo) to the scope.
    scope.AddDefaults();

    CBioseq_Handle handle = scope.GetBioseqHandle(idh);
    // Check if the handle is valid
    if ( !handle ) {
        ERR_POST(Fatal << "Bioseq not found");
        Abort();
    }

    size_t count = 0;
    for (CFeat_CI fit(handle); fit; ++fit) {
        ++count;
    }
    NcbiCout << "Found " << count << " features" << NcbiEndl;

    count = 0;
    for (CSeqdesc_CI dit(handle); dit; ++dit) {
        ++count;
    }
    NcbiCout << "Found " << count << " descriptors" << NcbiEndl;

    CSeqVector vect(handle);
    string data;
    vect.GetSeqData(vect.begin(), vect.end(), data);
    NcbiCout << "Seq-data: " << NStr::PrintableString(data.substr(0, 10))
             << NcbiEndl;

    NcbiCout << "Done" << NcbiEndl;
    return 0;
}


END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//  MAIN


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    int ret = CDemoApp().AppMain(argc, argv);
    NcbiCout << NcbiEndl;
    return ret;
}

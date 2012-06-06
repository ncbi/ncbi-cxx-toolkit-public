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
 * Author:  Anatoliy Kuznetsov, Aleksey Grichenko
 *
 * File Description:
 *   Demo of using local data storage version 2.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/data_loaders/lds2/lds2_dataloader.hpp>
#include <objtools/lds2/lds2.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CSampleLds2Application : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);

private:
    void x_InitLDS2(const string& lds_path,
                    const string& data_path);
};



void CSampleLds2Application::Init(void)
{
    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddKey("data_dir", "DataDir", "Directory with the data files.",
        CArgDescriptions::eString);

    arg_desc->AddKey("db", "DbFile", "LDS2 database file name.",
        CArgDescriptions::eString);

    // GI to fetch
    arg_desc->AddKey("id", "SeqEntryID", "ID of the Seq-Entry to fetch",
        CArgDescriptions::eString);

    arg_desc->AddFlag("print_entry", "Print seq-entry");
    arg_desc->AddFlag("print_feats", "Print features");
    arg_desc->AddFlag("print_aligns", "Print alignments");

    arg_desc->AddOptionalKey("group_aligns", "group_size",
        "Group standalone seq-aligns into blobs",
        CArgDescriptions::eInteger);

    // Program description
    string prog_description = "Example of the LDS2 usage\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


int CSampleLds2Application::Run(void)
{
    // Process command line args
    const CArgs& args = GetArgs();

    const string& data_path = args["data_dir"].AsString();
    const string& db_path = args["db"].AsString();

    //
    // Initialize the local data storage
    //
    try {
        CRef<CLDS2_Manager> mgr(new CLDS2_Manager(db_path));
        // Allow to split GB release bioseq-sets
        mgr->SetGBReleaseMode(CLDS2_Manager::eGB_Guess);
        if ( args["group_aligns"] ) {
            mgr->SetSeqAlignGroupSize(args["group_aligns"].AsInteger());
        }
        mgr->AddDataDir(data_path);
        mgr->UpdateData();
    }
    catch(CException& e) {
        LOG_POST(Error << "Error initializing local data storage: "
                 << e.what());
        return 1;
    }

    // Create OM and LDS2 data loader
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    try {
        CLDS2_DataLoader::RegisterInObjectManager(*object_manager,
            db_path, -1, CObjectManager::eDefault);
    }
    catch (CException& e) {
        LOG_POST(Error << "Error registering LDS2 data loader: " 
                       << e.what());
        return 2;
    }

    // Check if an id was requested, try to fetch some data
    if ( args["id"] ) {
        string id = args["id"].AsString();
        // Create Seq-id, set it to the GI specified on the command line
        CSeq_id seq_id(id);
        // Create a new scope ("attached" to our OM).
        CScope scope(*object_manager);
        // Add default loaders (GB loader in this demo) to the scope.
        scope.AddDefaults();

        // Get synonyms
        CBioseq_Handle::TId bh_ids = scope.GetIds(seq_id);
        NcbiCout << "Synonyms for " << id << ": ";
        string sep = "";
        ITERATE (CBioseq_Handle::TId, id_it, bh_ids) {
            cout << sep << id_it->AsString();
            sep = ", ";
        }
        cout << endl;

        // Get Bioseq handle for the Seq-id.
        CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(seq_id);
        if ( !bioseq_handle ) {
            ERR_POST("Bioseq not found, with id=" << id);
        }
        else {
            // Dump the seq-entry.
            if ( args["print_entry"] ) {
                cout << MSerial_AsnText <<
                    *bioseq_handle.GetTopLevelEntry().GetCompleteSeq_entry();
            }
        }

        // Test features
        SAnnotSelector sel;
        sel.SetSearchUnresolved()
            .SetResolveAll();
        CSeq_loc loc;
        loc.SetWhole().Assign(seq_id);
        cout << "Features by location:" << endl;
        CFeat_CI fit(scope, loc, sel);
        int fcount = 0;
        for (; fit; ++fit) {
            if ( args["print_feats"] ) {
                cout << MSerial_AsnText << fit->GetOriginalFeature();
            }
            fcount++;
        }
        cout << fcount << " features found" << endl;

        cout << "Features by product:" << endl;
        sel.SetByProduct(true);
        CFeat_CI fitp(scope, loc, sel);
        fcount = 0;
        for (; fitp; ++fitp) {
            if ( args["print_feats"] ) {
                cout << MSerial_AsnText << fitp->GetOriginalFeature();
            }
            fcount++;
        }
        cout << fcount << " features found" << endl;

        // Test alignments
        cout << "Alignments:" << endl;
        sel.SetByProduct(false);
        CAlign_CI ait(scope, loc, sel);
        int acount = 0;
        for (; ait; ++ait) {
            if ( args["print_aligns"] ) {
                cout << MSerial_AsnText << ait.GetOriginalSeq_align();
            }
            acount++;
        }
        cout << acount << " alignments found" << endl;
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CSampleLds2Application().AppMain(argc, argv);
}

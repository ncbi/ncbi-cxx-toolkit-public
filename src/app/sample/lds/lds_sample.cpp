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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Demo of using local data storage
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

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>

//#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objtools/lds/admin/lds_admin.hpp>
#include <objtools/lds/lds.hpp>
#include <objtools/lds/lds_reader.hpp>
#include <objtools/data_loaders/lds/lds_dataloader.hpp>

using namespace ncbi;
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CSampleLdsApplication : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);

private:
    void x_InitLDS();
private:
    auto_ptr<CLDS_Database>  m_LDS_db;
};



void CSampleLdsApplication::Init(void)
{
    SetDiagPostLevel(eDiag_Info);

    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // GI to fetch
    arg_desc->AddKey("gi", "SeqEntryID", "GI id of the Seq-Entry to fetch",
                     CArgDescriptions::eInteger);



    // Program description
    string prog_description = "Example of the LDS usage\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


int CSampleLdsApplication::Run(void)
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    // CONNECT_Init(&GetConfig());


    /////////////////////////////////////////////////////////////////////////
    // Create object manager
    // * We use CRef<> here to automatically delete the OM on exit.
    // * While the CRef<> exists GetInstance() will return the same object.
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();


    //
    // initialize the local data storage
    //
    try {
        x_InitLDS();
    }
    catch(CException& e) {
        LOG_POST(Error << "error initializing local data storage: "
                 << e.what());
        return 1;
    }

    // Process command line args:  get GI to load
    const CArgs& args = GetArgs();
    int gi = args["gi"].AsInteger();

    NcbiCout << "Searching for gi:" << gi << NcbiEndl;

    // Create a new scope ("attached" to our OM).
    CScope scope(*object_manager);
    // Add default loaders (GB loader in this demo) to the scope.
    scope.AddDefaults();

    // Create Seq-id, set it to the GI specified on the command line
    CSeq_id seq_id;
    seq_id.SetGi(gi);

    /////////////////////////////////////////////////////////////////////////
    // Get list of synonyms for the Seq-id.
    // * With GenBank loader this request should not load the whole Bioseq.
    CBioseq_Handle::TId ids = scope.GetIds(seq_id);
    NcbiCout << "ID: ";
    ITERATE (CBioseq_Handle::TId, id_it, ids) {
        if (id_it != ids.begin())
            NcbiCout << " + "; // print id separator
        NcbiCout << id_it->AsString();
    }
    NcbiCout << NcbiEndl;


    // Done
    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    return 0;
}

void CSampleLdsApplication::x_InitLDS()
{
    const string lds_section_name = "lds";

    const CNcbiRegistry& reg = GetConfig();

    const string& lds_path = reg.GetString(lds_section_name,  "Path", "", IRegistry::fTruncate);
    const string& lds_alias = reg.GetString(lds_section_name, "Alias", "", IRegistry::fTruncate);

    bool recurse_sub_dir = reg.GetBool(lds_section_name, "SubDir", true);

    if (lds_path.empty()) {
    }


    CLDS_Management::ERecurse recurse = 
        (recurse_sub_dir) ? CLDS_Management::eRecurseSubDirs : 
                            CLDS_Management::eDontRecurse;

        bool crc32 = 
            reg.GetBool(lds_section_name, "ControlSum", true);

        // if ControlSum key is true (default), try an alternative "CRC32" key
        // (more straightforward synonym for the same setting)
        if (crc32) {
            crc32 = reg.GetBool(lds_section_name, "CRC32", true);
        }

        CLDS_Management::EComputeControlSum control_sum =
            (crc32) ? CLDS_Management::eComputeControlSum : 
                      CLDS_Management::eNoControlSum;

        bool is_created;
        CLDS_Database *ldb = 
            CLDS_Management::OpenCreateDB(lds_path, "lds.db", 
                                          &is_created, recurse, 
                                          control_sum);
        m_LDS_db.reset(ldb);

        if (!lds_alias.empty()) {
            ldb->SetAlias(lds_alias);
        }

        if (!is_created) {
            CLDS_Management mgmt(*ldb);
            mgmt.SyncWithDir(lds_path, recurse, control_sum);
        }


        try {
            CRef<CLDS_DataLoader> loader
                (CLDS_DataLoader::RegisterInObjectManager
                 (*CObjectManager::GetInstance(),
                  *ldb,
                  CObjectManager::eDefault,
                  80).GetLoader());
        }
        catch (CException& e) {
            LOG_POST(Error << "error creating LDS data loader: " 
                           << e.what() << " Alias:" << lds_alias);
        }

}

/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return 
        CSampleLdsApplication().AppMain(
                    argc, argv, 0, eDS_Default, "lds_sample.ini");
}




/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/01/11 20:41:47  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

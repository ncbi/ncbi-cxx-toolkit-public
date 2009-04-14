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
#include <objmgr/util/sequence.hpp>

//#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objtools/lds/lds_manager.hpp>
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
    arg_desc->AddKey("id", "SeqEntryID", "ID of the Seq-Entry to fetch",
                     CArgDescriptions::eString);


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
    string id = args["id"].AsString();

    NcbiCout << "Searching for :" << id << NcbiEndl;

    // Create a new scope ("attached" to our OM).
    CScope scope(*object_manager);
    // Add default loaders (GB loader in this demo) to the scope.
    scope.AddDefaults();

    // Create Seq-id, set it to the GI specified on the command line
    CSeq_id seq_id (CSeq_id::e_Local, id);
    //CSeq_id seq_id (id);

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

    
    /////////////////////////////////////////////////////////////////////////
    // Get Bioseq handle for the Seq-id.
    // * Most of requests will use this handle.
    CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(seq_id);

    // Terminate the program if the GI cannot be resolved to a Bioseq.
    if ( !bioseq_handle ) {
        ERR_POST(Fatal << "Bioseq not found, with id=" << id);
    }
    string object_title = sequence::GetTitle(bioseq_handle);
    NcbiCout << "Title: " << object_title << NcbiEndl;


    CSeq_entry_Handle eh = bioseq_handle.GetTopLevelEntry();
    CConstRef<CSeq_entry> se = eh.GetCompleteSeq_entry();

    // NcbiCout << MSerial_Xml << *se;

    // Done
    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    return 0;
}


void CSampleLdsApplication::x_InitLDS()
{
    const string lds_section_name = "lds";

    const CNcbiRegistry& reg = GetConfig();

    const string& lds_path = reg.Get(lds_section_name,  "Path",
                                     IRegistry::fTruncate);
    const string& lds_alias = reg.Get(lds_section_name, "Alias",
                                      IRegistry::fTruncate);

    bool recurse_sub_dir = reg.GetBool(lds_section_name, "SubDir", true);

    CLDS_Manager::ERecurse recurse = 
        (recurse_sub_dir) ? CLDS_Manager::eRecurseSubDirs : 
                            CLDS_Manager::eDontRecurse;

        bool crc32 = 
            reg.GetBool(lds_section_name, "ControlSum", true);

        // if ControlSum key is true (default), try an alternative "CRC32" key
        // (more straightforward synonym for the same setting)
        if (crc32) {
            crc32 = reg.GetBool(lds_section_name, "CRC32", true);
        }

        CLDS_Manager::EComputeControlSum control_sum =
            (crc32) ? CLDS_Manager::eComputeControlSum : 
                      CLDS_Manager::eNoControlSum;

        CLDS_Manager mgr(lds_path,lds_path, lds_alias);

        try {
            m_LDS_db.reset(mgr.ReleaseDB());
        } catch (CBDB_ErrnoException& ) {
            mgr.Index(recurse, control_sum);
            m_LDS_db.reset(mgr.ReleaseDB());
        }

        try {
            CRef<CLDS_DataLoader> loader
                (CLDS_DataLoader::RegisterInObjectManager
                 (*CObjectManager::GetInstance(),
                  *m_LDS_db,
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

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
 * Author:  Maxim Didenko
 *
 * File Description:
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

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/scope_transaction.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/data_loader.hpp>

#include <objmgr/edits_db_saver.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/patcher/loaderpatcher.hpp>


//#include "sample_patcher.hpp"
//#include "sample_saver.hpp"
#include "file_db_engine.hpp"

using namespace ncbi;
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


///////////////////////////////////////////////////////////////////////////////////////////
class CEditBioseqSampleApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);

private:
    void x_RunEdits(const CSeq_id& id, IEditsDBEngine& engine, 
                    CNcbiOstream& os);
    void x_RunCheck(const CSeq_id& id, IEditsDBEngine& engine, 
                    CNcbiOstream& os);

};


void CEditBioseqSampleApp::Init(void)
{
    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // GI to fetch
    arg_desc->AddKey("gi", "SeqEntryID", "GI id of the Seq-Entry to fetch",
                     CArgDescriptions::eInteger);
    // arg_desc->SetConstraint
    //    ("gi", new CArgAllow_Integers(2, 40000000));

    // Program description
    string prog_description = "Example of the C++ Object Manager usage\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


void s_PrintDescr(const CBioseq_Handle& handle, CNcbiOstream& os)
{
    unsigned desc_count = 0;
    for (CSeqdesc_CI desc_it(handle); desc_it; ++desc_it) {
        desc_count++;
        const CSeqdesc& desc = *desc_it;
        string label;
        desc.GetLabel(&label, CSeqdesc::eBoth);
        os << label << NcbiEndl;
    }      
    os << "# of descriptors:  " << desc_count << NcbiEndl;
}

void s_PrintIds(const CBioseq_Handle& handle, CNcbiOstream& os)
{
    const CBioseq_Handle::TId& ids = handle.GetId();
    ITERATE(CBioseq_Handle::TId, id, ids) {
        if (id != ids.begin()) os << " ; ";
        os << id->AsString();
    }
    os << NcbiEndl;
}

void s_PrintFeat(const CBioseq_Handle& handle, CNcbiOstream& os)
{
    unsigned feat_count = 0;
    for (CFeat_CI feat_it(handle); feat_it; ++feat_it) {
        feat_count++;
        CSeq_annot_Handle annot = feat_it.GetAnnot();
    }
    os << "# of featues:  " << feat_count << NcbiEndl;
}

void s_RemoveBioseq(int gi, CScope& scope, CNcbiOstream& os)
{
    CSeq_id seq_id;
    seq_id.SetGi(gi);
    CBioseq_Handle handle = scope.GetBioseqHandle(seq_id);

    // Terminate the program if the GI cannot be resolved to a Bioseq.
    if ( !handle ) {
        ERR_POST( "Bioseq not found, with GI=" << gi);
        return;
    }
    handle.GetEditHandle().Remove();
}

int CEditBioseqSampleApp::Run(void)
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    // Process command line args:  get GI to load
    const CArgs& args = GetArgs();
    int gi = args["gi"].AsInteger();

    string db_path = "AsnDB";    
    CFileDBEngine engine(db_path);
    engine.CleanDB();

    // Create Seq-id, set it to the GI specified on the command line
    CSeq_id seq_id;
    seq_id.SetGi(gi);

    CNcbiOstream& os = NcbiCout;

    x_RunEdits(seq_id, engine, os);
    os << NcbiEndl;
    x_RunCheck(seq_id, engine, os);
    return 0;
}

void CEditBioseqSampleApp::x_RunEdits(const CSeq_id& seq_id, 
                                      IEditsDBEngine& engine,
                                      CNcbiOstream& os)
{
    CRef<IEditsDBEngine> ref_engine(&engine);
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CDataLoader> loader(
                 CGBDataLoader::RegisterInObjectManager(*object_manager,
                                           "ID2",
                                            CObjectManager::eNonDefault)
                 .GetLoader());


    CRef<IEditSaver> saver(new CEditsSaver(engine));
    
    CDataLoaderPatcher::RegisterInObjectManager(*object_manager,
                                                loader,
                                                ref_engine,
                                                saver,
                                                CObjectManager::eDefault,
                                                88);
  
    // Create a new scope ("attached" to our OM).
    CScope scope(*object_manager);
    // Add default loaders (GB loader in this demo) to the scope.
    scope.AddDefaults();
        
  
    s_RemoveBioseq(45679, scope, os);

    /////////////////////////////////////////////////////////////////////////
    // Get list of synonyms for the Seq-id.
    // * With GenBank loader this request should not load the whole Bioseq.
    CBioseq_Handle::TId ids = scope.GetIds(seq_id);
    os << "ID: ";
    ITERATE (CBioseq_Handle::TId, id_it, ids) {
        if (id_it != ids.begin())
            os << " + "; // print id separator
        os << id_it->AsString();
    }
    os << NcbiEndl;

    /////////////////////////////////////////////////////////////////////////
    // Get Bioseq handle for the Seq-id.
    // * Most of requests will use this handle.
    CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(seq_id);

    // Terminate the program if the GI cannot be resolved to a Bioseq.
    if ( !bioseq_handle ) {
        ERR_POST(Fatal << "Bioseq not found, with GI=" << seq_id.GetGi());
    }
    s_PrintIds(bioseq_handle, os);

    /////////////////////////////////////////////////////////////////////////
    // Use CSeqdesc iterator.
    // Iterate through all descriptors -- starting from the Bioseq, go
    // all the way up the Seq-entries tree, to the top-level Seq-entry (TSE).

    CBioseq_EditHandle edit = bioseq_handle.GetEditHandle();
    CSeq_entry_Handle parent = bioseq_handle.GetParentEntry();

    CSeq_id id_to_add;
    id_to_add.SetGi(2122545143);
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id_to_add);
    edit.AddId(idh);
    //    edit.RemoveId(idh);
    
    os << "---------------- Before descr is added -----------" << NcbiEndl;       
    s_PrintDescr(bioseq_handle, os);
    os << "----------------------- end ----------------------" << NcbiEndl;
    {
        CRef<CSeqdesc> desc(new CSeqdesc);
        desc->SetComment("-------------- ADDED COMMENT1 ------------");
        CScopeTransaction trans = scope.GetTransaction();
        CBioseq_EditHandle edit = bioseq_handle.GetEditHandle();
        edit.AddSeqdesc(*desc);
        trans.Commit();
    }
    /*
    os << "------------- After descr is added -----" << NcbiEndl;       
    s_PrintDescr(bioseq_handle, os);
    os << "--------------------  end  -------------" << NcbiEndl;       

    */
    os << "Before featre is removed : ";
    CSeq_annot_Handle annot;
    s_PrintFeat(bioseq_handle, os);
    {       
        CScopeTransaction tr = scope.GetTransaction();
        for (CFeat_CI feat_it(bioseq_handle); feat_it; ++feat_it) {
            annot = feat_it.GetAnnot();
            //            annot.GetEditHandle().Remove();
            const CMappedFeat& feat = *feat_it;
            feat.GetSeq_feat_Handle().Remove();
            break;
            
        }
        tr.Commit();
    }
    os << "Before featre is added : ";
    {       
        CScopeTransaction tr = scope.GetTransaction();
        if (annot) {
            CSeq_feat *nf = new CSeq_feat;
            nf->SetTitle("Add Featue");
            nf->SetData().SetComment();
            nf->SetLocation().SetWhole().Assign(*bioseq_handle.GetSeqId());           
            annot.GetEditHandle().AddFeat(*nf);
            /*        for (CFeat_CI feat_it(bioseq_handle); feat_it; ++feat_it) {
            CSeq_annot_Handle annot = feat_it.GetAnnot();
            CSeq_feat *nf = new CSeq_feat;
            nf->SetTitle("Add Featue");
            nf->SetData().SetComment();
            nf->SetLocation().SetWhole().Assign(*bioseq_handle.GetSeqId());           
            annot.GetEditHandle().AddFeat(*nf);*/
            }
        tr.Commit();
    }
    os << "After featre is added : ";
    s_PrintFeat(bioseq_handle, os);
    // Done
    os << "Changes are made." << NcbiEndl;
}


void CEditBioseqSampleApp::x_RunCheck(const CSeq_id& seq_id, 
                                      IEditsDBEngine& engine, 
                                      CNcbiOstream& os)
{
    CRef<IEditsDBEngine> ref_engine(&engine);
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CDataLoader> loader(
                 CGBDataLoader::RegisterInObjectManager(*object_manager,
                                           "ID2",
                                            CObjectManager::eNonDefault)
                 .GetLoader());


    CRef<IEditSaver> saver(new CEditsSaver(engine));
    
    CDataLoaderPatcher::RegisterInObjectManager(*object_manager,
                                                loader,
                                                ref_engine,
                                                saver,
                                                CObjectManager::eDefault,
                                                88);
  
    // Create a new scope ("attached" to our OM).
    CScope scope(*object_manager);
    // Add default loaders (GB loader in this demo) to the scope.
    scope.AddDefaults();

    CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(seq_id);
    s_PrintIds(bioseq_handle, os);
    os << "*******************************" << NcbiEndl;
    s_PrintDescr(bioseq_handle, os);
    os << "*******************************" << NcbiEndl;
    s_PrintFeat(bioseq_handle, os);

    os << "Check is done." << NcbiEndl;
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CEditBioseqSampleApp().AppMain(argc, argv, 0, eDS_Default, 0);
}




/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/01/25 19:00:55  didenko
 * Redisigned bio objects edit facility
 *
 * Revision 1.1  2005/11/15 19:25:22  didenko
 * Added bioseq_edit_sample sample
 *
 * ===========================================================================
 */

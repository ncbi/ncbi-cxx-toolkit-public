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
* Authors:  Maxim Didenko
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiexpt.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/scope_transaction.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/unsupp_editsaver.hpp>

#include <objmgr/util/sequence.hpp>

#include <objmgr/impl/bioseq_set_info.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/patcher/loaderpatcher.hpp>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;
using namespace sequence;

//////////////////////////////////////////////////////////////////////////////
///

class CEmptyDBEngine : public  IEditsDBEngine
{
public:
    
    virtual ~CEmptyDBEngine() {};

    virtual bool HasBlob(const string& blobid) const { return false; }
    virtual bool FindSeqId(const CSeq_id_Handle& id, string& blobid) const 
    { return false; }
    virtual void NotifyIdChanged(const CSeq_id_Handle&, const string&) {}

    virtual void BeginTransaction() {}
    virtual void CommitTransaction() {}
    virtual void RollbackTransaction() {}

    virtual void SaveCommand(const CSeqEdit_Cmd& cmd) {}
    virtual void GetCommands(const string& blobid, TCommands& cmds) const {}
};

//////////////////////////////////////////////////////////////////////////////
///
class CTestApp;

class CTestEditSaver : public CUnsupportedEditSaver
{
public:
    CTestEditSaver(CTestApp& app);

    virtual void BeginTransaction();
    virtual void CommitTransaction();
    virtual void RollbackTransaction();

    virtual void AddDesc(const CBioseq_Handle&, const CSeqdesc&, ECallMode);
    virtual void RemoveDesc(const CBioseq_Handle&, const CSeqdesc&, ECallMode);

    virtual void Add(const CSeq_annot_Handle&, const CSeq_feat&, ECallMode);
    virtual void Replace(const CSeq_feat_Handle&, const CSeq_feat&, ECallMode);
    virtual void Remove(const CSeq_annot_Handle&, const CSeq_feat&, ECallMode);

    virtual void Attach(const CBioObjectId&,
                        const CSeq_entry_Handle&, const CBioseq_Handle&, ECallMode);
    virtual void Attach(const CBioObjectId&,
                        const CSeq_entry_Handle&, const CBioseq_set_Handle&, ECallMode);
    virtual void Remove(const CBioseq_set_Handle&, const CSeq_entry_Handle&,
                        int, ECallMode);
    virtual void Attach(const CBioseq_set_Handle&, const CSeq_entry_Handle&, 
                        int, ECallMode);

    virtual void Detach(const CSeq_entry_Handle&, const CBioseq_Handle&, ECallMode mode);
    virtual void Detach(const CSeq_entry_Handle&, const CBioseq_set_Handle&,
                        ECallMode mode);
    virtual void Remove(const CSeq_entry_Handle&, const CSeq_annot_Handle&, ECallMode);
    virtual void Attach(const CSeq_entry_Handle&, const CSeq_annot_Handle&, ECallMode);



    virtual void AddDesc(const CBioseq_set_Handle& h, const CSeqdesc& d,
                         ECallMode m)
        { CUnsupportedEditSaver::AddDesc(h, d, m); }
    virtual void RemoveDesc(const CBioseq_set_Handle& h, const CSeqdesc& d,
                            ECallMode m)
        { CUnsupportedEditSaver::RemoveDesc(h, d, m); }
    virtual void Add(const CSeq_annot_Handle& h, const CSeq_graph& g, ECallMode m)
    { CUnsupportedEditSaver::Add(h, g, m); }
    virtual void Add(const CSeq_annot_Handle& h, const CSeq_align& a, ECallMode m)
    { CUnsupportedEditSaver::Add(h, a, m); }
    virtual void Replace(const CSeq_graph_Handle& h, const CSeq_graph& g, ECallMode m)
    { CUnsupportedEditSaver::Replace(h, g, m); }
    virtual void Replace(const CSeq_align_Handle& h, const CSeq_align& a, ECallMode m)
    { CUnsupportedEditSaver::Replace(h, a, m); }
    virtual void Remove(const CSeq_annot_Handle& h, const CSeq_graph& g, ECallMode m)
    { CUnsupportedEditSaver::Remove(h, g, m); }
    virtual void Remove(const CSeq_annot_Handle& h, const CSeq_align& a, ECallMode m)
    { CUnsupportedEditSaver::Remove(h, a, m); }

private:
    CTestApp& m_App;
};

//////////////////////////////////////////////////////////////////////////////
///

#define DEFINE_OP(operation) \
    void Call##operation() { ++m_##operation; } \
    int Get##operation##Calls() const { return m_##operation; }

class CTestApp : public CNcbiApplication
{
public:
    CTestApp();

    virtual void Init(void);
    virtual int  Run (void);

    DEFINE_OP(BeginTransaction);
    DEFINE_OP(CommitTransaction);
    DEFINE_OP(RollbackTransaction);
    DEFINE_OP(AddDesc);
    DEFINE_OP(RemoveDesc);
    DEFINE_OP(AddFeat);
    DEFINE_OP(ReplaceFeat);
    DEFINE_OP(RemoveFeat);
    DEFINE_OP(AttachAnnot);
    DEFINE_OP(RemoveAnnot);

    DEFINE_OP(AttachBioseq);
    DEFINE_OP(AttachBioseq_set);
    DEFINE_OP(AttachSeq_entry);
    DEFINE_OP(RemoveSeq_entry);
    DEFINE_OP(DetachBioseq);
    DEFINE_OP(DetachBioseq_set);

    void Clear();

private:

    int m_BeginTransaction;
    int m_CommitTransaction;
    int m_RollbackTransaction;

    int m_AddDesc;
    int m_RemoveDesc;
    
    int m_AddFeat;
    int m_ReplaceFeat;
    int m_RemoveFeat;

    int m_AttachAnnot;
    int m_RemoveAnnot;

    int m_AttachBioseq;
    int m_AttachBioseq_set;
    int m_AttachSeq_entry;
    int m_RemoveSeq_entry;
    int m_DetachBioseq;
    int m_DetachBioseq_set;

};

CTestApp::CTestApp()
{
    Clear();
}
void CTestApp::Clear() 
{
    m_BeginTransaction = 0;
    m_CommitTransaction = 0;
    m_RollbackTransaction = 0;

    m_AddDesc = 0;
    m_RemoveDesc = 0;
    
    m_AddFeat = 0;
    m_ReplaceFeat = 0;
    m_RemoveFeat = 0;
    
    m_AttachAnnot = 0;
    m_RemoveAnnot = 0;

    m_AttachBioseq = 0;
    m_AttachBioseq_set = 0;
    m_AttachSeq_entry = 0;
    m_RemoveSeq_entry = 0;
    m_DetachBioseq = 0;
    m_DetachBioseq_set = 0;

}

//////////////////////////////////////////////////////////////////////////////
///

CTestEditSaver::CTestEditSaver(CTestApp& app)
    : m_App(app)
{
}

void CTestEditSaver::BeginTransaction()
{
    m_App.Clear();
    m_App.CallBeginTransaction();
}
void CTestEditSaver::CommitTransaction()
{
    m_App.CallCommitTransaction();
}
void CTestEditSaver::RollbackTransaction()
{
    m_App.CallRollbackTransaction();
}

void CTestEditSaver::AddDesc(const CBioseq_Handle&, const CSeqdesc&, 
                             IEditSaver::ECallMode mode)
{
    m_App.CallAddDesc();
}
void CTestEditSaver::RemoveDesc(const CBioseq_Handle&, const CSeqdesc&, 
                                IEditSaver::ECallMode mode)
{
    m_App.CallRemoveDesc();
}

void CTestEditSaver::Add(const CSeq_annot_Handle&, const CSeq_feat&, 
                         IEditSaver::ECallMode mode)
{
    m_App.CallAddFeat();
}
void CTestEditSaver::Remove(const CSeq_annot_Handle&, const CSeq_feat&, ECallMode)
{
    m_App.CallRemoveFeat();
}
void CTestEditSaver::Replace(const CSeq_feat_Handle&, const CSeq_feat&, 
                             IEditSaver::ECallMode mode) 
{
    m_App.CallReplaceFeat();
}
void CTestEditSaver::Attach(const CBioObjectId& old_id,
                            const CSeq_entry_Handle&, const CBioseq_Handle&, 
                            IEditSaver::ECallMode mode)
{
    m_App.CallAttachBioseq();
}
void CTestEditSaver::Attach(const CBioObjectId& old_id,
                            const CSeq_entry_Handle&, const CBioseq_set_Handle&, 
                            IEditSaver::ECallMode mode)
{
    m_App.CallAttachBioseq_set();
}
void CTestEditSaver::Remove(const CBioseq_set_Handle&, const CSeq_entry_Handle&,
                            int, IEditSaver::ECallMode mode)
{
    m_App.CallRemoveSeq_entry();
}
void CTestEditSaver::Attach(const CBioseq_set_Handle&, const CSeq_entry_Handle&, 
                            int, IEditSaver::ECallMode mode)
{
    m_App.CallAttachSeq_entry();
}
void CTestEditSaver::Attach(const CSeq_entry_Handle&, 
                            const CSeq_annot_Handle&, IEditSaver::ECallMode mode)
{
    m_App.CallAttachAnnot();
}
void CTestEditSaver::Remove(const CSeq_entry_Handle&, 
                            const CSeq_annot_Handle&, IEditSaver::ECallMode mode)
{
    m_App.CallRemoveAnnot();
}
void CTestEditSaver::Detach(const CSeq_entry_Handle&, 
                            const CBioseq_Handle&,
                            IEditSaver::ECallMode mode)
{
    m_App.CallDetachBioseq();
}
void CTestEditSaver::Detach(const CSeq_entry_Handle&, 
                            const CBioseq_set_Handle&,
                            IEditSaver::ECallMode mode)
{
    m_App.CallDetachBioseq_set();
}

//////////////////////////////////////////////////////////////////////////////
///

#define THROW(message) NCBI_THROW(CException,eUnknown,(message))

void CTestApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddKey("gi", "SeqEntryID", "GI id of the Seq-Entry to fetch",
                     CArgDescriptions::eInteger);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "test_editsaver", false);

    SetupArgDescriptions(arg_desc.release());
}

static void s_AddDesc(const CBioseq_Handle& handle)
{
    CBioseq_EditHandle ehandle = handle.GetEditHandle();
    CRef<CSeqdesc> desc(new CSeqdesc);
    desc->SetComment("-------------- ADDED COMMENT1 ------------");
    ehandle.AddSeqdesc(*desc);
}

const string kAddedFeatTitle = "Added Feature";
const string kAddedAnnotName = "TestAnnot";
static void s_AddFeat(const CBioseq_Handle& handle) 
{
    TGi gi;
    CSeq_id_Handle sid = GetId(handle, eGetId_ForceGi);
    if (!sid)
        THROW("Bioseq does not have a gi seq_id");
    gi = sid.GetGi();
    CSeq_annot_EditHandle feat_annot;
    CSeq_annot_CI annot_it(handle);
    for(; annot_it; ++annot_it) {
        const CSeq_annot_Handle& annot = *annot_it;
        if ( annot.IsNamed() && annot.GetName() == kAddedAnnotName
             && annot.IsFtable()) {
            feat_annot = annot.GetEditHandle();
            break;
        }
    }
    if (!feat_annot) 
        return;
    CRef<CSeq_feat> new_feat(new CSeq_feat);
    new_feat->SetTitle(kAddedFeatTitle);
    new_feat->SetData().SetComment();
    new_feat->SetLocation().SetWhole().SetGi(gi);
    CSeq_feat_EditHandle fh = feat_annot.AddFeat(*new_feat);
    feat_annot.TakeFeat(fh);
}

static void s_RemoveFeat(const CBioseq_Handle& handle)
{
    CFeat_CI feat_it(handle);
    for(; feat_it; ++feat_it) {
        const CMappedFeat& feat = *feat_it;
        if (feat.IsSetTitle() && feat.GetTitle() == kAddedFeatTitle) 
            CSeq_feat_EditHandle(feat.GetSeq_feat_Handle()).Remove();
    }
}

static void s_AddAnnot(const CBioseq_Handle& handle)
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetNameDesc(kAddedAnnotName);
    annot->SetData().SetFtable();
    handle.GetEditHandle().AttachAnnot(*annot);
}
static void s_RemoveAnnot(const CBioseq_Handle& handle)
{
    vector<CSeq_annot_EditHandle> to_remove;
    CSeq_annot_CI annot_it(handle.GetParentEntry());
    for(; annot_it; ++annot_it) {
        const CSeq_annot_Handle& annot = *annot_it;
        if ( annot.IsNamed() && annot.GetName() ==  kAddedAnnotName
             && annot.IsFtable()) {
            to_remove.push_back(annot.GetEditHandle());
        }
    }
    ITERATE(vector<CSeq_annot_EditHandle>, it, to_remove) {
        it->Remove();
    }
}

#define CHECK3(operation, count, message)    \
    if ( Get##operation##Calls() != count )         \
        THROW( #operation " : wrong calls number. " message+\
               NStr::IntToString(Get##operation##Calls())+" <> "+\
               NStr::IntToString(count))

#define CHECK(operation, count)  CHECK3(operation, count, "")


int CTestApp::Run(void)
{
    const CArgs& args = GetArgs();
    TGi gi = GI_FROM(int, args["gi"].AsInteger());

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

    CRef<CDataLoader> loader(
           CGBDataLoader::RegisterInObjectManager(*object_manager,
                                                  "ID2",
                                                  CObjectManager::eDefault)
           .GetLoader());
    CRef<IEditsDBEngine> dbengine(new CEmptyDBEngine);
    CRef<IEditSaver> saver(new CTestEditSaver(*this));

    
    CDataLoaderPatcher::RegisterInObjectManager(*object_manager,
                                                loader,
                                                dbengine,
                                                saver,
                                                CObjectManager::eDefault,
                                                88);
  
    CScope scope(*object_manager);
    scope.AddDefaults();
        
    CSeq_id seq_id;
    seq_id.SetGi(gi);

    bool ok = true;
    try {
        CBioseq_Handle bioseq = scope.GetBioseqHandle(seq_id);

        if ( !bioseq ) 
            THROW("Bioseq not found");

        CSeq_entry_Handle entry = bioseq.GetParentEntry();
        
        if( entry.IsSeq() ) {
            {
                CScopeTransaction tr = scope.GetTransaction();
                entry.GetEditHandle().ConvertSeqToSet();
                CHECK(BeginTransaction, 1);
                CHECK(DetachBioseq, 1);
                CHECK(AttachBioseq_set, 1);
                CHECK(AttachSeq_entry, 1);
                CHECK(AttachBioseq, 1);
            }
            CHECK(RollbackTransaction, 1);
            CHECK(DetachBioseq_set, 1);
            CHECK(RemoveSeq_entry, 1);
            CHECK(AttachBioseq, 2);
            {
                CScopeTransaction tr = scope.GetTransaction();
                CBioseq_set_Handle bioseq_set = 
                    entry.GetEditHandle().ConvertSeqToSet();
                bioseq = bioseq_set.GetParentEntry().GetSingleSubEntry().GetSeq();
                s_AddDesc(bioseq);
                s_AddAnnot(bioseq);
                tr.Commit();
            }
            CHECK(BeginTransaction, 1);
            CHECK(CommitTransaction, 1);
            CHECK(RollbackTransaction, 0);
            {
                CScopeTransaction tr = scope.GetTransaction();
                entry.GetEditHandle().ConvertSetToSeq();
                CHECK(BeginTransaction, 1);
                CHECK(DetachBioseq_set, 1);
                CHECK(AttachBioseq, 1);
            }
            CHECK(RollbackTransaction, 1);
            CHECK(DetachBioseq, 2);
            CHECK(AttachBioseq_set, 1);
            CHECK(AttachBioseq, 2);
            bioseq = entry.GetEditHandle().ConvertSetToSeq();
            CHECK(BeginTransaction, 1);
            CHECK(DetachBioseq_set, 1);
            CHECK(AttachBioseq, 1);
            CHECK(CommitTransaction, 1);
        }
        {
            CScopeTransaction tr = scope.GetTransaction();
            s_AddDesc(bioseq);
            CHECK(BeginTransaction, 1);
            CHECK(AddDesc, 1);
        }
        CHECK(RemoveDesc, 1);
        CHECK(RollbackTransaction, 1);
        {
            CScopeTransaction tr = scope.GetTransaction();
            s_AddDesc(bioseq);
            tr.Commit();
        }
        CHECK(CommitTransaction, 1);
        {
            CScopeTransaction tr = scope.GetTransaction();
            s_AddAnnot(bioseq);
            CHECK(AttachAnnot, 1);
        }
        CHECK(RemoveAnnot, 1);
        Clear();
        s_AddAnnot(bioseq);
        CHECK(BeginTransaction, 1);
        CHECK(CommitTransaction, 1);
        CHECK(AttachAnnot, 1);
        {
            CScopeTransaction tr = scope.GetTransaction();
            s_AddFeat(bioseq);
            CHECK(AddFeat, 2);
        }
        CHECK(RemoveFeat, 3);
        s_AddFeat(bioseq);
        {
            CScopeTransaction tr = scope.GetTransaction();
            s_RemoveFeat(bioseq);
            CHECK(RemoveFeat, 1);
        }
        CHECK(AddFeat, 1);
        s_RemoveFeat(bioseq);
        {
            CScopeTransaction tr = scope.GetTransaction();
            s_RemoveAnnot(bioseq);
            CHECK(RemoveAnnot, 2);
        }
        CHECK(AttachAnnot, 2);
        s_RemoveAnnot(bioseq);

        {
            CScopeTransaction tr = scope.GetTransaction();
            bioseq.GetEditHandle().Remove();
            CHECK(DetachBioseq, 1);
            CHECK(RemoveSeq_entry, 1);
        }
        CHECK(AttachSeq_entry, 1);
        CHECK(AttachBioseq, 1);
        

    } catch (exception& ex) {
        ERR_POST("An Exception is caught : " << ex.what());
        ok = false;
    }
    

    if ( ok ) {
        NcbiCout << " Passed" << NcbiEndl << NcbiEndl;
    }
    else {
        NcbiCout << " Failed" << NcbiEndl << NcbiEndl;
    }

    return ok ? 0 : 1;

}


//////////////////////////////////////////////////////////////////////////////
///
END_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//  MAIN

USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CTestApp().AppMain(argc, argv, 0, eDS_Default, 0);
}

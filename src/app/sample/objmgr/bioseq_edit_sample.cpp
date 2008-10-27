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

#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

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
#include <objmgr/bioseq_ci.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/patcher/loaderpatcher.hpp>


#include "file_db_engine.hpp"

using namespace ncbi;
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


///////////////////////////////////////////////////////////////////////////////////////////

class IOperationTest
{
public:
    IOperationTest(const string& name) : m_Name(name) {}
    virtual ~IOperationTest() {}
    
    virtual bool Do(CScope& scope, CNcbiOstream* os) = 0;
    virtual bool Check(CScope& scope, CNcbiOstream* os) = 0;   

    string GetName() const { return m_Name; }

protected:
    void SetName(const string& name) { m_Name = name; }

private:    
    string m_Name;
};

void s_PrintIds(const CBioseq_Handle& handle, CNcbiOstream& os)
{
    const CBioseq_Handle::TId& ids = handle.GetId();
    ITERATE(CBioseq_Handle::TId, id, ids) {
        if (id != ids.begin()) os << " ; ";
        os << id->AsString();
    }
}

static const CSeq_id_Handle& s_GetNewIdHandle()
{
    static CSeq_id_Handle id = CSeq_id_Handle::GetGiHandle(2122545143);
    return id;
}

void s_PrintIds1(const CBioseq_Handle::TId& ids, CNcbiOstream& os)
{
    os << "ID: ";
    ITERATE (CBioseq_Handle::TId, id_it, ids) {
        if (id_it != ids.begin())
            os << " + "; // print id separator
        os << id_it->AsString();
    }
}

template<typename TParam, typename TFunction>
string ToString(const TParam& param, TFunction func)
{
    CNcbiOstrstream ostr;
    func(param,ostr);
    return CNcbiOstrstreamToString(ostr);
}
class CAddIdChecker : public IOperationTest
{
public:
    CAddIdChecker(const CSeq_id_Handle& seq_id) 
        : IOperationTest("Check Add Id"), 
          m_SeqId(seq_id) {}
    virtual ~CAddIdChecker() {}

    virtual bool Do(CScope& scope, CNcbiOstream* os) 
    {
        CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(m_SeqId);
        CBioseq_EditHandle edit_handle = bioseq_handle.GetEditHandle();
        edit_handle.AddId(s_GetNewIdHandle());
        CBioseq_Handle::TId ids = scope.GetIds(m_SeqId);
        m_Test1 = ToString(ids, &s_PrintIds1);
        m_Test2 = ToString(bioseq_handle, &s_PrintIds);
        return true;
    }
    virtual bool Check(CScope& scope, CNcbiOstream* os)
    {
        CBioseq_Handle::TId ids = scope.GetIds(m_SeqId);
        if (ToString(ids, &s_PrintIds1) != m_Test1 )
            return false;
        ids = scope.GetIds(s_GetNewIdHandle());
        if (ToString(ids, &s_PrintIds1) != m_Test1 )
            return false;
        CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(m_SeqId);
        if (ToString(bioseq_handle, &s_PrintIds) != m_Test2 )
            return false;
        return true;
    }

private:
    const CSeq_id_Handle& m_SeqId;
    string m_Test1;
    string m_Test2;
};

class CRemoveIdChecker : public IOperationTest
{
public:
    CRemoveIdChecker(const CSeq_id_Handle& seq_id) 
        : IOperationTest("Check Remove Id"), 
          m_SeqId(seq_id) {}

    virtual ~CRemoveIdChecker() {}

    virtual bool Do(CScope& scope, CNcbiOstream* os) 
    {
        CBioseq_Handle::TId ids = scope.GetIds(m_SeqId);
        if( ids.empty() )
            return false;
        ITERATE(CBioseq_Handle::TId, id, ids) {
            if (*id != m_SeqId) {
                m_SIH = *id;
                break;
            }
        }
        if ( !m_SIH )
            return false;
        CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(m_SeqId);
        bioseq_handle.GetEditHandle().RemoveId(m_SeqId);
        m_Test1 = ToString(bioseq_handle, &s_PrintIds);
        return true;
    }
    virtual bool Check(CScope& scope, CNcbiOstream* os)
    {
        CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(m_SeqId);
        if (bioseq_handle)
            return false;
        bioseq_handle = scope.GetBioseqHandle(m_SIH);
        if (!bioseq_handle)
            return false;
        if (ToString(bioseq_handle, &s_PrintIds) != m_Test1 )
            return false;
        return true;
    }

private:
    const CSeq_id_Handle& m_SeqId;
    CSeq_id_Handle m_SIH;
    string m_Test1;
};

class CResetIdChecker : public IOperationTest
{
public:
    CResetIdChecker(const CSeq_id_Handle& seq_id) 
        : IOperationTest("Check Reset Id"), 
          m_SeqId(seq_id) {}

    virtual ~CResetIdChecker() {}

    virtual bool Do(CScope& scope, CNcbiOstream* os) 
    {
        CBioseq_Handle::TId ids = scope.GetIds(m_SeqId);
        if( ids.empty() )
            return false;
        CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(m_SeqId);
        CBioseq_EditHandle edit_handle = bioseq_handle.GetEditHandle();
        CScopeTransaction tr = scope.GetTransaction();
        edit_handle.ResetId();
        edit_handle.AddId(s_GetNewIdHandle());
        tr.Commit();
        m_Test1 = ToString(bioseq_handle, &s_PrintIds);
        if (os) *os << m_Test1 << " --> ";
        return true;
    }
    virtual bool Check(CScope& scope, CNcbiOstream* os)
    {
        CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(m_SeqId);
        if (bioseq_handle)
            return false;
        bioseq_handle = scope.GetBioseqHandle(s_GetNewIdHandle());
        if (!bioseq_handle)
            return false;
        if (os) *os << ToString(bioseq_handle, &s_PrintIds) << " " ;
        if (ToString(bioseq_handle, &s_PrintIds) != m_Test1 )
            return false;

        return true;
    }

private:
    const CSeq_id_Handle& m_SeqId;
    string m_Test1;
};

class CRemoveBioseqChecker : public IOperationTest
{
public:
    CRemoveBioseqChecker(const CSeq_id_Handle& seq_id, bool keep_seqentry) 
        : IOperationTest("Check Remove bioseq"), 
          m_SeqId(seq_id) 
    {
        m_RemoveMode = keep_seqentry ? CBioseq_EditHandle::eKeepSeq_entry :
            CBioseq_EditHandle::eRemoveSeq_entry;
        if (keep_seqentry) SetName( GetName() + " (KeepSeq_entry)");
    }

    virtual ~CRemoveBioseqChecker() {}

    virtual bool Do(CScope& scope, CNcbiOstream* os) 
    {
        CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(m_SeqId);     
        if (!bioseq_handle)
            return false;
        CSeq_entry_Handle parent = bioseq_handle.GetParentEntry();
        if (!parent.HasParentEntry())
            return false;
        CBioseq_set_Handle bioseq_set_handle = parent.GetParentBioseq_set();
        {
            if (m_RemoveMode !=  CBioseq_EditHandle::eKeepSeq_entry )  {
                auto_ptr<CObjectOStream> oss( CObjectOStream::Open(eSerial_AsnText,
                                                           "bseq0.ss") );
                *oss << *bioseq_set_handle.GetCompleteObject();
            }
        }
        bioseq_handle.GetEditHandle().Remove(m_RemoveMode);
        CBioseq_CI iter(bioseq_set_handle, CSeq_inst::eMol_not_set, 
                        CBioseq_CI::eLevel_Mains);
        for(; iter; ++iter) {
            CBioseq_Handle bs = *iter;
            if( bs.GetParentEntry().GetParentBioseq_set() != bioseq_set_handle)
                continue;
            CBioseq_Handle::TId ids = bs.GetId();
            if (!ids.empty()) {
                m_SIH = *ids.begin();
                break;
            }
        }
        if (!m_SIH)
            return false;
        m_BSet.Reset(new CBioseq_set);
        m_BSet->Assign(*bioseq_set_handle.GetCompleteObject());
        
        if (m_RemoveMode !=  CBioseq_EditHandle::eKeepSeq_entry )  {
            auto_ptr<CObjectOStream> oss( CObjectOStream::Open(eSerial_AsnText,
                                                               "bseq1.ss") );
            //oss->SetVerifyData(eSerialVerifyData_Never);
            *oss << *m_BSet;
        }
        if (!m_BSet->Equals(*bioseq_set_handle.GetCompleteObject()))
            throw "dfsdffsd";
        return true;
    }
    virtual bool Check(CScope& scope, CNcbiOstream* os)
    {
        CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(m_SeqId);
        if (bioseq_handle)
            return false;
        bioseq_handle = scope.GetBioseqHandle(m_SIH);
        if (!bioseq_handle)
            return false;
        CBioseq_set_Handle bioseq_set_handle = bioseq_handle.GetParentEntry().
            GetParentBioseq_set();
        CConstRef<CBioseq_set> bset = bioseq_set_handle.GetCompleteObject();
        
        if (m_RemoveMode !=  CBioseq_EditHandle::eKeepSeq_entry )  {
            auto_ptr<CObjectOStream> oss( CObjectOStream::Open(eSerial_AsnText,
                                                               "bseq2.ss") );
            //oss->SetVerifyData(eSerialVerifyData_Never);
            *oss << *bset;
        }
        if (m_BSet->Equals(*bset))
            return true;
        return false;
    }

private:
    const CSeq_id_Handle& m_SeqId;
    CSeq_id_Handle m_SIH;
    CBioseq_EditHandle::ERemoveMode m_RemoveMode;
    CRef<CBioseq_set> m_BSet;
};

class CAddBioseqChecker : public IOperationTest
{
public:
    CAddBioseqChecker(const CSeq_id_Handle& seq_id) 
        : IOperationTest("Check Add bioseq"), 
          m_SeqId(seq_id),
          m_BSeq(new CBioseq)
    {       
    }

    virtual ~CAddBioseqChecker() {}

    virtual bool Do(CScope& scope, CNcbiOstream* os) 
    {
        CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(m_SeqId);     
        if (!bioseq_handle)
            return false;
        m_BSeq->Assign(*bioseq_handle.GetCompleteObject());
        m_BSeq->ResetId();
        CRef<CSeq_id> id(new CSeq_id);
        id->Assign(*s_GetNewIdHandle().GetSeqId());
        m_BSeq->SetId().push_back(id);
        CSeq_entry_Handle parent = bioseq_handle.GetParentEntry();
        if (!parent.HasParentEntry())
            return false;
        CBioseq_set_Handle bioseq_set_handle = parent.GetParentBioseq_set();
        bioseq_set_handle.GetEditHandle().AttachBioseq(*m_BSeq);
        return true;
    }
    virtual bool Check(CScope& scope, CNcbiOstream* os)
    {
        CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(s_GetNewIdHandle());
        if (!bioseq_handle)
            return false;
        CConstRef<CBioseq> bseq = bioseq_handle.GetCompleteObject();
        if (m_BSeq->Equals(*bseq))
            return true;
        return false;
    }

private:
    const CSeq_id_Handle& m_SeqId;
    CRef<CBioseq> m_BSeq;
};

class CRemoveFeatChecker : public IOperationTest
{
public:
    CRemoveFeatChecker(const CSeq_id_Handle& seq_id)
        : IOperationTest("Check Remove Feat"), 
          m_SeqId(seq_id),
          m_BSeq(new CBioseq)
    {
    }

    virtual ~CRemoveFeatChecker() {}

    virtual bool Do(CScope& scope, CNcbiOstream* os) 
    {
        CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(m_SeqId);     
        if (!bioseq_handle)
            return false;
        CBioseq_EditHandle beh = bioseq_handle.GetEditHandle();
        CSeq_loc seq_loc;
        seq_loc.SetWhole().SetGi(m_SeqId.GetGi());
        SAnnotSelector sel(CSeqFeatData::e_not_set);
        sel.SetResolveAll();
        for (CFeat_CI feat_it(scope, seq_loc, sel); feat_it; ++feat_it) {
            CSeq_feat_EditHandle(feat_it->GetSeq_feat_Handle()).Remove();
            break;
        }
        m_BSeq->Assign(*bioseq_handle.GetCompleteObject());
        
        return true;
    }

    virtual bool Check(CScope& scope, CNcbiOstream* os)
    {
        CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(m_SeqId);
        if (!bioseq_handle)
            return false;
        CConstRef<CBioseq> bseq = bioseq_handle.GetCompleteObject();
        if (m_BSeq->Equals(*bseq))
            return true;
        return false;
    }
private:
    const CSeq_id_Handle& m_SeqId;
    CRef<CBioseq> m_BSeq;
};

class CAddFeatChecker : public IOperationTest
{
public:
    CAddFeatChecker(const CSeq_id_Handle& seq_id)
        : IOperationTest("Check Add Feat"), 
          m_SeqId(seq_id),
          m_BSeq(new CBioseq)
    {
    }

    virtual ~CAddFeatChecker() {}

    virtual bool Do(CScope& scope, CNcbiOstream* os) 
    {
        CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(m_SeqId);     
        if (!bioseq_handle)
            return false;
        CRef<CSeq_annot> annot(new CSeq_annot);
        annot->AddName("Test Annot");
        annot->SetData().SetFtable();
        CSeq_annot_Handle ah = bioseq_handle.GetEditHandle().AttachAnnot(*annot);

        CRef<CSeq_feat> new_feat(new CSeq_feat);
        new_feat->SetTitle("Test Feature");
        new_feat->SetData().SetComment();
        new_feat->SetLocation().SetWhole().SetGi(m_SeqId.GetGi());
        ah.GetEditHandle().AddFeat(*new_feat);

        m_BSeq->Assign(*bioseq_handle.GetCompleteObject());
        
        return true;
    }

    virtual bool Check(CScope& scope, CNcbiOstream* os)
    {
        CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(m_SeqId);
        if (!bioseq_handle)
            return false;
        CConstRef<CBioseq> bseq = bioseq_handle.GetCompleteObject();
        if (m_BSeq->Equals(*bseq))
            return true;
        return false;
    }
private:
    const CSeq_id_Handle& m_SeqId;
    CRef<CBioseq> m_BSeq;
};



class CEditBioseqSampleApp : public CNcbiApplication
{
public:
    CEditBioseqSampleApp();

    virtual void Init(void);
    virtual int  Run (void);
   

private:
    CRef<CScope> x_CreateScope();

    typedef AutoPtr<IOperationTest> TOperation;
    typedef list<TOperation> TOperations;
    TOperations m_Operations;
    
    CFileDBEngine m_DbEngine;
};


CEditBioseqSampleApp::CEditBioseqSampleApp()
    : m_DbEngine("AsnDB")
{
    m_DbEngine.DropDB();
}

CRef<CScope> CEditBioseqSampleApp::x_CreateScope()
{
    CRef<IEditsDBEngine> ref_engine(&m_DbEngine);
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CDataLoader> loader(
                 CGBDataLoader::RegisterInObjectManager(*object_manager,
                                           "ID2",
                                            CObjectManager::eNonDefault)
                 .GetLoader());

    CRef<IEditSaver> saver(new CEditsSaver(m_DbEngine));

    CDataLoaderPatcher::RegisterInObjectManager(*object_manager,
                                                loader,
                                                ref_engine,
                                                saver,
                                                CObjectManager::eDefault,
                                                88);

    // Create a new scope ("attached" to our OM).
    CRef<CScope> scope(new CScope(*object_manager));
    // Add default loaders (GB loader in this demo) to the scope.
    scope->AddDefaults();
    return scope;
}


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

/*
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
*/

int CEditBioseqSampleApp::Run(void)
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    // Process command line args:  get GI to load
    const CArgs& args = GetArgs();
    int gi = args["gi"].AsInteger();

    // Create Seq-id, set it to the GI specified on the command line
    CSeq_id_Handle seq_id(CSeq_id_Handle::GetGiHandle(gi));

    CNcbiOstream& os = NcbiCout;

    m_Operations.push_back(new CAddIdChecker(seq_id));
    m_Operations.push_back(new CRemoveIdChecker(seq_id));
    m_Operations.push_back(new CResetIdChecker(seq_id));
    m_Operations.push_back(new CRemoveBioseqChecker(seq_id, true));
    m_Operations.push_back(new CRemoveBioseqChecker(seq_id, false));
    m_Operations.push_back(new CAddBioseqChecker(seq_id));
    m_Operations.push_back(new CRemoveFeatChecker(seq_id));
    m_Operations.push_back(new CAddFeatChecker(seq_id));

    NON_CONST_ITERATE(TOperations, it, m_Operations) {
        m_DbEngine.DropDB();
        IOperationTest& ot = **it;
        os << ot.GetName() << "... ";
        {
        CRef<CScope> scope = x_CreateScope();
        if (!ot.Do(*scope, &os)) {
            os << "[Skipped]";
            continue;
        }
        }
        {
        CRef<CScope> scope = x_CreateScope();
        if (ot.Check(*scope, &os)) os << "[OK]";
        else os << "[FAILED]";
        }
        os << NcbiEndl;           
    }
    
    return 0;
}

/*
void CEditBioseqSampleApp::x_RunEdits(const CSeq_id& seq_id, 
                                      CNcbiOstream& os)
{
    CRef<CScope> pscope = CreateScope();
    CScope& scope = *pscope;
         
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

    edit.AddId(GetNewIdHandle());
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
    os << "------------- After descr is added -----" << NcbiEndl;       
    s_PrintDescr(bioseq_handle, os);
    os << "--------------------  end  -------------" << NcbiEndl;       

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
                    for (CFeat_CI feat_it(bioseq_handle); feat_it; ++feat_it) {
            CSeq_annot_Handle annot = feat_it.GetAnnot();
            CSeq_feat *nf = new CSeq_feat;
            nf->SetTitle("Add Featue");
            nf->SetData().SetComment();
            nf->SetLocation().SetWhole().Assign(*bioseq_handle.GetSeqId());           
            annot.GetEditHandle().AddFeat(*nf);
            }
        tr.Commit();
    }
    os << "After featre is added : ";
    s_PrintFeat(bioseq_handle, os);
    // Done
    os << "Changes are made." << NcbiEndl;
}


void CEditBioseqSampleApp::x_RunCheck(const CSeq_id& seq_id, 
                                      CNcbiOstream& os)
{
    CRef<CScope> pscope = CreateScope();
    CScope& scope = *pscope;

    CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(seq_id);
    s_PrintIds(bioseq_handle, os);
    os << "*******************************" << NcbiEndl;
    s_PrintDescr(bioseq_handle, os);
    os << "*******************************" << NcbiEndl;
    s_PrintFeat(bioseq_handle, os);

    os << "Check is done." << NcbiEndl;
}
*/



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CEditBioseqSampleApp().AppMain(argc, argv);
}

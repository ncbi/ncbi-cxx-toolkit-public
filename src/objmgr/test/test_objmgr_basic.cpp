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
* Author:
*           Andrei Gourianov
*
* File Description:
*           1.1 Basic tests
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/data_loader.hpp>
#include <objmgr/annot_selector.hpp>
#include <map>
#include <set>
#include <vector>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;


//===========================================================================
// CTestDataLoader

class CTestDataLoader : public CDataLoader
{
public:
    typedef SRegisterLoaderInfo<CTestDataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& loader_name,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_Default);
    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& /*id*/,
                                    EChoice /*choice*/)
        {
            return TTSE_LockSet();
        }

private:
    friend class CTestLoaderMaker;

    CTestDataLoader(const string& loader_name) : CDataLoader(loader_name)
        {
        }
};


class CTestLoaderMaker : public CLoaderMaker_Base
{
public:
    CTestLoaderMaker(const string& name)
        {
            m_Name = name;
        }

    virtual CDataLoader* CreateLoader(void) const
        {
            return new CTestDataLoader(m_Name);
        }
    typedef CTestDataLoader::TRegisterLoaderInfo TRegisterInfo;
    TRegisterInfo GetRegisterInfo(void)
        {
            TRegisterInfo info;
            info.Set(m_RegisterInfo.GetLoader(), m_RegisterInfo.IsCreated());
            return info;
        }
};


CTestDataLoader::TRegisterLoaderInfo CTestDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& loader_name,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    CTestLoaderMaker maker(loader_name);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


//===========================================================================
// CTestApplication

class CTestApplication : public CNcbiApplication
{
public:
    CRef<CSeq_entry> CreateTestEntry(void);
    virtual int Run( void);
};

CRef<CSeq_entry> CTestApplication::CreateTestEntry(void)
//---------------------------------------------------------------------------
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSeq().SetId();
    entry->SetSeq().SetInst();
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_not_set);
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_not_set);
    return entry;
}

int CTestApplication::Run()
//---------------------------------------------------------------------------
{
    int error = 0;

    string name1("DL_1"), name2("DL_2");

NcbiCout << "1.1.1 Creating CScope ==============================" << NcbiEndl;
{
    {
        CRef< CObjectManager> pOm = CObjectManager::GetInstance();
        {
            CTestDataLoader::RegisterInObjectManager
                (*pOm, name1, CObjectManager::eNonDefault);
            CTestDataLoader::RegisterInObjectManager
                (*pOm, name2, CObjectManager::eDefault);

            // scope in CRef container
            CRef< CScope> pScope1(new CScope(*pOm));
            pScope1->AddDefaults(); // loader 2 added
            pScope1->AddDataLoader(name1);
            // scope on the stack
            CScope scope2(*pOm);
            scope2.AddDefaults(); // loader 2 added
            // scope on the heap
            CScope* pScope3 = new CScope(*pOm);
            pScope3->AddDefaults(); // loader 2 added
            delete pScope3; //  loader 2 alive
        }
        // scopes deleted, all dataloaders alive
        pOm->RevokeDataLoader(name1);
        pOm->RevokeDataLoader(name2);
    }
// objmgr deleted, all dataloaders deleted
}
NcbiCout << "1.1.2 Adding Seq_entry to the scope=================" << NcbiEndl;
{
    {
        CRef< CObjectManager> pOm = CObjectManager::GetInstance();
        {
            // 3 scopes
            CRef< CScope> pScope1(new CScope(*pOm));
            CScope scope2(*pOm);
            CRef<CScope> pScope3(new CScope(*pOm));
            CRef< CSeq_entry> pEntry = CreateTestEntry();
            // add entry to all scopes
            pScope1->AddTopLevelSeqEntry( *pEntry);
            scope2.AddTopLevelSeqEntry( *pEntry);
            CSeq_entry_Handle eh = pScope3->AddTopLevelSeqEntry( *pEntry);

            pScope1.Reset();
            pScope3.Reset();
            eh.Reset();
            //delete pScope3; // data source and seq_entry alive
        }
        // scopes deleted, seq_entry and data source deleted
    }
// objmgr deleted
}
NcbiCout << "1.1.3 Handling Data loader==========================" << NcbiEndl;
{
    {
        CRef< CObjectManager> pOm = CObjectManager::GetInstance();
        {
            CScope* pScope1 = new CScope(*pOm);
            pScope1->AddDefaults(); // nothing added
            // must throw an exception: dataloader1 not found
            NcbiCout << "Expecting exception:" << NcbiEndl;
            try {
                pScope1->AddDataLoader( name1);
                NcbiCout << "ERROR: AddDataLoader has succeeded" << NcbiEndl;
                error += 1;
            }
            catch (exception& e) {
                NcbiCout << "Expected exception: " << e.what() << NcbiEndl;
            }
            CTestDataLoader::RegisterInObjectManager(*pOm, name1);
            pScope1->AddDefaults(); // nothing added
            pScope1->AddDataLoader( name1); // ok
            // must fail - dataloader1 is in use
            NcbiCout << "Expecting error:" << NcbiEndl;
            if (pOm->RevokeDataLoader( name1))
            {
                NcbiCout << "ERROR: RevokeDataLoader has succeeded" << NcbiEndl;
                error += 2;
            }
            delete pScope1; // loader1 alive
            // delete dataloader1
            pOm->RevokeDataLoader( name1); // ok
            // must throw an exception - dataloader1 not registered
            NcbiCout << "Expecting exception:" << NcbiEndl;
            try {
                pOm->RevokeDataLoader( name1);
                NcbiCout << "ERROR: RevokeDataLoader has succeeded" << NcbiEndl;
                error += 4;
            }
            catch (exception& e) {
                NcbiCout << "Expected exception: " << e.what() << NcbiEndl;
            }
        }
    }
}
{
    SAnnotSelector sel;
    map<string, set<int> > nav;
    vector<string> aa;
    for ( int i = 0; i < 100; ++i ) {
        string acc = "NA"+NStr::IntToString(rand()%1000);
        if ( nav.count(acc) ) continue;
        aa.push_back(acc);
        set<int>& vv = nav[acc];
        int nv = rand()%4;
        for ( int j = 0; j < nv; ++j ) {
            int v = rand()%10;
            vv.insert(v);
            string accv = acc+"."+NStr::IntToString(v);
            sel.IncludeNamedAnnotAccession(accv);
            //NcbiCout << "IncludeNamedAnnotAccession("<<accv<<")"<<NcbiEndl;
        }
        if ( nv == 0 ) {
            sel.IncludeNamedAnnotAccession(acc);
            //NcbiCout << "IncludeNamedAnnotAccession("<<acc<<")"<<NcbiEndl;
        }
    }
    for ( size_t t = 0; t < aa.size(); ++t ) {
        string acc = aa[t];
        acc[acc.size()-1]--;
        for ( int t1 = 0; t1 < 2; ++t1, acc[acc.size()-1]++ ) {
            const set<int>* vv = nav.count(acc)? &nav[acc]: 0;
            //NcbiCout << "IsIncludedNamedAnnotAccession("<<acc<<") "<< (vv?vv->size():0) <<NcbiEndl;
            if ( !vv || !vv->empty() ) {
                assert(!sel.IsIncludedNamedAnnotAccession(acc));
            }
            else {
                assert(sel.IsIncludedNamedAnnotAccession(acc));
            }
            for ( int s = 0; s < 100; ++s ) {
                int v = rand()%10;
                string accv = acc+"."+NStr::IntToString(v);
                //NcbiCout<<"IsIncludedNamedAnnotAccession("<<accv<<")"<<NcbiEndl;
                if ( !vv || (!vv->empty() && !vv->count(v)) ) {
                    assert(!sel.IsIncludedNamedAnnotAccession(accv));
                }
                else {
                    assert(sel.IsIncludedNamedAnnotAccession(accv));
                }
            }
        }
    }
}
if ( error > 0 ) {
NcbiCout << "==================================================" << NcbiEndl;
 NcbiCout << "ERROR " << error << ": Some tests failed." << NcbiEndl;
}
else {
NcbiCout << "==================================================" << NcbiEndl;
NcbiCout << "Test completed successfully" << NcbiEndl;
}
    return error;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;

//===========================================================================
// entry point

int main( int argc, const char* argv[])
{
    return CTestApplication().AppMain( argc, argv, 0, eDS_Default, 0);
}

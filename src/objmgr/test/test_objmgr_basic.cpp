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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.23  2004/07/26 14:13:32  grichenk
* RegisterInObjectManager() return structure instead of pointer.
* Added CObjectManager methods to manipuilate loaders.
*
* Revision 1.22  2004/07/21 15:51:25  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.21  2004/05/21 21:42:14  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.20  2004/03/16 15:47:28  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.19  2003/11/26 20:57:06  vasilche
* Removed redundant const before enum parameter.
*
* Revision 1.18  2003/09/30 16:22:05  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.17  2003/06/19 18:23:46  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.16  2003/06/02 16:06:39  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.15  2003/05/21 16:03:08  vasilche
* Fixed access to uninitialized optional members.
* Added initialization of mandatory members.
*
* Revision 1.14  2003/05/20 15:44:39  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.13  2003/04/29 19:51:14  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.12  2003/04/24 20:48:47  vasilche
* Added missing header.
*
* Revision 1.11  2003/03/27 21:54:58  grichenk
* Renamed test applications and makefiles, updated references
*
* Revision 1.10  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.9  2002/11/04 21:29:14  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.8  2002/10/23 12:43:31  gouriano
* comment out unused parameters
*
* Revision 1.7  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.6  2002/05/14 20:06:28  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.5  2002/05/06 03:28:53  vakatov
* OM/OM1 renaming
*
* Revision 1.4  2002/04/02 14:51:52  gouriano
* *** empty log message ***
*
* Revision 1.3  2002/03/20 17:03:51  gouriano
* *** empty log message ***
*
* Revision 1.2  2002/03/01 20:12:17  gouriano
* *** empty log message ***
*
* Revision 1.1  2002/03/01 19:41:35  gouriano
* *** empty log message ***
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/data_loader.hpp>

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
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    virtual void GetRecords(const CSeq_id_Handle& /*id*/,
                            EChoice /*choice*/)
        {
        }

private:
    CTestDataLoader(const string& loader_name) : CDataLoader( loader_name)
        {
        }
};


CTestDataLoader::TRegisterLoaderInfo CTestDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& loader_name,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    TRegisterLoaderInfo info;
    CDataLoader* loader = om.FindDataLoader(loader_name);
    if ( loader ) {
        info.Set(loader, false);
        return info;
    }
    loader = new CTestDataLoader(loader_name);
    CObjectManager::TRegisterLoaderInfo base_info =
        CDataLoader::RegisterInObjectManager(om, loader_name, *loader,
                                             is_default, priority);
    info.Set(base_info.GetLoader(), base_info.IsCreated());
    return info;
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
            CTestDataLoader *pLoader1 =
                CTestDataLoader::RegisterInObjectManager(
                *pOm, name1, CObjectManager::eNonDefault).GetLoader();
            CTestDataLoader *pLoader2 =
                CTestDataLoader::RegisterInObjectManager(
                *pOm, name2, CObjectManager::eDefault).GetLoader();

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
            pScope3->AddTopLevelSeqEntry( *pEntry);

            pScope1.Reset();
            pScope3.Reset();
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
            pOm->RevokeDataLoader(name1);
            CScope* pScope1 = new CScope(*pOm);
            pScope1->AddDefaults(); // nothing added
            // must throw an exception: dataloader1 not found
            NcbiCout << "Expecting exception:" << NcbiEndl;
            try {
                pScope1->AddDataLoader( name1);
                NcbiCout << "ERROR: AddDataLoader has succeeded" << NcbiEndl;
                error = 1;
            }
            catch (exception& e) {
                NcbiCout << "Expected exception: " << e.what() << NcbiEndl;
            }
            CTestDataLoader *pLoader1 =
                CTestDataLoader::RegisterInObjectManager(*pOm, name1)
                .GetLoader();
            pScope1->AddDefaults(); // nothing added
            pScope1->AddDataLoader( name1); // ok
            // must fail - dataloader1 is in use
            NcbiCout << "Expecting error:" << NcbiEndl;
            if (pOm->RevokeDataLoader( name1))
            {
                NcbiCout << "ERROR: RevokeDataLoader has succeeded" << NcbiEndl;
                error = 1;
            }
            delete pScope1; // loader1 alive
            // delete dataloader1
            pOm->RevokeDataLoader( name1); // ok
            // must throw an exception - dataloader1 not registered
            NcbiCout << "Expecting exception:" << NcbiEndl;
            try {
                pOm->RevokeDataLoader( name1);
                NcbiCout << "ERROR: RevokeDataLoader has succeeded" << NcbiEndl;
                error = 1;
            }
            catch (exception& e) {
                NcbiCout << "Expected exception: " << e.what() << NcbiEndl;
            }
        }
    }
}
if ( error ) {
NcbiCout << "==================================================" << NcbiEndl;
NcbiCout << "ERROR: Some tests failed." << NcbiEndl;
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


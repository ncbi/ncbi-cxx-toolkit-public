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
* Revision 1.2  2002/03/21 01:34:57  kimelman
* gbloader related bugfixes
*
* Revision 1.1  2002/03/20 04:50:16  kimelman
* GB loader added
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

#include <corelib/ncbiapp.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objects/objmgr1/object_manager.hpp>
#include <objects/objmgr1/scope.hpp>
#include <objects/objmgr1/gbloader.hpp>
#include <objects/objmgr1/reader_id1.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/objmgr1/bioseq_handle.hpp>
#include <objects/objmgr1/seq_vector.hpp>
#include <objects/objmgr1/desc_ci.hpp>
#include <objects/objmgr1/feat_ci.hpp>
#include <objects/objmgr1/align_ci.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/general/Date.hpp>

#include <serial/serial.hpp>
#include <serial/objostrasn.hpp>
#include <serial/typeref.hpp>

BEGIN_NCBI_SCOPE
using namespace objects;

//===========================================================================
// CTestDataLoader

class CTestDataLoader : public CGBDataLoader
{
public:
    CTestDataLoader(const string& loader_name) : CGBDataLoader(loader_name) {};
    CTestDataLoader() : CGBDataLoader("tiny-id",new CId1Reader) {};
};


//===========================================================================
// CTestApplication

class CTestApplication : public CNcbiApplication
{
public:
    virtual int Run( void);
};

int CTestApplication::Run()
//---------------------------------------------------------------------------
{
  string name1("ID_regular");
NcbiCout << "      Reading Data    ==============================" << NcbiEndl;
{
    {
        CRef< CObjectManager> pOm = new CObjectManager;
        {
            CRef< CTestDataLoader> pLoader1 = new CTestDataLoader(name1);
            pOm->RegisterDataLoader( *pLoader1, CObjectManager::eNonDefault);
            CRef< CTestDataLoader> pLoader2 = new CTestDataLoader();
            pOm->RegisterDataLoader( *pLoader2, CObjectManager::eDefault);
            
            int i = 16;
            while(i<1800)
              {
                CScope scope2(*pOm);
                scope2.AddDefaults(); // loader 2 added
                NcbiCout << "gi " << i << NcbiEndl;
                CSeq_id x;
                x.SetGi(i);
                CObjectOStreamAsn oos(NcbiCout);
                try
                  {
                    iterate(list< CRef< CSeq_id > >, it, scope2.GetBioseqHandle(x).GetBioseq().GetId())
                      {
                        oos << **it;
                        NcbiCout << NcbiEndl;
                      }
                    NcbiCout << NcbiEndl;
                  }
                catch (exception e)
                  {
                    cout << e.what();
                  }
                NcbiCout << NcbiEndl;
                i++;
              }
        }
        // scopes deleted, all dataloaders alive
    }
// objmgr deleted, all dataloaders deleted
}
NcbiCout << "==================================================" << NcbiEndl;
NcbiCout << "Test completed" << NcbiEndl;
    return 0;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;

//===========================================================================
// entry point

int main( int argc, const char* argv[])
{
    return CTestApplication().AppMain( argc, argv, 0, eDS_Default, 0);
}


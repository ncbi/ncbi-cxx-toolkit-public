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
*           Andrei Gourianov, Michael Kimelman
*
* File Description:
*           Basic test of GenBank data loader
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.17  2002/04/16 18:32:37  grichenk
* +feature iterator tests
*
* Revision 1.16  2002/04/10 22:47:58  kimelman
* added pubseq_reader as default one
*
* Revision 1.15  2002/04/09 19:05:02  kimelman
* make gcc happy
*
* Revision 1.14  2002/04/09 18:48:17  kimelman
* portability bugfixes: to compile on IRIX, sparc gcc
*
* Revision 1.13  2002/04/04 01:35:37  kimelman
* more MT tests
*
* Revision 1.12  2002/04/02 16:02:33  kimelman
* MT testing
*
* Revision 1.11  2002/03/30 19:37:07  kimelman
* gbloader MT test
*
* Revision 1.10  2002/03/29 02:47:07  kimelman
* gbloader: MT scalability fixes
*
* Revision 1.9  2002/03/26 17:24:58  grichenk
* Removed extra ++i
*
* Revision 1.8  2002/03/26 15:40:31  kimelman
* get rid of catch clause
*
* Revision 1.7  2002/03/26 00:15:52  vakatov
* minor beautification
*
* Revision 1.6  2002/03/25 15:44:47  kimelman
* proper logging and exception handling
*
* Revision 1.5  2002/03/22 21:53:07  kimelman
* bugfix: skip missed gi's
*
* Revision 1.4  2002/03/21 19:15:53  kimelman
* GB related bugfixes
*
* Revision 1.3  2002/03/21 19:14:55  kimelman
* GB related bugfixes
*
* Revision 1.2  2002/03/21 16:18:21  gouriano
* *** empty log message ***
*
* Revision 1.1  2002/03/20 21:25:00  gouriano
* *** empty log message ***
*
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objects/objmgr1/object_manager.hpp>
#include <objects/objmgr1/scope.hpp>
#include <objects/objmgr1/gbloader.hpp>
#include <objects/objmgr1/reader_id1.hpp>
#include <objects/objmgr1/seq_vector.hpp>
#include <objects/objmgr1/feat_ci.hpp>

#include <serial/serial.hpp>
#include <serial/objostrasn.hpp>


BEGIN_NCBI_SCOPE
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  CTestApplication::
//


class CTestApplication : public CNcbiApplication
{
public:
    virtual int Run( void);
};



int CTestApplication::Run()
{
    time_t start=time(0);
    NcbiCout << "      Reading Data    ==============================" << NcbiEndl;

    CRef< CObjectManager> pOm = new CObjectManager;

    // CRef< CGBDataLoader> pLoader = new CGBDataLoader;
    // pOm->RegisterDataLoader(*pLoader, CObjectManager::eDefault);
    pOm->RegisterDataLoader(*new CGBDataLoader("ID", 0, 2),CObjectManager::eDefault);

    for (int i = 1;  i < 500;  i++) {
        CScope scope(*pOm);
        scope.AddDefaults();
        int gi = i  + 18565551 - i  ; 
        CSeq_id x;
        x.SetGi(gi);
        CObjectOStreamAsn oos(NcbiCout);
        CBioseq_Handle h = scope.GetBioseqHandle(x);
        if ( !h ) {
            LOG_POST("Gi (" << gi << "):: not found in ID");
        } else {
//          scope.ResetHistory();
          iterate (list<CRef<CSeq_id> >, it, h.GetBioseq().GetId()) {
            //oos << **it;
            //NcbiCout << NcbiEndl;
            ;
          }
          CSeqVector v = h.GetSeqVector();
          v.SetIupacCoding();
          LOG_POST("Vector size = " << v.size());
          string vs = "";
          for (unsigned cc = 0; cc < v.size(); cc++) {
              vs += v[cc];
              if (cc > 40) break;
          }
          LOG_POST("Data: " << NStr::PrintableString(vs.substr(0, 40)));
          CRef<CSeq_loc> loc = new CSeq_loc;
          loc->SetWhole().SetGi(gi);
          int fcount = 0;
          {{ // Creating a block to destroy the iterator after using it
              CFeat_CI feat_it1(scope, *loc, CSeqFeatData::e_Cdregion);
              LOG_POST("Iterating CDS features, no references resolving");
              for ( ; feat_it1;  ++feat_it1) {
                  fcount++;
              }
          }}
          LOG_POST("CDS count (non-resolved) = " << fcount);
          fcount = 0;
          {{ // Creating a block to destroy the iterator after using it
              CFeat_CI feat_it2(scope, *loc, CSeqFeatData::e_Cdregion, CFeat_CI::eResolve_All);
              LOG_POST("Iterating CDS features, resolving references");
              for ( ; feat_it2;  ++feat_it2) {
                  fcount++;
              }
          }}
          LOG_POST("CDS count (resolved) = " << fcount);
          LOG_POST("Gi (" << gi << "):: OK");
        }
    }

    NcbiCout << "==================================================" << NcbiEndl;
    NcbiCout << "Test completed (" << (time(0)-start) << " sec ) " << NcbiEndl;
    return 0;
}


END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//
//  MAIN
//


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CTestApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}


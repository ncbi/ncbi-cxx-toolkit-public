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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/feat_ci.hpp>

#include <serial/serial.hpp>
#include <serial/objostrasn.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/seqref.hpp>

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
    CORE_SetLOG(LOG_cxx2c());

    CRef<CObjectManager> pOm = CObjectManager::GetInstance();

    CRef<CGBDataLoader> pLoader(CGBDataLoader::RegisterInObjectManager(*pOm)
        .GetLoader());

    for ( int gi = 18565540;  gi < 18565650; gi++ ) {
        CSeq_id id;
        id.SetGi(gi);
        CConstRef<CSeqref> sr = pLoader->GetSatSatkey(id);
        if ( !sr ) {
            ERR_POST(Fatal << "Gi (" << gi << "):: not found in ID");
        }
        else {
            LOG_POST("Gi (" << gi << "):: sat="<<sr->GetSat()<<
                     " satkey="<<sr->GetSatKey());
        }
    }

    for ( int gi = 18565530;  gi < 18565550; gi++ ) {
        CScope scope(*pOm);
        scope.AddDefaults();
        CSeq_id id;
        id.SetGi(gi);
        CBioseq_Handle h = scope.GetBioseqHandle(id);
        if ( !h ) {
            ERR_POST(Fatal << "Gi (" << gi << "):: not found in ID");
        } else {
//          scope.ResetHistory();
            CConstRef<CBioseq> core = h.GetBioseqCore();
            ITERATE (list<CRef<CSeq_id> >, it, core->GetId()) {
                //CObjectOStreamAsn oos(NcbiCout);
                //oos << **it;
                //NcbiCout << NcbiEndl;
                ;
            }
            CSeqVector v = h.GetSeqVector();
            v.SetIupacCoding();
            LOG_POST("Vector size = " << v.size());
            string vs;
            for (TSeqPos cc = 0; cc < v.size(); cc++) {
                vs += v[cc];
                if (cc > 40) break;
            }
            LOG_POST("Data: " << NStr::PrintableString(vs.substr(0, 40)));
            CRef<CSeq_loc> loc(new CSeq_loc);
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
                CFeat_CI feat_it2(scope, *loc,
                                  SAnnotSelector(CSeqFeatData::e_Cdregion)
                                  .SetResolveAll());
                LOG_POST("Iterating CDS features, resolving references");
                for ( ; feat_it2;  ++feat_it2) {
                    fcount++;
                }
            }}
            LOG_POST("CDS count (resolved) = " << fcount);
            LOG_POST("Gi (" << gi << "):: OK");
        }
    }

    NcbiCout << "=================================================="<<NcbiEndl;
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
    return CTestApplication().AppMain(argc, argv, 0, eDS_Default, 
                                      "test_objmgr_gbloader.ini");
}


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2005/03/10 20:55:07  vasilche
* New CReader/CWriter schema of CGBDataLoader.
*
* Revision 1.10  2004/11/01 19:33:09  grichenk
* Removed deprecated methods
*
* Revision 1.9  2004/08/04 14:55:18  vasilche
* Changed TSE locking scheme.
* TSE cache is maintained by CDataSource.
* Added ID2 reader.
* CSeqref is replaced by CBlobId.
*
* Revision 1.8  2004/07/26 14:13:32  grichenk
* RegisterInObjectManager() return structure instead of pointer.
* Added CObjectManager methods to manipuilate loaders.
*
* Revision 1.7  2004/07/21 15:51:26  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.6  2004/05/21 21:42:52  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.5  2004/04/05 15:56:15  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.4  2004/03/16 15:47:29  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.3  2004/01/13 16:55:58  vasilche
* CReader, CSeqref and some more classes moved from xobjmgr to separate lib.
* Headers moved from include/objmgr to include/objtools/data_loaders/genbank.
*
* Revision 1.2  2003/12/30 22:14:45  vasilche
* Updated genbank loader and readers plugins.
*
* Revision 1.30  2003/12/30 19:51:55  vasilche
* Test CGBDataLoader::GetSatSatkey() method.
*
* Revision 1.29  2003/06/02 16:06:39  dicuccio
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
* Revision 1.28  2003/04/24 16:12:39  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.27  2003/04/15 14:23:11  vasilche
* Added missing includes.
*
* Revision 1.26  2003/03/18 21:48:33  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.25  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.24  2002/12/06 15:36:03  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.23  2002/11/04 21:29:14  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.22  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.21  2002/05/09 22:26:11  kimelman
* test more than one record
*
* Revision 1.20  2002/05/08 22:32:00  kimelman
* log flags
*
* Revision 1.19  2002/05/06 03:28:53  vakatov
* OM/OM1 renaming
*
* Revision 1.18  2002/05/03 21:28:12  ucko
* Introduce T(Signed)SeqPos.
*
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

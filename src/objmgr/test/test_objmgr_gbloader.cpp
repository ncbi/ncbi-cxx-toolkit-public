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

#include <objects/objmgr1/object_manager.hpp>
#include <objects/objmgr1/scope.hpp>
#include <objects/objmgr1/gbloader.hpp>
#include <objects/objmgr1/reader_id1.hpp>

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
    NcbiCout << "      Reading Data    =============================="
             << NcbiEndl;

    CRef< CObjectManager> pOm = new CObjectManager;

    // CRef< CGBDataLoader> pLoader = new CGBDataLoader;
    // pOm->RegisterDataLoader(*pLoader, CObjectManager::eDefault);
    pOm->RegisterDataLoader(*new CGBDataLoader("ID", new CId1Reader, 2),
                            CObjectManager::eDefault);

    int ecount = 0;
    for (int i = 1;  i < 1800;  i++) {
        CScope scope(*pOm);
        scope.AddDefaults();

        NcbiCout << "gi " << i << NcbiEndl;

        CSeq_id x;
        x.SetGi(i);
        CObjectOStreamAsn oos(NcbiCout);
        try {
            CBioseq_Handle h = scope.GetBioseqHandle(x);
            if ( !h ) {
                LOG_POST("Gi (" << i << "):: not found in ID");
            } else {
                iterate (list<CRef<CSeq_id> >, it, h.GetBioseq().GetId()) {
                    //oos << **it;
                    //NcbiCout << NcbiEndl;
                    ;
                }
            }
        }
        catch (exception& e) {
            LOG_POST("TROUBLE(" << i << "):: " << e.what());
            ecount++;
            if (ecount > 20)
                break;
        }
        NcbiCout << NcbiEndl;
        i++;
    }


    NcbiCout << "=================================================="
             << NcbiEndl;
    NcbiCout << "Test completed" << NcbiEndl;
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


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
#include <objtools/data_loaders/genbank/readers.hpp>
#include <dbapi/driver/drivers.hpp>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  CTestApplication::
//


class CTestApplication : public CNcbiApplication
{
public:
    CTestApplication(void)
        : m_Verbose(false)
        {
        }

    virtual void Init(void);
    virtual bool TestApp_Init(const CArgs& args);
    virtual bool TestApp_Args(CArgDescriptions& args);
    virtual int Run( void);

    bool m_Verbose;
};



bool CTestApplication::TestApp_Args(CArgDescriptions& args)
{
    args.AddOptionalKey("other_loaders", "OtherLoaders",
                        "Extra data loaders as plugins (comma separated)",
                        CArgDescriptions::eString);
    args.AddFlag("verbose", "Print extra information");

    args.SetUsageContext(GetArguments().GetProgramBasename(),
                         "test_objmgr_gbloader", false);
    return true;
}


bool CTestApplication::TestApp_Init(const CArgs& args)
{
    m_Verbose = args["verbose"];
    return true;
}


void CTestApplication::Init(void)
{
    //CONNECT_Init(&GetConfig());
    //CORE_SetLOCK(MT_LOCK_cxx2c());
    //CORE_SetLOG(LOG_cxx2c());
    SetDiagPostLevel(eDiag_Info);

    // Prepare command line descriptions
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    // Let test application add its own arguments
    TestApp_Args(*arg_desc);
    SetupArgDescriptions(arg_desc.release());

    if ( !TestApp_Init(GetArgs()) )
        THROW1_TRACE(runtime_error, "Cannot init test application");
}


int CTestApplication::Run()
{
    time_t start=time(0);
    NcbiCout << "      Reading Data    ==============================" << NcbiEndl;
    CORE_SetLOG(LOG_cxx2c());

    CRef<CObjectManager> pOm = CObjectManager::GetInstance();
#ifdef HAVE_PUBSEQ_OS
    DBAPI_RegisterDriver_FTDS();
    GenBankReaders_Register_Pubseq();
    GenBankReaders_Register_Pubseq2();
#endif
    CRef<CGBDataLoader> pLoader(CGBDataLoader::RegisterInObjectManager(*pOm)
        .GetLoader());

    for ( TGi gi = GI_CONST(18565540);  gi < GI_CONST(18565650); gi++ ) {
        auto sr = pLoader->GetBlobId(CSeq_id_Handle::GetGiHandle(gi));
        if ( !sr ) {
            ERR_POST(Fatal << "Gi (" << gi << "):: not found in ID");
        }
        else if ( m_Verbose ) {
            LOG_POST("Gi (" << gi << "):: "<<sr->ToString());
        }
    }

    for ( TGi gi = GI_CONST(18565530);  gi < GI_CONST(18565550); gi++ ) {
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
            if ( m_Verbose ) {
                LOG_POST("Vector size = " << v.size());
            }
            string vs;
            for (TSeqPos cc = 0; cc < v.size(); cc++) {
                vs += v[cc];
                if (cc > 40) break;
            }
            if ( m_Verbose ) {
                LOG_POST("Data: " << NStr::PrintableString(vs.substr(0, 40)));
            }
            CRef<CSeq_loc> loc(new CSeq_loc);
            loc->SetWhole().SetGi(gi);
            int fcount = 0;
            {{ // Creating a block to destroy the iterator after using it
                CFeat_CI feat_it1(scope, *loc, CSeqFeatData::e_Cdregion);
                if ( m_Verbose ) {
                    LOG_POST("Iterating CDS features, no references resolving");
                }
                for ( ; feat_it1;  ++feat_it1) {
                    fcount++;
                }
            }}
            if ( m_Verbose ) {
                LOG_POST("CDS count (non-resolved) = " << fcount);
            }
            fcount = 0;
            {{ // Creating a block to destroy the iterator after using it
                CFeat_CI feat_it2(scope, *loc,
                                  SAnnotSelector(CSeqFeatData::e_Cdregion)
                                  .SetResolveAll());
                if ( m_Verbose ) {
                    LOG_POST("Iterating CDS features, resolving references");
                }
                for ( ; feat_it2;  ++feat_it2) {
                    fcount++;
                }
            }}
            if ( m_Verbose ) {
                LOG_POST("CDS count (resolved) = " << fcount);
                LOG_POST("Gi (" << gi << "):: OK");
            }
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

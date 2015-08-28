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
#include <corelib/ncbithr.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers.hpp>
#include <dbapi/driver/drivers.hpp>

#include <serial/serial.hpp>
#include <serial/objostrasn.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;

/////////////////////////////////////////////////////////////////////////////
//
//  Test thread
//

class CTestThread : public CThread
{
public:
    CTestThread(unsigned id, CScope& scope,int start,int stop);
    virtual ~CTestThread();
  
protected:
    virtual void* Main(void);

private:
    unsigned        m_mode;
    CScope         *m_Scope;
    int             m_Start;
    int             m_Stop;
    bool            m_Verbose;
};

CTestThread::CTestThread(unsigned id, CScope& scope,
                         int start, int stop)
    : m_mode(id), m_Scope(&scope),
      m_Start(start), m_Stop(stop),
      m_Verbose(false)
{
    LOG_POST("Thread " << start << " - " << stop << " - started");
}

CTestThread::~CTestThread()
{
    LOG_POST("Thread " << m_Start << " - " << m_Stop << " - completed");
}

void* CTestThread::Main(void)
{
    CRef<CObjectManager> pom = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*pom);
  
    CScope scope1(*pom);
    scope1.AddDefaults();

    LOG_POST(" Processing gis from "<< m_Start << " to " << m_Stop);
  
    for (TIntId i = m_Start;  i < m_Stop;  i++) {
        CScope scope2(*pom);
        scope2.AddDefaults();

        CScope *s;
        switch(m_mode&3) {
        case 2: s =  &*m_Scope; break;
        case 1: s =  &scope1;   break;
        case 0: s =  &scope2;   break;
        default:
            throw runtime_error("unexpected mode");
        }
    
        TGi gi = i ; // (i + m_Idx)/2+3;
        CSeq_id x;
        x.SetGi(gi);
        CBioseq_Handle h = s->GetBioseqHandle(x);
        if ( !h ) {
            if ( m_Verbose ) {
                LOG_POST(setw(3) << CThread::GetSelf() << ":: gi=" << gi << " :: not found in ID");
            }
        } else {
            //CObjectOStreamAsn oos(NcbiCout);
            CConstRef<CBioseq> core = h.GetBioseqCore();
            ITERATE (list<CRef<CSeq_id> >, it, core->GetId()) {
                //oos << **it;
                //NcbiCout << NcbiEndl;
                ;
            }
            if ( m_Verbose ) {
                LOG_POST(setw(3) << CThread::GetSelf() << ":: gi=" << gi << " OK");
            }
        }
        s->ResetHistory();
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
//
//  CTestApplication::
//


class CTestApplication : public CNcbiApplication
{
public:
    virtual int Run(void);
    int Test(const unsigned test_id,const unsigned thread_count);
};

const unsigned c_TestFrom = 2;
const unsigned c_TestTo   = 202;
const unsigned c_GI_count = c_TestTo - c_TestFrom;

int CTestApplication::Test(const unsigned test_mode,const unsigned thread_count)
{
    int step= c_GI_count/thread_count;
    CRef<CObjectManager> pOm = CObjectManager::GetInstance();
    CScope         scope(*pOm);
    typedef CTestThread* CTestThreadPtr;
  
    CTestThreadPtr  *thr = new CTestThreadPtr[thread_count];

    CGBDataLoader::RegisterInObjectManager(*pOm);
    scope.AddDefaults();
    
    for (unsigned i=0; i<thread_count; ++i)
        {
            thr[i] = new CTestThread(test_mode, scope,
                                     c_TestFrom+i*step,
                                     c_TestFrom+(i+1)*step);
            thr[i]->Run(CThread::fRunAllowST);
        }
    
    for (unsigned i=0; i<thread_count; i++) {
        LOG_POST("Thread " << i << " @join");
        thr[i]->Join();
    }
    
#if 0 
    // Destroy all threads : has already been destroyed by join
    for (unsigned i=0; i<thread_count; i++) {
        LOG_POST("Thread " << i << " @delete");
        delete thr[i];
    }
#endif
    
    delete [] thr;
    return 0;
}

const char* const kGlobalOMTags[] = { "local ", "global" };
const char* const kGlobalScopeTags[]
    = {"auto      ", "per thread", "global    " };

int CTestApplication::Run()
{
    unsigned timing[4/*threads*/][2/*om*/][3/*scope*/];
    unsigned tc = sizeof(timing)/sizeof(*timing);

    memset(timing,0,sizeof(timing));
  
    CORE_SetLOCK(MT_LOCK_cxx2c());
    CORE_SetLOG(LOG_cxx2c());
#ifdef HAVE_PUBSEQ_OS
    DBAPI_RegisterDriver_FTDS();
    GenBankReaders_Register_Pubseq();
#endif

    {
#if defined(HAVE_PUBSEQ_OS)
        time_t x = time(0);
        LOG_POST("START: " << time(0) );
        CGBDataLoader::RegisterInObjectManager(*CObjectManager::GetInstance());
        LOG_POST("Loader started: " << time(0)-x  );
#endif
    }
  
    for(unsigned thr=tc,i=0 ; thr > 0 ; --thr)
        for(unsigned global_om=0;global_om<=(thr>1U?1U:0U); ++global_om)
            for(unsigned global_scope=0;global_scope<=(thr==1U?1U:(global_om==0U?1U:2U)); ++global_scope)
                {
                    unsigned mode = (global_om<<2) + global_scope ;
                    LOG_POST("TEST: threads:" << thr <<
                             ", om=" << kGlobalOMTags[global_om] <<
                             ", scope=" << kGlobalScopeTags[global_scope]);
                    time_t start=time(0);
                    Test(mode,thr);
                    timing[thr-1][global_om][global_scope] = time(0)-start ;
                    LOG_POST("==================================================");
                    LOG_POST("Test(" << i++ << ") completed  ===============");
                }
  
    for(unsigned global_om=0;global_om<=1; ++global_om)
        for(unsigned global_scope=0;global_scope<=2; ++global_scope)
            for(unsigned thr=0; thr < tc ; ++thr)
                {
                    if(timing[thr][global_om][global_scope]==0) continue;
                    if(timing[thr][global_om][global_scope]>0)
                        LOG_POST("TEST: threads:" << (thr + 1) <<
                                 ", om=" << kGlobalOMTags[global_om] <<
                                 ", scope=" << kGlobalScopeTags[global_scope] <<
                                 " ==>> " << timing[thr][global_om][global_scope] << " sec");
                }
  
    LOG_POST("Tests completed");
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

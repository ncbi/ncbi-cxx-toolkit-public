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

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <serial/serial.hpp>
#include <serial/objostrasn.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>


BEGIN_NCBI_SCOPE
using namespace objects;

/////////////////////////////////////////////////////////////////////////////
//
//  Test thread
//

class CTestThread : public CThread
{
public:
  CTestThread(unsigned id, CObjectManager& objmgr, CScope& scope,int start,int stop);
  virtual ~CTestThread();
  
protected:
    virtual void* Main(void);

private:
  unsigned        m_mode;
  CScope         *m_Scope;
  CObjectManager *m_ObjMgr;
  int             m_Start;
  int             m_Stop;
};

CTestThread::CTestThread(unsigned id, CObjectManager& objmgr, CScope& scope,int start,int stop)
    : m_mode(id), m_Scope(&scope), m_ObjMgr(&objmgr), m_Start(start), m_Stop(stop)
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
  
    for (int i = m_Start;  i < m_Stop;  i++) {
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
    
        int gi = i ; // (i + m_Idx)/2+3;
        CSeq_id x;
        x.SetGi(gi);
        CBioseq_Handle h = s->GetBioseqHandle(x);
        if ( !h ) {
            LOG_POST(setw(3) << CThread::GetSelf() << ":: gi=" << gi << " :: not found in ID");
        } else {
            //CObjectOStreamAsn oos(NcbiCout);
            CConstRef<CBioseq> core = h.GetBioseqCore();
            ITERATE (list<CRef<CSeq_id> >, it, core->GetId()) {
                //oos << **it;
                //NcbiCout << NcbiEndl;
                ;
            }
            LOG_POST(setw(3) << CThread::GetSelf() << ":: gi=" << gi << " OK");
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

const unsigned c_TestFrom = 1;
const unsigned c_TestTo   = 201;
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
      thr[i] = new CTestThread(test_mode, *pOm, scope,c_TestFrom+i*step,c_TestFrom+(i+1)*step);
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

int CTestApplication::Run()
{
  unsigned timing[4/*threads*/][2/*om*/][3/*scope*/];
  unsigned tc = sizeof(timing)/sizeof(*timing);

  memset(timing,0,sizeof(timing));
  
  CORE_SetLOCK(MT_LOCK_cxx2c());
  CORE_SetLOG(LOG_cxx2c());

  {
#if defined(HAVE_PUBSEQ_OS)
    time_t x = time(0);
    LOG_POST("START: " << time(0) );
    CGBDataLoader::RegisterInObjectManager(*CObjectManager::GetInstance());
    LOG_POST("CTLIB loaded: " << time(0)-x  );
#endif
  }
  
  for(unsigned thr=tc,i=0 ; thr > 0 ; --thr)
    for(unsigned global_om=0;global_om<=(thr>1U?1U:0U); ++global_om)
      for(unsigned global_scope=0;global_scope<=(thr==1U?1U:(global_om==0U?1U:2U)); ++global_scope)
        {
          unsigned mode = (global_om<<2) + global_scope ;
          LOG_POST("TEST: threads:" << thr << ", om=" << (global_om?"global":"local ") <<
                   ", scope=" << (global_scope==0?"auto      ":(global_scope==1?"per thread":"global    ")));
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
          LOG_POST("TEST: threads:" << thr+1 << ", om=" << (global_om?"global":"local ") <<
                   ", scope=" << (global_scope==0?"auto      ":(global_scope==1?"per thread":"global    ")) <<
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


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2004/07/21 15:51:26  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.3  2004/05/21 21:42:52  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.2  2004/03/16 15:47:29  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.1  2003/12/16 17:51:20  kuznets
* Code reorganization
*
* Revision 1.24  2003/06/02 16:06:39  dicuccio
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
* Revision 1.23  2003/05/08 20:50:14  grichenk
* Allow MT tests to run in ST mode using CThread::fRunAllowST flag.
*
* Revision 1.22  2003/04/24 16:12:39  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.21  2003/04/15 14:23:11  vasilche
* Added missing includes.
*
* Revision 1.20  2003/03/27 21:54:58  grichenk
* Renamed test applications and makefiles, updated references
*
* Revision 1.19  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.18  2002/07/23 15:32:49  kimelman
* tuning test
*
* Revision 1.17  2002/07/22 22:49:05  kimelman
* test fixes for confidential data retrieval
*
* Revision 1.16  2002/06/06 20:42:17  kimelman
* cosmetics
*
* Revision 1.15  2002/06/06 06:17:07  vakatov
* Workaround a weird compiler bug (WorkShop 5.3 on INTEL in ReleaseMT mode)
*
* Revision 1.14  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.13  2002/05/08 22:23:49  kimelman
* MT fixes
*
* Revision 1.12  2002/05/06 03:28:53  vakatov
* OM/OM1 renaming
*
* Revision 1.11  2002/04/18 23:24:24  kimelman
* bugfix: out of bounds...
*
* Revision 1.10  2002/04/12 22:57:34  kimelman
* warnings cleanup(linux-gcc)
*
* Revision 1.9  2002/04/12 21:10:35  kimelman
* traps for coredumps
*
* Revision 1.8  2002/04/11 20:03:29  kimelman
* switch to pubseq
*
* Revision 1.7  2002/04/09 18:48:17  kimelman
* portability bugfixes: to compile on IRIX, sparc gcc
*
* Revision 1.6  2002/04/05 23:47:20  kimelman
* playing around tests
*
* Revision 1.5  2002/04/05 19:53:13  gouriano
* reset scope history more accurately (was incorrect)
*
* Revision 1.4  2002/04/04 01:35:38  kimelman
* more MT tests
*
* Revision 1.3  2002/04/02 17:24:54  gouriano
* skip useless test passes
*
* Revision 1.2  2002/04/02 16:02:33  kimelman
* MT testing
*
* Revision 1.1  2002/03/30 19:37:08  kimelman
* gbloader MT test
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

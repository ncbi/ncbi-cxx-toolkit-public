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
//  Test thread
//

class CTestThread : public CThread
{
public:
    CTestThread(int id, CObjectManager& objmgr, CScope& scope,int start,int stop);
  
protected:
    virtual void* Main(void);

private:
  int m_mode;
  CRef<CScope> m_Scope;
  CRef<CObjectManager> m_ObjMgr;
  int m_Start;
  int m_Stop;
};

CTestThread::CTestThread(int id, CObjectManager& objmgr, CScope& scope,int start,int stop)
    : m_mode(id), m_ObjMgr(&objmgr), m_Scope(&scope), m_Start(start), m_Stop(stop)
{
    //### Initialize the thread
}

void* CTestThread::Main(void)
{
  CObjectManager om;
  om.RegisterDataLoader(*new CGBDataLoader("ID", new CId1Reader(1),2),CObjectManager::eDefault);
  CObjectManager *pom=0;
  switch((m_mode>>2)&1) {
  case 0: pom =  &*m_ObjMgr; break;
  case 1: pom =  &om;        break;
  }
  
  CScope scope1(*pom);
  scope1.AddDefaults();
  
  for (int i = m_Start;  i < m_Stop;  i++) {
    CScope scope2(*pom);
    scope2.AddDefaults();

    CScope *s;
    switch(m_mode&3) {
    case 0: s =  &*m_Scope; break;
    case 1: s =  &scope1;   break;
    case 2: s =  &scope2;   break;
    default:
      throw runtime_error("unexpected mode");
    }
    
    int gi = i ; // (i + m_Idx)/2+3;
    CSeq_id x;
    x.SetGi(gi);
    CBioseq_Handle h = s->GetBioseqHandle(x);
    if ( !h ) {
      GBLOG_POST(" gi=" << gi << " :: not found in ID");
    } else {
      //CObjectOStreamAsn oos(NcbiCout);
      iterate (list<CRef<CSeq_id> >, it, h.GetBioseq().GetId()) {
        //oos << **it;
        //NcbiCout << NcbiEndl;
               ;
      }
      GBLOG_POST(" gi=" << gi << " OK");
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
    int Test(const int test_id,const int thread_count);
};

const unsigned c_TestFrom = 1;
const unsigned c_TestTo   = 21;
const unsigned c_GI_count = c_TestTo - c_TestFrom;

int CTestApplication::Test(const int test_mode,const int thread_count)
{
  CRef<CTestThread>  *thr = new CRef<CTestThread>[thread_count];
  int step= c_GI_count/thread_count;
  
  CRef< CObjectManager> pOm = new CObjectManager;
  
  // CRef< CGBDataLoader> pLoader = new CGBDataLoader;
  // pOm->RegisterDataLoader(*pLoader, CObjectManager::eDefault);
  pOm->RegisterDataLoader(*new CGBDataLoader("ID", new CId1Reader(thread_count),c_GI_count/10),
                          CObjectManager::eDefault);
  
  CRef<CScope> scope = new CScope(*pOm);

  scope->AddDefaults();
  
  for (int i=0; i<thread_count; ++i)
    {
      thr[i] = new CTestThread(test_mode, *pOm, *scope,c_TestFrom+i*step,c_TestTo+(i+1)*step);
      thr[i]->Run();
    }
  
  for (unsigned int i=0; i<thread_count; i++) {
    thr[i]->Join();
  }
  
  scope->ResetHistory();
  
  // Destroy all threads
  for (unsigned int i=0; i<thread_count; i++) {
    thr[i].Reset();
  }
  
  delete [] thr;
  return 0;
}

int CTestApplication::Run()
{
  int timing[5/*threads*/][2/*om*/][3/*scope*/];
  int tc = sizeof(timing)/sizeof(*timing);
  
  LOG_POST("START: " << time(0) );;
  
  for(int thr=tc-1,i=0 ; thr >= 0 ; --thr)
    for(int global_om=0;global_om<=1 ; ++global_om)
      for(int global_scope=0;global_scope<=2 ; ++global_scope)
        {
          int mode = global_om<<2 + global_scope ;
          LOG_POST("Test(" << i << ") # threads = " << thr );
          time_t start=time(0);
          Test(mode,thr);
          timing[thr][global_om][global_scope] = time(0)-start ;
          LOG_POST("==================================================");
          LOG_POST("Test(" << i++ << ") completed  ===============");
        }
  
  for(int thr=tc-1 ; thr >= 0 ; --thr)
    for(int global_om=0;global_om<=1 ; ++global_om)
      for(int global_scope=0;global_scope<=2 ; ++global_scope)
        {
          LOG_POST("TEST: threads:" << thr << ", om=" << (global_om?"global":"local") <<
                   ", scope=" << (global_scope==0?"auto":(global_scope==1?"per thread":"global")) <<
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


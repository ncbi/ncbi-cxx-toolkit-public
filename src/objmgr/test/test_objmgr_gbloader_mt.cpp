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

  int m_Idx;
  CRef<CScope> m_Scope;
  CRef<CObjectManager> m_ObjMgr;
  int m_Start;
  int m_Stop;
};

CTestThread::CTestThread(int id, CObjectManager& objmgr, CScope& scope,int start,int stop)
    : m_Idx(id), m_ObjMgr(&objmgr), m_Scope(&scope), m_Start(start), m_Stop(stop)
{
    //### Initialize the thread
}

void* CTestThread::Main(void)
{
  for (int i = m_Start;  i < m_Stop;  i++) {
    CScope scope(*m_ObjMgr);
    scope.AddDefaults();
    int gi = i ; // (i + m_Idx)/2+3;
    
    CSeq_id x;
    x.SetGi(gi);
    CObjectOStreamAsn oos(NcbiCout);
    CScope *s = (m_Idx) % 2 ? &scope: &*m_Scope ;
    CBioseq_Handle h = s->GetBioseqHandle(x);
    if ( !h ) {
      LOG_POST("Thread(" << m_Idx << ") :: gi=" << gi << " :: not found in ID");
    } else {
      iterate (list<CRef<CSeq_id> >, it, h.GetBioseq().GetId()) {
        //oos << **it;
        //NcbiCout << NcbiEndl;
               ;
      }
      LOG_POST("Thread(" << m_Idx << ") :: gi=" << gi << " OK");
    }
    //    if(i%10==0)
    //  m_Scope->ResetHistory();
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

int CTestApplication::Test(const int test_id,const int thread_count)
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
      thr[i] = new CTestThread(i, *pOm, *scope,c_TestFrom+i*step,c_TestTo+(i+1)*step);
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
  
  delete thr;
  return 0;
}

int CTestApplication::Run()
{
  int timing[4];
  int tc = sizeof(timing)/sizeof(*timing)+1;
  int cnt = ( c_GI_count/5 > tc ? tc : c_GI_count/5);
  
  LOG_POST("START: " << time(0) );;

  for(int i=cnt ; i > 0 ; --i)
    {
      LOG_POST("Test(" << i << ") # threads = " << i );
      time_t start=time(0);
      Test(i,i);
  
      timing[i-1] = time(0)-start ;
      LOG_POST("==================================================");
      LOG_POST("Test(" << i << ") completed  ===============");
    }
  
  for(int i=1 ; i <= cnt ; ++i)
    {
      LOG_POST("TEST #" << i << "   completed in " << timing[i-1] << " sec");
    }
  
  LOG_POST("STOP : " << time(0) );;
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


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
*  Author: Michael Kimelman
*
*  File Description: GenBank Data loader
*
*/

#include <corelib/ncbistd.hpp>
#include "handle_range.hpp"
#include <objects/objmgr1/gbloader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//===============================================================================
// Support Classes
//

/////////////////////////////////////////////////////////////////////////////////
//
// CTimer 

CTimer::CTimer() :
  m_RequestsDevider(0), m_Requests(0)
{
  m_ReasonableRefreshDelay = 0;
  m_LastCalibrated = m_Time= time(0);
};

time_t
CTimer::Time()
{
  if(--m_Requests>0)
    return m_Time;
  m_RequestsLock.Lock();
  if(m_Requests<=0)
    {
      time_t x = time(0);
      if(x==m_Time) {
        m_Requests += m_RequestsDevider + 1;
        m_RequestsDevider = m_RequestsDevider*2 + 1;
      } else {
        m_Requests = m_RequestsDevider / ( x - m_Time );
        m_Time=x;
      }
    }
  m_RequestsLock.Unlock();
  return m_Time;
};  

void
CTimer::Start()
{
  m_TimerLock.Lock();
  m_StartTime = Time();
};

void
CTimer::Stop()
{
  int x = Time() - m_StartTime; // test request timing in seconds
  m_ReasonableRefreshDelay = 60 /*sec*/ * 
    (x==0 ? 5 /*min*/ : x*50 /* 50 min per sec of test request*/);
  m_LastCalibrated = m_Time;
  m_TimerLock.Unlock();
};

time_t
CTimer::RetryTime()
{
  return Time() + (m_ReasonableRefreshDelay>0?m_ReasonableRefreshDelay:24*60*60); /* 24 hours */
};

bool
CTimer::NeedCalibration()
{
  return
    (m_ReasonableRefreshDelay==0) ||
    (m_Time-m_LastCalibrated>100*m_ReasonableRefreshDelay);
};

/* =========================================================================== */
// MutexPool
//

CMutexPool::CMutexPool()
{
  m_size =0;
  m_Locks=0;
}

void
CMutexPool::SetSize(int size)
{
  _VERIFY(m_size==0 && !m_Locks);
  m_size=size;
  m_Locks = new CMutex[m_size];
}

CMutexPool::~CMutexPool(void)
{
  if(m_size>0 && m_Locks)
    delete [] m_Locks;
}

#if 0
void
CMutexPool::Lock(void *p)
{
  int i = ((long)p/8) % m_size;
  m_Locks[i].Lock();
}

void
CMutexPool::Unlock(void *p)
{
  int i = ((long)p/8) % m_size;
  m_Locks[i].Unlock();
}

CMutex& CMutexPool::GetMutex(int x)
{
  return m_Locks[x];
}

#endif

/*=========================================================================== */
// CTSEUpload
//
END_SCOPE(objects)
END_NCBI_SCOPE

/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2002/03/29 02:47:03  kimelman
* gbloader: MT scalability fixes
*
* Revision 1.4  2002/03/27 20:23:49  butanaev
* Added connection pool.
*
* Revision 1.3  2002/03/20 21:24:59  gouriano
* *** empty log message ***
*
* Revision 1.2  2002/03/20 17:03:24  gouriano
* minor changes to make it compilable on MS Windows
*
* Revision 1.1  2002/03/20 04:50:13  kimelman
* GB loader added
*
* ===========================================================================
*/

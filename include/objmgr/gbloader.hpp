#ifndef GBLOADER__HPP_INCLUDED
#define GBLOADER__HPP_INCLUDED

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
*  ===========================================================================
*
*  Author: Aleksey Grichenko, Michael Kimelman, Anton Butanayev
*
*  File Description:
*   Data loader base class for object manager
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <map>
#include <sys/types.h>
#include <time.h>

#include <objects/objmgr1/data_loader.hpp>
#include <objects/objmgr1/reader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////////
//
// GBDataLoader
//

class CMultiLock;
class CTSEUpload;
class CMutexPool;
class CHandleRange;

class CTimer
{
public:
  CTimer();
  time_t Time();
  void   Start();
  void   Stop();
  time_t RetryTime();
  bool   NeedCalibration();
private:
  int    m_ReasonableRefreshDelay;
  int    m_RequestsDevider;
  int    m_Requests;
  CMutex m_RequestsLock;
  time_t m_Time;
  time_t m_LastCalibrated;
  
  time_t m_StartTime;
  CMutex m_TimerLock;
};

//========================================================================
class CRefresher
{
public:
  CRefresher() : m_RefreshTime(0) { };
  bool NeedRefresh(CTimer &timer) { return timer.Time() > m_RefreshTime; };
  void Reset      (CTimer &timer) { m_RefreshTime = timer.RetryTime();   };
private:
  time_t m_RefreshTime;
};

class CMutexPool
{
public:
  CMutexPool();
  ~CMutexPool(void);
  void SetSize(int size);
  void Lock(void *p);
  void Unlock(void *p);
  CMutex& GetMutex(void *p);
private:
  int             m_size;
  CMutex         *m_Locks;
};

class CTSEUpload {
public:
  enum EChoice { eNone,eDone,ePartial };
  CTSEUpload() : m_tse(0), m_mode(eNone) {};
  EChoice                       m_mode;
  CSeq_entry                   *m_tse;
};

class CGBDataLoader : public CDataLoader
{
public:
  CGBDataLoader(const string& loader_name="GENBANK",CReader *driver=0,int gc_threshold=100);
  virtual ~CGBDataLoader(void);
  
  virtual bool DropTSE(const CSeq_entry* sep);
  virtual bool GetRecords(const CHandleRangeMap& hrmap, const EChoice choice);
  
  virtual CTSE_Info*
  ResolveConflict(const CSeq_id_Handle& handle,
                  const TTSESet&        tse_set);
  
private:
  class CCmpTSE
  {
    private:
      CSeqref *m_sr;
    public:
      CCmpTSE(CSeqref *sr)           : m_sr(sr)        {};
      CCmpTSE(auto_ptr<CSeqref> &asr): m_sr(asr.get()) {};
      ~CCmpTSE(void) {};
      operator bool  (void)       const { return m_sr!=0;};
      bool operator< (const CCmpTSE &b) const { return m_sr->Compare(*b.m_sr,CSeqref::eTSE)< 0;};
      bool operator==(const CCmpTSE &b) const { return m_sr->Compare(*b.m_sr,CSeqref::eTSE)==0;};
      bool operator<=(const CCmpTSE &b) const { return *this == b && *this < b ;};
      CSeqref& get() const { return *m_sr; };
  };
  struct STSEinfo;
  struct SSeqrefs;
  
  typedef CIntStreamable::TInt             TInt;
  
  typedef map<CCmpTSE          ,STSEinfo*> TSr2TSEinfo   ;
  typedef map<const CSeq_entry*,STSEinfo*> TTse2TSEinfo  ;
  typedef map<TSeq_id_Key     , SSeqrefs*> TSeqId2Seqrefs;
  
  CReader        *m_Driver;
  TSr2TSEinfo     m_Sr2TseInfo;
  TTse2TSEinfo    m_Tse2TseInfo;

  TSeqId2Seqrefs  m_Bs2Sr;
  
  CTimer          m_Timer;

  int             m_SlowTraverseMode;
  CMutex          m_LookupMutex;
  CMutexPool      m_Pool;

  STSEinfo       *m_UseListHead;
  STSEinfo       *m_UseListTail;
  int             m_TseCount;
  int             m_TseGC_Threshhold;
  bool            m_InvokeGC;
  void            x_UpdateDropList(STSEinfo *p);
  void            x_GC(void);
  
  //
  // private code
  //
  const CSeq_id*  x_GetSeqId(const TSeq_id_Key h);
  
  TInt            x_Request2SeqrefMask(const EChoice choice);
  TInt            x_Request2BlobMask(const EChoice choice);
  
  
  typedef map<CSeq_id_Handle,CHandleRange> TLocMap;
  bool            x_GetRecords(const TSeq_id_Key key,const CHandleRange &hrange, EChoice choice);
  bool            x_ResolveHandle(const TSeq_id_Key h,SSeqrefs* &sr);
  bool            x_NeedMoreData(CTSEUpload *tse_up,CSeqref* srp,int from,int to,TInt blob_mask);
  bool            x_GetData(CTSEUpload *tse_up,CSeqref* srp,int from,int to,TInt blob_mask);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif
/* ---------------------------------------------------------------------------
 *
 * $Log$
 * Revision 1.5  2002/03/21 01:34:49  kimelman
 * gbloader related bugfixes
 *
 * Revision 1.4  2002/03/20 21:26:23  gouriano
 * *** empty log message ***
 *
 * Revision 1.3  2002/03/20 19:06:29  kimelman
 * bugfixes
 *
 * Revision 1.2  2002/03/20 17:04:25  gouriano
 * minor changes to make it compilable on MS Windows
 *
 * Revision 1.1  2002/03/20 04:50:35  kimelman
 * GB loader added
 *
 */

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
#include <corelib/ncbiobj.hpp>
#include "tse_info.hpp"
#include "handle_range_map.hpp"
#include "data_source.hpp"
#include <objects/objmgr1/reader_id1.hpp>
#include <objects/objmgr1/gbloader.hpp>
#include <bitset>
#include <set>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


//=======================================================================
//   GBLoader sub classes 
//
struct CGBDataLoader::STSEinfo
{
  typedef set<TSeq_id_Key>  TSeqids;
  enum { eDead, eConfidential, eLast };
  
  STSEinfo         *next;
  STSEinfo         *prev;
  bitset<eLast>     mode;
  auto_ptr<CSeqref> key;
  
  TSeqids           m_SeqIds;
  CTSEUpload        m_upload;
  STSEinfo() : prev(0), next(0),m_upload() { };
};

struct CGBDataLoader::SSeqrefs
{
  typedef vector<CSeqref*> TV;
  
  class TSeqrefs : public TV {
  public:
    TSeqrefs() : TV(0) {};
    ~TSeqrefs() { iterate(TV,it,*this) delete *it; };
  };
  
  TSeqrefs       *m_Sr;
  CRefresher      m_Timer;
  SSeqrefs() : m_Sr(0) {};
};

//=======================================================================
// GBLoader Public interface 
// 

CGBDataLoader::CGBDataLoader(const string& loader_name,CReader *driver,int gc_threshold)
  : CDataLoader(loader_name)
{
  LOG_POST( "CGBDataLoader");
  m_Driver=(driver?driver:new CId1Reader);
  m_UseListHead = m_UseListTail = 0;
  
  int i = m_Driver->ParalellLevel();
  m_Pool.SetSize(i<=0?10:i);
  m_SlowTraverseMode=0;
  
  m_TseCount=0;
  m_TseGC_Threshhold=gc_threshold;
  m_InvokeGC=false;
  //LOG_POST( "CGBDataLoader(" << loader_name <<"::" << gc_threshold << ")" );
}

CGBDataLoader::~CGBDataLoader(void)
{
  LOG_POST( "~CGBDataLoader");
  iterate(TSeqId2Seqrefs,sih_it,m_Bs2Sr)
    {
      if(sih_it->second)
        {
          if(sih_it->second->m_Sr)
            delete sih_it->second->m_Sr;
          delete sih_it->second;
        }
    }
  iterate(TSr2TSEinfo,tse_it,m_Sr2TseInfo)
    delete tse_it->second;
  delete m_Driver;
}

bool
CGBDataLoader::GetRecords(const CHandleRangeMap& hrmap, const EChoice choice)
{
  /*
    SeqLoc -> CHandleRangeMap ->
    foreach(CBioseq from Map)
    {
      if(!present) {
        get SSeqrefs
       filter : choose foreign live and 1 with_sequence (either already present or live)
      }
      foreach(CSeqref) {
        forech(get_list_of_subtrees to upload for given choice and range)
         {
          CBlob_I it = CSeqref.getBlobs(range.min,range.max,choice,subtree_id);
         }
      }
   */
  x_GC();
  iterate (TLocMap, hrange, hrmap.GetMap() )
    {
      //LOG_POST( "GetRecords-0" );
      TSeq_id_Key  sih       = x_GetSeq_id_Key(hrange->first);
      while(true) if(x_GetRecords(sih,hrange->second,choice)) break;
    }
  //LOG_POST( "GetRecords-end" );
  return true;
}

bool
CGBDataLoader::DropTSE(const CSeq_entry *sep)
{
  m_LookupMutex.Lock();
  TTse2TSEinfo::iterator it = m_Tse2TseInfo.find(sep);
  if (it == m_Tse2TseInfo.end())
    {
      // oops - apprently already done;
      m_LookupMutex.Unlock();
      return true;
    }
  STSEinfo *tse = it->second;
  if(m_SlowTraverseMode>0) m_Pool.GetMutex(tse).Lock();
  LOG_POST( "DropTse(" << tse <<")" );
  
  m_Tse2TseInfo.erase(it);
  _VERIFY(tse);
  m_Sr2TseInfo.erase(CCmpTSE(tse->key.get()));
  iterate(STSEinfo::TSeqids,sih_it,tse->m_SeqIds)
    {
      TSeqId2Seqrefs::iterator bsit = m_Bs2Sr.find(*sih_it);
      if(bsit == m_Bs2Sr.end()) continue;
      // delete sih
      if(bsit->second->m_Sr)
        delete bsit->second->m_Sr;
      bsit->second->m_Sr=0;
      m_Bs2Sr.erase(bsit);
    }
  if(m_UseListHead==tse) m_UseListHead=tse->next;
  if(m_UseListTail==tse) m_UseListHead=tse->prev;
  if(tse->next) tse->next->prev=tse->prev;
  if(tse->prev) tse->prev->next=tse->next;
  delete tse;
  if(m_SlowTraverseMode>0) m_Pool.GetMutex(tse).Unlock();
  m_TseCount --;
  m_LookupMutex.Unlock();
  return true;
}

CTSE_Info*
CGBDataLoader::ResolveConflict(const CSeq_id_Handle& handle,const TTSESet& tse_set)
{
  TSeq_id_Key  sih       = x_GetSeq_id_Key(handle);
  SSeqrefs*    sr=0;
  CTSE_Info*   best=0;
  bool         conflict=false;
  
  LOG_POST( "ResolveConflict" );
  do m_LookupMutex.Lock();
  while (!x_ResolveHandle(sih,sr));
  iterate(TTSESet, sit, tse_set)
    {
      CTSE_Info *ti = *sit;
      TTse2TSEinfo::iterator it = m_Tse2TseInfo.find(ti->m_TSE);
      if(it==m_Tse2TseInfo.end()) continue;
      if(it->second->mode.test(STSEinfo::eDead) && !ti->m_Dead)
        ti->m_Dead=true;
      if(it->second->m_SeqIds.find(sih)==it->second->m_SeqIds.end()) // not listed for given TSE
        continue;
      if(!best)
        { best=ti; conflict=false; }
      else if(!ti->m_Dead && best->m_Dead)
        { best=ti; conflict=false; }
      else if(ti->m_Dead && best->m_Dead)
        { conflict=true; }
      else if(ti->m_Dead && !best->m_Dead)
        continue;
      else
        _VERIFY(ti->m_Dead || best->m_Dead);
    }
  if(best && !conflict)
    {
      m_LookupMutex.Unlock();
      return best;
    }
  
  // try harder
  
  best=0;conflict=false;
  if (sr->m_Sr)
    {
      iterate (SSeqrefs::TSeqrefs, srp, *sr->m_Sr)
        {
          TSr2TSEinfo::iterator tsep = m_Sr2TseInfo.find(CCmpTSE(*srp));
          if (tsep == m_Sr2TseInfo.end()) continue;
          iterate(TTSESet, sit, tse_set)
            {
              CTSE_Info *ti = *sit;
              TTse2TSEinfo::iterator it = m_Tse2TseInfo.find(ti->m_TSE);
              if(it==m_Tse2TseInfo.end()) continue;
              if(it->second==tsep->second)
                {
                  if(!best)           best=ti;
                  else if(ti != best) conflict=true;
                }
            }
        }
    }
  if(conflict) best=0;
  m_LookupMutex.Unlock();
  return best;
}

//=======================================================================
// GBLoader private interface
// 

const CSeq_id*
CGBDataLoader::x_GetSeqId(const TSeq_id_Key h)
{
  return x_GetSeq_id(x_GetSeq_id_Handle(h));
};

void
CGBDataLoader::x_UpdateDropList(STSEinfo *tse)
{
  _VERIFY(tse);
  // reset LRU links
  if(tse == m_UseListTail) // already the last one
    return;
  
  // Unlink from current place
  if(tse->prev) tse->prev->next=tse->next;
  if(tse->next) tse->next->prev=tse->prev;
  if(tse == m_UseListHead )
    {
      _VERIFY(tse->next);
      m_UseListHead = tse->next;
    }
  tse->prev = tse->next=0;
  if(m_UseListTail)
    {
      tse->prev = m_UseListTail;
      m_UseListTail->next = tse;
      m_UseListTail = tse;
    }
  else
    {
      _VERIFY(m_UseListHead==0);
      m_UseListHead = m_UseListTail = tse; 
    }
}

void
CGBDataLoader::x_GC(void)
{
  // LOG_POST( "X_GC " << m_TseCount << "," << m_TseGC_Threshhold << "," << m_InvokeGC);
  // dirty read - but that ok for garbage collector
  if(m_TseCount<m_TseGC_Threshhold) return;
  if(!m_InvokeGC) return ;
  LOG_POST( "X_GC " << m_TseCount);
  //GetDataSource()->x_CleanupUnusedEntries();

//#if 0
  int skip=0;
  CMutexGuard x(m_LookupMutex);
  while(skip+1<m_TseCount - 0.9*m_TseGC_Threshhold)
    {
      int i=skip;
      STSEinfo *tse_to_drop=m_UseListHead;
      while(tse_to_drop && i-->0)
        tse_to_drop = tse_to_drop->next;
      _VERIFY(tse_to_drop);
      const CSeq_entry *sep=tse_to_drop->m_upload.m_tse;
      if(!sep)
        skip++;
      else
        {
          char b[100];
          LOG_POST("X_GC::DropTSE(" << tse_to_drop << "::" << tse_to_drop->key->printTSE(b,sizeof(b)) << ")");
          if(!GetDataSource()->DropTSE(*sep))
            skip++;
        }
    }
//#endif
  m_InvokeGC=false;
  //LOG_POST( "X_GC " << m_TseCount);
}


CGBDataLoader::TInt
CGBDataLoader::x_Request2SeqrefMask(const EChoice choice)
{
  switch(choice)
    {
      // split code
    default:
      return ~0;
    }
  return 0;
}

CGBDataLoader::TInt
CGBDataLoader::x_Request2BlobMask(const EChoice choice)
{
  // split code
  switch(choice)
    {
      // split code
    default:
      return 1;
    }
  return 1;
}

bool
CGBDataLoader::x_GetRecords(const TSeq_id_Key sih,const CHandleRange &hrange,EChoice choice)
{
  TInt         sr_mask   = x_Request2SeqrefMask(choice);
  TInt         blob_mask = x_Request2BlobMask(choice);
  SSeqrefs*    sr=0;
  
  //LOG_POST( "x_GetRecords" );
  m_LookupMutex.Lock();
  if(!x_ResolveHandle(sih,sr))
    return false;// mutex has already been unlocked

  if(!sr->m_Sr) // no data for given seqid
    {
      m_LookupMutex.Unlock();
      return true ;
    }
  //LOG_POST( "x_GetRecords-Seqref_iterate" );
  iterate (SSeqrefs::TSeqrefs, srp, *sr->m_Sr)
    {
      // skip TSE which doesn't contain requested type of info
      //LOG_POST( "x_GetRecords-Seqref_iterate_0" );
      if( ((~(*srp)->Flag()) & sr_mask) == 0 ) continue;
      //LOG_POST( "list uploaded TSE");
      // char b[100];
      //for(TSr2TSEinfo::iterator tsep = m_Sr2TseInfo.begin(); tsep != m_Sr2TseInfo.end(); ++tsep)
      //  {
      //     LOG_POST(tsep->first.get().printTSE(b,sizeof(b)));
      //  }
      // LOG_POST("x_GetRecords-Seqref_iterate_1" << (*srp)->print(b,sizeof(b)) );
      
      // find TSE info for each seqref
      TSr2TSEinfo::iterator tsep = m_Sr2TseInfo.find(CCmpTSE(*srp));
      STSEinfo *tse;
      if (tsep != m_Sr2TseInfo.end())
        {
          tse = tsep->second;
          //LOG_POST( "x_GetRecords-oldTSE(" << tse << ") mode=" << (tse->m_upload.m_mode));
        }
      else
        {
          tse = new STSEinfo();
          tse->key.reset((*srp)->Dup());
          m_Sr2TseInfo[CCmpTSE(tse->key.get())] = tse;
          m_TseCount++;
          //LOG_POST( "x_GetRecords-newTSE(" << tse << ") mode=" << (tse->m_upload.m_mode));
        }
      
      bool use_global_lock=true;
      bool use_local_lock=m_SlowTraverseMode>0;
      if(use_local_lock) m_Pool.GetMutex(tse).Lock();
      {{ // make sure we have reverse reference to handle
        STSEinfo::TSeqids &sid = tse->m_SeqIds;
        if (sid.find(sih) == sid.end())
          sid.insert(sih);
      }}
      x_UpdateDropList(tse);
      bool new_tse=false;
      
      iterate (CHandleRange::TRanges, lrange , hrange.GetRanges())
        {
          //LOG_POST( "x_GetRecords-range_0" );
          // check Data
          //LOG_POST( "x_GetRecords-range_0" );
          if(!x_NeedMoreData(&tse->m_upload,
                             *srp,
                             lrange->first.GetFrom(),
                             lrange->first.GetTo(),
                             blob_mask)
             )
            continue;
          
          _VERIFY(tse->m_upload.m_mode != CTSEUpload::eDone);

          //LOG_POST( "x_GetRecords-range_1" );
          // need update
          if(use_global_lock)
            { // switch to local lock mode
              m_SlowTraverseMode++;
              use_global_lock=false;
              m_LookupMutex.Unlock();
              if(!use_local_lock)
                {
                  m_Pool.GetMutex(tse).Lock();
                  use_local_lock=true;
                }
            }
          _VERIFY(use_local_lock);
          new_tse = new_tse ||
            x_GetData(&tse->m_upload,
                                *srp,
                                lrange->first.GetFrom(),
                                lrange->first.GetTo(),
                                blob_mask);
        }
      if(use_local_lock) m_Pool.GetMutex(tse).Unlock();
      if(!use_global_lock)
        { // uploaded some data
          m_LookupMutex.Lock();
          m_SlowTraverseMode--;
          if(new_tse)
            m_Tse2TseInfo[tse->m_upload.m_tse]=tse;
          m_LookupMutex.Unlock();
          return false; // restrart traverse to make sure seqrefs list are the same we started from
        }
      if(new_tse)
        m_Tse2TseInfo[tse->m_upload.m_tse]=tse;
    } // iterate seqrefs
  m_LookupMutex.Unlock();
  return true;
}

bool
CGBDataLoader::x_ResolveHandle(const TSeq_id_Key h,SSeqrefs* &sr)
{
  sr=0;
  TSeqId2Seqrefs::iterator bsit = m_Bs2Sr.find(h);
  if (bsit == m_Bs2Sr.end() )
    {
      sr = new SSeqrefs() ;
      m_Bs2Sr[h]=sr;
    }
  else
    sr = bsit->second;

  bool local_lock=m_SlowTraverseMode>0;
  if(local_lock) m_Pool.GetMutex(sr).Lock();
  if(!sr->m_Timer.NeedRefresh(m_Timer))
    {
      if(local_lock) m_Pool.GetMutex(sr).Unlock();
      return true;
    }
  // update required
  m_SlowTraverseMode++;
  m_LookupMutex.Unlock();
  if(!local_lock) m_Pool.GetMutex(sr).Lock();

  LOG_POST( "ResolveHandle-before(" << h << ") " << (sr->m_Sr?sr->m_Sr->size():0) );
  
  bool calibrating = m_Timer.NeedCalibration();
  if(calibrating) m_Timer.Start();
  
  SSeqrefs::TSeqrefs *osr=sr->m_Sr;
  sr->m_Sr=0;
  for(CIStream srs(m_Driver->SeqrefStreamBuf(*x_GetSeqId(h)));! srs.Eof();)
    {
      CSeqref *seqRef = m_Driver->RetrieveSeqref(srs);
      if(!sr->m_Sr) sr->m_Sr = new SSeqrefs::TSeqrefs();
      sr->m_Sr->push_back(seqRef);
    }
  if(calibrating) m_Timer.Stop();
  sr->m_Timer.Reset(m_Timer);
  
  m_Pool.GetMutex(sr).Unlock();
  m_LookupMutex.Lock();
  m_SlowTraverseMode--;
  local_lock=m_SlowTraverseMode>0;
  
  LOG_POST( "ResolveHandle(" << h << ") " << (sr->m_Sr?sr->m_Sr->size():0) );
  if(sr->m_Sr)
    {
      iterate(SSeqrefs::TSeqrefs, srp, *(sr->m_Sr))
        {
          char b[100];
          LOG_POST( (*srp)->print(b,sizeof(b)));
        }
    }
  
  if(osr)
    {
      bsit = m_Bs2Sr.find(h); // make sure we are not deleted in the unlocked time 
      if (bsit != m_Bs2Sr.end())
        {
          SSeqrefs::TSeqrefs *nsr=bsit->second->m_Sr;
          
          // catch dissolving TSE and mark them dead
          //LOG_POST( "old seqrefs");
          iterate(SSeqrefs::TSeqrefs,srp,*osr)
            {
              //(*srp)->print(); cout);
              bool found=false;
              iterate(SSeqrefs::TSeqrefs,nsrp,*nsr)
                if(CCmpTSE(*srp)==CCmpTSE(*nsrp)) {found=true; break;}
              if(found) continue;
              TSr2TSEinfo::iterator tsep = m_Sr2TseInfo.find(CCmpTSE(*srp));
              if (tsep == m_Sr2TseInfo.end()) continue;
              
              // update TSE info 
              STSEinfo *tse = tsep->second;
              if(local_lock) m_Pool.GetMutex(tse).Lock();
              bool mark_dead  = tse->mode.test(STSEinfo::eDead);
              if(mark_dead) tse->mode.set(STSEinfo::eDead);
              tse->m_SeqIds.erase(h); // drop h as refewrenced seqid
              if(local_lock) m_Pool.GetMutex(tse).Unlock();
              if(mark_dead)
                { // inform data_source :: make sure to avoid deadlocks
                  GetDataSource()->x_UpdateTSEStatus(*(tse->m_upload.m_tse),true);
                }
            }
        }
      delete osr;
    }
  m_LookupMutex.Unlock();
  return false;
}

bool
CGBDataLoader::x_NeedMoreData(CTSEUpload *tse_up,CSeqref* srp,int from,int to,TInt blob_mask)
{
  bool need_data=true;
  
  if (tse_up->m_mode==CTSEUpload::eDone)
    need_data=false;
  if (tse_up->m_mode==CTSEUpload::ePartial)
    {
      // split code : check tree for presence of data and
      // and return from routine if all data already loaded
      // present;
    }
  //LOG_POST( "x_NeedMoreData(" << srp << "," << tse_up << ") need_data " << need_data <<);
  return need_data;
}

bool
CGBDataLoader::x_GetData(CTSEUpload *tse_up,CSeqref* srp,int from,int to,TInt blob_mask)
{
  if(!x_NeedMoreData(tse_up,srp,from,to,blob_mask))
    return false;
  m_InvokeGC=true;
  bool new_tse = false;
  char s[100];
  LOG_POST( "GetBlob(" << srp->printTSE(s,sizeof(s)) << "," << tse_up << ") " <<
    from << ":"<< to << ":=" << tse_up->m_mode);
  _VERIFY(tse_up->m_mode != CTSEUpload::eDone);
  if (tse_up->m_mode == CTSEUpload::eNone)
    {
      CBlobClass cl;
      int count=0;
      cl.Value() = blob_mask;
      for(CIStream bs(srp->BlobStreamBuf(from, to, cl)); ! bs.Eof();count++ )
        {
          CBlob *blob = srp->RetrieveBlob(bs);
          //LOG_POST( "GetBlob(" << srp << ") " << from << ":"<< to << "  class("<<blob->Class()<<")");
          if (blob->Class()==0)
            {
              LOG_POST( "GetBlob(" << s << ") " << "- whole blob retrieved");
              tse_up->m_mode = CTSEUpload::eDone;
              tse_up->m_tse    = blob->Seq_entry();
              new_tse=true;
              GetDataSource()->AddTSE(*(tse_up->m_tse));
            }
          else
            {
              LOG_POST("GetBlob(" << s << ") " << "- partial load");
              _ASSERT(tse_up->m_mode != CTSEUpload::eDone);
              if(tse_up->m_mode != CTSEUpload::ePartial)
                tse_up->m_mode = CTSEUpload::ePartial;
              // split code : upload tree
              _VERIFY(0);
            }
          delete blob;
        }
      if(count==0)
        {
          tse_up->m_mode = CTSEUpload::eDone;
          // TODO log message
          LOG_POST("ERROR: can not retrive blob : " << s);
        }
      
      //LOG_POST( "GetData-after:: " << from << to <<  endl;
    }
  else
    {
      _VERIFY(tse_up->m_mode==CTSEUpload::ePartial);
      _VERIFY(0);
    }
  return new_tse;
}

END_SCOPE(objects)
END_NCBI_SCOPE

/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.12  2002/03/25 15:44:46  kimelman
* proper logging and exception handling
*
* Revision 1.11  2002/03/22 18:56:05  kimelman
* GC list fix
*
* Revision 1.10  2002/03/22 18:51:18  kimelman
* stream WS skipping fix
*
* Revision 1.9  2002/03/22 18:15:47  grichenk
* Unset "skipws" flag in binary stream
*
* Revision 1.8  2002/03/21 23:16:32  kimelman
* GC bugfixes
*
* Revision 1.7  2002/03/21 21:39:48  grichenk
* garbage collector bugfix
*
* Revision 1.6  2002/03/21 19:14:53  kimelman
* GB related bugfixes
*
* Revision 1.5  2002/03/21 01:34:53  kimelman
* gbloader related bugfixes
*
* Revision 1.4  2002/03/20 21:24:59  gouriano
* *** empty log message ***
*
* Revision 1.3  2002/03/20 19:06:30  kimelman
* bugfixes
*
* Revision 1.2  2002/03/20 17:03:24  gouriano
* minor changes to make it compilable on MS Windows
*
* Revision 1.1  2002/03/20 04:50:13  kimelman
* GB loader added
*
* ===========================================================================
*/

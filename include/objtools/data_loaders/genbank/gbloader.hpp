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
*  Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko,
*          Anton Butanayev
*
*  File Description:
*   Data loader base class for object manager
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/plugin_manager.hpp>

#if !defined(NDEBUG) && defined(DEBUG_SYNC)
// for GBLOG_POST()
# include <corelib/ncbithr.hpp>
#endif

#include <map>
#include <bitset>
#include <time.h>

#if defined(NCBI_OS_MAC)
#   include <types.h>
#else
#   include <sys/types.h>
#endif

#include <objmgr/data_loader.hpp>

#include <objmgr/impl/mutex_pool.hpp>

#include <objtools/data_loaders/genbank/blob_id.hpp>
#include <objtools/data_loaders/genbank/gbload_util.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CReader;
class CSeqref;
class CBlob_id;
class CHandleRange;
class CSeq_entry;
class CLoadInfoSeq_ids;
class CLoadInfoBlob_ids;
class CLoadInfoBlob;

#if !defined(NDEBUG) && defined(DEBUG_SYNC)
#  if defined(NCBI_THREADS)
#    define GBLOG_POST(x) LOG_POST(setw(3) << CThread::GetSelf() << ":: " << x)
#  else
#    define GBLOG_POST(x) LOG_POST("0:: " << x)
#  endif 
#else
#  ifdef DEBUG_SYNC
#    undef DEBUG_SYNC
#  endif
#  define GBLOG_POST(x)
#endif
  
/////////////////////////////////////////////////////////////////////////////////
//
// GBDataLoader
//

class CGBReaderRequestResult;

class CMutexArray
{
public:
    typedef CMutex TMutex;

    CMutexArray(void)
        : m_Size(0), m_Mutexes(0)
        {
        }
    ~CMutexArray(void)
        {
            Reset();
        }

    void SetSize(size_t size)
        {
            Reset();
            m_Mutexes = new TMutex[size];
            m_Size = size;
        }
    size_t GetSize(void) const
        {
            return m_Size;
        }

    void Reset(void)
        {
            m_Size = 0;
            delete[] m_Mutexes;
            m_Mutexes = 0;
        }

    TMutex& operator[](size_t index)
        {
            _ASSERT(index < m_Size);
            return m_Mutexes[index];
        }

private:
    CMutexArray(const CMutexArray&);
    CMutexArray& operator=(const CMutexArray&);

    size_t  m_Size;
    TMutex* m_Mutexes;
};


// Parameter names used by loader factory

const string kCFParam_GB_ReaderPtr  = "ReaderPtr";  // = CReader*
const string kCFParam_GB_ReaderName = "ReaderName"; // = string


// string: any value except "TakeOwnership" results in eNoOwnership
const string kCFParam_GB_Ownership = "Ownership";


class NCBI_XLOADER_GENBANK_EXPORT CGBDataLoader : public CDataLoader
{
public:
    // typedefs from CReader
    typedef unsigned                  TConn;
        
    virtual ~CGBDataLoader(void);
    
    virtual void DropTSE(CRef<CTSE_Info> tse_info);
    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh,
                                    EChoice choice);
    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh,
                                    const SRequestDetails& details);
    virtual void GetChunk(TChunk chunk);
    virtual void GetChunks(const TChunkSet& chunks);

    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh);
    virtual TBlobVersion GetBlobVersion(const TBlobId& id);
    CBlob_id GetBlobId(const TBlobId& blob_id) const;

    // Create GB loader and register in the object manager if
    // no loader with the same name is registered yet.
    typedef SRegisterLoaderInfo<CGBDataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CReader* driver = 0,
        CObjectManager::EIsDefault is_default = CObjectManager::eDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(CReader* driver = 0);

    // Select reader by name. If failed, select default reader.
    // Reader name may be the same as in environment: PUBSEQOS or ID1
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string&   reader_name,
        CObjectManager::EIsDefault is_default = CObjectManager::eDefault,
        CObjectManager::TPriority  priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(const string& reader_name);


    virtual CConstRef<CSeqref> GetSatSatkey(const CSeq_id_Handle& idh);
    CConstRef<CSeqref> GetSatSatkey(const CSeq_id& id);

    bool LessBlobId(const TBlobId& id1, const TBlobId& id2) const;
    
    virtual TTSE_Lock ResolveConflict(const CSeq_id_Handle& handle,
                                      const TTSE_LockSet& tse_set);
    
    virtual void GC(void);
    
    CBlob_id GetBlobId(const CTSE_Info& tse_info) const;

    CRef<CLoadInfoSeq_ids> GetLoadInfoSeq_ids(const string& key);
    CRef<CLoadInfoSeq_ids> GetLoadInfoSeq_ids(const CSeq_id_Handle& key);
    CRef<CLoadInfoBlob_ids> GetLoadInfoBlob_ids(const CSeq_id_Handle& key);
    //CRef<CLoadInfoBlob> GetLoadInfoBlob(const CBlob_id& key);

protected:
    friend class CGBReaderRequestResult;
    void AllocateConn(CGBReaderRequestResult& result);
    void FreeConn(CGBReaderRequestResult& result);

    TBlobContentsMask x_MakeContentMask(EChoice choice) const;
    TBlobContentsMask x_MakeContentMask(const SRequestDetails& details) const;

    TTSE_LockSet x_GetRecords(const CSeq_id_Handle& idh,
                              TBlobContentsMask sr_mask);

private:
    typedef CParamLoaderMaker<CGBDataLoader, CReader*>       TReaderPtrMaker;
    typedef CParamLoaderMaker<CGBDataLoader, const string&>  TReaderNameMaker;
    friend class CParamLoaderMaker<CGBDataLoader, CReader*>;
    friend class CParamLoaderMaker<CGBDataLoader, const string&>;

    CGBDataLoader(const string& loader_name,
                  CReader*      driver);
    CGBDataLoader(const string& loader_name,
                  const string& reader_name);

    typedef map<string, CRef<CLoadInfoSeq_ids> >            TLoadMapSeq_ids;
    typedef map<CSeq_id_Handle, CRef<CLoadInfoSeq_ids> >    TLoadMapSeq_ids2;
    typedef map<CSeq_id_Handle, CRef<CLoadInfoBlob_ids> >   TLoadMapBlob_ids;
    //typedef map<CBlob_id, CRef<CLoadInfoBlob> >             TLoadMapBlob;

    typedef CRWLock                     TRWLock;
    typedef TRWLock::TReadLockGuard     TReadLockGuard;
    typedef TRWLock::TWriteLockGuard    TWriteLockGuard;

    
    CInitMutexPool          m_MutexPool;
    CMutexArray             m_ConnMutexes;

    CRef<CReader>           m_Driver;

    TRWLock                 m_LoadMap_Lock;
    TLoadMapSeq_ids         m_LoadMapSeq_ids;
    TLoadMapSeq_ids2        m_LoadMapSeq_ids2;
    TLoadMapBlob_ids        m_LoadMapBlob_ids;
    //TLoadMapBlob            m_LoadMapBlob;
    
    CTimer                  m_Timer;

    //
    // private code
    //
    
    void            x_CreateDriver(const string& driver_name = kEmptyStr);
    CReader*        x_CreateReader(const string& env);

    CGBDataLoader(const CGBDataLoader&);
    CGBDataLoader& operator=(const CGBDataLoader&);
};


END_SCOPE(objects)


extern NCBI_XLOADER_GENBANK_EXPORT const string kDataLoader_GB_DriverName;

extern "C"
{

NCBI_XLOADER_GENBANK_EXPORT
void NCBI_EntryPoint_DataLoader_GB(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_GENBANK_EXPORT
void NCBI_EntryPoint_xloader_genbank(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif

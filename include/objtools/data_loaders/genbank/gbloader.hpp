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
#include <objtools/data_loaders/genbank/seqref.hpp>
#include <objtools/data_loaders/genbank/gbload_util.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CReader;
class CSeqref;
class CHandleRange;
class CSeq_entry;

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

//========================================================================
class CRefresher
{
public:
    CRefresher(void)
        : m_RefreshTime(0)
        {
        }

    void Reset(CTimer &timer)
        {
            m_RefreshTime = timer.RetryTime();
        }
    void Reset(void)
        {
            m_RefreshTime = 0;
        }

    bool NeedRefresh(CTimer &timer) const
        {
            return timer.Time() > m_RefreshTime;
        }

private:
    time_t m_RefreshTime;
};


struct NCBI_XLOADER_GENBANK_EXPORT SSeqrefs : public CObject
{
    typedef vector< CRef<CSeqref> > TSeqrefs;

    SSeqrefs(const CSeq_id_Handle& h);
    ~SSeqrefs(void);

    CSeq_id_Handle  m_Handle;
    TSeqrefs        m_Sr;
    CRefresher      m_Timer;

private:
    SSeqrefs(const SSeqrefs&);
    SSeqrefs operator=(const SSeqrefs&);
};


class NCBI_XLOADER_GENBANK_EXPORT CGBDataLoader : public CDataLoader
{
public:
    // typedefs from CReader
    typedef unsigned TConn;
    typedef vector< CRef<CSeqref> > TSeqrefs;
    typedef pair<pair<int, int>, int> TKeyByTSE;
    typedef int TMask;
    typedef CPluginManager<CReader>   TReader_PluginManager;

    CGBDataLoader(const string& loader_name="GENBANK",
                  CReader *driver=0,
                  int gc_threshold=100);

    CGBDataLoader(const string& loader_name/* ="GENBANK"*/,
                  TReader_PluginManager *plugin_manager /*=0*/,
                  EOwnership  take_plugin_manager /*= eNoOwnership */,
                  int gc_threshold);

    virtual ~CGBDataLoader(void);
  
    virtual void DropTSE(const CTSE_Info& tse_info);
    virtual void GetRecords(const CSeq_id_Handle& idh, EChoice choice);
    virtual void GetChunk(CTSE_Chunk_Info& chunk_info);
    virtual CConstRef<CSeqref> GetSatSatkey(const CSeq_id_Handle& idh);
    CConstRef<CSeqref> GetSatSatkey(const CSeq_id& id);
  
    virtual CConstRef<CTSE_Info> ResolveConflict(const CSeq_id_Handle& handle,
                                                 const TTSE_LockSet& tse_set);
  
    virtual void GC(void);
    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

    const CSeqref& GetSeqref(const CTSE_Info& tse_info);
  
private:
    struct STSEinfo : public CObject
    {
        typedef set<CSeq_id_Handle>  TSeqids;
        enum { eDead, eConfidential, eLast };
  
        STSEinfo*         next;
        STSEinfo*         prev;
        bitset<eLast>     mode;
        CRef<CSeqref>     seqref;
        TKeyByTSE         key;
        int               locked;
        CTSE_Info        *tseinfop;
  
        TSeqids           m_SeqIds;

        enum ELoadState {
            eLoadStateNone,
            eLoadStateDone,
            eLoadStatePartial
        };
        ELoadState        m_LoadState;

        STSEinfo();
        STSEinfo(const STSEinfo&);
        ~STSEinfo();
    };

    typedef map<TKeyByTSE, CRef<STSEinfo> >    TSr2TSEinfo;
    typedef map<CSeq_id_Handle, CRef<SSeqrefs> >        TSeqId2Seqrefs;
  
    CRef<CReader>           m_Driver;
    TReader_PluginManager*  m_ReaderPluginManager;
    EOwnership              m_OwnReaderPluginManager;

    TSr2TSEinfo     m_Sr2TseInfo;
  
    TSeqId2Seqrefs  m_Bs2Sr;
  
    CTimer          m_Timer;
  
    CGBLGuard::SLeveledMutex m_Locks;
  
    STSEinfo*       m_UseListHead;
    STSEinfo*       m_UseListTail;
    unsigned        m_TseCount;
    unsigned        m_TseGC_Threshhold;
    bool            m_InvokeGC;
    void            x_DropTSEinfo(STSEinfo* tse);
    void            x_UpdateDropList(STSEinfo* p);
    void            x_ExcludeFromDropList(STSEinfo* p);
    void            x_AppendToDropList(STSEinfo* p);
  
    //
    // private code
    //

    void            x_GetRecords(const char* type_name,
                                 const CSeq_id_Handle& idh,
                                 TMask sr_mask);
    void            x_GetRecords(const CSeq_id_Handle& key,
                                 TMask sr_mask);
    CRef<SSeqrefs>  x_ResolveHandle(const CSeq_id_Handle& h);
    bool            x_NeedMoreData(const STSEinfo& tse);
    void            x_GetData(CRef<STSEinfo> tse, TConn conn);
    void            x_GetChunk(CRef<STSEinfo> tse, TConn conn,
                               CTSE_Chunk_Info& chunk_info);
    void            x_GetChunk(CRef<STSEinfo> tse,
                               CTSE_Chunk_Info& chunk_info);
    void            x_Check(const STSEinfo* me = 0);
    void            x_CreateReaderPluginManager(void);
    CReader*        x_CreateReader(const string& env);
    void            x_CreateDriver(void);

    CRef<STSEinfo> GetTSEinfo(const CTSE_Info& tse_info);

    CGBDataLoader(const CGBDataLoader&);
    CGBDataLoader& operator=(const CGBDataLoader&);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif
/* ---------------------------------------------------------------------------
 *
 * $Log$
 * Revision 1.49  2004/06/30 21:02:02  vasilche
 * Added loading of external annotations from 26 satellite.
 *
 * Revision 1.48  2004/01/13 21:58:42  vasilche
 * Requrrected new version
 *
 * Revision 1.5  2004/01/13 16:55:52  vasilche
 * CReader, CSeqref and some more classes moved from xobjmgr to separate lib.
 * Headers moved from include/objmgr to include/objtools/data_loaders/genbank.
 *
 * Revision 1.4  2003/12/30 22:14:39  vasilche
 * Updated genbank loader and readers plugins.
 *
 * Revision 1.46  2003/12/30 19:51:24  vasilche
 * Implemented CGBDataLoader::GetSatSatkey() method.
 *
 * Revision 1.45  2003/12/03 15:13:38  kuznets
 * CReader management re-written to use plugin manager
 *
 * Revision 1.44  2003/11/26 20:56:21  vasilche
 * Added declaration of private constructors for MSVC DLL.
 *
 * Revision 1.43  2003/11/26 17:55:53  vasilche
 * Implemented ID2 split in ID1 cache.
 * Fixed loading of splitted annotations.
 *
 * Revision 1.42  2003/10/27 15:05:41  vasilche
 * Added correct recovery of cached ID1 loader if gi->sat/satkey cache is invalid.
 * Added recognition of ID1 error codes: private, etc.
 * Some formatting of old code.
 *
 * Revision 1.41  2003/10/07 13:43:22  vasilche
 * Added proper handling of named Seq-annots.
 * Added feature search from named Seq-annots.
 * Added configurable adaptive annotation search (default: gene, cds, mrna).
 * Fixed selection of blobs for loading from GenBank.
 * Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
 * Fixed leaked split chunks annotation stubs.
 * Moved some classes definitions in separate *.cpp files.
 *
 * Revision 1.40  2003/09/30 16:21:59  vasilche
 * Updated internal object manager classes to be able to load ID2 data.
 * SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
 * Scope caches results of requests for data to data loaders.
 * Optimized CSeq_id_Handle for gis.
 * Optimized bioseq lookup in scope.
 * Reduced object allocations in annotation iterators.
 * CScope is allowed to be destroyed before other objects using this scope are
 * deleted (feature iterators, bioseq handles etc).
 * Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
 * Added 'adaptive' option to objmgr_demo application.
 *
 * Revision 1.39  2003/08/27 14:24:43  vasilche
 * Simplified CCmpTSE class.
 *
 * Revision 1.38  2003/06/02 16:01:36  dicuccio
 * Rearranged include/objects/ subtree.  This includes the following shifts:
 *     - include/objects/alnmgr --> include/objtools/alnmgr
 *     - include/objects/cddalignview --> include/objtools/cddalignview
 *     - include/objects/flat --> include/objtools/flat
 *     - include/objects/objmgr/ --> include/objmgr/
 *     - include/objects/util/ --> include/objmgr/util/
 *     - include/objects/validator --> include/objtools/validator
 *
 * Revision 1.37  2003/05/20 19:53:49  vasilche
 * Added private assignment operator to make it compilable on MSVC with DLL.
 *
 * Revision 1.36  2003/05/20 16:18:42  vasilche
 * Fixed compilation errors on GCC.
 *
 * Revision 1.35  2003/05/20 15:44:37  vasilche
 * Fixed interaction of CDataSource and CDataLoader in multithreaded app.
 * Fixed some warnings on WorkShop.
 * Added workaround for memory leak on WorkShop.
 *
 * Revision 1.34  2003/05/13 20:14:40  vasilche
 * Catching exceptions and reconnection were moved from readers to genbank loader.
 *
 * Revision 1.33  2003/05/13 18:32:10  vasilche
 * Fixed GBLOG_POST() macro.
 *
 * Revision 1.32  2003/04/29 19:51:12  vasilche
 * Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
 * Made some typedefs more consistent.
 *
 * Revision 1.31  2003/04/24 16:12:37  vasilche
 * Object manager internal structures are splitted more straightforward.
 * Removed excessive header dependencies.
 *
 * Revision 1.30  2003/04/15 14:24:07  vasilche
 * Changed CReader interface to not to use fake streams.
 *
 * Revision 1.29  2003/03/03 20:34:51  vasilche
 * Added NCBI_THREADS macro - it's opposite to NCBI_NO_THREADS.
 * Avoid using _REENTRANT macro - use NCBI_THREADS instead.
 *
 * Revision 1.28  2003/03/01 22:26:07  kimelman
 * performance fixes
 *
 * Revision 1.27  2002/12/26 20:51:35  dicuccio
 * Added Win32 export specifier
 *
 * Revision 1.26  2002/07/23 15:31:18  kimelman
 * fill statistics for MutexPool
 *
 * Revision 1.25  2002/07/22 22:53:20  kimelman
 * exception handling fixed: 2level mutexing moved to Guard class + added
 * handling of confidential data.
 *
 * Revision 1.24  2002/07/19 18:36:14  lebedev
 * NCBI_OS_MAC: include path changed for types.h
 *
 * Revision 1.23  2002/07/08 20:50:56  grichenk
 * Moved log to the end of file
 * Replaced static mutex (in CScope, CDataSource) with the mutex
 * pool. Redesigned CDataSource data locking.
 *
 * Revision 1.22  2002/06/04 17:18:32  kimelman
 * memory cleanup :  new/delete/Cref rearrangements
 *
 * Revision 1.21  2002/05/14 20:06:23  grichenk
 * Improved CTSE_Info locking by CDataSource and CDataLoader
 *
 * Revision 1.20  2002/05/09 21:41:01  kimelman
 * MT tuning
 *
 * Revision 1.19  2002/05/06 03:30:35  vakatov
 * OM/OM1 renaming
 *
 * Revision 1.18  2002/05/03 21:28:01  ucko
 * Introduce T(Signed)SeqPos.
 *
 * Revision 1.17  2002/04/11 02:09:52  vakatov
 * Get rid of a warning
 *
 * Revision 1.16  2002/04/09 19:04:21  kimelman
 * make gcc happy
 *
 * Revision 1.15  2002/04/09 18:48:14  kimelman
 * portability bugfixes: to compile on IRIX, sparc gcc
 *
 * Revision 1.14  2002/04/09 15:53:42  kimelman
 * turn off debug messages
 *
 * Revision 1.13  2002/04/08 23:09:23  vakatov
 * CMutexPool::Select()  -- fixed for 64-bit compilation
 *
 * Revision 1.12  2002/04/05 23:47:17  kimelman
 * playing around tests
 *
 * Revision 1.11  2002/04/04 01:35:33  kimelman
 * more MT tests
 *
 * Revision 1.10  2002/04/02 16:02:28  kimelman
 * MT testing
 *
 * Revision 1.9  2002/03/30 19:37:05  kimelman
 * gbloader MT test
 *
 * Revision 1.8  2002/03/29 02:47:01  kimelman
 * gbloader: MT scalability fixes
 *
 * Revision 1.7  2002/03/27 20:22:31  butanaev
 * Added connection pool.
 *
 * Revision 1.6  2002/03/26 15:39:24  kimelman
 * GC fixes
 *
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

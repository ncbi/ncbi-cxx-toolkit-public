#ifndef NCBI_OBJMGR_SPLIT_CACHE__HPP
#define NCBI_OBJMGR_SPLIT_CACHE__HPP

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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Application for splitting blobs withing ID1 cache
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>

#include <memory>
#include <set>

#include <serial/serialdef.hpp>

#include <objmgr/seq_id_handle.hpp>

#include <objmgr/split/blob_splitter_params.hpp>

BEGIN_NCBI_SCOPE

class ICache;

BEGIN_SCOPE(objects)

class CObjectManager;
class CSeq_id;
class CScope;
class CBioseq_Handle;
class CDataLoader;
class CCachedId1Reader;
class CGBDataLoader;
class CSeqref;

/////////////////////////////////////////////////////////////////////////////
//
//  Application class
//


class CLevelGuard
{
public:
    typedef size_t TLevel;
    CLevelGuard(TLevel& level)
        : m_Level(level)
        {
            ++m_Level;
        }
    ~CLevelGuard(void)
        {
            --m_Level;
        }

    operator TLevel(void) const
        {
            return m_Level;
        }

private:
    TLevel& m_Level;
};


class CLog;
class CSplitDataMaker;
#define WAIT_LINE4(app) CLog line(app); line
#define WAIT_LINE WAIT_LINE4(this)
#define LINE(Msg) do { WAIT_LINE << Msg; } while(0)


class CSplitCacheApp : public CNcbiApplication
{
public:
    CSplitCacheApp(void);
    ~CSplitCacheApp(void);

    virtual void Init(void);
    virtual int  Run (void);

    void SetupCache(void);
    void Process(void);

    void ProcessSeqId(const CSeq_id& seq_id);
    void ProcessGi(int gi);
    void ProcessBlob(const CSeqref& seqref);

    CNcbiOstream& Info(void) const;

    static string GetFileName(const string& key,
                              const string& suffix,
                              const string& ext);

    const SSplitterParams GetParams(void) const
    {
        return m_SplitterParams;
    }

    const CCachedId1Reader& GetReader(void) const
    {
        return *m_Reader;
    }

    ICache& GetCache(void)
    {
        return *m_Cache;
    }

    ICache& GetIdCache(void)
    {
        return *m_IdCache;
    }

protected:
    CConstRef<CSeqref> GetSeqref(CBioseq_Handle bh);

    typedef set< pair<int, int> > TProcessedBlobs;
    typedef set<CSeq_id_Handle> TProcessedIds;

private:
    // parameters
    bool m_DumpAsnText;
    bool m_DumpAsnBinary;
    bool m_Resplit;
    bool m_Recurse;
    SSplitterParams m_SplitterParams;
    
    // splitter loaders/managers
    auto_ptr<ICache>            m_Cache;
    auto_ptr<ICache>            m_IdCache;
    CCachedId1Reader*           m_Reader;
    CRef<CGBDataLoader>         m_Loader;
    CRef<CObjectManager>        m_ObjMgr;
    CRef<CScope>                m_Scope;

    // splitter process state
    size_t      m_RecursionLevel;
    TProcessedBlobs  m_ProcessedBlobs;
    TProcessedIds  m_ProcessedIds;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2004/04/28 16:29:15  vasilche
* Store split results into new ICache.
*
* Revision 1.9  2004/03/16 15:47:29  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.8  2004/01/07 17:37:37  vasilche
* Fixed include path to genbank loader.
* Moved split_cache application.
*
* Revision 1.7  2003/12/02 23:24:33  vasilche
* Added "-recurse" option to split all sequences referenced by SeqMap.
*
* Revision 1.6  2003/12/02 19:59:31  vasilche
* Added GetFileName() declaration.
*
* Revision 1.5  2003/12/02 19:49:31  vasilche
* Added missing forward declarations.
*
* Revision 1.4  2003/12/02 19:12:24  vasilche
* Fixed compilation on MSVC.
*
* Revision 1.3  2003/11/26 23:05:00  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.2  2003/11/26 17:56:03  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.1  2003/11/12 16:18:32  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
#endif//NCBI_OBJMGR_SPLIT_CACHE__HPP

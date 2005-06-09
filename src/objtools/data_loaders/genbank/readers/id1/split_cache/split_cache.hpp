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

#include <objects/seq/seq_id_handle.hpp>

#include <objmgr/split/blob_splitter_params.hpp>

BEGIN_NCBI_SCOPE

class ICache;

BEGIN_SCOPE(objects)

class CObjectManager;
class CSeq_id;
class CScope;
class CBioseq_Handle;
class CSeq_entry_Handle;
class CDataLoader;
class CGBDataLoader;
class CBlob_id;
class CID2S_Chunk_Id;
class CID2S_Chunk_Content;

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


class CSplitContentIndex : public CObject
{
public:
    CSplitContentIndex(void)
        : m_SplitContent(0),
          m_SeqDescCount(0),
          m_SetDescCount(0)
        {}

    void IndexChunkContent(int chunk_id,
                           const CID2S_Chunk_Content& content);

    enum EContentFlags {
        fSeqDesc   = 1 << 0,
        fSetDesc   = 1 << 1,
        fDescLow   = 1 << 2,
        fDescHigh  = 1 << 3,
        fDescOther = 1 << 4,
        fDesc      = fSeqDesc + fSetDesc + fDescLow + fDescHigh + fDescOther,
        fAnnot     = 1 << 5,
        fData      = 1 << 6,
        fAssembly  = 1 << 7,
        fOther     = 1 << 8
    };
    typedef int                     TContentFlags;
    typedef map<int, TContentFlags> TContentIndex;

    TContentFlags GetSplitContent(void) const
        {
            return m_SplitContent;
        }
    bool HaveSplitDesc(void) const
        {
            return (m_SplitContent & fDesc) != 0;
        }
    bool HaveSplitAnnot(void) const
        {
            return (m_SplitContent & fAnnot) != 0;
        }
    bool HaveSplitData(void) const
        {
            return (m_SplitContent & fData) != 0;
        }
    bool HaveSplitAssembly(void) const
        {
            return (m_SplitContent & fAssembly) != 0;
        }
    bool HaveSplitOther(void) const
        {
            return (m_SplitContent & fOther) != 0;
        }
    const TContentIndex& GetContentIndex(void) const
        {
            return m_ContentIndex;
        }

    void SetSeqDescCount(size_t count)
        {
            m_SeqDescCount = count;
        }
    void SetSetDescCount(size_t count)
        {
            m_SetDescCount = count;
        }
    size_t GetSeqDescCount(void) const
        {
            return m_SeqDescCount;
        }
    size_t GetSetDescCount(void) const
        {
            return m_SetDescCount;
        }
    size_t GetDescCount(void) const
        {
            return m_SeqDescCount + m_SetDescCount;
        }

private:
    TContentFlags m_SplitContent;
    TContentIndex m_ContentIndex;
    size_t        m_SeqDescCount;
    size_t        m_SetDescCount;
};


class CSplitCacheApp : public CNcbiApplication
{
public:
    CSplitCacheApp(void);
    ~CSplitCacheApp(void);

    virtual void Init(void);
    virtual int  Run (void);

    void SetupCache(void);
    void Process(void);
    void TestSplit(void);

    void ProcessSeqId(const CSeq_id& seq_id);
    void ProcessGi(int gi);
    void ProcessBlob(CBioseq_Handle& bh, const CSeq_id_Handle& idh);

    void TestSplitBlob(CSeq_id_Handle id, const CSplitContentIndex& content);

    bool GetBioseqHandle(CBioseq_Handle& bh, const CSeq_id_Handle& idh);
    void PrintVersion(int version);

    CNcbiOstream& Info(void) const;

    static string GetFileName(const string& key,
                              const string& suffix,
                              const string& ext);

    const SSplitterParams GetParams(void) const
    {
        return m_SplitterParams;
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
    const CBlob_id& GetBlob_id(CSeq_entry_Handle tse);

    typedef set<CBlob_id> TProcessedBlobs;
    typedef set<CSeq_id_Handle> TProcessedIds;
    typedef map< CSeq_id_Handle, CRef<CSplitContentIndex> > TContentMap;

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
    CRef<CGBDataLoader>         m_Loader;
    CRef<CObjectManager>        m_ObjMgr;
    CRef<CScope>                m_Scope;

    // splitter process state
    size_t           m_RecursionLevel;
    TProcessedBlobs  m_ProcessedBlobs;
    TProcessedIds    m_ProcessedIds;

    TContentMap      m_ContentMap;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.17  2005/06/09 20:33:55  grichenk
* Fixed loading of split descriptors by CSeqdesc_CI
*
* Revision 1.16  2005/06/09 15:19:08  grichenk
* Redesigned split_cache to work with the new GB loader.
* Test loading of the split data.
*
* Revision 1.15  2004/08/19 17:00:51  vasilche
* Moved Sat/SatKey enums from obsolete class CSeqref.
*
* Revision 1.14  2004/08/04 14:55:18  vasilche
* Changed TSE locking scheme.
* TSE cache is maintained by CDataSource.
* Added ID2 reader.
* CSeqref is replaced by CBlobId.
*
* Revision 1.13  2004/07/12 15:05:32  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.12  2004/06/30 21:02:02  vasilche
* Added loading of external annotations from 26 satellite.
*
* Revision 1.11  2004/06/15 14:07:39  vasilche
* Added possibility to split sequences.
*
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

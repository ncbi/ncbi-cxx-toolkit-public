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
        fBioseq    = 1 << 8,
        fOther     = 1 << 9
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
    bool HaveSplitBioseq(void) const
        {
            return (m_SplitContent & fBioseq) != 0;
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
    void TestSplitBioseq(CSeq_entry_Handle seh);

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

#endif//NCBI_OBJMGR_SPLIT_CACHE__HPP

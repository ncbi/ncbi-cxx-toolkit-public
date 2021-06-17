#ifndef OBJTOOLS_DATA_LOADERS_SRA___VDBGRAPHLOADER_IMPL__HPP
#define OBJTOOLS_DATA_LOADERS_SRA___VDBGRAPHLOADER_IMPL__HPP

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
 * Author: Eugene Vasilchenko
 *
 * File Description: VDB graph file data loader
 *
 * ===========================================================================
 */


#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <sra/data_loaders/vdbgraph/vdbgraphloader.hpp>
#include <sra/readers/sra/graphread.hpp>
#include <util/limited_size_map.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;
class CVDBGraphBlobId;

class CVDBGraphDataLoader_Impl : public CObject
{
public:
    typedef vector<string> TVDBFiles;
    typedef CDataLoader::TBlobId TBlobId;
    typedef CDataLoader::TTSE_Lock TTSE_Lock;
    typedef CDataLoader::TTSE_LockSet TTSE_LockSet;

    explicit CVDBGraphDataLoader_Impl(const TVDBFiles& vdb_files);
    ~CVDBGraphDataLoader_Impl(void);

    CObjectManager::TPriority GetDefaultPriority(void) const;
    
    typedef vector<CAnnotName> TAnnotNames;
    TAnnotNames GetPossibleAnnotNames(void) const;

    TBlobId GetBlobId(const CSeq_id_Handle& idh);
    TBlobId GetBlobIdFromString(const string& str) const;
    TTSE_Lock GetBlobById(CDataSource* ds, const TBlobId& blob_id);
    TTSE_LockSet GetRecords(CDataSource* ds,
                            const CSeq_id_Handle& idh,
                            CDataLoader::EChoice choice);
    TTSE_LockSet GetOrphanAnnotRecords(CDataSource* ds,
                                       const CSeq_id_Handle& idh,
                                       const SAnnotSelector* sel,
                                       CDataLoader::TProcessedNAs* processed_nas);
    void GetChunk(CTSE_Chunk_Info& chunk);

    CRef<CSeq_entry> LoadFullEntry(const CVDBGraphBlobId& blob_id);
    void LoadSplitEntry(CTSE_Info& tse, const CVDBGraphBlobId& blob_id);

    struct SVDBFileInfo : CObject {
        CVDBGraphDb m_VDB;
        string m_VDBFile;
        string m_BaseAnnotName;
        string GetMainAnnotName(void) const;
        string GetOverviewAnnotName(void) const;
        string GetMidZoomAnnotName(void) const;
        bool ContainsAnnotsFor(const CSeq_id_Handle& id) const;
    };
protected:
    typedef map<string, CRef<SVDBFileInfo> > TFixedFileMap;
    typedef limited_size_map<string, CRef<SVDBFileInfo> > TAutoFileMap;
    typedef limited_size_map<string, bool> TMissingFileSet;

    CRef<SVDBFileInfo> x_GetFileInfo(const string& name);
    CRef<SVDBFileInfo> x_GetNAFileInfo(const string& na_acc);

    // mutex guarding input into the map
    CMutex m_Mutex;
    CVDBMgr m_Mgr;
    TFixedFileMap m_FixedFileMap;
    TAutoFileMap m_AutoFileMap;
    TMissingFileSet m_MissingFileSet;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_SRA___VDBGRAPHLOADER_IMPL__HPP

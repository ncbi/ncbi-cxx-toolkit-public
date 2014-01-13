#ifndef OBJTOOLS_DATA_LOADERS_WGS___WGSLOADER_IMPL__HPP
#define OBJTOOLS_DATA_LOADERS_WGS___WGSLOADER_IMPL__HPP

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
 * File Description: WGS file data loader
 *
 * ===========================================================================
 */


#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <sra/data_loaders/wgs/wgsloader.hpp>
#include <sra/readers/sra/wgsread.hpp>
#include <util/limited_size_map.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;
class CWGSDataLoader_Impl;
class CWGSSeqInfo;
class CWGSFileInfo;

class CWGSBlobId : public CBlobId
{
public:
    CWGSBlobId(CTempString str);
    CWGSBlobId(CTempString prefix, bool scaffold, Uint8 row_id);
    ~CWGSBlobId(void);

    // wgs file name or SRR accession
    string m_WGSPrefix;
    bool m_Scaffold;
    Uint8 m_RowId;

    // string blob id representation:
    // eBlobType_annot_plain_id
    string ToString(void) const;
    void FromString(CTempString str);

    bool operator<(const CBlobId& id) const;
    bool operator==(const CBlobId& id) const;
};


class CWGSFileInfo : public CObject
{
public:
    CWGSFileInfo(CWGSDataLoader_Impl& impl,
                 CTempString prefix);
    
    CTempString GetWGSPrefix(void)
        {
            return m_WGSPrefix;
        }

    bool IsValidRowId(bool scaffold, Uint8 row, SIZE_TYPE row_digits);

    CMutex& GetMutex(void) const
        {
            return m_WGSMutex;
        }

    CWGSDb& GetDb(void)
        {
            return m_WGSDb;
        }
    operator CWGSDb&(void)
        {
            return GetDb();
        }

    void LoadBlob(const CWGSBlobId& blob_id,
                  CTSE_LoadLock& load_lock);
    void LoadChunk(const CWGSBlobId& blob_id,
                   CTSE_Chunk_Info& chunk);

protected:
    friend class CWGSDataLoader_Impl;

    void x_Initialize(CWGSDataLoader_Impl& impl,
                      CTempString prefix);
    void x_InitMasterDescr(void);

    string m_WGSPrefix;
    mutable CMutex m_WGSMutex;
    CWGSDb m_WGSDb;
    Uint8 m_FirstBadRowId[2];
};


class CWGSDataLoader_Impl : public CObject
{
public:
    CWGSDataLoader_Impl(const CWGSDataLoader::SLoaderParams& params);
    ~CWGSDataLoader_Impl(void);

    CRef<CWGSFileInfo> GetWGSFile(const string& acc);

    CRef<CWGSFileInfo> GetFileInfo(const CWGSBlobId& blob_id);
    CRef<CWGSFileInfo> GetFileInfo(const string& acc,
                                   bool* scaffold_ptr = 0,
                                   Uint8* row_id_ptr = 0);
    CRef<CWGSFileInfo> GetFileInfo(const CSeq_id_Handle& idh,
                                   bool* scaffold_ptr = 0,
                                   Uint8* row_id_ptr = 0);

    CWGSSeqIterator GetSeqIterator(const CSeq_id_Handle& id,
                                   CWGSScaffoldIterator* iter2_ptr = 0);

    CDataLoader::TTSE_LockSet GetRecords(CDataSource* data_source,
                                         const CSeq_id_Handle& idh,
                                         CDataLoader::EChoice choice);
    CRef<CWGSBlobId> GetBlobId(const CSeq_id_Handle& idh);
    CTSE_LoadLock GetBlobById(CDataSource* data_source,
                              const CWGSBlobId& blob_id);
    void LoadBlob(const CWGSBlobId& blob_id,
                  CTSE_LoadLock& load_lock);
    void LoadChunk(const CWGSBlobId& blob_id,
                   CTSE_Chunk_Info& chunk);

    typedef vector<CSeq_id_Handle> TIds;
    void GetIds(const CSeq_id_Handle& idh, TIds& ids);
    CSeq_id_Handle GetAccVer(const CSeq_id_Handle& idh);
    int GetGi(const CSeq_id_Handle& idh);
    int GetTaxId(const CSeq_id_Handle& idh);
    TSeqPos GetSequenceLength(const CSeq_id_Handle& idh);
    CSeq_inst::TMol GetSequenceType(const CSeq_id_Handle& idh);

protected:
    friend class CWGSFileInfo;

private:
    // first:
    //   false if explicitly listed file in the loader params
    //   true if dynamically loaded SRA
    // second: SRA accession or wgs file path

    // WGS files by accession
    typedef map<string, CRef<CWGSFileInfo> > TFixedFiles;
    typedef limited_size_map<string, CRef<CWGSFileInfo> > TFoundFiles;

    // mutex guarding input into the map
    CMutex  m_Mutex;
    CVDBMgr m_Mgr;
    string  m_WGSVolPath;
    TFixedFiles m_FixedFiles;
    TFoundFiles m_FoundFiles;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_WGS___WGSLOADER_IMPL__HPP

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
class CWGSBlobId;
class CWGSResolver;

class CWGSFileInfo : public CObject
{
public:
    CWGSFileInfo(CWGSDataLoader_Impl& impl,
                 CTempString prefix);
    
    CTempString GetWGSPrefix(void) const
        {
            return m_WGSPrefix;
        }

    struct SAccFileInfo {
        SAccFileInfo(void)
            : row_id(0),
              seq_type('\0'),
              has_version(false),
              version(0)
            {
            }
        DECLARE_OPERATOR_BOOL_REF(file);

        bool IsContig(void) const {
            return seq_type == '\0';
        }
        bool IsScaffold(void) const {
            return seq_type == 'S';
        }
        bool IsProtein(void) const {
            return seq_type == 'P';
        }

        CWGSSeqIterator GetContigIterator(void) const;
        CWGSScaffoldIterator GetScaffoldIterator(void) const;
        CWGSProteinIterator GetProteinIterator(void) const;

        bool IsValidRowId(void) const;

        CConstRef<CWGSFileInfo> file;
        TVDBRowId row_id;
        char seq_type; // '\0' - regular nuc, 'S' - scaffold, 'P' - protein
        bool has_version;
        int version;
    };

    bool IsCorrectVersion(const SAccFileInfo& info) const;

    bool FindGi(SAccFileInfo& info, TGi gi) const;
    bool FindProtAcc(SAccFileInfo& info, const string& acc) const;

    const CWGSDb& GetDb(void) const
        {
            return m_WGSDb;
        }
    operator const CWGSDb&(void) const
        {
            return GetDb();
        }

    void LoadBlob(const CWGSBlobId& blob_id,
                  CTSE_LoadLock& load_lock) const;
    void LoadChunk(const CWGSBlobId& blob_id,
                   CTSE_Chunk_Info& chunk) const;

    CWGSSeqIterator GetContigIterator(const CWGSBlobId& blob_id) const;
    CWGSScaffoldIterator GetScaffoldIterator(const CWGSBlobId& blob_id) const;
    CWGSProteinIterator GetProteinIterator(const CWGSBlobId& blob_id) const;

protected:
    friend class CWGSDataLoader_Impl;

    void x_Initialize(CWGSDataLoader_Impl& impl,
                      CTempString prefix);
    void x_InitMasterDescr(void);

    string m_WGSPrefix;
    CWGSDb m_WGSDb;
};


class CWGSDataLoader_Impl : public CObject
{
public:
    explicit CWGSDataLoader_Impl(const CWGSDataLoader::SLoaderParams& params);
    ~CWGSDataLoader_Impl(void);

    CConstRef<CWGSFileInfo> GetWGSFile(const string& acc);

    CConstRef<CWGSFileInfo> GetFileInfo(const CWGSBlobId& blob_id);
    typedef CWGSFileInfo::SAccFileInfo SAccFileInfo;
    SAccFileInfo GetFileInfoByGi(TGi gi);
    SAccFileInfo GetFileInfoByProtAcc(const string& acc);
    SAccFileInfo GetFileInfoByAcc(const string& acc);
    SAccFileInfo GetFileInfo(const CSeq_id_Handle& idh);

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
    TGi GetGi(const CSeq_id_Handle& idh);
    int GetTaxId(const CSeq_id_Handle& idh);
    TSeqPos GetSequenceLength(const CSeq_id_Handle& idh);
    int GetSequenceHash(const CSeq_id_Handle& idh);
    CSeq_inst::TMol GetSequenceType(const CSeq_id_Handle& idh);

    bool GetAddWGSMasterDescr(void) const
        {
            return m_AddWGSMasterDescr;
        }

    void SetAddWGSMasterDescr(bool flag)
        {
            m_AddWGSMasterDescr = flag;
        }
    
protected:
    friend class CWGSFileInfo;

    CWGSResolver& GetResolver(void);

private:
    // first:
    //   false if explicitly listed file in the loader params
    //   true if dynamically loaded SRA
    // second: SRA accession or wgs file path

    // WGS files by accession
    typedef map<string, CConstRef<CWGSFileInfo> > TFixedFiles;
    typedef limited_size_map<string, CConstRef<CWGSFileInfo> > TFoundFiles;

    // mutex guarding input into the map
    CMutex  m_Mutex;
    CVDBMgr m_Mgr;
    string  m_WGSVolPath;
    CRef<CWGSResolver> m_Resolver;
    TFixedFiles m_FixedFiles;
    TFoundFiles m_FoundFiles;
    bool m_AddWGSMasterDescr;
    bool m_ResolveGIs;
    bool m_ResolveProtAccs;
};


class CWGSBlobId : public CBlobId
{
public:
    explicit CWGSBlobId(CTempString str);
    explicit CWGSBlobId(const CWGSFileInfo::SAccFileInfo& info);
    ~CWGSBlobId(void);

    // wgs file name or SRR accession
    string m_WGSPrefix;
    char m_SeqType;
    TVDBRowId m_RowId;

    // string blob id representation:
    // eBlobType_annot_plain_id
    string ToString(void) const;
    void FromString(CTempString str);

    bool operator<(const CBlobId& id) const;
    bool operator==(const CBlobId& id) const;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_WGS___WGSLOADER_IMPL__HPP

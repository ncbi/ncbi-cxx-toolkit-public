#ifndef OBJTOOLS_DATA_LOADERS_BLASTDB___BDBLOADER__HPP
#define OBJTOOLS_DATA_LOADERS_BLASTDB___BDBLOADER__HPP

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
*  Author: Christiam Camacho
*
* ===========================================================================
*/

/** @file bdbloader.hpp
  * Data loader implementation that uses the blast databases
  */

#include <corelib/ncbistd.hpp>
#include <objmgr/data_loader.hpp>
#include <objtools/data_loaders/blastdb/blastdb_adapter.hpp>
#include <objects/seqset/Seq_entry.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Parameter names used by loader factory

const string kCFParam_BlastDb_DbName = "DbName"; // = string
const string kCFParam_BlastDb_DbType = "DbType"; // = EDbType (e.g. "Protein")


class NCBI_XLOADER_BLASTDB_EXPORT CBlastDbDataLoader : public CDataLoader
{
public:
    /// Describes the type of blast database to use
    enum EDbType {
        eNucleotide = 0,    ///< nucleotide database
        eProtein = 1,       ///< protein database
        eUnknown = 2        ///< protein is attempted first, then nucleotide
    };

    struct NCBI_XLOADER_BLASTDB_EXPORT SBlastDbParam
    {
        SBlastDbParam(const string& db_name = "nr",
                      EDbType       dbtype = eUnknown,
                      bool          use_fixed_size_slices = true);

        SBlastDbParam(CRef<CSeqDB> db_handle,
                      bool         use_fixed_size_slices = true);

        string          m_DbName;
        EDbType         m_DbType;
        bool            m_UseFixedSizeSlices;
        CRef<CSeqDB>    m_BlastDbHandle;
    };

    typedef SRegisterLoaderInfo<CBlastDbDataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& dbname = "nr",
        const EDbType dbtype = eUnknown,
        bool use_fixed_size_slices = true,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CRef<CSeqDB> db_handle,
        bool use_fixed_size_slices = true,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(CConstRef<CSeqDB> db_handle);
    static string GetLoaderNameFromArgs(const SBlastDbParam& param);
    static string GetLoaderNameFromArgs(const string& dbname = "nr",
                                        const EDbType dbtype = eUnknown)
    {
        return GetLoaderNameFromArgs(SBlastDbParam(dbname, dbtype));
    }
    
    virtual ~CBlastDbDataLoader();
    
    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;


    /// Load TSE
    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh, EChoice choice);
    /// Load a description or data chunk.
    virtual void GetChunk(TChunk chunk);

    virtual TSeqPos GetSequenceLength(const CSeq_id_Handle& idh);
    virtual CSeq_inst::TMol GetSequenceType(const CSeq_id_Handle& idh);
    virtual void GetSequenceTypes(const TIds& ids, TLoaded& loaded,
                                  TSequenceTypes& ret);

    /// Gets the blob id for a given sequence.
    ///
    /// Given a Seq_id_Handle, this method finds the corresponding top
    /// level Seq-entry (TSE) and returns a blob corresponding to it.
    /// The BlobId is initialized with a pointer to that CSeq_entry if
    /// the sequence is known to this data loader, which will be true
    /// if GetRecords() was called for this sequence.
    ///
    /// @param idh
    ///   Indicates the sequence for which to get a blob id.
    /// @return
    ///   A TBlobId corresponding to the provided Seq_id_Handle.
    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh);
    
    /// Test method for GetBlobById feature.
    ///
    /// The caller will use this method to determine whether this data
    /// loader allows blobs to be managed by ID.
    ///
    /// @return
    ///   Returns true to indicate that GetBlobById() is available.
    virtual bool CanGetBlobById() const;
    
    /// For a given TBlobId, get the TTSE_Lock.
    ///
    /// If the provided TBlobId is known to this code, the
    /// corresponding TTSE_Lock data will be fetched and returned.
    /// Otherwise, an empty valued TTSE_Lock is returned.
    ///
    /// @param blob_id
    ///   Indicates which data to get.
    /// @return
    ///   The returned data.
    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id);
    
    /// A mapping from sequence identifier to blob ids.
    typedef map< CSeq_id_Handle, int > TIdMap;

    /// @note this is added to temporarily comply with the toolkit's stable
    /// components rule of having backwards compatible APIs
    NCBI_DEPRECATED
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& dbname,
        const EDbType dbtype,
        CObjectManager::EIsDefault is_default,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    /// @note this is added to temporarily comply with the toolkit's stable
    /// components rule of having backwards compatible APIs
    NCBI_DEPRECATED
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        CRef<CSeqDB> db_handle,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
protected:
    /// TPlace is a Seq-id or an integer id, this data loader uses the former.
    typedef int TBioseq_setId;
    typedef CSeq_id_Handle TBioseqId;
    typedef pair<TBioseqId, TBioseq_setId> TPlace;
    
    typedef CParamLoaderMaker<CBlastDbDataLoader, SBlastDbParam> TMaker;
    friend class CParamLoaderMaker<CBlastDbDataLoader, SBlastDbParam>;

    /// Default (no-op) constructor
    CBlastDbDataLoader() {}
    /// Parametrized constructor
    /// @param loader_name name of this data loader [in]
    /// @param param parameters to initialize this data loader [in]
    CBlastDbDataLoader(const string& loader_name, const SBlastDbParam& param);
    
    /// Prevent automatic copy constructor generation
    CBlastDbDataLoader(const CBlastDbDataLoader &);
    
    /// Prevent automatic assignment operator generation
    CBlastDbDataLoader & operator=(const CBlastDbDataLoader &);

    /// Gets the OID from m_Ids cache or the BLAST databases
    int x_GetOid(const CSeq_id_Handle& idh);
    /// Gets the OID from a TBlobId (see typedef in bdbloader.cpp)
    int x_GetOid(const TBlobId& blob_id) const;
    
    /// Load sequence data from cache or from the database.
    ///
    /// This checks the OID cache and loads the sequence data from
    /// there or if not found, from the CSeqDB database.  When new
    /// data is built, the sequence is also split into chunks.  A
    /// description of what data is available will be returned in the
    /// "lock" parameter.
    ///
    /// @param idh
    ///   A handle to the sequence identifier.
    /// @param oid
    ///   Object id in BLAST DB
    /// @param lock
    ///   Information about the sequence data is returned here.
    void x_LoadData(const CSeq_id_Handle& idh, int oid, CTSE_LoadLock & lock,
                    int slice_size);
    
    string          m_DBName;      ///< Blast database name
    EDbType         m_DBType;      ///< Is this database protein or nucleotide?
    CRef<IBlastDbAdapter> m_BlastDb;       ///< The sequence database

    TIdMap          m_Ids;         ///< ID to OID translation

    /// Configuration value specified to the CCachedSequence
    bool            m_UseFixedSizeSlices;
};

END_SCOPE(objects)


extern NCBI_XLOADER_BLASTDB_EXPORT const string kDataLoader_BlastDb_DriverName;

extern "C"
{

NCBI_XLOADER_BLASTDB_EXPORT
void NCBI_EntryPoint_DataLoader_BlastDb(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

NCBI_XLOADER_BLASTDB_EXPORT
void NCBI_EntryPoint_xloader_blastdb(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif /* OBJTOOLS_DATA_LOADERS_BLASTDB___BDBLOADER__HPP */

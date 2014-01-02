#ifndef LDS2_DB_HPP__
#define LDS2_DB_HPP__
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
 * Author: Aleksey Grichenko
 *
 * File Description:   Local data storage v.2, database access.
 *
 */

#include <corelib/ncbiobj.hpp>
#include <util/format_guess.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <util/range.hpp>
#include <set>

BEGIN_NCBI_SCOPE

// Forward declarations
class CSQLITE_Connection;
class CSQLITE_Statement;

BEGIN_SCOPE(objects)


//////////////////////////////////////////////////////////////////
///
/// LDS2 database.
///

/// File info structure
struct SLDS2_File
{
    typedef CFormatGuess::EFormat TFormat;

    Int8    id;
    string  name;
    TFormat format;
    string  handler;
    Int8    size;
    Int8    time;
    Uint4   crc;

    SLDS2_File(void) { Reset(); }

    SLDS2_File(const string& file_name) {
        Reset();
        name = file_name;
    }

    void Reset(void) {
        id = 0;
        name = kEmptyStr;
        format = CFormatGuess::eUnknown;
        handler = kEmptyStr;
        size = -1;
        time = 0;
        crc = 0;
    }

    bool operator==(const SLDS2_File& f) const {
        return id == f.id  &&
            name == f.name  &&
            format == f.format  &&
            handler == f.handler  &&
            size == f.size  &&
            time == f.time  &&
            crc == f.crc;
    }

    bool operator!=(const SLDS2_File& f) const {
        return !(*this == f);
    }

    bool exists(void) const {
        // non-existent files are indicated by negative size
        return size >= 0;
    }
};


/// Chunk info
struct SLDS2_Chunk {
    /// Chunk position in the raw file
    Int8 raw_pos;
    /// Chunk position in the processed (e.g. unzipped) stream
    Int8 stream_pos;
    /// Extra data required to load the chunk. The owner of the
    /// pointer is responsible for deleting the buffer.
    void* data;
    /// Extra data size
    size_t data_size;

    SLDS2_Chunk(void)
        : raw_pos(0),
          stream_pos(0),
          data(NULL),
          data_size(0)
    {}

    SLDS2_Chunk(Int8 raw, Int8 str)
        : raw_pos(raw),
          stream_pos(str),
          data(NULL),
          data_size(0)
    {}

    ~SLDS2_Chunk(void)
    {
        DeleteData();
    }

    void DeleteData(void)
    {
        if ( data ) {
            delete[] (unsigned char*)data;
            data = NULL;
        }
        data_size = 0;
    }

    void InitData(size_t sz)
    {
        if ( data ) {
            DeleteData();
        }
        data_size = sz;
        if ( data_size ) {
            data = new unsigned char[data_size];
        }
    }

private:
    // Prohibit copy operations.
    SLDS2_Chunk(const SLDS2_Chunk&);
    SLDS2_Chunk& operator=(const SLDS2_Chunk&);
};


/// Top level object info
struct SLDS2_Blob
{
    /// Top-level object types
    enum EBlobType {
        eUnknown            = 0,
        eSeq_entry          = 1,
        eBioseq             = 2,
        eBioseq_set         = 3,
        /// Used for indexing individual seq-entries from a top-level
        /// bioseq-set.
        eBioseq_set_element = 4,
        eSeq_annot          = 5,
        eSeq_align_set      = 6,
        eSeq_align          = 7,
        eSeq_submit         = 8
    };

    Int8        id;
    EBlobType   type;
    Int8        file_id;
    Int8        file_pos;

    SLDS2_Blob(void)
        : id(0),
          type(eUnknown),
          file_id(0),
          file_pos(-1)
    {}
};


/// Info about seq-id used in an annotation.
struct SLDS2_AnnotIdInfo
{
    typedef CRange<TSeqPos> TRange;

    bool   external;
    TRange range;

    SLDS2_AnnotIdInfo(void)
        : external(true),
          range(TRange::GetEmpty())
    {}
};


/// Annotation info.
struct SLDS2_Annot
{
    typedef map<CSeq_id_Handle, SLDS2_AnnotIdInfo> TIdMap;

    /// Annotation type
    enum EType {
        eUnknown        = 0,
        eSeq_feat       = 1,
        eSeq_align      = 2,
        eSeq_graph      = 3
    };

    Int8   id;
    EType  type;
    Int8   blob_id;
    bool   is_named;
    string name;
    TIdMap ref_ids;

    SLDS2_Annot(void)
        : id(-1),
          type(eUnknown),
          blob_id(-1),
          is_named(false)
    {}
};


class NCBI_LDS2_EXPORT CLDS2_Database : public CObject
{
public:
    /// Database access mode flags.
    enum EAccessMode {
        eRead,  ///< Read-only access.
        eWrite  ///< Read/write access.
    };

    CLDS2_Database(const string& db_file, EAccessMode mode = eWrite);

    ~CLDS2_Database(void);

    /// Create the database. If the LDS2 database already exists all data will
    /// be cleaned up. Access mode is automatically set to read/write.
    /// NOTE: The function may fail if the db has been accessed from other
    /// threads and some of the threads are still alive.
    void Create(void);

    /// Open LDS2 database. If the database does not exist, throws exception.
    void Open(EAccessMode mode = eWrite);

    /// Get database file name.
    const string& GetDbFile(void) const { return m_DbFile; }

    /// Get SQLite flags, see CSQLITE_Connection::TOperationFlags.
    int GetSQLiteFlags(void) const { return m_DbFlags; }
    /// Set SQLite flags. This funtion resets the db connection.
    void SetSQLiteFlags(int flags);

    /// Get current access mode.
    EAccessMode GetAccessMode(void) const { return m_Mode; }
    /// Set new access mode, re-open the database.
    void SetAccessMode(EAccessMode mode);

    typedef set<string> TStringSet;

    /// Get all known file names
    void GetFileNames(TStringSet& files) const;
    /// Get complete file info
    SLDS2_File GetFileInfo(const string& file_name) const;
    /// Add new file record. On success file_info.id is not zero.
    void AddFile(SLDS2_File& info);
    /// Update info for the known file. The 'id' of the info will change.
    void UpdateFile(SLDS2_File& info);
    /// Delete file and all related entries from the database
    void DeleteFile(const string& file_name);
    void DeleteFile(Int8 file_id);

    /// Add blob, return the new blob id.
    Int8 AddBlob(Int8                   file_id,
                 SLDS2_Blob::EBlobType  blob_type,
                 Int8                   file_pos);

    typedef set<CSeq_id_Handle> TSeqIdSet;

    /// Add bioseq, return the new bioseq id.
    Int8 AddBioseq(Int8 blob_id, const TSeqIdSet& ids);

    /// Add annotation, return the new annot id.
    Int8 AddAnnot(SLDS2_Annot& annot);

    /// Check if the db contains a bioseq with the given id.
    /// Return -1 on conflict.
    Int8 GetBioseqId(const CSeq_id_Handle& idh) const;

    /// List of ids (blob_id, bioseq_id, lds_id etc.)
    typedef set<Int8> TLdsIdSet;
    /// List of seq-ids.
    typedef vector<CSeq_id_Handle> TSeqIds;

    /// Get all lds-id synonyms for the seq-id (including lds-id
    /// for the seq-id itself). Return empty set if there is a
    /// conflict.
    void GetSynonyms(const CSeq_id_Handle& idh, TLdsIdSet& ids);

    /// Get all synonyms for the seq-id (including the original seq-id).
    /// Return empty set on conflict.
    void GetSynonyms(const CSeq_id_Handle& idh, TSeqIds& ids);

    /// Find blob containing the requested bioseq. Return empty info if
    /// the seq-id is unknown or there are multiple bioseqs with the same id.
    SLDS2_Blob GetBlobInfo(const CSeq_id_Handle& idh);

    /// Get blob info by blob id
    SLDS2_Blob GetBlobInfo(Int8 blob_id);

    /// Get file info.
    SLDS2_File GetFileInfo(Int8 file_id);

    /// A set of ids (file_id, blob_id etc.).
    typedef vector<SLDS2_Blob> TBlobSet;

    /// Get all blobs, containing bioseqs with the seq-id.
    void GetBioseqBlobs(const CSeq_id_Handle& idh, TBlobSet& blobs);

    /// Annotation type flags
    enum EAnnotChoice {
        fAnnot_Internal = 1, ///< Annots from the blob with the bioseq
        fAnnot_External = 2, ///< Annots from blobs not containing the bioseq

        fAnnot_All = fAnnot_Internal | fAnnot_External
    };
    typedef int TAnnotChoice;

    /// Get all blobs, containing annotations for the seq-id.
    void GetAnnotBlobs(const CSeq_id_Handle& idh,
                       TAnnotChoice          choice,
                       TBlobSet&             blobs);

    /// Get number of annotations grouped into a single blob.
    Int8 GetAnnotCountForBlob(Int8 blob_id);

    typedef vector< AutoPtr<SLDS2_Annot> > TLDS2Annots;

    /// Get details about all annotations from a blob.
    void GetAnnots(Int8 blob_id, TLDS2Annots& infos);

    /// Store the chunk info in the database.
    void AddChunk(const SLDS2_File&  file_info,
                  const SLDS2_Chunk& chunk_info);

    /// Load chunk containing the required stream position.
    /// Return true on success.
    bool FindChunk(const SLDS2_File& file_info,
                   SLDS2_Chunk&      chunk_info,
                   Int8              stream_pos);

    /// Get seq-id for the given lds-id.
    CRef<CSeq_id> GetSeq_idForLdsSeqId(int lds_id);

    /// Prepare to update the DB. Drop most indexes.
    /// To rebuild the indexes call Analyze.
    void PrepareUpdate(void);

    /// Start update transaction.
    void BeginUpdate(void);

    /// End update transaction, commit the changes.
    void EndUpdate(void);

    /// Cancel the update, rollback the changes.
    void CancelUpdate(void);

    /// Start reading transaction, lock the db.
    void BeginRead(void);

    /// End reading transaction, release the lock.
    void EndRead(void);

    /// Analyze the DB.
    void Analyze(void);

    /// Dump the selected table (use empty string to dump table names
    /// or * to dump all tables.
    void Dump(const string& table, CNcbiOstream& out);

private:
    CLDS2_Database(const CLDS2_Database&);
    CLDS2_Database& operator=(const CLDS2_Database&);

    // Execute multiple sql queries.
    void x_ExecuteSqls(const char* sqls[], size_t len);
    // Initialize 'get bioseqs' sql statement for the id handle.
    CSQLITE_Statement& x_InitGetBioseqsSql(const CSeq_id_Handle& idh) const;

    // Return lds-id for the seq-id. Adds new lds-id if necessary.
    Int8 x_GetLdsSeqId(const CSeq_id_Handle& id);

    // Load seq-id from the blob.
    CRef<CSeq_id> x_BlobToSeq_id(CSQLITE_Statement& st,
                                 int size_idx,
                                 int data_idx) const;
    // Prepared statements
    enum EStatement {
        eSt_GetFileNames = 0,
        eSt_GetFileInfoByName,
        eSt_GetFileInfoById,
        eSt_GetLdsSeqIdForIntId,
        eSt_GetLdsSeqIdForTxtId,
        eSt_GetBioseqIdForIntId,
        eSt_GetBioseqIdForTxtId,
        eSt_GetSynonyms,
        eSt_GetBlobInfo,
        eSt_GetBioseqForIntId,
        eSt_GetBioseqForTxtId,
        eSt_GetAnnotBlobsByIntId,
        eSt_GetAnnotBlobsAllByIntId,
        eSt_GetAnnotBlobsByTxtId,
        eSt_GetAnnotBlobsAllByTxtId,
        eSt_GetAnnotCountForBlob,
        eSt_GetAnnotInfosForBlob,
        eSt_AddFile,
        eSt_AddLdsSeqId,
        eSt_AddBlob,
        eSt_AddBioseqToBlob,
        eSt_AddBioseqIds,
        eSt_AddAnnotToBlob,
        eSt_AddAnnotIds,
        eSt_DeleteFileByName,
        eSt_DeleteFileById,
        eSt_AddChunk,
        eSt_FindChunk,
        eSt_GetSeq_idForLdsSeqId,
        eSt_GetSeq_idSynonyms,
        eSt_StatementsCount
    };
    typedef vector< AutoPtr<CSQLITE_Statement> > TStatements;

    // Structure to hold per-thread connection and all statements.
    struct SLDS2_DbConnection {
        auto_ptr<CSQLITE_Connection> Connection;
        TStatements                  Statements;

        SLDS2_DbConnection(void);
    };
    typedef CTls<SLDS2_DbConnection> TDbConnectionsTls;

    // TLS cleanup function.
    static void sx_DbConn_Cleanup(SLDS2_DbConnection* value,
                                  void* cleanup_data);

    // Get SLDS2_DbConnection for the current thread (create one
    // if necessary).
    SLDS2_DbConnection& x_GetDbConnection(void) const;
    // Access database connection for the current thread.
    CSQLITE_Connection& x_GetConn(void) const;
    // Reset connection and clear statements cache.
    void x_ResetDbConnection(void);
    // Get the requested statement, prepare it if necessary.
    CSQLITE_Statement& x_GetStatement(EStatement st) const;

    string                          m_DbFile;
    int                             m_DbFlags;
    // Connections and prepared statements are per-thread.
    mutable CRef<TDbConnectionsTls> m_DbConn;
    EAccessMode                     m_Mode;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // LDS2_DB_HPP__

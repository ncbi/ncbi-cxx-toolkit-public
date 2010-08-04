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
        eSeq_align          = 7
    };

    Int8        id;
    EBlobType   type;
    Int8        file_id;
    Int8        file_pos;

    SLDS2_Blob(void) : id(0), type(eUnknown), file_id(0), file_pos(-1) {}
};


struct SLDS2_Annot {
    /// Annotation type
    enum EAnnotType {
        eUnknown        = 0,
        eSeq_feat       = 1,
        eSeq_align      = 2,
        eSeq_graph      = 3
    };

    Int8        id;
    EAnnotType  type;
    Int8        blob_id;
    bool        external;

    SLDS2_Annot(void) : id(0), type(eUnknown), blob_id(0), external(false) {}
};


class NCBI_LDS2_EXPORT CLDS2_Database : public CObject
{
public:
    CLDS2_Database(const string& db_file);

    ~CLDS2_Database(void);

    /// Create the database. If the LDS2 database already exists all data will
    /// be cleaned up.
    void Create(void);

    /// Open LDS2 database. If the database does not exist, throws exception.
    void Open(void);

    /// Get database file name.
    const string& GetDbFile(void) const { return m_DbFile; }

    /// Get/set SQLite flags, see CSQLITE_Connection::TOperationFlags
    int GetSQLiteFlags(void) const { return m_DbFlags; }
    void SetSQLiteFlags(int flags);

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

    /// Seq-id with 'external annotation' flag.
    typedef pair<CSeq_id_Handle, bool> TAnnotRef;
    typedef vector<TAnnotRef>          TAnnotRefSet;
    /// Add annotation, return the new annot id.
    Int8 AddAnnot(Int8                      blob_id,
                  SLDS2_Annot::EAnnotType   annot_type,
                  const TAnnotRefSet&       refs);

    /// Check if the db contains a bioseq with the given id.
    /// Return -1 on conflict.
    Int8 GetBioseqId(const CSeq_id_Handle& idh) const;

    /// List of ids (blob_id, bioseq_id, lds_id etc.)
    typedef set<Int8> TLdsIdSet;

    /// Get all lds-id synonyms for the seq-id (including lds-id
    /// for the seq-id itself). Return empty set if there is a
    /// conflict.
    void GetSynonyms(const CSeq_id_Handle& idh, TLdsIdSet& ids);

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

    /// Analyze the database indices. This can make queries about
    /// 10-100 times faster. The method should be called after any
    /// updates.
    void Analyze(void);

private:
    CLDS2_Database(const CLDS2_Database&);
    CLDS2_Database& operator=(const CLDS2_Database&);

    CSQLITE_Connection& x_GetConn(void) const;

    // Return lds-id for the seq-id. Adds new lds-id if necessary.
    Int8 x_GetLdsSeqId(const CSeq_id_Handle& id);

    // Find and remove any unused seq-ids (and corresponding lds-ids).
    void x_PurgeIds(void);

    void x_InitGetBioseqsSql(const CSeq_id_Handle& idh,
                             CSQLITE_Statement&    st) const;

    string                               m_DbFile;
    int                                  m_DbFlags;
    mutable auto_ptr<CSQLITE_Connection> m_Conn;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // LDS2_DB_HPP__

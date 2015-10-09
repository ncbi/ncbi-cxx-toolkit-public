#ifndef OBJTOOLS_WRITERS_WRITEDB__WRITEDB_VOLUME_HPP
#define OBJTOOLS_WRITERS_WRITEDB__WRITEDB_VOLUME_HPP

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
 * Author:  Kevin Bealer
 *
 */

/// @file writedb_volume.hpp
/// Code for database volume construction.
///
/// Defines classes:
///     CWriteDBVolume
///
/// Implemented for: UNIX, MS-Windows

#include <objtools/blast/seqdb_writer/writedb.hpp>
#include <objects/seq/seq__.hpp>
#include <objtools/blast/seqdb_writer/writedb_files.hpp>
#include <objtools/blast/seqdb_writer/writedb_isam.hpp>
#include "writedb_column.hpp"

BEGIN_NCBI_SCOPE

/// Import definitions from the objects namespace.
USING_SCOPE(objects);

/// CWriteDB_GiIndex class
///
/// This class creates OID->GI lookup file
class CWriteDB_GiIndex : public CWriteDB_File {
public:
    CWriteDB_GiIndex(const string & dbname,
                     bool           protein,
                     int            index,
                     Uint8          max_fsize)
    : CWriteDB_File  (dbname, (protein ? "pog" : "nog"), index, max_fsize, false){ }

    ~CWriteDB_GiIndex() { };

    void AddGi(TGi gi) {
        m_Gi.push_back(gi);
    }

private:
    void x_Flush() {

        Int4 nGi = m_Gi.size();

        if (! nGi) return;

        Create();
        WriteInt4(kVersion);
        WriteInt4(kFileType);
        WriteInt4(kGiSize);
        WriteInt4(nGi);

        for (Int4 i=0; i<4; i++) {
            WriteInt4(0);
        }

        for (Int4 i=0; i<nGi; i++) {
            WriteInt4(GI_TO(Int4, m_Gi[i]));
        }

        vector<TGi> tmp;
        m_Gi.swap(tmp);
    }

    static const int kVersion = 1;
    static const int kFileType = 0;
    static const int kGiSize = 4;
    vector<TGi> m_Gi;
};


/// CWriteDB_Volume class
///
/// This manufactures a blast database volume from sequences.

class CWriteDB_Volume : public CObject {
public:
    /// Whether and what kind of indices to build.
    typedef CWriteDB::EIndexType EIndexType;

    /// Type used for lists of identifiers.
    typedef vector< CRef<CSeq_id> > TIdList;

    /// Type used for lists of identifiers.
    typedef vector< CRef<CBlastDbBlob> > TBlobList;

    // Setup and control

    /// Build a database volume.
    ///
    /// @param dbname Base name of the database, such as 'nr'.
    /// @param protein True if the database is a protein database.
    /// @param title Title of the database.
    /// @param date Creation date of the database.
    /// @param index Volume index (for filename).
    /// @param max_file_size Maximum file size for this volume.
    /// @param max_letters Maximum number of letters for this volume.
    /// @param indices Type of indices to build.
    CWriteDB_Volume(const string     & dbname,
                    bool               protein,
                    const string     & title,
                    const string     & date,
                    int                index,
                    Uint8              max_file_size,
                    Uint8              max_letters,
                    EIndexType         indices);

    /// Destructor.
    ///
    /// The Close() method will be called if it has not already been.
    ~CWriteDB_Volume();

    /// Add a sequence to this volume.
    ///
    /// The provided data represents all information for one
    /// non-redundant sequence that will be added to this volume.
    ///
    /// @param seq Sequence data in format ncbi2na or ncbistdaa.
    /// @param ambig Ambiguities (for protein this should be empty).
    /// @param binhdr Binary headers (blast deflines in binary ASN.1).
    /// @param ids List of identifiers for ISAM construction.
    /// @param pig PIG protein identifier (zero if not available.)
    /// @param hash Sequence Hash (zero if not available.)
    bool WriteSequence(const string    & seq,
                       const string    & ambig,
                       const string    & binhdr,
                       const TIdList   & ids,
                       int               pig,
                       int               hash,
                       const TBlobList & blobs,
                       int               maskcol_id=-1);

    /// Rename all volumes files to single-volume names.
    ///
    /// When volume component files are generated by WriteDB, the
    /// volume names include a volume index.  This method renames the
    /// generated files for this volume to names that do not include
    /// the volume index.  This method should not be called until the
    /// volume is Close()s.
    void RenameSingle();

    /// Close the volume.
    ///
    /// This method finalizes and closes all files associated with
    /// this volume.  (This is not a trivial operation, because ISAM
    /// indices and the index file (pin or nin) cannot be written
    /// until all of the data has been seen.)
    void Close();

    /// Get the name of the volume.
    ///
    /// The volume name includes the path and version (if a version is
    /// used) but does not include the extension.  It is the name that
    /// would be provided to SeqDB to open this volume.  This method
    /// should be called after RenameSingle() if that method is going
    /// to be called.
    ///
    /// @return The volume name.
    const string & GetVolumeName() const
    {
        return m_VolName;
    }

    /// Get the current OID of the volume.
    ///
    /// The current OID is needed for generating BL_ORD_ID.
    ///
    /// @return the OID
    const int & GetOID() const
    {
        return m_OID;
    }

    /// List all files associated with this volume.
    /// @param files The filenames will be appended to this vector.
    void ListFiles(vector<string> & files) const;

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    /// Type used for database column meta-data.
    typedef CWriteDB_Column::TColumnMeta TColumnMeta;

    /// Create a new database column.
    ///
    /// @param title The title of the new column.
    /// @param meta Metadata to store in the new column.
    /// @param max_sz max file size.
    /// @return The numeric column ID.
    int CreateColumn(const string      & title,
                     const TColumnMeta & meta,
                     Uint8               max_sz,
                     bool                mbo = true);

    /// Add meta data to a column.
    ///
    /// In addition to normal blob data, database columns can store a
    /// `dictionary' of user-defined metadata in key/value form.  This
    /// method adds one such key/value pair to the column.  Specifying
    /// a key a second time causes replacement of the previous value.
    /// Using this mechanism to store large amounts of data may have a
    /// negative impact on performance.
    ///
    /// @param col_id Specifies the column to add this metadata to.
    /// @param key    A unique key string.
    /// @param value  A value string.
    void AddColumnMetaData(int            col_id,
                           const string & key,
                           const string & value);
#endif

private:
    // Configuration.

    string           m_DbName;      ///< Base name of the database.
    string           m_VolName;     ///< Database name plus version (if used).
    bool             m_Protein;     ///< True for protein; false for nucleotide.
    string           m_Title;       ///< Database title (same for all volumes).
    string           m_Date;        ///< Construct time (same for all volumes).
    int              m_Index;       ///< Index of this volume (1 based).
    EIndexType       m_Indices;     ///< Indices are sparse, full, or disabled.
    Uint8            m_MaxFileSize; ///< Maximum size for any component file.

    // Status.

    int  m_OID;  ///< Next assigned OID.
    bool m_Open; ///< True if user can still append sequences.

    // Components

    CRef<CWriteDB_IndexFile>    m_Idx; ///< Index file (pin / nin).
    CRef<CWriteDB_HeaderFile>   m_Hdr; ///< Header file (phr / nhr).
    CRef<CWriteDB_SequenceFile> m_Seq; ///< Sequence file (psq / nsq).

    CRef<CWriteDB_Isam> m_AccIsam;   ///< Accession index (psi+psd / nsi+nsd).
    CRef<CWriteDB_Isam> m_GiIsam;    ///< GI index (pni+pnd / nni+nnd).
    CRef<CWriteDB_Isam> m_PigIsam;   ///< PIG index (ppi+ppd, protein only).
    CRef<CWriteDB_Isam> m_TraceIsam; ///< Trace ID index (pti+ptd or nti+ntd).
    CRef<CWriteDB_Isam> m_HashIsam;  ///< Hash index (phi+phd or nhi+nhd).
    CRef<CWriteDB_GiIndex> m_GiIndex;///< OID->GI lookup (pgx or ngx).

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    /// Database columns.
    vector< CRef<CWriteDB_Column> > m_Columns;
#endif

    /// Included Seq_ids
    set<string> m_IdSet;

    // Functions

    /// Compute base-length of compressed nucleotide sequence.
    ///
    /// Nucleotide sequences stored on disk are packed 4 bases to a byte,
    /// except for the last byte.  That byte has 0-3 bases of real sequence
    /// data plus a 'remainder' value (from 0-3) that indicates how many of
    /// the bases of the last byte are sequence data.  This method finds the
    /// exact length in bases for a nucleotide sequence packed in this way.
    ///
    /// @param seq Ncbi2na sequence with length remainder encoding.
    /// @return Length in bases of actual sequence data in this sequence.
    int x_FindNuclLength(const string & seq);
};

END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS_WRITEDB__WRITEDB_VOLUME_HPP


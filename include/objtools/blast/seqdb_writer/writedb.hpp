#ifndef OBJTOOLS_BLAST_SEQDB_WRITER___WRITEDB__HPP
#define OBJTOOLS_BLAST_SEQDB_WRITER___WRITEDB__HPP

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

/// @file writedb.hpp
/// Defines BLAST database construction classes.
///
/// Defines classes:
///     CWriteDB
///
/// Implemented for: UNIX, MS-Windows

#include <objtools/blast/seqdb_writer/writedb_error.hpp>
#include <objtools/blast/seqdb_reader/seqdbblob.hpp>
#include <objects/blastdb/Blast_def_line.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/blastdb/defline_extra.hpp>
#include <objects/seq/seq__.hpp>

#include <objmgr/bioseq_handle.hpp>


BEGIN_NCBI_SCOPE

/// Include definitions from the objects namespace.
USING_SCOPE(objects);

/// Forward definition for PIMPL idiom.
class CWriteDB_Impl;

/// This represents a set of masks for a given sequence. It is represented as a
/// vector because there can be multiple types of filtering applied to a single
/// sequence (e.g.: DUST, WINDOWMASKER, REPEATS, etc).
/// The type of masking data produced in IMaskDataSource
class NCBI_XOBJWRITE_EXPORT CMaskedRangesVector: public vector<SBlastDbMaskData>
{
public:
    /// Our parent class
    typedef vector<SBlastDbMaskData> TParent;

    /// Redefine empty to mean no elements or none of its elements being empty
    bool empty() const {
        ITERATE(TParent, itr, *this) {
            if ( !itr->empty() ) {
                return false;
            }
        }
        return true;
    }
};


/// CWriteDB
///
/// User interface class for blast databases.
///
/// This class provides the top-level interface class for BLAST
/// database users.  It defines access to the database component by
/// calling methods on objects which represent the various database
/// files, such as the index, header, sequence, and alias files.

class NCBI_XOBJWRITE_EXPORT CWriteDB : public CObject
{
public:
    /// Sequence types.
    enum ESeqType {
        /// Protein database.
        eProtein = 0,

        /// Nucleotide database.
        eNucleotide = 1
    };

    /// Whether and what kind of indices to build.
    enum EIndexType {
        /// Build a database without any indices.
        eNoIndex = 0,

        /// Use only simple accessions in the string index.
        eSparseIndex = 0x1,

        /// Use several forms of each Seq-id in the string index.
        eFullIndex = 0x2,

        /// OR this in to add an index for trace IDs.
        eAddTrace = 0x4,

        /// Like eFullIndex but also build a numeric Trace ID index.
        eFullWithTrace = eFullIndex | eAddTrace,

        /// Like eFullIndex but also build a numeric Trace ID index.
        eDefault = eFullIndex | eAddTrace,

        // Specialized ISAMs; these can be ORred into the above.

        /// Add an index from sequence hash to OID.
        eAddHash = 0x100
    };
    typedef int TIndexType; ///< Bitwise OR of "EIndexType"

    //
    // Setup
    //

    /// Constructor
    ///
    /// Starts construction of a blast database.
    ///
    /// @param dbname
    ///   A list of database or alias names, seperated by spaces. [in]
    /// @param seqtype
    ///   Specify eProtein, eNucleotide, or eUnknown. [in]
    /// @param title
    ///   The database title. [in]
    /// @param itype
    ///   Indicates the type of indices to build if specified. [in]
    /// @param parse_ids
    ///   If true, generate ISAM files [in]
    /// @param use_gi_mask
    ///   If true, generate GI-based mask files [in]
    CWriteDB(const string & dbname,
             ESeqType       seqtype,
             const string & title,
             int            itype = eDefault,
             bool           parse_ids = true,
             bool           use_gi_mask = false);

    /// Destructor.
    ///
    /// This will return resources acquired by this object, and call Close()
    /// if it has not already been called.
    ~CWriteDB();

    //
    // Adding data
    //

    // Each new sequence is started when the client calls one of the
    // AddSequence() methods.  This can optionally be followed by one
    // or more calls to Set...() methods or AddDefline(), to add or
    // change other data.  The accumulated data for the sequence is
    // combined and written when the sequence after it is started
    // (with another AddSequence() call), or when Close() is called.

    /// Add a sequence as a CBioseq.
    ///
    /// This adds the sequence data in the specified CBioseq to the
    /// database.  If the CBioseq contains deflines, they will also be
    /// used unless there is a call to SetDeflines() or AddDefline().
    /// Note that the CBioseq will be held by CWriteDB at least until
    /// the next sequence is provided.  If this method is used, the
    /// CBioseq is expected to contain sequence data accessible via
    /// GetInst().GetSeq_data().  If this might not be true, it may be
    /// better to use the version of this function that also takes a
    /// CSeqVector.
    ///
    /// @param bs The sequence and related data as a CBioseq. [in]
    void AddSequence(const CBioseq & bs);

    /// Add a sequence as a CBioseq.
    ///
    /// This adds the sequence data in the specified CSeqVector, and
    /// the meta data in the specified CBioseq, to the database.  If
    /// the CBioseq contains deflines, they will also be used unless
    /// there is a call to SetDeflines() or AddDefline().  Note that
    /// the CBioseq will be held by CWriteDB at least until the next
    /// sequence is provided.  This version will use the CSeqVector if
    /// the sequence data is not found in the CBioseq.
    ///
    /// @param bs A CBioseq containing meta data for the sequence. [in]
    /// @param sv The sequence data for the sequence. [in]
    void AddSequence(const CBioseq & bs, CSeqVector & sv);

    /// Add a sequence as a CBioseq.
    ///
    /// This adds the sequence found in the given CBioseq_Handle to
    /// the database.
    ///
    /// @param bsh The sequence and related data as a CBioseq_Handle. [in]
    void AddSequence(const CBioseq_Handle & bsh);

    /// Add a sequence as raw data.
    ///
    /// This adds a sequence provided as raw sequence data.  The raw
    /// data must be (and is assumed to be) encoded correctly for the
    /// format of database being produced.  For protein databases, the
    /// ambiguities string should be empty (and is thus optional).  If
    /// this version of AddSequence() is used, the user must also
    /// provide one or more deflines with SetDeflines() or
    /// AddDefline() calls.
    ///
    /// @param sequence The sequence data as a string of bytes. [in]
    /// @param ambiguities The ambiguity data as a string of bytes. [in]
    void AddSequence(const CTempString & sequence,
                     const CTempString & ambiguities = "");

    /// Set the PIG to be used for the sequence.
    ///
    /// For proteins, this sets the PIG of the protein sequence.
    ///
    /// @param pig PIG identifier as an integer. [in]
    void SetPig(int pig);

    /// Set the deflines to be used for the sequence.
    ///
    /// This method sets all the deflines at once as a complete set,
    /// overriding any deflines provided by AddSequence().  If this
    /// method is used with the CBioseq version of AddSequence, it
    /// replaces the deflines found in the CBioseq.
    ///
    /// @param deflines Deflines to use for this sequence. [in]
    void SetDeflines(const CBlast_def_line_set & deflines);

    /// Register a type of filtering data found in this database.
    ///
    /// @return algorithm ID for the filtering data.
    /// @param program Program used to produce this masking data. [in]
    /// @param options Algorithm options provided to the program. [in]
    /// @param name Name of the GI-based mask. [in]
    int RegisterMaskAlgorithm(EBlast_filter_program program,
                              const string & options = string(),
                              const string & name = string());

    /// Register a type of filtering data found in this database.
    ///
    /// @return algorithm ID for the filtering data.
    /// @param id A string to identify the masking data. [in]
    /// @param description Details about the masking data. [in]
    /// @param options Algorithm options provided to the program. [in]
    int RegisterMaskAlgorithm(const string & id,
                              const string & description = string(),
                              const string & options = string());

    /// Set filtering data for a sequence.
    ///
    /// This method specifies filtered regions for this sequence.  A
    /// sequence may have filtering data from one or more algorithms.
    /// For each algorithm_id value specified in ranges, a description
    /// should be added to the database using RegisterMaskAlgorithm().
    /// This must be done before the first call to SetMaskData() that
    /// uses the algorithm id for a non-empty offset range list.
    ///
    /// @param ranges Filtered ranges for this sequence and algorithm.
    /// @param gis GIs associated with this sequence.
//#ifdef NCBI_INT8_GI
#if 0
    void SetMaskData(const CMaskedRangesVector & ranges,
                     const vector<int>         & gis);
#endif
    void SetMaskData(const CMaskedRangesVector & ranges,
                     const vector<TGi>         & gis);

    //
    // Output
    //

    /// List Volumes
    ///
    /// Returns the base names of all volumes constructed by this
    /// class; the returned list may not be complete until Close() has
    /// been called.
    ///
    /// @param vols The set of volumes produced by this class. [out]
    void ListVolumes(vector<string> & vols);

    /// List Filenames
    ///
    /// Returns a list of the files constructed by this class; the
    /// returned list may not be complete until Close() has been
    /// called.
    ///
    /// @param files The set of resolved database path names. [out]
    void ListFiles(vector<string> & files);

    /// Close the Database.
    ///
    /// Flush all data to disk and close any open files.
    void Close();

    //
    // Controls
    //

    // The blast volume format has internal limits for these fields;
    // these are called 'hard limits' here.  If the value specified
    // here exceeds that limit, it will be silently reduced.  Limits
    // are applied simultaneously; creation of a new volume is
    // triggered as soon as any of the limits is reached (unless the
    // current volume is empty).

    /// Set maximum size for output files.
    ///
    /// The provided size is applied as a limit on the size of output
    /// files.  If adding a sequence would cause any output file to
    /// exceed this size, the volume is closed and a new volume is
    /// started (unless the current volume is empty, in which case the
    /// size limit is ignored and a one-sequence volume is created).
    /// The default value is 2^30-1.  There is also a hard limit
    /// required by the database format.
    ///
    /// @param sz Maximum size in bytes of any volume component file. [in]
    void SetMaxFileSize(Uint8 sz);

    /// Set maximum letters for output volumes.
    ///
    /// The provided size is applied as a limit on the size of output
    /// volumes.  If adding a sequence would cause a volume to exceed
    /// this many protein or nucleotide letters (*not* bytes), the
    /// volume is closed and a new volume is started (unless the
    /// volume is currently empty).  There is no default, but there is
    /// a hard limit required by the format definition.  Ambiguity
    /// encoding is not counted toward this limit.
    ///
    /// @param letters Maximum letters to pack in one volume. [in]
    void SetMaxVolumeLetters(Uint8 letters);

    /// Extract Deflines From Bioseq.
    ///
    /// Deflines are extracted from the CBioseq and returned to the
    /// user.  The caller can then modify or inspect the deflines, and
    /// apply them to a sequence with SetDeflines().
    ///
    /// @param bs The bioseq from which to extract a defline set. [in]
    /// @param parse_ids If seqid should be parsed [in]
    /// @return A set of deflines for this CBioseq.
    static CRef<CBlast_def_line_set>
    ExtractBioseqDeflines(const CBioseq & bs, bool parse_ids=true);

    /// Set letters that should not be used in sequences.
    ///
    /// This method specifies letters that should not be used in the
    /// resulting database.  The masked letters are expected to be
    /// specified in an IUPAC (alphabetic) encoding, and will be
    /// replaced by 'X' (for protein) when the sequences are packed.
    /// This method should be called before any sequences are added.
    /// This method only works with protein (the motivating case
    /// cannot happen with nucleotide).
    ///
    /// @param masked Letters to disinclude. [in]
    void SetMaskedLetters(const string & masked);

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    /// Find an existing column.
    ///
    /// This looks for an existing column with the specified title and
    /// returns the column ID if found.
    ///
    /// @param title The column title to look for.
    /// @return The column ID if this title is defined, otherwise -1.
    int FindColumn(const string & title) const;

    /// Set up a user-defined CWriteDB column.
    ///
    /// This method creates a user-defined column associated with this
    /// database.  The column is indexed by OID and contains arbitrary
    /// binary data, which is applied using the SetBlobData method
    /// below.  The `title' parameter identifies the column and must
    /// be unique within this database.  Because tables are accessed
    /// by title, it is not necessary to permanently associate file
    /// extensions with specific purposes or data types.  The return
    /// value of this method is an integer that identifies this column
    /// for the purpose of inserting blob data.  (The number of columns
    /// allowed is currently limited due to the file naming scheme,
    /// but some columns are used for built-in purposes.)
    ///
    /// @param title Name identifying this column.
    /// @return Column identifier (a positive integer).
    int CreateUserColumn(const string & title);

    /// Add meta data to a user-defined column.
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

    /// Add blob data to a user-defined column.
    ///
    /// To add data to a user-defined blob column, call this method,
    /// providing the column handle.  A blob object will be returned;
    /// the user data should be stored in this object.  The data can
    /// be stored any time up to the next call to an `AddSequence'
    /// method (just as with any other per-sequence data) but access
    /// to the returned object after that point results is incorrect
    /// and will have undefined consequences.
    ///
    /// @param column_id Identifier for a user-defined column.
    /// @return Blob data should be written to this object.
    CBlastDbBlob & SetBlobData(int column_id);
#endif

protected:
    /// Implementation object.
    CWriteDB_Impl * m_Impl;
};


/// Binary GI or TI List Builder.
///
/// This class assists in building binary GI or TI lists for use with
/// BLAST databases and associated software.
class NCBI_XOBJWRITE_EXPORT CBinaryListBuilder
{
public:
    /// Type definition of the container that stores the IDs for this class
    typedef vector<Int8> TContainerType;

    /// Standard size_type definition
    typedef TContainerType::size_type size_type;

    /// Identifier types.
    enum EIdType {
        /// Genomic id.
        eGi,

        /// Trace id.
        eTi
    };

    /// Construct a list of a given type.
    CBinaryListBuilder(EIdType id_type);

    /// Write the list to a file.
    /// @param fname Filename of the file to write the object to.
    void Write(const string & fname);

    /// Write the list to a stream
    /// @param stream Stream to write the object to.
    void Write(CNcbiOstream& stream);

    /// Add an identifier to the list.
    void AppendId(const Int8 & id)
    {
        m_Ids.push_back(id);
    }

    /// Add several 4 byte IDs to the list.
    ///
    /// This should take begin and end indicators, such as pointers to
    /// the beginning and end (past the last element) of an array of
    /// integers, or begin() and end() iterators to a compatible STL
    /// collection type such as vector<Int4> or set<int>.
    ///
    /// @param a Iterator to the first included element.
    /// @param b Iterator to element after the last included element.
    template<class T>
    void AppendIdList(const T & a, const T & b)
    {
        for(T c = a; c != b; ++c) {
            Int8 id = *c;
            AppendId(id);
        }
    }

    /// Returns the number of IDs stored in an instance of this class
    size_type Size() const {
        return m_Ids.size();
    }

private:
    /// List of identifiers to use.
    TContainerType m_Ids;

    /// Whether to use GIs or TIs.
    EIdType m_IdType;

    /// Prevent copy construction.
    CBinaryListBuilder(CBinaryListBuilder&);

    /// Prevent copy assignment.
    CBinaryListBuilder& operator=(CBinaryListBuilder &);
};


/// Builder for BlastDb format column files.
///
/// This class supports construction of BlastDb format column files
/// outside of BlastDb volumes.  To build column files as part of a
/// volume, use CWriteDB's column related methods.  This class is an
/// interface to the column file construction functionality, but is
/// intended for data not associated with specific BlastDb volumes.
/// Columns built with CWriteDB::CreateColumn participate in WriteDB's
/// other volume-oriented policies such as volume breaking to enforce
/// file size limits, and compatibility with component file naming
/// conventions for CWriteDB and CSeqDB.

class NCBI_XOBJWRITE_EXPORT CWriteDB_ColumnBuilder : public CObject {
public:
    /// Construct a BlastDb format column.
    ///
    /// The `title' string names this column, and can be used to
    /// uniquely identify it in cases where the file name must be
    /// chosen arbitrarily.  This version chooses file extensions
    /// using a basic pattern (<name>.x?[ab]) designed to not conflict
    /// with columns created by WriteDB as part of a volume.  The
    /// file_id character must be alphanumeric.
    ///
    /// @param title      Internal name of this column.
    /// @param basename   Column filename (minus extension).
    /// @param file_id    Identifier for this column.
    CWriteDB_ColumnBuilder(const string & title,
                           const string & basename,
                           char           file_id = 'a');

    /// Add meta data to the column.
    ///
    /// In addition to normal blob data, database columns can store a
    /// `dictionary' of user-defined metadata in key/value form.  This
    /// method adds one such key/value pair to the column.  Specifying
    /// a key a second time causes replacement of the previous value.
    /// Using this mechanism to store large amounts of data may have a
    /// negative impact on performance.
    ///
    /// @param key   Key string.
    /// @param value Value string.
    void AddMetaData(const string & key, const string & value);

    /// Destructor.
    ~CWriteDB_ColumnBuilder();

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    /// Add a blob to the column.
    ///
    /// The data described by `blob' is added to the column.  If the
    /// blob is empty, no data is stored but the OID is incremented.
    ///
    /// @param blob The blob to add to the column.
    void AddBlob(const CBlastDbBlob & blob);
#endif

    /// Complete and close the column files.
    void Close();

    /// List Filenames
    ///
    /// Returns a list of the files constructed by this class; the
    /// returned list may not be complete until Close() has been
    /// called.
    ///
    /// @param files The list of files created for this column.
    void ListFiles(vector<string> & files) const;

private:
    /// Prevent the copy constructor.
    CWriteDB_ColumnBuilder(const CWriteDB_ColumnBuilder&);

    /// Prevent copy assignment.
    CWriteDB_ColumnBuilder & operator= (CWriteDB_ColumnBuilder&);

    /// Implementation object.
    class CWriteDB_Column * m_Impl;
};

/// Defines the possible filtering types that can be applied to an alias file
enum EAliasFileFilterType {
    eNoAliasFilterType,         ///< Sentinel value
    eGiList,                    ///< Filter a BLAST database via GIs
    eTiList,                    ///< Filter a BLAST database via TIs (Trace IDs)
    eSeqIdList                  ///< Filter a BLAST database via a Seq-id list
};

/**
 * @brief Writes an alias file that restricts a database with a gi list.
 *
 * @param file_name alias file name to create, it will overwrite any existing
 * files of that name.  It can be specified as an absolute path, or a path
 * relative to the current working directory [in]
 * @param db_name database name to restrict.  Can be specified as an absolute path,
 * or a path relative to the target directory or the default directory [in]
 * @param seq_type type of sequences stored in the database [in]
 * @param gi_file_name name of the file containing gis [in]
 * @param title title to use in this alias file [in]
 * @param alias_type Type of alias file to create [in]
 */
NCBI_XOBJWRITE_EXPORT
void CWriteDB_CreateAliasFile(const string& file_name,
                              const string& db_name,
                              CWriteDB::ESeqType seq_type,
                              const string& gi_file_name,
                              const string& title = string(),
                              EAliasFileFilterType alias_type = eGiList);

/**
 * @brief Writes an alias file that aggregates multiple existing BLAST
 * databases.
 *
 * @param file_name alias file name to create, it will overwrite any existing
 * files of that name.  It can be specified as an absolute path, or a path
 * relative to the current working directory [in]
 * @param db_names database names to aggregate.  Can be specified as absolute paths,
 * or paths relative to the target directory or the default directory [in]
 * @param gi_file_name name of the file containing gis [in]
 * @param seq_type type of sequences stored in the database [in]
 * @param title title to use in this alias file [in]
 * @param alias_type Type of alias file to create [in]
 */
NCBI_XOBJWRITE_EXPORT
void CWriteDB_CreateAliasFile(const string& file_name,
                              const vector <string> & db_names,
                              CWriteDB::ESeqType seq_type,
                              const string& gi_file_name,
                              const string& title = string(),
                              EAliasFileFilterType alias_type = eGiList);

/**
 * @brief Writes an alias file that aggregates multiple existing BLAST
 * database volumes. For instance, it can be used to request a top level alias
 * file for a database called wgs composed of 3 volumes, creating wgs.nal,
 * which refers to wgs.00, wgs.01, and wgs.02
 *
 * @param file_name alias file name to create, it will overwrite any existing
 * files of that name [in]
 * @param num_volumes Number of volumes that will be referred to in the alias
 * file [in]
 * @param seq_type type of sequences stored in the database [in]
 * @param title title to use in this alias file [in]
 */
NCBI_XOBJWRITE_EXPORT
void CWriteDB_CreateAliasFile(const string& file_name,
                              unsigned int num_volumes,
                              CWriteDB::ESeqType seq_type,
                              const string& title = string());

NCBI_XOBJWRITE_EXPORT
void CWriteDB_CreateAliasFile(const string& file_name,
                              const vector<string>& db_names,
                              CWriteDB::ESeqType seq_type,
                              const TSeqRange& oid_range,
                              const string& title = string());

/** Consolidate the alias files specified into a group alias file.
 * @param alias_files list of alias file names with extension to
 * consolidate [in]
 * @param delete_source_alias_files if true, the alias files in the alias_files
 * argument are deleted [in]
 * @post a group alias file is written in the current working directory
 * @throws CWriteDBException if no alias files are provided to write group
 * alias file
 */
NCBI_XOBJWRITE_EXPORT
void CWriteDB_ConsolidateAliasFiles(const list<string>& alias_files,
                                    const string& output_directory = kEmptyStr,
                                    bool delete_source_alias_files = false);

/** Consolidate all the alias files in the current working directory.
 * @param delete_source_alias_files if true, the alias files consolidated are
 * deleted [in]
 * @throws CWriteDBException if no alias files can be consolidated
 */
NCBI_XOBJWRITE_EXPORT
void CWriteDB_ConsolidateAliasFiles(bool delete_source_alias_files = false);

END_NCBI_SCOPE

#endif // OBJTOOLS_BLAST_SEQDB_WRITER___WRITEDB__HPP

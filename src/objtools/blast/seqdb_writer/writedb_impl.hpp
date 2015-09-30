#ifndef OBJTOOLS_WRITERS_WRITEDB__WRITEDB_IMPL_HPP
#define OBJTOOLS_WRITERS_WRITEDB__WRITEDB_IMPL_HPP

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

/// @file writedb_impl.hpp
/// Defines implementation class of WriteDB.
///
/// Defines classes:
///     CWriteDBHeader
///
/// Implemented for: UNIX, MS-Windows

#include <objects/seq/seq__.hpp>
#include <objects/blastdb/blastdb__.hpp>
#include <objects/blastdb/defline_extra.hpp>
#include <objtools/blast/seqdb_writer/writedb.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include "writedb_volume.hpp"
#include "writedb_gimask.hpp"
#include "mask_info_registry.hpp"

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE

/// Import definitions from the objects namespace.
USING_SCOPE(objects);

/// CWriteDB_Impl class
///
/// This manufactures blast database header files from input data.

class CWriteDB_Impl {
public:
    /// Whether and what kind of indices to build.
    typedef CWriteDB::EIndexType EIndexType;

    // Setup and control

    /// Constructor.
    /// @param dbname Name of the database to create.
    /// @param protein True for protein, false for nucleotide.
    /// @param title Title string for volumes and alias file.
    /// @param indices Type of indexing to do for string IDs.
    /// @param parse_ids If true generate ISAM files
    /// @param use_gi_mask If true generate GI-based mask files.
    CWriteDB_Impl(const string     & dbname,
                  bool               protein,
                  const string     & title,
                  EIndexType         indices,
                  bool               parse_ids,
                  bool               use_gi_mask);

    /// Destructor.
    ~CWriteDB_Impl();

    /// Close the file and flush any remaining data to disk.
    void Close();

    // Sequence Data

    /// Add a new sequence as raw sequence and ambiguity data.
    ///
    /// A new sequence record is started, and data from any previous
    /// sequence is combined and written to disk.  Each sequence needs
    /// sequence data and header data.  This method takes sequence
    /// data in the form of seperated sequence data and compressed
    /// ambiguities packed in the blast database disk format.  It is
    /// intended for efficiently copying sequences from sources that
    /// provide this format, such as CSeqDBExpert().  If this method
    /// is used for protein data, the ambiguities string should be
    /// empty.  If this method is used, header data must also be
    /// specified with a call to SetDeflines().
    ///
    /// @param sequence Sequence data in blast db disk format.
    /// @param ambiguities Ambiguity data in blast db disk format.
    void AddSequence(const CTempString & sequence,
                     const CTempString & ambiguities);

    /// Add a new sequence as a CBioseq.
    ///
    /// A new sequence record is started, and data from any previous
    /// sequence is combined and written to disk.  Each sequence needs
    /// sequence data and header data.  This method can extract both
    /// from the provided CBioseq.  If other header data is preferred,
    /// SetDeflines() can be called after this method to replace the
    /// header data from the CBioseq.  Note that CBioseqs from some
    /// sources are not guaranteed to contain sequence data; if this
    /// might be the case, consider the versions of AddSequence that
    /// take either CBioseq_Handle or CBioseq and CSeqVector.  In
    /// order to use this method, sequence data should be accessible
    /// from bs.GetInst().GetSeq_data().  (Note: objects provided to
    /// WriteDB will be kept alive until the next AddSequence call.)
    ///
    /// @param bs Bioseq containing sequence and header data.
    void AddSequence(const CBioseq & bs);

    /// Add a new sequence as a CBioseq_Handle.
    ///
    /// A new sequence record is started, and data from any previous
    /// sequence is combined and written to disk.  Each sequence needs
    /// sequence data and header data.  This method can extract both
    /// from the provided CBioseq_Handle.  If other header data is
    /// preferred, SetDeflines() can be called after this method to
    /// replace the header data from the CBioseq.  (Note: objects
    /// provided to WriteDB will be kept alive until the next
    /// AddSequence call.)
    ///
    /// @param bsh Bioseq_Handle for sequence to add.
    void AddSequence(const CBioseq_Handle & bsh);

    /// Add a new sequence as a CBioseq_Handle.
    ///
    /// A new sequence record is started, and data from any previous
    /// sequence is combined and written to disk.  Each sequence needs
    /// sequence data and header data.  This method will extract
    /// header data from the provided CBioseq.  If the CBioseq
    /// contains sequence data, it will be used; otherwise sequence
    /// data will be fetched from the provided CSeqVector.  If other
    /// header data is preferred, SetDeflines() can be called after
    /// this method.  (Note: objects provided to WriteDB will be kept
    /// alive until the next AddSequence call.)
    ///
    /// @param bs Bioseq_Handle for header and sequence data.
    /// @param sv CSeqVector for sequence data.
    void AddSequence(const CBioseq & bs, CSeqVector & sv);

    /// This method replaces any stored header data for the current
    /// sequence with the provided CBlast_def_line_set.  Header data
    /// can be constructed directly by the caller, or extracted from
    /// an existing CBioseq using ExtractBioseqDeflines (see below).
    /// Once it is in the correct form, it can be attached to the
    /// sequence with this method.  (Note: objects provided to WriteDB
    /// will be kept alive until the next AddSequence call.)
    ///
    /// @param deflines Header data for the most recent sequence.
    void SetDeflines(const CBlast_def_line_set & deflines);

    /// Set the PIG identifier of this sequence.
    ///
    /// For protein sequences, this sets the PIG identifier.  PIG ids
    /// are per-sequence, so it will only be attached to the first
    /// defline in the set.
    ///
    /// @param pig PIG identifier as an integer.
    void SetPig(int pig);

    // Options

    /// Set the maximum size for any file in the database.
    ///
    /// This method sets the maximum size for any file in a database
    /// volume.  If adding a sequence would cause any file in the
    /// generated database to exceed this size, the current volume is
    /// ended and a new volume is started.  This is not a strict
    /// limit, inasmuch as it always puts at least one sequence in
    /// each volume regardless of that sequence's size.
    ///
    /// @param sz Maximum file size (in bytes).
    void SetMaxFileSize(Uint8 sz);

    /// Set the maximum letters in one volume.
    ///
    /// This method sets the maximum number of sequence letters per
    /// database volume.  If adding a sequence would cause the volume
    /// to have more than this many letters, the current volume is
    /// ended and a new volume is started.  This is not a strict
    /// limit, inasmuch as it always puts at least one sequence in
    /// each volume regardless of that sequence's size.
    ///
    /// @param sz Maximum sequence letters per volume.
    void SetMaxVolumeLetters(Uint8 sz);

    /// Extract deflines from a CBioseq.
    ///
    /// Given a CBioseq, this method extracts and returns header info
    /// as a defline set.  The deflines will not be applied to the
    /// current sequence unless passed to SetDeflines.  The expected
    /// use of this method is in cases where the caller has a CBioseq
    /// or CBioseq_Handle but wishes to examine and/or change the
    /// deflines before passing them to CWriteDB.  Some elements of
    /// the CBioseq may be shared by the returned defline set, notably
    /// the Seq-ids.
    ///
    /// @param bs Bioseq from which to construct the defline set.
    /// @param parse_ids If we should parse seq_ids.
    /// @return The blast defline set.
    static CRef<CBlast_def_line_set>
    ExtractBioseqDeflines(const CBioseq & bs, bool parse_ids);

    /// Set bases that should not be used in sequences.
    ///
    /// This method specifies nucelotide or protein bases that should
    /// not be used in the resulting database.  The bases in question
    /// will be replaced with N (for nucleotide) or X (for protein).
    /// The input data is expected to be specified in the appropriate
    /// 'alphabetic' encoding (either IUPACAA and IUPACNA).
    ///
    /// @param masked
    void SetMaskedLetters(const string & masked);

    /// List Volumes
    ///
    /// Returns the base names of all volumes constructed by this
    /// class; the returned list may not be complete until Close() has
    /// been called.
    ///
    /// @param vols
    ///   The set of volumes produced by this class.
    void ListVolumes(vector<string> & vols);

    /// List Filenames
    ///
    /// Returns a list of the files constructed by this class; the
    /// returned list may not be complete until Close() has been
    /// called.
    ///
    /// @param files
    ///   The set of resolved database path names.
    void ListFiles(vector<string> & files);

    /// Register a type of filtering data found in this database.
    ///
    /// The BlastDb format supports storage of masking data (lists of
    /// masked ranges) for each database sequence, as well as an
    /// indication of the source (or sources) of this masking data (e.g.:
    /// masking algorithm used to create them).
    /// This method stores a description of one of these masking data
    /// sources in this database, including which basic algorithm was
    /// used, as well as the options passed to that algorithm.  Each
    /// description is associated with a numeric `algorithm id' (return value
    /// of this method), which identifies that data source when adding data
    /// with SetMaskData.
    ///
    /// @return algorithm ID for the filtering data.
    /// @param program Program used to produce this masking data. [in]
    /// @param options Algorithm options provided to the program. [in]
    /// @param name Name of a GI-based mask [in]
    int RegisterMaskAlgorithm(EBlast_filter_program   program,
                             const string           & options,
                             const string           & name = "");

    /// Register a type of filtering data found in this database.
    ///
    /// The BlastDb format supports storage of masking data (lists of
    /// masked ranges) for each database sequence, as well as an
    /// indication of the source (or sources) of this masking data (e.g.:
    /// masking algorithm used to create them).
    /// This method stores a description of one of these masking data
    /// sources in this database, including which basic algorithm was
    /// used, as well as the options passed to that algorithm.  Each
    /// description is associated with a numeric `algorithm id' (return value
    /// of this method), which identifies that data source when adding data
    /// with SetMaskData.
    ///
    /// @return algorithm ID for the filtering data.
    /// @param id A string to identify this masking data. [in]
    /// @param description Details about the masking data. [in]
    /// @param options Algorithm options provided to the program. [in]
    int RegisterMaskAlgorithm(const string          & id,
                             const string           & description,
                             const string           & options);

    /// Set filtering data for a sequence.
    ///
    /// This method specifies filtered regions for the sequence.  Each
    /// sequence can have filtering data from various algorithms.
    ///
    /// @param ranges Filtered ranges for this sequence and algorithm.
    /// @param gis The GIs associated with this sequence
//#ifdef NCBI_INT8_GI
#if 0
    void SetMaskData(const CMaskedRangesVector & ranges,
                     const vector <int>        & gis);
#endif
    void SetMaskData(const CMaskedRangesVector & ranges,
                     const vector <TGi>        & gis);

    /// Set up a generic CWriteDB metadata column.
    ///
    /// This method creates a column with the specified name (title).
    /// The name must be unique among names provided to this database.
    /// An integer column descriptor is returned, which must be used
    /// to identify this column when applying blob data.  This call
    /// will fail with an exception if too many user defined columns
    /// have already been created for this database (this limit is due
    /// to BlastDb file naming conventions).  The title identifies
    /// this column and is also used to access the column with SeqDB.
    ///
    /// @param title   Name identifying this column.
    /// @return Column identifier (a positive integer).
    int CreateColumn(const string & title, bool mbo=false);

    /// Find an existing column.
    ///
    /// This looks for an existing column with the specified title and
    /// returns the column ID if found.
    ///
    /// @param title The column title to look for.
    /// @return The column ID if this column title is already defined.
    int FindColumn(const string & title) const;

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

    /// Get a blob to use for a given column letter.
    ///
    /// To add data for a `blob' type column, this method should be
    /// called to get a reference to a CBlastDbBlob object.  Add the
    /// user-defined blob data to this object.  It is not correct to
    /// call this more than once for the same sequence and column.
    /// Reading, writing, or otherwise using this object after the
    /// current sequence is published is an error and has undefined
    /// consequences.  ('Publishing' of a sequence usually occurs
    /// during the following AddSequence(*) call or during Close().)
    ///
    /// @param col_id Indicates the column receiving the blob data.
    /// @return The user data should be stored in this blob.
    CBlastDbBlob & SetBlobData(int col_id);

private:
    // Configuration

    string        m_Dbname;           ///< Database base name.
    bool          m_Protein;          ///< True if DB is protein.
    string        m_Title;            ///< Title field of database.
    string        m_Date;             ///< Time stamp (for all volumes.)
    Uint8         m_MaxFileSize;      ///< Maximum size of any file.
    Uint8         m_MaxVolumeLetters; ///< Max letters per volume.
    EIndexType    m_Indices;          ///< Indexing mode.
    bool          m_Closed;           ///< True if database has been closed.
    string        m_MaskedLetters;    ///< Masked protein letters (IUPAC).
    string        m_MaskByte;         ///< Byte that replaced masked letters.
    vector<char>  m_MaskLookup;       ///< Is (blast-aa) byte masked?
    int           m_MaskDataColumn;   ///< Column ID for masking data column.
    map<int, int> m_MaskAlgoMap;      ///< Mapping from algo_id to gi-mask id
    bool          m_ParseIDs;         ///< Generate ISAM files
    bool          m_UseGiMask;        ///< Generate GI-based mask files

    /// Column titles.
    vector<string> m_ColumnTitles;

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    /// Per-column metadata.
    typedef CWriteDB_Column::TColumnMeta TColumnMeta;

    /// Meta data for all columns.
    vector< TColumnMeta > m_ColumnMetas;

    /// Gi-based masks
    vector< CRef<CWriteDB_GiMask> > m_GiMasks;
#endif

    // Functions

    /// Flush accumulated sequence data to volume.
    void x_Publish();

    /// Compute name of alias file produced.
    string x_MakeAliasName();

    /// Flush accumulated sequence data to volume.
    void x_MakeAlias();

    /// Clear sequence data from last sequence.
    void x_ResetSequenceData();

    /// Convert and compute final data formats.
    void x_CookData();

    /// Convert header data into usable forms.
    void x_CookHeader();

    /// Collect ids for ISAM files.
    void x_CookIds();

    /// Compute the length of the current sequence.
    int x_ComputeSeqLength();

    /// Convert sequence data into usable forms.
    void x_CookSequence();

    /// Prepare column data to be appended to disk.
    void x_CookColumns();

    /// Replace masked input letters with m_MaskByte value.
    void x_MaskSequence();

    /// Get binary version of deflines from 'user' data in Bioseq.
    ///
    /// Some CBioseq objects (e.g. those from CSeqDB) have an ASN.1
    /// octet array containing a binary ASN.1 version of the blast
    /// defline set for the sequence.  This method looks for that data
    /// and returns it if found.  If not found, it returns an empty
    /// string.
    ///
    /// @param bioseq Bioseq from which to fetch header. [in]
    /// @param binhdr Header data as binary ASN.1. [out]
    static void x_GetBioseqBinaryHeader(const CBioseq & bioseq,
                                        string        & binhdr);

    /// Construct deflines from a CBioseq and other meta-data.
    ///
    /// This method builds deflines from various data found in the
    /// Bioseq, along with other meta data (like the PIG and
    /// membership and linkout lists.)
    ///
    /// @param bioseq Defline data will be built from this. [in]
    /// @param deflines A defline set will be returned here. [out]
    /// @param membits Membership bits for each defline. [in]
    /// @param linkout Linkout bits for each defline. [in]
    /// @param pig PIG to attach to a protein sequence. [in]
    static void
    x_BuildDeflinesFromBioseq(const CBioseq                  & bioseq,
                              CConstRef<CBlast_def_line_set> & deflines,
                              const vector< vector<int> >    & membits,
                              const vector< vector<int> >    & linkout,
                              int                              pig);

    /// Extract a defline set from a binary ASN.1 blob.
    /// @param bin_hdr Binary ASN.1 encoding of defline set. [in]
    /// @param deflines Defline set. [out]
    static void
    x_SetDeflinesFromBinary(const string                   & bin_hdr,
                            CConstRef<CBlast_def_line_set> & deflines);

    /// Extract a defline set from a CFastaReader generated CBioseq.
    ///
    /// CBioseq objects produced by CFastaReader have an internal
    /// 'user' field that contains the original FASTA, which can be
    /// used to build blast deflines.  If the original FASTA deflines
    /// were delimited with control-A characters, then those will be
    /// found here too.  If the caller wishes to accept '>' as an
    /// alternate delimiter, then accept_gt should be specified.
    ///
    /// @param bioseq Bioseq object produced by CFastaReader. [in]
    /// @param deflines Defline set. [out]
    /// @param membits Membership bits for each defline. [in]
    /// @param linkout Linkout bits for each defline. [in]
    /// @param pig PIG to attach to a protein sequence. [in]
    /// @param accept_gt Whether greater-than is a delimiter. [in]
    /// @param parse_ids Whether seq_id should not be parsed. [in]
    static void
    x_GetFastaReaderDeflines(const CBioseq                  & bioseq,
                             CConstRef<CBlast_def_line_set> & deflines,
                             const vector< vector<int> >    & membits,
                             const vector< vector<int> >    & linkout,
                             int                              pig,
                             bool                             accept_gt,
                             bool                             parse_ids);

    /// Returns true if we have unwritten sequence data.
    bool x_HaveSequence() const;

    /// Records that we now have unwritten sequence data.
    void x_SetHaveSequence();

    /// Records that we no longer have unwritten sequence data.
    void x_ClearHaveSequence();

    /// Get deflines from a CBioseq and other meta-data.
    ///
    /// This method extracts binary ASN.1 deflines from a CBioseq if
    /// possible, and otherwise builds deflines from various data
    /// found in the Bioseq, along with other meta data (like the PIG
    /// and membership and linkout lists.)  It returns the result as
    /// a blast defline set.  If a binary version of the headers is
    /// computed during this method, it will be returned in bin_hdr.
    ///
    /// @param bioseq Defline data will be built from this. [in]
    /// @param deflines A defline set will be returned here. [out]
    /// @param bin_hdr Binary header data may be returned here. [out]
    /// @param membbits Membership bits for each defline. [in]
    /// @param linkouts Linkout bits for each defline. [in]
    /// @param pig PIG to attach to a protein sequence. [in]
    /// @param OID the current OID for local id. [in]
    /// @param parse_ids whether we should not parse id. [in]
    static void x_ExtractDeflines(CConstRef<CBioseq>             & bioseq,
                                  CConstRef<CBlast_def_line_set> & deflines,
                                  string                         & bin_hdr,
                                  const vector< vector<int> >    & membbits,
                                  const vector< vector<int> >    & linkouts,
                                  int                              pig,
                                  int                              OID=-1,
                                  bool                             parse_ids=true);

    /// Compute the hash of a (raw) sequence.
    ///
    /// The hash of the provided sequence will be computed and
    /// assigned to the m_Hash member.  The sequence and optional
    /// ambiguities are 'raw', meaning they are packed just as
    /// sequences are packed in nsq and psq files.
    ///
    /// @param sequence The sequence data. [in]
    /// @param ambiguities Nucleotide ambiguities are provided here. [in]
    void x_ComputeHash(const CTempString & sequence,
                       const CTempString & ambiguities);

    /// Compute the hash of a (Bioseq) sequence.
    ///
    /// The hash of the provided sequence will be computed and
    /// assigned to the m_Hash member.  The sequence is packed as a
    /// CBioseq.
    ///
    /// @param sequence The sequence as a CBioseq. [in]
    void x_ComputeHash(const CBioseq & sequence);

    /// Get the mask data column id.
    ///
    /// The mask data column is created if it does not exist, and its
    /// column ID number is returned.
    ///
    /// @return The column ID for the mask data column.
    int x_GetMaskDataColumnId();

    //
    // Accumulated sequence data.
    //

    /// Bioseq object for next sequence to write.
    CConstRef<CBioseq> m_Bioseq;

    /// SeqVector for next sequence to write.
    CSeqVector m_SeqVector;

    /// Deflines to write as header.
    CConstRef<CBlast_def_line_set> m_Deflines;

    /// Ids for next sequence to write, for use during ISAM construction.
    vector< CRef<CSeq_id> > m_Ids;

    /// Linkout bits - outer vector is per-defline, inner is bits.
    vector< vector<int> > m_Linkouts;

    /// Membership bits - outer vector is per-defline, inner is bits.
    vector< vector<int> > m_Memberships;

    /// PIG to attach to headers for protein sequences.
    int m_Pig;

    /// Sequence hash for this sequence.
    int m_Hash;

    /// When a sequence is added, this will be populated with the length of that sequence.
    int m_SeqLength;

    /// True if we have a sequence to write.
    bool m_HaveSequence;

    // Cooked

    /// Sequence data in format that will be written to disk.
    string m_Sequence;

    /// Ambiguities in format that will be written to disk.
    string m_Ambig;

    /// Binary header in format that will be written to disk.
    string m_BinHdr;

    // Volumes

    /// This volume is currently accepting sequences.
    CRef<CWriteDB_Volume> m_Volume;

    /// List of all volumes so far, up to and including m_Volume.
    vector< CRef<CWriteDB_Volume> > m_VolumeList;

    /// Blob data for the current sequence, indexed by letter.
    vector< CRef<CBlastDbBlob> > m_Blobs;

    /// List of blob columns that are active for this sequence.
    vector<int> m_HaveBlob;

    /// Registry for masking algorithms in this database.
    CMaskInfoRegistry m_MaskAlgoRegistry;
};

END_NCBI_SCOPE


#endif // OBJTOOLS_WRITERS_WRITEDB__WRITEDB_IMPL_HPP



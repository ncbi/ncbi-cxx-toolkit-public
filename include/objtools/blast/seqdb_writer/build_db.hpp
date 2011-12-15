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

/** @file
*     Code to build a database given various sources of sequence data.
*/

#ifndef OBJTOOLS_BLAST_SEQDB_WRITER___BUILD_DB__HPP
#define OBJTOOLS_BLAST_SEQDB_WRITER___BUILD_DB__HPP

#include <corelib/ncbistd.hpp>

// Blast databases
#include <objtools/blast/seqdb_reader/seqdbexpert.hpp>
#include <objtools/blast/seqdb_writer/writedb.hpp>

// ObjMgr
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

// Local
#include <objtools/blast/seqdb_writer/taxid_set.hpp>
#include <objtools/blast/seqdb_writer/multisource_util.hpp>

BEGIN_NCBI_SCOPE

/// Interface to a source of Bioseq objects.
class NCBI_XOBJWRITE_EXPORT IBioseqSource : public CObject {
public:
    /// Get a Bioseq object if there are any more to get.
    ///
    /// This method should return Bioseq objects one at a time until
    /// there are no more to get, then it should return NULL.
    ///
    /// @return A Bioseq object, or NULL if there are no more.
    virtual CConstRef<objects::CBioseq> GetNext() = 0;
};

/// Interface to a source of raw sequence data.
///
/// This interface provides raw data, including column data, from a
/// BlastDB-like data source.

class NCBI_XOBJWRITE_EXPORT IRawSequenceSource : public CObject {
public:
    /// Get a raw sequence.
    ///
    /// This method provides information about a sequence, including
    /// the header, zero or more columns, and (raw) sequence data.
    ///
    /// The sequence data is split into the sequence + ambiguities
    /// strings, which should be in the format used by BlastDB
    /// databases.  (If the database type is protein, "ambiguities"
    /// should be an empty string.)
    ///
    /// For each column of the database for which this OID has blob
    /// data, the column index should be found in column_ids, and a
    /// reference to the blob data should be stored at the same index
    /// in the column_blob vector.  If this OID does not have a data
    /// object for this column, either the column may be missing from
    /// the list, or the blob data string should be empty.
    /// 
    /// @param sequence Sequence data packed in BlastDB disk format. [out]
    /// @param ambiguities Ambiguities packed in BlastDB disk format. [out]
    /// @param deflines This OID's headers as a Blast-def-line-set. [out]
    /// @param column_ids Column IDs of this OID's nonempty blobs. [out]
    /// @param column_blobs All non-empty blobs for this OID. [out]
    virtual bool GetNext(CTempString               & sequence,
                         CTempString               & ambiguities,
                         CRef<objects::CBlast_def_line_set> & deflines,
                         vector<SBlastDbMaskData>  & mask_ranges,
                         vector<int>               & column_ids,
                         vector<CTempString>       & column_blobs) = 0;
    
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    /// Get the names of all columns defined by this sequence source.
    /// @param names A list of column names. [out]
    virtual void GetColumnNames(vector<string> & names) = 0;
    
    /// Get the column ID for a column mentioned by name.
    /// @param name The name (column title) of the column. [in]
    /// @return The corresponding Column-ID.
    virtual int GetColumnId(const string & name) = 0;
    
    /// Get metadata for the column with the specified Column ID.
    /// @param id The column-id for which to get meta-data. [in]
    /// @return All meta-data for this column-id.
    virtual const map<string,string> & GetColumnMetaData(int id) = 0;
#endif    
};

/// An interface providing lookups of mask-data by Seq-id.
class NCBI_XOBJWRITE_EXPORT IMaskDataSource : public CObject {
public:
    /// Get ranges of masking data for the given Seq-ids.
    /// @param id Seq-ids for which to get masking data.
    /// @return Masking data for these Seq-ids.
    virtual CMaskedRangesVector & 
        GetRanges(const list< CRef<CSeq_id> > & id) = 0;
};

/// Build BlastDB format databases from various data sources.
/// 
/// This class provides an API for building BlastDB format databases.
/// The WriteDB library is used internally to produce the actual
/// database; the functionality provided by this class helps to bridge
/// the gap between the WriteDB API and the needs of a command line
/// database construction tool.
class NCBI_XOBJWRITE_EXPORT CBuildDatabase : public CObject {
public:
    /// Constructor.
    ///
    /// Create a database with the specified name, type, and other
    /// characteristics.  The database will use the specified dbname
    /// as the base name for database volumes.  Note that the indexing
    /// argument will be combined with either eSparseIndex or eDefault,
    /// depending on the "sparse" flag.
    ///
    /// @param dbname Name of the database to create. [in]
    /// @param title Title to use for newly created database. [in]
    /// @param is_protein Use true for protein, false for nucleotide. [in]
    /// @param sparse Specify true to use sparse Seq-id indexing. [in]
    /// @param Logging will be done to this stream. [in]
    /// @param use_gi_mask if true will generate GI-based mask files [in]
    /// @param logfile file to write the log to [in]
    CBuildDatabase(const string         & dbname,
                   const string         & title,
                   bool                   is_protein,
                   CWriteDB::TIndexType   indexing,
                   bool                   use_gi_mask,
                   ostream              * logfile);
    
    // Note -- should deprecate (or just remove) the following one:
    // - sparse does nothing
    // - parse_seqids is always true
    
    /// Constructor.
    ///
    /// Create a database with the specified name, type, and other
    /// characteristics.  The database will use the specified dbname
    /// as the base name for database volumes.  Note that the indexing
    /// argument will be combined with either eSparseIndex or eDefault,
    /// depending on the "sparse" flag.
    ///
    /// @param dbname Name of the database to create. [in]
    /// @param title Title to use for newly created database. [in]
    /// @param is_protein Use true for protein, false for nucleotide. [in]
    /// @param sparse Specify true to use sparse Seq-id indexing. [in]
    /// @param parse_seqids specify true to parse the sequence IDs [in]
    /// @param use_gi_mask if true will generate GI-based mask files [in]
    /// @param indexing index fields to add to database. [in]
    CBuildDatabase(const string         & dbname,
                   const string         & title,
                   bool                   is_protein,
                   bool                   sparse,
                   bool                   parse_seqids,
                   bool                   use_gi_mask,
                   ostream              * logfile);

    ~CBuildDatabase();
    
    /// Specify a mapping of sequence ids to taxonomic ids.
    ///
    /// When adding sequences CBuildDatabase will use the object
    /// provided here to find TaxIDs for sequences it adds to the
    /// newly created database.
    ///
    /// @param taxids An object providing defline-to-TaxID lookups. [in]
    void SetTaxids(CTaxIdSet & taxids);
    
    /// Specify letters to mask out of protein sequence data.
    ///
    /// Protein sequences sometimes contain rare (or recently defined)
    /// letters that cause trouble for some algorithms.  This method
    /// specifies a list of protein letters that might be found in the
    /// input sequences, but which should be replaced by "X" before
    /// adding those sequences to the database.
    ///
    /// @param taxids An object providing defline-to-TaxID lookups. [in]
    void SetMaskLetters(const string & mask_letters);
    
    /// Specify source database(s) via the database name(s).
    ///
    /// The provided name will be used to find a source database (or
    /// several) to look up sequence information for the list of
    /// sequences specified by AddIds().
    ///
    /// @param src_db_name Database name of the source database. [in]
    void SetSourceDb(const string & src_db_name);
    
    /// Specify source database.
    ///
    /// The provided source database will be used to look up sequence
    /// information for the list of sequences specified by AddIds().
    ///
    /// @param src_db The source database. [in]
    void SetSourceDb(CRef<CSeqDBExpert> src_db);
    
    /// Specify a linkout bit lookup object.
    ///
    /// The provided mapping will be used to look up linkout bits for
    /// sequences added to the database.
    ///
    /// @param src_db The source database. [in]
    void SetLinkouts(const TLinkoutMap & linkouts,
                     bool                keep_links);
    
    /// Specify a membership bit lookup object.
    ///
    /// The provided mapping will be used to look up membership bit
    /// data for sequences added to the database.
    ///
    /// @param src_db The source database. [in]
    void SetMembBits(const TLinkoutMap & membbits,
                     bool                keep_mbits);
    
    /// Build the database.
    ///
    /// This method builds a database from the given list of Sequence
    /// IDs and the provided file, which should contain FASTA format
    /// data.  It is equivalent to calling StartBuild(), AddIds(),
    /// AddFasta(), and EndBuild() in that order (except that a little
    /// additional logging is done with summary information.).
    ///
    /// @param ids List of identifiers to add to the database.
    /// @param fasta_file FASTA format data for 
    bool Build(const vector<string> & ids,
               CNcbiIstream         * fasta_file);
    
    /// Start building a new database.
    ///
    /// This method sets up a new database to begin receiving
    /// sequences.  It should be called before AddIds, AddFasta,
    /// AddSequences, or AddRawSequences is called.
    void StartBuild();
    
    /// Add the specified sequences from the source database.
    ///
    /// The list of strings are interpreted as GIs if they're composed
    /// only of numeric digits, or as Seq-ids otherwise.  The sequence
    /// IDs will be resolved, and a sequence corresponding to each ID
    /// will be added to the output database.  If remote resolution is
    /// enabled, it will be used to find up-to-date versions for any
    /// ambiguously versioned IDs (i.e. unversioned IDs of versioned
    /// Seq-id types).  Then local fetching will be used to process
    /// IDs using the source database if one was specified.  If any
    /// sequences have not be found, and remote services are enabled,
    /// remote fetching will be used for IDs not resolved locally.  If
    /// any IDs are not found at all, they will be reported as part of
    /// the logging output.
    ///
    /// @param ids List of sequence IDs as strings.
    /// @return true if all sequences were found locally or remotely.
    bool AddIds(const vector<string> & ids);
    
    /// Add sequences from a file containing FASTA data.
    ///
    /// The provided file is expected to contain FASTA data for one or
    /// more sequences.  The data should be suitable input as required
    /// by CFastaReader.
    ///
    /// @param fasta_file A file containing FASTA data.
    /// @return True if at least one sequence was added.
    bool AddFasta(CNcbiIstream & fasta_file);
    
    /// Add sequences from an IBioseqSource object.
    ///
    /// The provided `src' object is queried using GetNext() to get a
    /// Bioseq object.  The Bioseq is added to the output database
    /// (with appropriate modifications of taxid, membership bits, and
    /// linkout bits, as configured here).  This process repeats until
    /// the GetNext() method returns NULL.
    ///
    /// @param src An object providing one or more Bioseq objects.
    /// @param add_pig true if PIG should be added if available
    /// @return True if at least one sequence was added.
    bool AddSequences(IBioseqSource & src, bool add_pig = false);
    
    /// Add sequences from an IRawSequenceSource object.
    ///
    /// The provided `src' object is queried using GetNext() to get
    /// various "raw format" sequence data and metadata components.
    /// These pieces of data are added to the output database (with
    /// appropriate modifications of taxid, membership bits, and
    /// linkout bits, as configured here).  This process repeats until
    /// the GetNext() method returns false.
    ///
    /// @param src An object providing one or more "raw" sequences.
    /// @return True if at least one sequence was added.
    bool AddSequences(IRawSequenceSource & src);
    
    /// Finish building a new database.
    ///
    /// This method closes the newly constructed database, flushing
    /// any unflushed volumes, creating an alias file to tie the
    /// volumes together, and so on.
    /// @param erase Will erase all files created if true.
    bool EndBuild(bool erase = false);
    
    /// Specify whether to use remote fetching for locally absent IDs.
    ///
    /// If identifiers in the list provided to Build or to AddIds is
    /// not found in the source database (if any), remote sequence
    /// fetching APIs can be used to fetch those sequences.  Normally
    /// this happens in two cases.  First, sequences listed in the
    /// list of IDs are sometimes too new to be found in the source
    /// database.  Secondly, sequences may be found in the source
    /// database, but newer versions might be available in the remote
    /// database.
    ///
    /// If the use_remote flag is set to true, this class finds the
    /// latest version number for unversioned IDs (but only of types
    /// that can have versions in the first place), and will attempt
    /// to remotely fetch any sequences for which the source database
    /// does not have the latest version.  If the flag is specified as
    /// false, no remote lookups will be done, and sequences found in
    /// ids but not found in the source database will not be added to
    /// the output database.
    ///
    /// Note: This does not affect the AddSequences, AddRawSequences,
    /// or AddFasta methods; in those cases, all provided sequences
    /// are added in the form they are provided in.
    ///
    /// The default value for this flag is "true".
    ///
    /// @param use_remote Specify true for remote checking & fetching.
    void SetUseRemote(bool use_remote)
    {
        m_UseRemote = use_remote;
    }
    
    /// Specify level of output verbosity.
    /// @param v Specify true if output should be more detailed.
    void SetVerbosity(bool v)
    {
        m_Verbose = v;
    }
    
    /// Set the maximum size of database component files.
    ///
    /// This will specify the maximum size of file that will be
    /// made as a component of a database volume manufactured by the
    /// WriteDB library.  The default value is 10^9 (one billion
    /// bytes.)
    ///
    /// @param max_file_size Maximum file size in bytes.
    void SetMaxFileSize(Uint8 max_file_size);
    
    /// Define a masking algorithm.
    ///
    /// The returned integer ID will be defined as corresponding to the
    /// provided program enumeration (e.g. DUST, SEG, etc) and options
    /// string, for subject masking.  Each program enumeration (such
    /// as DUST) may be used several times with different options
    /// strings, however, the combination of program and options
    /// should be unique for each algorithm ID.  The options string is
    /// a free-form string (at least from this class's point of view).
    ///
    /// @param program One of the predefined masking types (dust etc). [in]
    /// @param options A free-form string describing this type of data.
    /// The empty string should be used to indicate default parameters. [in]
    /// @param name Name of the GI-base mask file [in]
    int
    RegisterMaskingAlgorithm(EBlast_filter_program program, 
                             const string        & options,
                             const string        & name = "");
    
    /// Specify an object mapping Seq-id to subject masking data.
    ///
    /// Masking data is provided to CBuildDatabase by implementing an
    /// interface that can produce masking data given the Seq-ids for
    /// the sequence that is to be masked.  This object could wrap a
    /// simple lookup table, an algorithm that produces the data on
    /// the fly, or a wrapper around an existing database that fetches
    /// the masking data from that database.
    ///
    /// @param ranges An object mapping Seq-ids to their masking data.
    void SetMaskDataSource(IMaskDataSource & ranges);
    
private:
    /// Get a scope for remote loading of objects.
    objects::CScope & x_GetScope();
    
    /// Duplicate IDs from local databases.
    /// 
    /// This method iterates over the list of IDs, copying sequences
    /// found in the source databases to the output database.
    void x_DupLocal();
    
    /// Resolve an ID remotely.
    ///
    /// This method looks up the given ID via remote services in order
    /// to find an ID for the most up-to-date version of the sequence.
    /// The remote service will return a list of Seq-ids; if at least
    /// one of these is a GI, that will be returned in `gi'.  If no GI
    /// is found, but at least one of the returned IDs is of the same
    /// type as the input Seq-id, the version number of the input
    /// Seq-id will be updated.
    ///
    /// @param seqid Sequence identifier to look up remotely. [in|out]
    /// @param gi Genomic ID if one is found, otherwise 0. [out]
    void x_ResolveRemoteId(CRef<objects::CSeq_id> & seqid, int & gi);
    
    /// Resolve various input IDs (as strings) to GIs.
    ///
    /// The input IDs are examined, the type of each is determined as
    /// a GIs or some other kind of Seq-id, and each ID is resolved to
    /// a GI where possible.  The list of GIs and other Seq-ids found
    /// is returned in a GI list.
    ///
    /// @param ids List of strings representing IDs to resolve.
    /// @return GI list produced from the input ids.
    CRef<CInputGiList> x_ResolveGis(const vector<string> & ids);
    
    /// Modify deflines with linkout and membership bits and taxids.
    ///
    /// The provided deflines are modified: the taxid is set (0 is
    /// used if no taxid is known), and linkout and membership bits
    /// are set.  The input object is modified.
    ///
    /// @param headers Headers to modify.
    void x_EditHeaders(CRef<objects::CBlast_def_line_set> headers);
    
    /// Modify a Bioseq as needed and add it to the database.
    ///
    /// The provided Bioseq is added to the database.  Modifications
    /// are made to the data as needed (but the input object is not
    /// affected).  In particular, the taxid is set (0 is used if no
    /// taxid is known), and linkout and membership bits are set.
    ///
    /// @param bs Bioseq to add to the database.
    /// @param bs Sequence data to add to the database.
    /// @param add_pig true if PIG should be added if available
    void x_EditAndAddBioseq(CConstRef<objects::CBioseq>   bs,
                            objects::CSeqVector         * sv,
                            bool 						  add_pig = false);
    
    /// Add the masks for the Seq-id(s) (usually just one) to the database
    /// being created
    /// @param ids Seq-id(s) of the sequence to which masks should be added [in]
    void x_AddMasksForSeqId(const list< CRef<CSeq_id> >& ids);

    /// Duplicate IDs from local databases.
    /// 
    /// This method iterates over the list of IDs; any IDs that were
    /// not found in the source database are added by fetching the
    /// sequence from remote services.  (Whether an ID was found
    /// locally can be determined by whether the OID found in the GI
    /// list is valid.)
    ///
    /// @param gi_list A list of GIs and Seq-ids.
    /// @return True if all IDs could be added.
    bool x_AddRemoteSequences(CInputGiList & gi_list);
    
    /// Write log messages for any unresolved IDs.
    /// @param gi_list List of GIs and Seq-ids.
    /// @return True if all sequences were resolved.
    bool x_ReportUnresolvedIds(const CInputGiList & gi_list) const;
    
    /// Store linkout and membership bits in provided headers.
    ///
    /// Each Seq-id found in each defline in the provided headers will
    /// be looked up in the set of linkout and membership bits
    /// provided for building this database, and the appropriate bits
    /// will be set for each defline.
    ///
    /// @param headers These deflines will be modified. [in|out]
    void x_SetLinkAndMbit(CRef<objects::CBlast_def_line_set> headers);
    
    /// Fetch a sequence from the remote service and add it to the db.
    ///
    /// The provided Seq-id will be used to fetch a Bioseq remotely,
    /// and this Bioseq will be added to this database.  If 
    ///
    /// @param seqid Identifies the sequence to fetch. [in]
    /// @param found Will be set to true if a sequence was found. [out]
    /// @param error Will be set to true if an error occurred. [out]
    void x_AddOneRemoteSequence(const objects::CSeq_id & seqid,
                                bool          & found,
                                bool          & error);
    
    /// Determine if this string ID can be found in the source database.
    ///
    /// The provided string will be looked up as an accession in the
    /// source database.  If a corresponding sequence is found, it
    /// will be returned in the `id' field.  The resolution is only
    /// considered a match if the provided string is a substring of
    /// the FASTA representation of the provided Seq-id, and if that
    /// substring seems to represent whole components (so that it's
    /// surrounded by delimeters such as `|' and `.' rather than by
    /// alphanumeric characters, which may be part of another ID).
    ///
    /// @param acc The accession or ID to look up. [in]
    /// @param id The returned Seq-id if one is found. [out]
    /// @return true if the resolution was successful.
    bool x_ResolveFromSource(const string & acc, CRef<objects::CSeq_id> & id);
    
    /// True for a protein database, false for nucleotide.
    bool m_IsProtein;
    
    /// True to keep linkout bits from source dbs, false to discard.
    bool m_KeepLinks;
    
    /// Table of linkout bits to apply to sequences.
    TIdToBits m_Id2Links;
    
    /// True to keep membership bits from source dbs, false to discard.
    bool m_KeepMbits;
    
    /// Table of membership bits to apply to sequences.
    TIdToBits m_Id2Mbits;
    
    /// Object manager, used for remote fetching.
    CRef<objects::CObjectManager>  m_ObjMgr;
    
    /// Sequence scope, used for remote fetching.
    CRef<objects::CScope>          m_Scope;
    
    /// Set of TaxIDs configured to apply to sequences.
    CRef<CTaxIdSet>       m_Taxids;
    
    /// Database being produced here.
    CRef<CWriteDB>        m_OutputDb;
    
    /// Database for duplicating sequences locally (-sourcedb option.)
    CRef<CSeqDBExpert>    m_SourceDb;
    
    /// Subject masking data.
    CRef<IMaskDataSource> m_MaskData;
    
    /// Logfile.
    ostream & m_LogFile;
    
    /// Whether to use remote resolution and sequence fetching.
    bool m_UseRemote;
    
    /// Define count.
    int m_DeflineCount;
    
    /// Number of OIDs stored in this database.
    int m_OIDCount;
    
    /// If true, more detailed log messages will be produced.
    bool m_Verbose;
    
    /// If true, string IDs found in FASTA input will be parsed as Seq-ids.
    bool m_ParseIDs;

    /// If true, there were sequences whose IDs matched those in the provided
    /// masking locations (via SetMaskDataSource). Used to display a warning in
    /// case this didn't happen
    bool m_FoundMatchingMasks;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_BLAST_SEQDB_WRITER___BUILD_DB__HPP


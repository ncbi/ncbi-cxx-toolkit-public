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
 * Author:  Jason Papadopoulos
 *
 */

/** @file algo/blast/blastinput/blast_input.hpp
 * Interface for converting sources of sequence data into
 * blast sequence input
 */

#ifndef ALGO_BLAST_BLASTINPUT___BLAST_INPUT__HPP
#define ALGO_BLAST_BLASTINPUT___BLAST_INPUT__HPP

#include <corelib/ncbistd.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Class that centralizes the configuration data for
/// sequences to be converted
///
class NCBI_BLASTINPUT_EXPORT CBlastInputSourceConfig {

public:

    /** This value and the seqlen_thresh2guess argument to this class'
      constructor are related as follows: if the default parameter value is
      used, then no sequence type guessing will occurs, instead the sequence
      type specified in
      CBlastInputSourceConfig::SDataLoader::m_IsLoadingProteins is assumed
      correct. If an alternate value is specified, then any sequences shorter
      than that length will be treated as described above, otherwise those
      sequences will have their sequence type guessed (and be subject to
      validation between what is guessed by CFastaReader and what is expected
      by CBlastInputSource).

      By design, the default setting should be fine for command line BLAST
      search binaries, but on the BLAST web pages we use kSeqLenThreshold2Guess
      to validate sequences longer than that length, and to accept sequences
      shorter than that length.
      
      @sa Implementation in CCustomizedFastaReader
      @sa TestSmallDubiousSequences unit test
    */
    static const unsigned int kSeqLenThreshold2Guess = 25;

    /// Constructor
    /// @param dlconfig Configuration object for the data loaders used in
    /// CBlastScopeSource [in]
    /// @param strand All SeqLoc types will have this strand assigned;
    ///             If set to 'other', the strand will be set to 'unknown'
    ///             for protein sequences and 'both' for nucleotide [in]
    /// @param lowercase If true, lowercase mask locations are generated
    ///                 for all input sequences [in]
    /// @param believe_defline If true, all sequences ID's are parsed;
    ///                 otherwise all sequences receive a local ID set
    ///                 to a monotonically increasing count value [in]
    /// @param retrieve_seq_data When gis/accessions are provided in the input,
    ///                 should the sequence data be fetched by this library?
    /// @param range Range restriction for all sequences (default means no
    ///                 restriction). To support the specification of a single
    ///                 coordinate (start or stop), use the SetRange() method,
    ///                 the missing coordinate will be set the default value 
    ///                 (e.g.: 0 for starting coordinate, sequence length for
    ///                 ending coordinate) [in]
    /// @param seqlen_thresh2guess sequence length threshold for molecule
    ///                 type guessing (see @ref kSeqLenThreshold2Guess) [in]
    /// @param local_id_counter counter used to create the CSeqidGenerator to
    ///                 create local identifiers for sequences read [in]
    CBlastInputSourceConfig(const SDataLoaderConfig& dlconfig,
                  objects::ENa_strand strand = objects::eNa_strand_other,
                  bool lowercase = false,
                  bool believe_defline = false,
                  TSeqRange range = TSeqRange(),
                  bool retrieve_seq_data = true,
                  int local_id_counter = 1,
                  unsigned int seqlen_thresh2guess = 
                    numeric_limits<unsigned int>::max());

    /// Destructor
    ///
    ~CBlastInputSourceConfig() {}

    /// Set the strand to a specified value
    /// @param strand The strand value
    ///
    void SetStrand(objects::ENa_strand strand) { m_Strand = strand; }

    /// Retrieve the current strand value
    /// @return the strand
    objects::ENa_strand GetStrand() const { return m_Strand; }

    /// Turn lowercase masking on/off
    /// @param mask boolean to toggle lowercase masking
    ///
    void SetLowercaseMask(bool mask) { m_LowerCaseMask = mask; }

    /// Retrieve lowercase mask status
    /// @return boolean to toggle lowercase masking
    ///
    bool GetLowercaseMask() const { return m_LowerCaseMask; }

    /// Turn parsing of sequence IDs on/off
    /// @param believe boolean to toggle parsing of seq IDs
    ///
    void SetBelieveDeflines(bool believe) { m_BelieveDeflines = believe; }

    /// Retrieve current sequence ID parsing status
    /// @return boolean to toggle parsing of seq IDs
    ///
    bool GetBelieveDeflines() const { return m_BelieveDeflines; }

    /// Set range for all sequences
    /// @param r range to use [in]
    void SetRange(const TSeqRange& r) { m_Range = r; }
    /// Set range for all sequences
    /// @return range to modify
    TSeqRange& SetRange(void) { return m_Range; }

    /// Get range for all sequences
    /// @return range specified for all sequences
    TSeqRange GetRange() const { return m_Range; }

    /// Retrieve the data loader configuration object for manipulation
    SDataLoaderConfig& SetDataLoaderConfig() { return m_DLConfig; }
    /// Retrieve the data loader configuration object for read-only access
    const SDataLoaderConfig& GetDataLoaderConfig() { return m_DLConfig; }

    /// Determine if this object is for configuring reading protein sequences
    bool IsProteinInput() const { return m_DLConfig.m_IsLoadingProteins; }

    /// True if the sequence data must be fetched
    bool RetrieveSeqData() const { return m_RetrieveSeqData; }
    /// Turn on or off the retrieval of sequence data
    /// @param value true to turn on, false to turn off [in]
    void SetRetrieveSeqData(bool value) { m_RetrieveSeqData = value; }

    /// Retrieve the local id counter initial value
    int GetLocalIdCounterInitValue() const { return m_LocalIdCounter; }
    /// Set the local id counter initial value
    void SetLocalIdCounterInitValue(int val) { m_LocalIdCounter = val; }
    
    /// Retrieve the custom prefix string used for generating local ids
    const string& GetLocalIdPrefix() const { return m_LocalIdPrefix; }
    /// Set the custom prefix string used for generating local ids
    void SetLocalIdPrefix(const string& prefix) { m_LocalIdPrefix = prefix; }
    /// Append query-specific prefix codes to all generated local ids 
    void SetQueryLocalIdMode() {m_LocalIdPrefix = "Query_";}
    /// Append subject-specific prefix codes to all generated local ids
    void SetSubjectLocalIdMode() {m_LocalIdPrefix = "Subject_";}

    /// Retrieve the sequence length threshold to guess the molecule type
    unsigned int GetSeqLenThreshold2Guess() const { 
        return m_SeqLenThreshold2Guess; 
    }
    /// Set the sequence length threshold to guess the molecule type
    void SetSeqLenThreshold2Guess(unsigned int val) { 
        m_SeqLenThreshold2Guess = val;
    }

private:
    /// Strand to assign to sequences
    objects::ENa_strand m_Strand;  
    /// Whether to save lowercase mask locs
    bool m_LowerCaseMask;          
    /// Whether to parse sequence IDs
    bool m_BelieveDeflines;        
    /// Sequence range
    TSeqRange m_Range;             
    /// Configuration object for data loaders, used by CBlastInputReader
    SDataLoaderConfig m_DLConfig;  
    /// Configuration for CBlastInputReader
    bool m_RetrieveSeqData;        
    /// Initialization parameter to CSeqidGenerator
    int m_LocalIdCounter;          
    /// The sequence length threshold to guess molecule type
    unsigned int m_SeqLenThreshold2Guess;
    /// Custom prefix string passed to CSeqidGenerator
    string m_LocalIdPrefix;
};



/// Defines user input exceptions
class NCBI_BLASTINPUT_EXPORT CInputException : public CException
{
public:
    /// Error types that reading BLAST input can generate
    enum EErrCode {
        eInvalidStrand,     ///< Invalid strand specification
        eSeqIdNotFound,     ///< The sequence ID cannot be resolved
        eEmptyUserInput,    ///< No input was provided
        eInvalidRange,      ///< Invalid range specification
        eSequenceMismatch,  ///< Expected sequence type isn't what was expected
        eInvalidInput       ///< Invalid input data
    };

    /// Translate from the error code value to its string representation
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eInvalidStrand:        return "eInvalidStrand";
        case eSeqIdNotFound:        return "eSeqIdNotFound";
        case eEmptyUserInput:       return "eEmptyUserInput";
        case eInvalidRange:         return "eInvalidRange";
        case eSequenceMismatch:     return "eSequenceMismatch";
        case eInvalidInput:         return "eInvalidInput";
        default:                    return CException::GetErrCodeString();
        }
    }

#ifndef SKIP_DOXYGEN_PROCESSING
    NCBI_EXCEPTION_DEFAULT(CInputException, CException);
#endif /* SKIP_DOXYGEN_PROCESSING */
};



/// Base class representing a source of biological sequences
///
class NCBI_BLASTINPUT_EXPORT CBlastInputSource : public CObject
{
protected:
    /// Destructor
    ///
    virtual ~CBlastInputSource() {}

    /// Retrieve a single sequence (in an SSeqLoc container)
    /// @param scope CScope object to use in SSeqLoc returned [in]
    /// @note Embedded Seq-loc returned must be of type interval or whole
    virtual SSeqLoc GetNextSSeqLoc(CScope& scope) = 0;

    /// Retrieve a single sequence (in a CBlastSearchQuery container)
    /// @param scope CScope object to use in CBlastSearchQuery returned [in]
    /// @note Embedded Seq-loc returned must be of type interval or whole
    virtual CRef<CBlastSearchQuery> GetNextSequence(CScope& scope) = 0;

    /// Signal whether there are any unread sequence left
    /// @return true if no unread sequences remaining
    virtual bool End() = 0;

    /// Declare CBlastInput as a friend
    friend class CBlastInput;
};


/// Generalized converter from an abstract source of
/// biological sequence data to collections of blast input
class NCBI_BLASTINPUT_EXPORT CBlastInput : public CObject
{
public:

    /// Constructor
    /// @param source Pointer to abstract source of sequences
    /// @param batch_size A hint specifying how many letters should
    ///               be in a batch of converted sequences
    ///
    CBlastInput(CBlastInputSource* source, int batch_size = kMax_Int)
        : m_Source(source), m_BatchSize(batch_size) {}

    /// Destructor
    ///
    ~CBlastInput() {}

    /// Read and convert all the sequences from the source
    /// @param scope CScope object to use in return value [in]
    /// @return The converted sequences
    ///
    TSeqLocVector GetAllSeqLocs(CScope& scope);

    /// Read and convert all the sequences from the source
    /// @param scope CScope object to use in return value [in]
    /// @return The converted sequences
    ///
    CRef<CBlastQueryVector> GetAllSeqs(CScope& scope);

    /// Read and convert the next batch of sequences
    /// @param scope CScope object to use in return value [in]
    /// @return The next batch of sequence. The size of the batch is
    ///        either all remaining sequences, or the size of sufficiently
    ///        many whole sequences whose combined size exceeds m_BatchSize,
    ///        whichever is smaller
    ///
    TSeqLocVector GetNextSeqLocBatch(CScope& scope);

    /// Read and convert the next batch of sequences
    /// @param scope CScope object to use in return value [in]
    /// @return The next batch of sequence. The size of the batch is
    ///        either all remaining sequences, or the size of sufficiently
    ///        many whole sequences whose combined size exceeds m_BatchSize,
    ///        whichever is smaller
    ///
    CRef<CBlastQueryVector> GetNextSeqBatch(CScope& scope);

    /// Retrieve the target size of a batch of sequences
    /// @return The current batch size
    ///                  
    TSeqPos GetBatchSize() const { return m_BatchSize; }

    /// Determine if we have reached the end of the BLAST input
    bool End() { return m_Source->End(); }

private:
    CRef<CBlastInputSource> m_Source;  ///< pointer to source of sequences
    TSeqPos m_BatchSize;          ///< total size of one block of sequences

    /// Prohibit copy constructor
    CBlastInput(const CBlastInput& rhs);

    /// Prohibit assignment operator
    CBlastInput& operator=(const CBlastInput& rhs);

    /// Perform the actual copy for assignment operator and copy constructor
    void do_copy(const CBlastInput& input);
};

/// Auxiliary class for creating Bioseqs given SeqIds
class NCBI_BLASTINPUT_EXPORT CBlastBioseqMaker : public CObject
{
public:
    /// Constructor
    /// @param scope scope object to use as a source for sequence data [in]
    ///
    CBlastBioseqMaker(CRef<CScope> scope) : m_scope(scope) {}

    /// Creates a Bioseq given a SeqId
    /// @param id Reference to the SeqId object identifying the sequence [in]
    /// @param retrieve_seq_data When gis/accessions are provided in the input,
    ///                 should the sequence data be fetched by this library?
    /// @return The newly created Bioseq object
    ///
    CRef<CBioseq> CreateBioseqFromId(CConstRef<CSeq_id> id,
                                     bool retrieve_seq_data);

    /// Checks the molecule type of the Bioseq identified by the given SeqId
    /// @param id Reference to the SeqId object identifying the sequence [in]
    /// @return True if the molecule type is protein
    ///
    bool IsProtein(CConstRef<CSeq_id> id);

    /// Checks whether the Bioseq actually contains sequence.
    /// E.g., master WGS accessions have no sequence.
    /// @param id Reference to the SeqId object identifying the sequence [in]
    /// @return True if there is sequence.
    ///
    bool HasSequence(CConstRef<CSeq_id> id);

    /// Returns true if the Bioseq contained in the seq_entry is empty (i.e.:
    /// it was created by this class)
    /// @param bioseq Bioseq object to inspect [in]
    static bool IsEmptyBioseq(const CBioseq& bioseq);

private:
    /// Scope object used to retrieve the bioseqs
    CRef<CScope> m_scope;
};

END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___BLAST_INPUT__HPP */

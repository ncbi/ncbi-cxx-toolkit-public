#ifndef ALGO_COBALT___COBALT_OPTIONS__HPP
#define ALGO_COBALT___COBALT_OPTIONS__HPP

/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: options.hpp

Author: Greg Boratyn

Contents: Interface for CMultiAlignerOptions

******************************************************************************/


/// @file options.hpp
/// Options for CMultiAligner

#include <corelib/ncbiobj.hpp>
#include <algo/cobalt/kmercounts.hpp>
#include <algo/align/nw/nw_pssm_aligner.hpp>
#include <objects/blast/Blast4_archive.hpp>

/// Default values for cobalt parameters
/// Rps-Blast e-value cutoff for creating contraints
#define COBALT_RPS_EVALUE 0.003
/// Weight for domain residue frequecies when creating MSA profiles 
#define COBALT_DOMAIN_BOOST 0.5
/// Hitlist size for Rps-Blast searches
#define COBALT_DOMAIN_HITLIST_SIZE 500

/// Blastp e-value cutoff for creating contraints
#define COBALT_BLAST_EVALUE 0.005
/// Weight for sequence residues when creating MSA profules
#define COBALT_LOCAL_BOOST 1.0

/// Pseudocount constant used in multiple alignment
#define COBALT_PSEUDO_COUNT 2.0
/// Conservation score cutoff used for selecting conserved columns in
/// initial MSA
#define COBALT_CONSERVED_CUTOFF 0.67

/// Default method for computing progressive alignment tree
#define COBALT_TREE_METHOD CMultiAlignerOptions::eClusters

/// Default substitution matrix used in multiple alignment
#define COBALT_DEFAULT_MATRIX "BLOSUM62"
/// End gap opening score
#define COBALT_END_GAP_OPEN -5
/// End gap extension score
#define COBALT_END_GAP_EXTNT -1
/// Gap opening score
#define COBALT_GAP_OPEN -11
/// Gap extension score
#define COBALT_GAP_EXTNT -1

/// Maximum cluster diameter for pre-alignment sequence clustering
#define COBALT_MAX_CLUSTER_DIAM 0.8
/// K-mer length for sequence clustering
#define COBALT_KMER_LEN 4
/// K-mer alphabet for sequence clustering
#define COBALT_KMER_ALPH CMultiAlignerOptions::TKMethods::eSE_B15

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)


/// Options and parameters for multiple alignement
///
class NCBI_COBALT_EXPORT CMultiAlignerOptions : public CObject
{

public:
    typedef CNWAligner::TScore TScore;
    typedef TKmerMethods<CSparseKmerCounts> TKMethods;

    /// Representation of CDD pattern 
    ///
    /// Pattern is represented either as string or pointer in order
    /// to avoid copying large blocks if patterns are already in memory.
    /// Representation is selected by the use of constructor. String
    /// constructor creates a copy of the argument, pointer one does not.
    class CPattern
    {
    public:

        /// Create empty pattern
        CPattern(void)
            : m_Pattern((char*)NULL), m_IsPointer(true) {}

        /// Create pattern as pointer. Referenced memory is not copied.
        /// @param pattern Pattern
        CPattern(char* pattern)
            : m_Pattern(pattern), m_IsPointer(true) {}

        /// Create pattern as string. The argument is copied.
        /// @param pattern Pattern
        CPattern(const string& pattern)
            : m_Pattern(pattern), m_IsPointer(false) {}

        /// Create copy of a pattern
        /// @param pattern Pattern
        CPattern(const CPattern& pattern)
        {
            if (pattern.m_IsPointer) {
                m_Pattern.pointer = pattern.m_Pattern.pointer;
            }
            else {
                m_Pattern.str = pattern.m_Pattern.str;
            }
            m_IsPointer = pattern.m_IsPointer;
        }

        /// Assignment operator
        /// @param pattern Pattern
        CPattern& operator=(const CPattern& pattern)
        {
            if (pattern.m_IsPointer) {
                m_Pattern.pointer = pattern.m_Pattern.pointer;
            }
            else {
                m_Pattern.str = pattern.m_Pattern.str;
            }
            m_IsPointer = pattern.m_IsPointer;

            return *this;
        }
            
        /// Get pattern as pointer
        /// @return Pointer to a pattern
        const char* AsPointer(void) const
        {return (m_IsPointer ? m_Pattern.pointer 
                 : m_Pattern.str.c_str());}

        /// Get a copy of a pattern as string
        /// @return Copy of pattern
        string AsString(void) const
        {return (m_IsPointer ? (string)m_Pattern.pointer : m_Pattern.str);}

        /// Check if pattern is stored as pointer
        /// @return
        ///     True if pattern is stored as pointer,
        ///     False otherwise
        bool IsPointer(void) const {return m_IsPointer;}

        /// Check if pattern is empty
        /// @return 
        ///    True if patter is empty,
        ///    False otherwise
        bool IsEmpty(void) const
        {return m_IsPointer ? !m_Pattern.pointer : m_Pattern.str.empty();}

    private:
        struct SPattern {
            char* pointer; 
            string str;

            SPattern(void) : pointer(NULL) {}
            SPattern(char* ptr) : pointer(ptr) {}
            SPattern(string s) : pointer(NULL), str(s) {}
        };

        SPattern m_Pattern;
        bool m_IsPointer;
    };


    /// Structure for representing single user constraint for pair-wise
    /// alignment
    struct SConstraint {
        int seq1_index;
        int seq1_start;
        int seq1_stop;

        int seq2_index;
        int seq2_start;
        int seq2_stop;

        /// Create empty constraint
        ///
        SConstraint(void) : seq1_index(-1), seq2_index(-1) {}

        /// Create constraint for given sequences and locations
        /// @param ind1 Index of sequence 1 in query array
        /// @param start1 Start location for sequence 1
        /// @param end1 End location for sequence 1
        /// @param ind2 Index of sequence 2 in query array
        /// @param start2 Start location for sequence 2
        /// @param end2 End location for sequence 2
        SConstraint(int ind1, int start1, int end1, int ind2, int start2,
                        int end2) 
            : seq1_index(ind1), seq1_start(start1), seq1_stop(end1),
              seq2_index(ind2), seq2_start(start2), seq2_stop(end2)
        {}

    };

    typedef vector<SConstraint> TConstraints;

    /// Mode of multi aligner setings. Values can be combined.
    enum EMode {

        // Qyery clusters 
        fNoQueryClusters = 1, ///< No query clustering

        // RPS Blast search
        fNoRpsBlast = 1<<2,        ///< Do not use RPS Blast

        // Regular expression patterns search
        fNoPatterns = 1<<3,        ///< Do not use conserved domain patterns

        // Iterative alignment
        fNoIterate = 1<<4,          ///< Do not use Iterative alignment

        fNoRealign = 1<<5,    ///< Do not realign with different tree root

        fFastAlign = 1<<6,    ///< Do Fast and rough profile-profile alignment

        /// Set options for very fast alignment (speed over accuracy)
        fFast = fNoRpsBlast | fNoIterate | fNoRealign | fFastAlign,
        
        // Other
        fNonStandard = 1<<7   ///< Not used as input, indicates that
                               ///< non-standard settings were selected after

    };

    typedef int TMode;

    /// Default options mode
    static const TMode kDefaultMode = 0;

    /// Method for construction of guide tree for progressive alignment
    enum ETreeMethod {
        eNJ = 0,  ///< Neighbot Joining
        eFastME,  ///< Fast Minimum Evolution
        eClusters ///< Clustering dendrogram
    };

    enum EInClustAlnMethod {
        eNone = 0,     ///< No clustering
        eToPrototype,  ///< All cluster elements are aligner to cluster 
                       ///< prototype

        eMulti         ///< Alignment guide tree for each cluster is attached
                       ///< to the main alignment guide tree
    };

public:
    
    /// Create options with default mode
    ///
    CMultiAlignerOptions(void);

    /// Create options with desired mode
    /// @param mode Desired mode of operation
    ///
    explicit CMultiAlignerOptions(TMode mode);

    /// Create options with RPS database and desired mode
    /// @param rps_db_name Name of RPS database
    /// @param mode Mode of operation
    ///
    CMultiAlignerOptions(const string& rps_db_name, TMode mode = kDefaultMode);


    // Turn on and off major options and set major parameters. Other parameter
    // are set to defaults.

    /// Set use of query clustering option
    ///
    /// If the option in set on, query sequences will be clustered. Each cluster
    /// Will be aligned independently without searching for conserved domains
    /// (RPS Blast) and local hist (Blastp). Multiple alignment will be
    /// performed on cluster profiles. Parameters of clustering procedure are
    /// set base on EMode value and can be chanded with expert functions.
    /// @param use Option used if true [in]
    ///
    void SetUseQueryClusters(bool use)
    {m_UseQueryClusters = use; m_Mode = fNonStandard;}


    /// Check if query clustering option is on
    /// @return 
    ///   - True if query clustering option is on, 
    ///   - False otherwise
    ///
    bool GetUseQueryClusters(void) const {return m_UseQueryClusters;}


    /// Set use of iterative alignment option
    ///
    /// After initial multiple alignment is done, conserved columns will be
    /// found and sequences will be re-aligned using this information. Default
    /// parameters will be used. Iterative alignment parameters can be changed
    /// with expert functions.
    /// @param use Option used if true, otherwise not used [in]
    ///
    void SetIterate(bool use)
    {m_Iterate = use; m_Mode = fNonStandard;}

    /// Set realigning MSA with different root nodes in the progressive
    /// alignment tree
    /// @param r Do realignment if true [in]
    ///
    void SetRealign(bool r)
    {m_Realign = r; m_Mode = fNonStandard;}

    /// Check if iterative alignmnet option is used
    /// @return 
    ///   - True if iterative alignment option is on, 
    ///   - False otherwise
    ///
    bool GetIterate(void) const {return m_Iterate;}

    /// Check if MSA is to be realigned for different rooting of progressive
    /// alignment tree
    /// @return True if MSA is to be realigned, false otherwise
    ///
    bool GetRealign(void) const {return m_Realign;}


    /// Use RPS Blast with given database
    ///
    /// RPS Blast will be used to find conserved domains in query sequences.
    /// Pairwise alignments are constrainted so that matching conserved domains
    /// are aligned.
    /// Default RPS Blast parameters will be used. RPS Blast parameters can be
    /// changed with expetr functions.
    /// @param dbname Path and name of RPS Blast data base [in]
    ///
    void SetRpsDb(const string& dbname)
    {m_RpsDb = dbname; m_Mode = m_Mode & ~fNoRpsBlast;}


    /// Get RPS Blast data base name
    /// @return RPS Blast data base name
    ///
    string GetRpsDb(void) const {return m_RpsDb;}


    /// Determine if RPS Blast is to be used
    /// @return True if RPS Blast will be used, false otherwise
    ///
    bool GetUseRpsBlast(void) const {return !m_RpsDb.empty();}


    /// Set regular expression patterns for identification of conserved domains.
    /// Patterns are not freed when object is deleted.
    ///
    /// Regular expresion patterns will be used to find conserved domains.
    /// Pairwise alignmnents will be constained to so that matching conserved
    /// domains are aligned. Parameter ownership is transfered to options.
    /// @return Reference to the list of conserved domain patterns
    ///
    vector<CPattern>& SetCddPatterns(void)
    {m_Mode = fNonStandard; return m_Patterns;}


    /// Set default patterns for identification of conserved domains.
    ///
    /// Regular expresion patterns will be used to find conserved domains.
    /// Pairwise alignmnents will be constained to so that matching conserved
    /// domains are aligned. Parameter ownership is transfered to options.
    ///
    void SetDefaultCddPatterns(void);


    /// Get regular expression patterns for identification of conserved domains
    /// @return List of conserved domain patterns
    ///
    const vector<CPattern>& GetCddPatterns(void) const {return m_Patterns;}


    /// Set user constraints.
    ///
    /// The constraits are used in progressive alignment.
    /// @return Reference to the list of user constraints
    ///
    TConstraints& SetUserConstraints(void)
    {m_Mode = fNonStandard; return m_UserHits;}


    /// Get user constraints
    /// @return User constraits
    ///
    const TConstraints& GetUserConstraints(void) const
    {return m_UserHits;}


    /// Set score for user alignment constraints
    /// @param score Score
    ///
    void SetUserConstraintsScore(int score)
    {m_UserHitsScore = score; m_Mode = fNonStandard;}


    /// Get score for user alignment constraints
    /// @return Score for user alignmnet constraints
    ///
    int GetUserConstraintsScore(void) const {return m_UserHitsScore;}


    //--- Expert Methods ---

    //--- Query clustering ---

    /// Set word size for creating word count vectors in query clustering
    /// @param len Number of letters in a word [in]
    ///
    void SetKmerLength(int len) {m_KmerLength = len; m_Mode = fNonStandard;}

    /// Get word size for creating word count vectors
    /// @return Number of letters in a word
    ///
    int GetKmerLength(void) const {return m_KmerLength;}

    /// Set alphabet for creating word count vectors
    /// @param alph Alphabet [in]
    ///
    void SetKmerAlphabet(TKMethods::ECompressedAlphabet alph)
    {m_KmerAlphabet = alph; m_Mode = fNonStandard;}

    /// Get alphabet used for creating word count vectors
    /// @return Alphabet
    ///
    TKMethods::ECompressedAlphabet GetKmerAlphabet(void) const
    {return m_KmerAlphabet;}

    /// Set measure for computing distance between word count vectors
    /// @param method Distance method [in]
    ///
    void SetKmerDistMeasure(TKMethods::EDistMeasures method)
    {m_ClustDistMeasure = method; m_Mode = fNonStandard;}
    

    /// Get method for computing distance between word count vectors
    /// @return Distance method
    ///
    TKMethods::EDistMeasures GetKmerDistMeasure(void) const
    {return m_ClustDistMeasure;}


    /// Set maximum allowed distance between sequences in a cluster
    /// @param dist Maximum allowed distance in cluster [in]
    ///
    void SetMaxInClusterDist(double dist)
    {m_MaxInClusterDist = dist; m_Mode = fNonStandard;}

    /// Get maximum allowed distance between sequences in a cluster
    /// @return Maxium allowed distance in cluster
    ///
    double GetMaxInClusterDist(void) const {return m_MaxInClusterDist;}


    //--- RPS Blast ---

    /// Set e-value threshold for accepting RPS Blast hits
    /// @param evalue E-value for acceting RPS Blast hits [in]
    ///
    void SetRpsEvalue(double evalue)
    {m_RpsEvalue = evalue; m_Mode = fNonStandard;}

    /// Get e-value threshold for accepting RPS Blast hits
    /// @return E-value for accepting RPS Blast hits
    ///
    double GetRpsEvalue(void) const {return m_RpsEvalue;}

    /// Set hitlist size (per sequence) for domain search
    /// @param size Hitlist size [in]
    ///
    void SetDomainHitlistSize(int size)
    {m_DomainHitlistSize = size; m_Mode = fNonStandard;}

    /// Get hitlist size (per sequence) for domain searches
    /// @return Hitlist size for domain searches
    ///
    int GetDomainHitlistSize(void) const {return m_DomainHitlistSize;}

    /// Set boost for residue frequencies in conserved domains from RPS data 
    /// base
    /// @param boost Boost for RPS residue frequencies [in]
    ///
    void SetDomainResFreqBoost(double boost) 
    {m_DomainResFreqBoost = boost; m_Mode = fNonStandard;}

    /// Get boost for residue frequencies in conserved domains from RPS data
    /// base
    /// @return Boost for RPS residue frequencies
    ///
    double GetDomainResFreqBoost(void) const {return m_DomainResFreqBoost;}

    /// Set use of precomputed RPS Blast hits
    /// @param use 
    void SetUsePreRpsHits(bool use)
    {m_UsePreRpsHits = use; m_Mode = fNonStandard;}

    /// Get use of precomputed RPS Blast hits
    /// @return 
    ///     - True if use precomputed RPS hits is on,
    ///     - False otherwise
    ///
    bool GetUsePreRpsHits(void) const {return m_UsePreRpsHits;}


    //--- Blastp ---

    /// Set e-value for accepting Blastp hits
    /// @param evalue E-value for accepting Blastp hits [in]
    ///
    void SetBlastpEvalue(double evalue)
    {m_BlastpEvalue = evalue; m_Mode = fNonStandard;}

    /// Get e-value for accepting Blastp hits
    /// @return E-value for accepting Blastp hits
    ///
    double GetBlastpEvalue(void) const {return m_BlastpEvalue;}


    //--- Iterative alignment

    /// Set cutoff score for conserved aligned columns
    /// @param score Cutoff score [in]
    ///
    void SetConservedCutoffScore(double score)
    {m_ConservedCutoff = score; m_Mode = fNonStandard;}

    /// Get cutoff score for conserved aligned columns
    /// @return Cutoff score
    ///
    double GetConservedCutoffScore(void) const {return m_ConservedCutoff;}

    /// Set pseudocount for calculating column entropy
    /// @param pseudocount Pseudocount [in]
    ///
    void SetPseudocount(double pseudocount)
    {m_Pseudocount = pseudocount; m_Mode = fNonStandard;}

    /// Get pseudocount for calculating column entropy
    /// @return Pseudocount
    ///
    double GetPseudocount(void) const {return m_Pseudocount;}


    //--- Progressive alignment

    /// Set method for creating tree that guides progressive alignment
    /// @param method Tree computation method [in]
    ///
    void SetTreeMethod(ETreeMethod method)
    {m_TreeMethod = method; m_Mode = fNonStandard;}

    /// Get method for creating tree that guides progressive alignment
    /// @return Tree method
    ///
    ETreeMethod GetTreeMethod(void) const {return m_TreeMethod;}

    /// Set frequency boost for a letter that appears in query sequence in
    /// given position
    /// @param boost Frequency boost [in]
    ///
    void SetLocalResFreqBoost(double boost)
    {m_LocalResFreqBoost = boost; m_Mode =fNonStandard;}

    /// Get frequency boost for a letter that appears in query sequence in
    /// given position
    /// @return Frequency boost
    ///
    double GetLocalResFreqBoost(void) const {return m_LocalResFreqBoost;}


    //--- Pairwise alignment

    /// Set alignment socre matrix name
    /// @param matrix Score matrix name [in]
    ///
    void SetScoreMatrixName(const string& matrix)
    {m_MatrixName = matrix; m_Mode = fNonStandard;}

    /// Get alignment score matrix name
    /// @return Score matrix name
    ///
    string GetScoreMatrixName(void) const {return m_MatrixName;}

    /// Set gap opening penalty for middle gaps in pairwise global alignment
    /// of profiles
    /// @param penalty Gap open penalty [in]
    ///
    void SetGapOpenPenalty(TScore penalty)
    {m_GapOpen = penalty; m_Mode = fNonStandard;}

    /// Get gap opening penalty for middle gaps in pairwise global alignment
    /// of profiles
    /// @return Gap open penalty
    ///
    TScore GetGapOpenPenalty(void) const {return m_GapOpen;}

    /// Set gap extension penalty for middle gaps in pairwise global alignment
    /// of profiles
    /// @param penalty Gap extension penalty [in]
    ///
    void SetGapExtendPenalty(TScore penalty)
    {m_GapExtend = penalty; m_Mode = fNonStandard;}

    /// Get gap extension penlaty for middle gaps in pairwise global alignment
    /// of profiles
    /// @return Gap extension penalty
    ///
    TScore GetGapExtendPenalty(void) const {return m_GapExtend;}

    /// Set gap opening penalty for end gaps in pairwise global alignment
    /// of profiles
    /// @param penalty Gap open penalty [in]
    ///
    void SetEndGapOpenPenalty(TScore penalty)
    {m_EndGapOpen = penalty; m_Mode = fNonStandard;}

    /// Get gap opening penalty for end gaps in pairwise global alignment
    /// of profiles
    /// @return Gap open penalty
    ///
    TScore GetEndGapOpenPenalty(void) const {return m_EndGapOpen;}

    /// Set gap extension penalty for end gaps in pairwise global alignment
    /// of profiles
    /// @param penalty Gap extension penalty [in]
    ///
    void SetEndGapExtendPenalty(TScore penalty)
    {m_EndGapExtend = penalty; m_Mode = fNonStandard;}

    /// Get gap extension penalty for end gaps in pairwise global alignment
    /// of profiles
    /// @return Gap extension penalty
    ///
    TScore GetEndGapExtendPenalty(void) const {return m_EndGapExtend;}


    //--- Other ---

    /// Get options mode
    /// @return Options mode
    ///
    TMode GetMode(void) const {return m_Mode;}

    /// Check whether parameter values belong to any of the standard modes
    ///
    /// The mode is standard when parameters are not changed after the options
    /// object is created, with exception of RPS database name.
    /// @return
    ///     True if parameter values belong to a stanard mode,
    ///     False otherwise
    ///
    bool IsStandardMode(void) const {return !(m_Mode & fNonStandard);}

    /// Set verbose mode
    ///
    /// If set, intermidiate results will be provided in stdout
    /// @param verbose Verbose mode set if true, not set otherwise [in]
    ///
    void SetVerbose(bool verbose) {m_Verbose = verbose;}

    /// Get verbose mode
    ///
    /// If set, intermidiate results will be provided in stdout
    /// @return
    ///   - true if verbose mode set
    ///   - false otherwise
    bool GetVerbose(void) const {return m_Verbose;}

    void SetInClustAlnMethod(EInClustAlnMethod method)
    {m_InClustAlnMethod = method;}

    EInClustAlnMethod GetInClustAlnMethod(void) const
    {return m_InClustAlnMethod;}

    /// Set pre-computed domain hits
    /// @param archive Blast4 archive with precomputed domain hits [in]
    ///
    void SetDomainHits(CConstRef<objects::CBlast4_archive> archive)
    {m_DomainHits = archive;}

    /// Get pre-computed domain hits
    /// @return Blast4 archive with pre-computed domain hits
    ///
    CConstRef<objects::CBlast4_archive> GetDomainHits(void) const
    {return m_DomainHits;}

    /// Are pre-computed domain hits set
    /// @return true if pre-computed domain hits are set, false otherwise
    ///
    bool CanGetDomainHits(void) const
    {return !m_DomainHits.Empty();}

    /// Check if fast alignment is to be used
    ///
    /// Fast alignment means that constraints will be used instead of profile-
    /// profile alignment
    /// @return If true, fast alignment will be used, regural otherwise
    ///
    bool GetFastAlign(void) const {return m_FastAlign;}

    /// Turn fast alignment method on/off
    ///
    /// Fast alignment means that constraints will be used instead of profile-
    /// profile alignment
    /// @param Fast alignment will be used if true [in]
    ///
    void SetFastAlign(bool f) {m_FastAlign = f; m_Mode = fNonStandard;}


    //--- Options validation ---

    /// Validate parameter values
    /// @return True if parameters valid, false otherwise
    ///
    bool Validate(void);

    /// Get warning messages
    /// @return Warning messages
    ///
    const vector<string>& GetMessages(void) {return m_Messages;}
    
private:

    /// Forbidding copy constructor
    CMultiAlignerOptions(const CMultiAlignerOptions&);

    /// Forbidding assignment operator
    CMultiAlignerOptions& operator=(const CMultiAlignerOptions&);

    /// Initiate parameter values based on the specified mode
    /// @param mode Mode
    void x_InitParams(TMode mode);


private:

    TMode m_Mode;

    // Query clustering
    bool m_UseQueryClusters;
    TKMethods::ECompressedAlphabet m_KmerAlphabet;
    unsigned int m_KmerLength;
    double m_MaxInClusterDist;
    TKMethods::EDistMeasures m_ClustDistMeasure;
    EInClustAlnMethod m_InClustAlnMethod;

    // RPS Blast
    string m_RpsDb;
    double m_RpsEvalue;
    int m_DomainHitlistSize;
    double m_DomainResFreqBoost;
    bool m_UsePreRpsHits;

    // Blastp
    double m_BlastpEvalue;

    // Patterns
    vector<CPattern> m_Patterns;

    // Iterative alignmnet
    bool m_Iterate;
    double m_ConservedCutoff;
    double m_Pseudocount;

    // Realign MSA for different progressive alignment tree rooting
    bool m_Realign;

    // Skip profile-profile alignment whenever possible and use information
    // from constraints to align sequences and profiles
    bool m_FastAlign;

    // User constraints
    TConstraints m_UserHits;
    int m_UserHitsScore;

    // Progressive alignment
    ETreeMethod m_TreeMethod;
    double m_LocalResFreqBoost;

    // Pairwise alignment
    string m_MatrixName;
    TScore m_GapOpen;
    TScore m_GapExtend;
    TScore m_EndGapOpen;
    TScore m_EndGapExtend;

    // Pre-computed hits
    CConstRef<objects::CBlast4_archive> m_DomainHits;

    bool m_Verbose;

    vector<string> m_Messages;

    static const int kDefaultUserConstraintsScore = 1000000;
};

END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif /* ALGO_COBALT___COBALT_OPTIONS__HPP */

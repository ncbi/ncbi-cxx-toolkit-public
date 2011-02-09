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

/** @file blast_args.hpp
 * Interface for converting blast-related command line
 * arguments into blast options
 */

#ifndef ALGO_BLAST_BLASTINPUT___BLAST_ARGS__HPP
#define ALGO_BLAST_BLASTINPUT___BLAST_ARGS__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/igblast.hpp>
#include <algo/blast/api/setup_factory.hpp> // for CThreadable
#include <algo/blast/blastinput/cmdline_flags.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>

#include <objmgr/scope.hpp>     // for CScope
#include <objects/seqloc/Na_strand.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/**
 * BLAST Command line arguments design
 * The idea is to have several small objects (subclasses of IBlastCmdLineArgs) 
 * which can do two things:
 * 1) On creation, add flags/options/etc to a CArgs object
 * 2) When passed in a CBlastOptions object, call the appropriate methods based
 * on the CArgs options set when the NCBI application framework parsed the
 * command line. If data collected by the small object (from the command line)
 * cannot be applied to the CBlastOptions object, then it's provided to the
 * application via some other interface methods.
 *
 * Each command line application will have its own argument class (e.g.:
 * CPsiBlastAppArgs), which will contain several of the aformentioned small 
 * objects. It will create and hold a reference to a CArgs class as well as 
 * a CBlastOptionsHandle object, which will pass to each of its small objects 
 * aggregated as data members and then return it to the caller (application)
 *
 * Categories of data to extract from command line options
 * 1) BLAST algorithm options
 * 2) Input/Output files, and their modifiers (e.g.: believe query defline)
 * 3) BLAST database information (names, limitations, num db seqs)
 * 4) Formatting options (html, display formats, etc)
*/

/** Interface definition for a generic command line option for BLAST
 */
class NCBI_BLASTINPUT_EXPORT IBlastCmdLineArgs : public CObject
{
public:
    /** Our virtual destructor */
    virtual ~IBlastCmdLineArgs() {}

    /** Sets the command line descriptions in the CArgDescriptions object
     * relevant to the subclass
     * @param arg_desc the argument descriptions object [in|out]
     */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc) = 0;

    /** Extracts BLAST algorithmic options from the command line arguments into
     * the CBlastOptions object. Default implementation does nothing.
     * @param cmd_line_args Command line arguments parsed by the NCBI
     * application framework [in]
     * @param options object to which the appropriate options will be set
     * [in|out]
     */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/** Argument class to retrieve input and output streams for a command line
 * program.
 */
class NCBI_BLASTINPUT_EXPORT CStdCmdLineArgs : public IBlastCmdLineArgs
{
public:
    /** Default constructor */
    CStdCmdLineArgs() : m_InputStream(0), m_OutputStream(0) {};
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
    /** Get the input stream for a command line application */
    CNcbiIstream& GetInputStream() const;
    /** Get the output stream for a command line application */
    CNcbiOstream& GetOutputStream() const;
    /** Set the input stream if read from a saved search strategy */
    void SetInputStream(CRef<CTmpFile> input_file);

private:
    CNcbiIstream* m_InputStream;    ///< Application's input stream
    CNcbiOstream* m_OutputStream;   ///< Application's output stream

    /// ASN.1 specification of query sequences when read from a saved search
    /// strategy
    CRef<CTmpFile> m_QueryTmpInputFile;
};

/** Argument class to populate an application's name and description */
class NCBI_BLASTINPUT_EXPORT CProgramDescriptionArgs : public IBlastCmdLineArgs
{
public:
    /** 
     * @brief Constructor
     * 
     * @param program_name application's name [in]
     * @param program_description application's description [in]
     */
    CProgramDescriptionArgs(const string& program_name, 
                            const string& program_description);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);

private:
    string m_ProgName;  ///< Application's name
    string m_ProgDesc;  ///< Application's description
};

/// Argument class to specify the supported tasks a given program
class NCBI_BLASTINPUT_EXPORT CTaskCmdLineArgs : public IBlastCmdLineArgs
{
public:
    /** Constructor 
     * @param supported_tasks list of supported tasks [in]
     * @param default_task One of the tasks above, to be displayed as
     * default in the command line arguments (cannot be empty or absent from
     * the set above) [in]
     */
    CTaskCmdLineArgs(const set<string>& supported_tasks,
                     const string& default_task);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
private:
    /// Set of supported tasks by this command line argument
    const set<string> m_SupportedTasks;
    /// Default task for this command line argument
    string m_DefaultTask;
};

/** Argument class to retrieve and set the window size BLAST algorithm 
 * option */
class NCBI_BLASTINPUT_EXPORT CWindowSizeArg : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions 
     * @note this depends on the matrix already being set...
     */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/** Argument class to retrieve and set the off-diagonal range used in 2-hit
    algorithm */
class NCBI_BLASTINPUT_EXPORT COffDiagonalRangeArg : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions 
     * @note this depends on the matrix already being set...
     */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/** Argument class to retrieve and set the word threshold BLAST algorithm 
 * option */
class NCBI_BLASTINPUT_EXPORT CWordThresholdArg : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions 
     * @note this depends on the matrix already being set...
     */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/** RMH: Argument class to retrieve and set the options specific to 
 *       the RMBlastN algorithm 
 */
class NCBI_BLASTINPUT_EXPORT CRMBlastNArg : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args,
                                         CBlastOptions& options);
};

/** Argument class to retrieve and set the scoring matrix name BLAST algorithm
 * option */
class NCBI_BLASTINPUT_EXPORT CMatrixNameArg : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/** Argument class for general search BLAST algorithm options: evalue, gap
 * penalties, query filter string, ungapped x-drop, initial and final gapped 
 * x-drop, word size, percent identity, and effective search space.
 */
class NCBI_BLASTINPUT_EXPORT CGenericSearchArgs : public IBlastCmdLineArgs
{
public:
    /** 
     * @brief Constructor
     * 
     * @param query_is_protein is the query sequence(s) protein? [in]
     * @param is_rpsblast is it RPS-BLAST? [in]
     * @param show_perc_identity should the percent identity be shown?
     * Currently only supported for blastn [in]
     */
    CGenericSearchArgs(bool query_is_protein = true, bool is_rpsblast = false,
                       bool show_perc_identity = false, bool is_tblastx = false)
        : m_QueryIsProtein(query_is_protein), m_IsRpsBlast(is_rpsblast),
          m_ShowPercentIdentity(show_perc_identity), m_IsTblastx(is_tblastx) {}
         
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
private:
    bool m_QueryIsProtein;  /**< true if the query is protein */
    bool m_IsRpsBlast;      /**< true if the search is RPS-BLAST */
    bool m_ShowPercentIdentity; /**< true if the percent identity option should
                                 be shown */
    bool m_IsTblastx; /**< true if the search is tblastx */
};

/** Argument class for collecting filtering options */
class NCBI_BLASTINPUT_EXPORT CFilteringArgs : public IBlastCmdLineArgs
{
public:
    /** 
     * @brief Constructor
     * 
     * @param query_is_protein is the query sequence(s) protein? [in]
     * @param filter_by_default should filtering be applied by default? [in]
     */
    CFilteringArgs(bool query_is_protein = true,
                   bool filter_by_default = true)
        : m_QueryIsProtein(query_is_protein), 
          m_FilterByDefault(filter_by_default) {}

    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
private:
    bool m_QueryIsProtein;  /**< true if the query is protein */
    bool m_FilterByDefault; /**< Should filtering be applied by default? */

    /** 
     * @brief Auxiliary method to tokenize the filtering string.
     * 
     * @param filtering_args string to tokenize [in]
     * @param output vector with tokens [in|out]
     */
    void x_TokenizeFilteringArgs(const string& filtering_args,
                                 vector<string>& output) const;
};

/// Defines values for match and mismatch in nucleotide comparisons as well as
/// non-greedy extension
class NCBI_BLASTINPUT_EXPORT CNuclArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/// Argument class to retrieve discontiguous megablast arguments
class NCBI_BLASTINPUT_EXPORT CDiscontiguousMegablastArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    /// Value to specify coding template type
    static const string kTemplType_Coding;
    /// Value to specify optimal template type
    static const string kTemplType_Optimal;
    /// Value to specify coding+optimal template type
    static const string kTemplType_CodingAndOptimal;
};

/** Argument class for collecting composition based statistics options */
class NCBI_BLASTINPUT_EXPORT CCompositionBasedStatsArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/** Argument class for collecting gapped options */
class NCBI_BLASTINPUT_EXPORT CGappedArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/** Argument class for collecting the largest intron size */
class NCBI_BLASTINPUT_EXPORT CLargestIntronSizeArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/// Argument class to collect the frame shift penalty for out-of-frame searches
class NCBI_BLASTINPUT_EXPORT CFrameShiftArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/// Argument class to collect the genetic code for all queries/subjects
class NCBI_BLASTINPUT_EXPORT CGeneticCodeArgs : public IBlastCmdLineArgs
{
public:
    /// Enumeration defining which sequences the genetic code applies to
    enum ETarget {
        eQuery,         ///< Query genetic code
        eDatabase       ///< Database genetic code
    };


    /** 
     * @brief Constructor
     * 
     * @param t genetic code target (query or database)
     */
    CGeneticCodeArgs(ETarget t) : m_Target(t) {};

    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

private:
    ETarget m_Target; ///< Genetic code target
};

/// Argument class to retrieve the gap trigger option
class NCBI_BLASTINPUT_EXPORT CGapTriggerArgs : public IBlastCmdLineArgs
{
public:
    /** 
     * @brief Constructor
     * 
     * @param query_is_protein is the query sequence(s) protein?
     */
    CGapTriggerArgs(bool query_is_protein) 
        : m_QueryIsProtein(query_is_protein) {}
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
private:
    bool m_QueryIsProtein;  /**< true if the query is protein */
};

/// Argument class to collect PSSM engine options
class NCBI_BLASTINPUT_EXPORT CPssmEngineArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/// Argument class to import/export the search strategy
class NCBI_BLASTINPUT_EXPORT CSearchStrategyArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    /// Get the input stream for the search strategy
    CNcbiIstream* GetImportStream(const CArgs& args) const;
    /// Get the output stream for the search strategy
    CNcbiOstream* GetExportStream(const CArgs& args) const;
};

/// Argument class to collect options specific to PSI-BLAST
class NCBI_BLASTINPUT_EXPORT CPsiBlastArgs : public IBlastCmdLineArgs
{
public:
    /// Enumeration to determine the molecule type of the database
    enum ETargetDatabase {
        eProteinDb,         ///< Traditional, iterated PSI-BLAST
        eNucleotideDb       ///< PSI-Tblastn, non-iterated
    };

    /** 
     * @brief Constructor
     * 
     * @param db_target Molecule type of the database
     */
    CPsiBlastArgs(ETargetDatabase db_target = eProteinDb) 
        : m_DbTarget(db_target), m_NumIterations(1),
        m_CheckPointOutput(0), m_AsciiMatrixOutput(0)
    {};

    /// Our virtual destructor
    virtual ~CPsiBlastArgs() {}

    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    /// Retrieve the number of iterations to perform
    size_t GetNumberOfIterations() const { 
        return m_NumIterations; 
    }
    /// Returns true if checkpoint PSSM is required to be printed
    bool RequiresCheckPointOutput() const {
        return m_CheckPointOutput != NULL;
    }
    /// Get the checkpoint file output stream
    /// @return pointer to output stream, not to be free'd by the caller
    CNcbiOstream* GetCheckPointOutputStream() {
        return m_CheckPointOutput ? m_CheckPointOutput->GetStream() : NULL;
    }
    /// Returns true if ASCII PSSM is required to be printed
    bool RequiresAsciiPssmOutput() const {
        return m_AsciiMatrixOutput != NULL;
    }
    /// Get the ASCII matrix output stream
    /// @return pointer to output stream, not to be free'd by the caller
    CNcbiOstream* GetAsciiMatrixOutputStream() {
        return m_AsciiMatrixOutput ? m_AsciiMatrixOutput->GetStream() : NULL;
    }

    /// Get the PSSM read from checkpoint file
    CRef<objects::CPssmWithParameters> GetInputPssm() const {
        return m_Pssm;
    }

    /// Set the PSSM read from saved search strategy
    void SetInputPssm(CRef<objects::CPssmWithParameters> pssm) {
        m_Pssm = pssm;
    }

private:
    /// Molecule of the database
    ETargetDatabase m_DbTarget;
    /// number of iterations to perform
    size_t m_NumIterations;
    /// checkpoint output file
    CRef<CAutoOutputFileReset> m_CheckPointOutput;
    /// ASCII matrix output file
    CRef<CAutoOutputFileReset> m_AsciiMatrixOutput;
    /// PSSM
    CRef<objects::CPssmWithParameters> m_Pssm;

    /// Prohibit copy constructor
    CPsiBlastArgs(const CPsiBlastArgs& rhs);
    /// Prohibit assignment operator
    CPsiBlastArgs& operator=(const CPsiBlastArgs& rhs);

    /// Auxiliary function to create a PSSM from a multiple sequence alignment
    /// file
    CRef<objects::CPssmWithParameters>
    x_CreatePssmFromMsa(CNcbiIstream& input_stream, CBlastOptions& opt, 
                        bool save_ascii_pssm, unsigned int msa_master_idx);
};

/// Argument class to collect options specific to PHI-BLAST
class NCBI_BLASTINPUT_EXPORT CPhiBlastArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/*****************************************************************************/
// Input options

/// Argument class to collect query options
class NCBI_BLASTINPUT_EXPORT CQueryOptionsArgs : public IBlastCmdLineArgs
{
public:
    /** 
     * @brief Constructor
     * 
     * @param query_cannot_be_nucl can the query not be nucleotide?
     */
    CQueryOptionsArgs(bool query_cannot_be_nucl = false)
        : m_Strand(objects::eNa_strand_unknown), m_Range(),
        m_UseLCaseMask(kDfltArgUseLCaseMasking), 
        m_ParseDeflines(kDfltArgParseDeflines),
        m_QueryCannotBeNucl(query_cannot_be_nucl)
    {};

    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    /// Get query sequence range restriction
    TSeqRange GetRange() const { return m_Range; }
    /// Set query sequence range restriction
    void SetRange(const TSeqRange& range) { m_Range = range; }
    /// Get strand to search in query sequence(s)
    objects::ENa_strand GetStrand() const { return m_Strand; }
    /// Use lowercase masking in FASTA input?
    bool UseLowercaseMasks() const { return m_UseLCaseMask; }
    /// Should the defline be parsed?
    bool GetParseDeflines() const { return m_ParseDeflines; }

    /// Is the query sequence protein?
    bool QueryIsProtein() const { return m_QueryCannotBeNucl; }
private:
    /// Strand(s) to search
    objects::ENa_strand m_Strand;
    /// range to restrict the query sequence(s)
    TSeqRange m_Range;
    /// use lowercase masking in FASTA input
    bool m_UseLCaseMask;
    /// Should the deflines be parsed?
    bool m_ParseDeflines;

    /// only false for blast[xn], and tblastx
    /// true in case of PSI-BLAST
    bool m_QueryCannotBeNucl;  
};

/// Argument class to collect database/subject arguments
class NCBI_BLASTINPUT_EXPORT CBlastDatabaseArgs : public IBlastCmdLineArgs
{
public:
    /// The default priority for subjects, should be used for
    /// subjects/databases
    static const int kSubjectsDataLoaderPriority = 10;

    /// alias for the database molecule type
    typedef CSearchDatabase::EMoleculeType EMoleculeType;

    /// Auxiliary function to determine if the database/subject sequence has
    /// been set
    static bool HasBeenSet(const CArgs& args);

    /// Constructor
    /// @param request_mol_type If true, the command line arguments will
    /// include a mandatory option to disambiguate whether a protein or a
    /// nucleotide database is searched
    /// @param is_rpsblast is it RPS-BLAST?
    CBlastDatabaseArgs(bool request_mol_type = false, bool is_rpsblast = false);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& args, 
                                         CBlastOptions& opts);

    /// Turns on/off database masking support
    void SetDatabaseMaskingSupport(bool val) {
        m_SupportsDatabaseMasking = val;
    }

    /// Is the database/subject protein?
    bool IsProtein() const { return m_IsProtein; }

    /// Get the BLAST database name
    /// @return empty string in the case of BLAST2Sequences, otherwise the
    /// BLAST database name
    string GetDatabaseName() const { 
        return m_SearchDb.Empty() ? kEmptyStr : m_SearchDb->GetDatabaseName(); 
    }

    /// Retrieve the search database information
    CRef<CSearchDatabase> GetSearchDatabase() const { return m_SearchDb; }
    /// Set the search database information.
    /// use case: recovering from search strategy
    void SetSearchDatabase(CRef<CSearchDatabase> search_db) {
        m_SearchDb = search_db;
        m_IsProtein = search_db->IsProtein();
    }

    /// Sets the subject sequences.
    /// use case: recovering from search strategy
    void SetSubjects(CRef<IQueryFactory> subjects, CRef<CScope> scope,
                     bool is_protein) {
        m_Subjects = subjects;
        m_Scope = scope;
        m_IsProtein = is_protein;
    }

    /// Retrieve subject sequences, if provided
    /// @param scope scope to which to sequence read will be added (if
    /// non-NULL) [in]
    /// @return empty CRef<> if no subjects were provided, otherwise a properly
    /// initialized IQueryFactory object
    CRef<IQueryFactory> GetSubjects(objects::CScope* scope = NULL) {
        if (m_Subjects && scope) {
            // m_Scope contains the subject(s) read
            _ASSERT(m_Scope.NotEmpty()); 
            // Add the scope with a lower priority to avoid conflicts
            scope->AddScope(*m_Scope, kSubjectsDataLoaderPriority);
        }
        return m_Subjects; 
    }

protected:
    CRef<CSearchDatabase> m_SearchDb;/**< Description of the BLAST database */
    bool m_RequestMoleculeType;     /**< Determines whether the database's
                                      molecule type should be requested in the
                                      command line, true in case of PSI-BLAST
                                      */
    bool m_IsRpsBlast;              /**< true if the search is RPS-BLAST */

    bool m_IsProtein;               /**< Is the database/subject(s) protein? */
    CRef<IQueryFactory> m_Subjects; /**< The subject sequences */
    CRef<objects::CScope> m_Scope;  /**< CScope object in which all subject
                                      sequences read are kept */
    bool m_SupportsDatabaseMasking; /**< true if it's supported */
};

/// Argument class to collect options specific to igBLAST
class NCBI_BLASTINPUT_EXPORT CIgBlastArgs : public IBlastCmdLineArgs
{
public:
    CIgBlastArgs() { };

    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    CRef<CIgBlastOptions> GetIgBlastOptions() { return m_IgOptions; }

    void AddIgSequenceScope(objects::CScope* scope = NULL) {
        if (scope && m_Scope.NotEmpty()) {
            // Add the scope with a lower priority to avoid conflicts
            scope->AddScope(*m_Scope, 
                  CBlastDatabaseArgs::kSubjectsDataLoaderPriority);
        }
    }

private:
    /// Igblast options to fill
    CRef<CIgBlastOptions> m_IgOptions;
    /// scope to get sequences
    CRef<objects::CScope> m_Scope;
};

/// Argument class to collect formatting options, use this to create a 
/// CBlastFormat object.
/// @note This object is also needed to set the maximum number of target
/// sequences to save (hitlist size)
class NCBI_BLASTINPUT_EXPORT CFormattingArgs : public IBlastCmdLineArgs
{
public:
    /// Defines the output formats supported by our command line formatter
    enum EOutputFormat {
        /// Standard pairwise alignments
        ePairwise = 0,                  
        ///< Query anchored showing identities
        eQueryAnchoredIdentities,
        ///< Query anchored no identities
        eQueryAnchoredNoIdentities,
        ///< Flat query anchored showing identities
        eFlatQueryAnchoredIdentities,
        ///< Flat query anchored no identities
        eFlatQueryAnchoredNoIdentities,
        /// XML output
        eXml,
        /// Tabular output
        eTabular,
        /// Tabular output with comments
        eTabularWithComments,
        /// ASN.1 text output
        eAsnText,
        /// ASN.1 binary output
        eAsnBinary,
        /// Comma-separated values
        eCommaSeparatedValues,
        /// BLAST archive format
        eArchiveFormat,
        /// Sentinel value for error checking
        eEndValue
    };

    /// Default constructor
    CFormattingArgs()
        : m_OutputFormat(ePairwise), m_ShowGis(false), 
        m_NumDescriptions(align_format::kDfltArgNumDescriptions),
        m_NumAlignments(align_format::kDfltArgNumAlignments), m_Html(false)
    {};

    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& args, 
                                         CBlastOptions& opts);

    /// Parses the output format command line option value, returns the
    /// requested output format type and any custom output formats (if
    /// any and applicable)
    /// @param args Command line arguments object [in]
    /// @param fmt_type Output format type requested in command line options
    /// [out]
    /// @param custom_fmt_spec Custom output format specification in command
    /// line options [out]
    static void
    ParseFormattingString(const CArgs& args,
                          EOutputFormat& fmt_type,
                          string& custom_fmt_spec);

    /// Get the choice of formatted output
    EOutputFormat GetFormattedOutputChoice() const {
        return m_OutputFormat;
    }

    /// Returns true if the desired output format is structured (needed to
    /// determine whether to print or not that a PSI-BLAST search has
    /// converged - this is not supported in structured formats)
    bool HasStructuredOutputFormat() const {
        return m_OutputFormat == eXml || 
            m_OutputFormat == eAsnText ||
            m_OutputFormat == eAsnBinary;
    }

    /// Display the NCBI GIs in formatted output?
    bool ShowGis() const {
        return m_ShowGis;
    }
    /// Number of one-line descriptions to show in traditional BLAST output
    TSeqPos GetNumDescriptions() const {
        return m_NumDescriptions;
    }
    /// Number of alignments to show in traditional BLAST output
    TSeqPos GetNumAlignments() const {
        return m_NumAlignments;
    }
    /// Display HTML output?
    bool DisplayHtmlOutput() const {
        return m_Html;
    }

    /// Retrieve for string that specifies the custom output format for tabular
    /// and comma-separated value
    string GetCustomOutputFormatSpec() const { 
        return m_CustomOutputFormatSpec; 
    }

    bool ArchiveFormatRequested(const CArgs& args) const;

private:
    EOutputFormat m_OutputFormat;   ///< Choice of formatting output
    bool m_ShowGis;                 ///< Display NCBI GIs?
    TSeqPos m_NumDescriptions;       ///< Number of 1-line descr. to show
    TSeqPos m_NumAlignments;         ///< Number of alignments to show
    bool m_Html;                    ///< Display HTML output?
    /// The format specification for custom output, e.g.: tabular or
    /// comma-separated value (populated if applicable)
    string m_CustomOutputFormatSpec;
};

/// Argument class to collect multi-threaded arguments
class NCBI_BLASTINPUT_EXPORT CMTArgs : public IBlastCmdLineArgs
{
public:
    /// Default Constructor
    CMTArgs() : m_NumThreads(CThreadable::kMinNumThreads) {}
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    /// Get the number of threads to spawn
    size_t GetNumThreads() const { return m_NumThreads; }
private:
    size_t m_NumThreads;        ///< Number of threads to spawn
};

/// Argument class to collect remote vs. local execution
class NCBI_BLASTINPUT_EXPORT CRemoteArgs : public IBlastCmdLineArgs
{
public:
    /// Default constructor
    CRemoteArgs() : m_IsRemote(false) {}
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    /// Return whether the search should be executed remotely or not
    bool ExecuteRemotely() const { return m_IsRemote; }

private:
    /// Should the search be executed remotely?
    bool m_IsRemote;
};

/// Argument class to collect debugging options.
/// Only show in command line if compiled with _DEBUG
class NCBI_BLASTINPUT_EXPORT CDebugArgs : public IBlastCmdLineArgs
{
public:
    /// Default constructor
    CDebugArgs() : m_DebugOutput(false), m_RmtDebugOutput(false) {}
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    /// Return whether debug (verbose) output should be produced on remote
    /// searches (only available when compiled with _DEBUG)
    bool ProduceDebugRemoteOutput() const { return m_RmtDebugOutput; }
    /// Return whether debug (verbose) output should be produced
    /// (only available when compiled with _DEBUG)
    bool ProduceDebugOutput() const { return m_DebugOutput; }
private:

    /// Should debugging (verbose) output be printed
    bool m_DebugOutput;
    /// Should debugging (verbose) output be printed for remote BLAST
    bool m_RmtDebugOutput;
};

/// Argument class to retrieve options for filtering HSPs (e.g.: culling
/// options, best hit algorithm options)
class NCBI_BLASTINPUT_EXPORT CHspFilteringArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& args, 
                                         CBlastOptions& opts);
};

/// Argument class to retrieve megablast database indexing options
class NCBI_BLASTINPUT_EXPORT CMbIndexArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& args, 
                                         CBlastOptions& opts);

    /// Auxiliary function to determine if the megablast database indexing
    /// options have been set
    static bool HasBeenSet(const CArgs& args);
};

/// Type definition of a container of IBlastCmdLineArgs
typedef vector< CRef<IBlastCmdLineArgs> > TBlastCmdLineArgs;


/// Base command line argument class for a generic BLAST command line binary
class NCBI_BLASTINPUT_EXPORT CBlastAppArgs : public CObject
{
public:
    /// Default constructor
    CBlastAppArgs();
    /// Our virtual destructor
    virtual ~CBlastAppArgs() {}

    /// Set the command line arguments
    CArgDescriptions* SetCommandLine();

    /// Get the task for this object
    string  GetTask() const {
        return m_Task;
    }

    /// Set the task for this object
    /// @param task task name to set [in]
    void SetTask(const string& task);

    /// Extract the command line arguments into a CBlastOptionsHandle object
    /// @param args Commad line arguments [in]
    CRef<CBlastOptionsHandle> SetOptions(const CArgs& args);

    /// Setter for the BLAST options handle, this is used if the options are
    /// recovered from a saved BLAST search strategy
    void SetOptionsHandle(CRef<CBlastOptionsHandle> opts_hndl) {
        m_OptsHandle = opts_hndl;
    }

    /// Get the BLAST database arguments
    CRef<CBlastDatabaseArgs> GetBlastDatabaseArgs() const {
        return m_BlastDbArgs;
    }
    /// Set the BLAST database arguments
    void SetBlastDatabaseArgs(CRef<CBlastDatabaseArgs> args) {
        m_BlastDbArgs = args;
    }

    /// Get the options for the query sequence(s)
    CRef<CQueryOptionsArgs> GetQueryOptionsArgs() const {
        return m_QueryOptsArgs;
    }

    /// Get the formatting options
    CRef<CFormattingArgs> GetFormattingArgs() const {
        return m_FormattingArgs;
    }

    /// Get the number of threads to spawn
    size_t GetNumThreads() const {
        return m_MTArgs->GetNumThreads();
    }

    /// Get the input stream
    CNcbiIstream& GetInputStream() const {
        return m_StdCmdLineArgs->GetInputStream();
    }
    /// Get the output stream
    CNcbiOstream& GetOutputStream() const {
        return m_StdCmdLineArgs->GetOutputStream();
    }

    /// Set the input stream to a temporary input file (needed when importing
    /// a search strategy)
    /// @param input_file temporary input file to read [in]
    void SetInputStream(CRef<CTmpFile> input_file) {
        m_StdCmdLineArgs->SetInputStream(input_file);
    }

    /// Get the input stream for the search strategy
    CNcbiIstream* GetImportSearchStrategyStream(const CArgs& args) {
        return m_SearchStrategyArgs->GetImportStream(args);
    }
    /// Get the output stream for the search strategy
    CNcbiOstream* GetExportSearchStrategyStream(const CArgs& args) {
        return m_SearchStrategyArgs->GetExportStream(args);
    }

    /// Determine whether the search should be executed remotely or not
    bool ExecuteRemotely() const {
        return m_RemoteArgs->ExecuteRemotely();
    }

    /// Return whether debug (verbose) output should be produced on remote
    /// searches (only available when compiled with _DEBUG)
    bool ProduceDebugRemoteOutput() const { 
        return m_DebugArgs->ProduceDebugRemoteOutput();
    }

    /// Return whether debug (verbose) output should be produced on remote
    /// searches (only available when compiled with _DEBUG)
    bool ProduceDebugOutput() const { 
        return m_DebugArgs->ProduceDebugOutput();
    }

    /// Get the query batch size
    virtual int GetQueryBatchSize() const = 0;

    /// Retrieve the client ID for remote requests
    string GetClientId() const {
        _ASSERT( !m_ClientId.empty() );
        return m_ClientId;
    }

protected:
    /// Set of command line argument objects
    TBlastCmdLineArgs m_Args;
    /// query options object
    CRef<CQueryOptionsArgs> m_QueryOptsArgs;
    /// database/subject object
    CRef<CBlastDatabaseArgs> m_BlastDbArgs;
    /// formatting options
    CRef<CFormattingArgs> m_FormattingArgs;
    /// multi-threaded options
    CRef<CMTArgs> m_MTArgs;
    /// remote vs. local execution options
    CRef<CRemoteArgs> m_RemoteArgs;
    /// standard command line arguments class
    CRef<CStdCmdLineArgs> m_StdCmdLineArgs;
    /// arguments for dealing with search strategies
    CRef<CSearchStrategyArgs> m_SearchStrategyArgs;
    /// Debugging arguments
    CRef<CDebugArgs> m_DebugArgs;
    /// HSP filtering arguments
    CRef<CHspFilteringArgs> m_HspFilteringArgs;
    /// The BLAST options handle, only non-NULL if assigned via
    /// SetOptionsHandle, i.e.: from a saved search strategy
    CRef<CBlastOptionsHandle> m_OptsHandle;
    /// Task specified in the command line
    string m_Task;
    /// Client ID used for remote BLAST submissions, must be populated by
    /// subclasses
    string m_ClientId;
    /// Is this application being run ungapped
    bool m_IsUngapped;

    /// Create the options handle based on the command line arguments
    /// @param locality whether the search will be executed locally or remotely
    /// [in]
    /// @param args command line arguments [in]
    virtual CRef<CBlastOptionsHandle>
    x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                          const CArgs& args) = 0;
};

/** 
 * @brief Create a CArgDescriptions object and invoke SetArgumentDescriptions
 * for each of the TBlastCmdLineArgs in its argument list
 * 
 * @param args arguments to configure the return value [in]
 * 
 * @return a CArgDescriptions object with the command line options set
 */
NCBI_BLASTINPUT_EXPORT
CArgDescriptions* 
SetUpCommandLineArguments(TBlastCmdLineArgs& args);

END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___BLAST_ARGS__HPP */

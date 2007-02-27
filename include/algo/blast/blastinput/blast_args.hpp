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
#include <algo/blast/blastinput/cmdline_flags.hpp>

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
class NCBI_XBLAST_EXPORT IBlastCmdLineArgs : public CObject
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
     * the CBlastOptions object
     * @param cmd_line_args Command line arguments parsed by the NCBI
     * application framework [in]
     * @param options object to which the appropriate options will be set
     * [in|out]
     */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options) {}
};

/** Argument class to retrieve input and output streams for a command line
 * program.
 */
class NCBI_XBLAST_EXPORT CStdCmdLineArgs : public IBlastCmdLineArgs
{
public:
    CStdCmdLineArgs() : m_InputStream(0), m_OutputStream(0) {};
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
    CNcbiIstream& GetInputStream() const;
    CNcbiOstream& GetOutputStream() const;

private:
    CNcbiIstream* m_InputStream;
    CNcbiOstream* m_OutputStream;
};

/** Argument class to populate a program's name and description */
class NCBI_XBLAST_EXPORT CProgramDescriptionArgs : public IBlastCmdLineArgs
{
public:
    CProgramDescriptionArgs(const string& program_name, 
                            const string& program_description);
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);

private:
    string m_ProgName;
    string m_ProgDesc;
};

/** Argument class to retrieve and set the window size BLAST algorithm 
 * option */
class NCBI_XBLAST_EXPORT CWindowSizeArg : public IBlastCmdLineArgs
{
public:
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /// @note this depends on the matrix already being set...
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/** Argument class to retrieve and set the word threshold BLAST algorithm 
 * option */
class NCBI_XBLAST_EXPORT CWordThresholdArg : public IBlastCmdLineArgs
{
public:
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /// @note this depends on the matrix already being set...
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/** Argument class to retrieve and set the scoring matrix name BLAST algorithm
 * option */
class NCBI_XBLAST_EXPORT CMatrixNameArg : public IBlastCmdLineArgs
{
public:
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/** Argument class for general search BLAST algorithm options: evalue, gap
 * penalties, query filter string, ungapped x-drop, initial and final gapped 
 * x-drop, word size, and effective search space
 */
class NCBI_XBLAST_EXPORT CGenericSearchArgs : public IBlastCmdLineArgs
{
public:
    CGenericSearchArgs(bool query_is_protein = true)
        : m_QueryIsProtein(query_is_protein) {}
         
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
private:
    bool m_QueryIsProtein;  /**< true if the query is protein */
};

class NCBI_XBLAST_EXPORT CFilteringArgs : public IBlastCmdLineArgs
{
public:
    CFilteringArgs(bool query_is_protein = true)
        : m_QueryIsProtein(query_is_protein) {}

    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
private:
    bool m_QueryIsProtein;  /**< true if the query is protein */

    void x_TokenizeFilteringArgs(const string& filtering_args,
                                 vector<string>& output) const;
};

/// Defines values for match and mismatch in nucleotide comparisons
class NCBI_XBLAST_EXPORT CNuclArgs : public IBlastCmdLineArgs
{
public:
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

class NCBI_XBLAST_EXPORT CCompositionBasedStatsArgs : public IBlastCmdLineArgs
{
public:
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

class NCBI_XBLAST_EXPORT CGappedArgs : public IBlastCmdLineArgs
{
public:
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

class NCBI_XBLAST_EXPORT CLargestIntronSizeArgs : public IBlastCmdLineArgs
{
public:
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/// frame shift penalty for out-of-frame searches
class NCBI_XBLAST_EXPORT CFrameShiftArgs : public IBlastCmdLineArgs
{
public:
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

class NCBI_XBLAST_EXPORT CGeneticCodeArgs : public IBlastCmdLineArgs
{
public:
    enum ETarget {
        eQuery,         ///< Query genetic code
        eDatabase       ///< Database genetic code
    };
    CGeneticCodeArgs(ETarget t) : m_Target(t) {};

    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

private:
    ETarget m_Target;
};

class NCBI_XBLAST_EXPORT CGapTriggerArgs : public IBlastCmdLineArgs
{
public:
    CGapTriggerArgs(bool query_is_protein) 
        : m_QueryIsProtein(query_is_protein) {}
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
private:
    bool m_QueryIsProtein;  /**< true if the query is protein */
};

class NCBI_XBLAST_EXPORT CPssmEngineArgs : public IBlastCmdLineArgs
{
public:
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

class NCBI_XBLAST_EXPORT CPsiBlastArgs : public IBlastCmdLineArgs
{
public:
    enum ETargetDatabase {
        eProteinDb,         ///< Traditional, iterated PSI-BLAST
        eNucleotideDb       ///< PSI-Tblastn, non-iterated
    };

    CPsiBlastArgs(ETargetDatabase db_target = eProteinDb) 
        : m_DbTarget(db_target), m_NumIterations(1),
        m_CheckPointOutputStream(0), m_AsciiMatrixOutputStream(0),
        m_CheckPointInputStream(0), m_MSAInputStream(0)
    {};

    virtual ~CPsiBlastArgs() {
        if (m_CheckPointOutputStream) {
            delete m_CheckPointOutputStream;
        }
        if (m_AsciiMatrixOutputStream) {
            delete m_AsciiMatrixOutputStream;
        }
        if (m_CheckPointInputStream) {
            delete m_CheckPointInputStream;
        }
        if (m_MSAInputStream) {
            delete m_MSAInputStream;
        }
    }

    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    size_t GetNumberOfIterations() const { 
        return m_NumIterations; 
    }
    CNcbiOstream* GetCheckPointOutputStream() const { 
        return m_CheckPointOutputStream; 
    }
    CNcbiOstream* GetAsciiMatrixFile() const { 
        return m_AsciiMatrixOutputStream; 
    }
    CNcbiIstream* GetMSAInputFile() const { 
        return m_MSAInputStream; 
    }
    // this is valid in both eProteinDb and eNucleotideDb
    CNcbiIstream* GetCheckPointInputStream() const { 
        return m_CheckPointInputStream; 
    }

    CRef<objects::CPssmWithParameters> GetPssm() const {
        return m_Pssm;
    }

private:
    ETargetDatabase m_DbTarget;
    size_t m_NumIterations;
    CNcbiOstream* m_CheckPointOutputStream;
    CNcbiOstream* m_AsciiMatrixOutputStream;
    CNcbiIstream* m_CheckPointInputStream;
    CNcbiIstream* m_MSAInputStream;
    CRef<objects::CPssmWithParameters> m_Pssm;
};

/*****************************************************************************/
// Input options

class NCBI_XBLAST_EXPORT CQueryOptionsArgs : public IBlastCmdLineArgs
{
public:
    CQueryOptionsArgs(bool query_cannot_be_nucl = false)
        : m_Strand(objects::eNa_strand_unknown), m_Range(),
        m_UseLCaseMask(false), m_BelieveQueryDefline(false),
        m_QueryCannotBeNucl(query_cannot_be_nucl)
    {};

    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    TSeqRange GetRange() const { return m_Range; }
    objects::ENa_strand GetStrand() const { return m_Strand; }
    bool UseLowercaseMasks() const { return m_UseLCaseMask; }
    bool BelieveQueryDefline() const { return m_BelieveQueryDefline; }

private:
    objects::ENa_strand m_Strand;
    TSeqRange m_Range;
    bool m_UseLCaseMask;
    bool m_BelieveQueryDefline;
    /// only false for blast[xn], and tblastx
    bool m_QueryCannotBeNucl;  // true in case of PSI-BLAST
};

class NCBI_XBLAST_EXPORT CBlastDatabaseArgs : public IBlastCmdLineArgs
{
public:
    typedef CSearchDatabase::TGiList TGiList;
    typedef CSearchDatabase::EMoleculeType EMoleculeType;

    /// Constructor
    /// @param request_mol_type If true, the command line arguments will
    /// include a mandatory option to disambiguate whether a protein or a
    /// nucleotide database is searched
    CBlastDatabaseArgs(bool request_mol_type = false);
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& args, 
                                         CBlastOptions& opts);

    bool IsProtein() const;
    /// The return value of this should be set in the CSeqDb ctor
    string GetGiListFileName() const { return m_GiListFileName; }
    string GetDatabaseName() const { return m_SearchDb->GetDatabaseName(); }

    /// Retrieve the search database information
    CRef<CSearchDatabase> GetSearchDatabase() const { return m_SearchDb; }

private:
    CRef<CSearchDatabase> m_SearchDb;/**< Description of the BLAST database */
    string m_GiListFileName;        /**< File name of gi list DB restriction */
    bool m_RequestMoleculeType;     /**< Determines whether the database's
                                      molecule type should be requested in the
                                      command line, true in case of PSI-BLAST
                                      */
};

// Use this to create a CBlastFormat object
class NCBI_XBLAST_EXPORT CFormattingArgs : public IBlastCmdLineArgs
{
public:
    CFormattingArgs()
        : m_FormattedOutputChoice(0), m_ShowGis(false), 
        m_NumDescriptions(kDfltArgNumDescriptions),
        m_NumAlignments(kDfltArgNumAlignments), m_Html(false)
    {};

    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& args, 
                                         CBlastOptions& opts);

    int GetFormattedOutputChoice() const {
        return m_FormattedOutputChoice;
    }
    bool ShowGis() const {
        return m_ShowGis;
    }
    size_t GetNumDescriptions() const {
        return m_NumDescriptions;
    }
    size_t GetNumAlignments() const {
        return m_NumAlignments;
    }
    bool DisplayHtmlOutput() const {
        return m_Html;
    }

private:
    int m_FormattedOutputChoice;
    bool m_ShowGis;
    size_t m_NumDescriptions;
    size_t m_NumAlignments;
    bool m_Html;
};

class NCBI_XBLAST_EXPORT CMTArgs : public IBlastCmdLineArgs
{
public:
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    size_t GetNumThreads() const { return m_NumThreads; }
private:
    size_t m_NumThreads;
};

class NCBI_XBLAST_EXPORT CRemoteArgs : public IBlastCmdLineArgs
{
public:
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    bool ExecuteRemotely() const { return m_IsRemote; }
private:
    bool m_IsRemote;
};

class NCBI_XBLAST_EXPORT CCullingArgs : public IBlastCmdLineArgs
{
public:
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    virtual void ExtractAlgorithmOptions(const CArgs& args, 
                                         CBlastOptions& opts);
};

/// Type definition of a container of IBlastCmdLineArgs
typedef vector< CRef<IBlastCmdLineArgs> > TBlastCmdLineArgs;


/// Base class for a generic BLAST command line binary
class NCBI_XBLAST_EXPORT CBlastAppArgs : public CObject
{
public:
    virtual ~CBlastAppArgs() {}

    CArgDescriptions* SetCommandLine();

    CRef<CBlastOptionsHandle> SetOptions(const CArgs& args);

    CRef<CBlastDatabaseArgs> GetBlastDatabaseArgs() const {
        return m_BlastDbArgs;
    }

    CRef<CQueryOptionsArgs> GetQueryOptionsArgs() const {
        return m_QueryOptsArgs;
    }

    CRef<CFormattingArgs> GetFormattingArgs() const {
        return m_FormattingArgs;
    }

    size_t GetNumThreads() const {
        return m_MTArgs->GetNumThreads();
    }

    CNcbiIstream& GetInputStream() const {
        return m_StdCmdLineArgs->GetInputStream();
    }
    CNcbiOstream& GetOutputStream() const {
        return m_StdCmdLineArgs->GetOutputStream();
    }

    bool ExecuteRemotely() const {
        return m_RemoteArgs->ExecuteRemotely();
    }

    virtual int GetQueryBatchSize() const = 0;

protected:
    TBlastCmdLineArgs m_Args;
    CRef<CQueryOptionsArgs> m_QueryOptsArgs;
    CRef<CBlastDatabaseArgs> m_BlastDbArgs;
    CRef<CFormattingArgs> m_FormattingArgs;
    CRef<CMTArgs> m_MTArgs;
    CRef<CRemoteArgs> m_RemoteArgs;
    CRef<CStdCmdLineArgs> m_StdCmdLineArgs;

    virtual CRef<CBlastOptionsHandle>
    x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                          const CArgs& args) = 0;
};

END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___BLAST_ARGS__HPP */

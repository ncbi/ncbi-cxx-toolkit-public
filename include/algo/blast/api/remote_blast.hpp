#ifndef ALGO_BLAST_API___REMOTE_BLAST__HPP
#define ALGO_BLAST_API___REMOTE_BLAST__HPP

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
 * Authors:  Kevin Bealer
 *
 */

/// @file remote_blast.hpp
/// Declares the CRemoteBlast class.


#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blastx_options.hpp>
#include <algo/blast/api/tblastn_options.hpp>
#include <algo/blast/api/tblastx_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>
#include <algo/blast/api/psiblast_options.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/blast/blast__.hpp>
#include <objects/scoremat/Score_matrix_parameters.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// API for Remote Blast Requests
///
/// API Class to facilitate submission of Remote Blast requests.
/// Provides an interface to build a Remote Blast request given an
/// object of a subclass of CBlastOptionsHandle.

class NCBI_XBLAST_EXPORT CRemoteBlast : public CObject
{
public:
    /// Use the specified RID to get results for an existing search.
    CRemoteBlast(const string & RID)
    {
        x_Init(RID);
    }
    
    /// Create a blastp (protein) search.
    CRemoteBlast(CBlastProteinOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "blastp", "plain");
    }
    
    /// Create a blastn (nucleotide) search.
    CRemoteBlast(CBlastNucleotideOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "blastn", "plain");
    }
    
    /// Create a blastx (translated query) search.
    CRemoteBlast(CBlastxOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "blastx", "plain");
    }
    
    /// Create a tblastn (translated database) search.
    CRemoteBlast(CTBlastnOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "tblastn", "plain");
    }
    
    /// Create a tblastx search, translating both query and database.
    CRemoteBlast(CTBlastxOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "tblastx", "plain");
    }
    
    /// Create a Discontiguous Megablast search.
    CRemoteBlast(CDiscNucleotideOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "blastn", "megablast");
    }
    
    /// Create a PSI-Blast search (only protein is supported).
    CRemoteBlast(CPSIBlastOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "blastp", "psi");
    }
    
    /// Destruct the search object.
    ~CRemoteBlast()
    {
    }
    
    /// This restricts the subject database to this list of GIs (this is not
    /// supported yet on the server end).
    void SetGIList(list<Int4> & gi_list)
    {
        if (gi_list.empty()) {
            NCBI_THROW(CBlastException, eBadParameter,
                       "Empty gi_list specified.");
        }
        x_SetOneParam("GiList", & gi_list);
    }
    
    /// Set the name of the database to search against.
    void SetDatabase(const string & x)
    {
        if (x.empty()) {
            NCBI_THROW(CBlastException, eBadParameter,
                       "Empty string specified for database.");
        }
        SetDatabase(x.c_str());
    }
    
    /// Alternate interface to set database name.
    void SetDatabase(const char * x);
    
    /// Restrict search to sequences matching this Entrez query.
    void SetEntrezQuery(const char * x)
    {
        if (!x) {
            NCBI_THROW(CBlastException, eBadParameter,
                       "NULL specified for entrez query.");
        }
        x_SetOneParam("EntrezQuery", &x);
    }
    
    /// Set the query as a Bioseq_set.
    void SetQueries(CRef<objects::CBioseq_set> bioseqs);
    
    /// Set the query as a list of Seq_locs.
    /// @param seqlocs One interval Seq_loc or a list of whole Seq_locs.
    void SetQueries(list< CRef<objects::CSeq_loc> > & seqlocs);
    
    /// Set a PSSM query (as for PSI blast), which must include a bioseq set.
    void SetQueries(CRef<objects::CScore_matrix_parameters> pssm);
    
    
    /// Set the PSSM matrix for a PSI-Blast search.
    /// 
    /// A PSI-Blast search can be configured by specifying the PSSM (including
    /// an internal query) in SetQueries, or by using this option to set the
    /// matrix and using a Bioseq_set with SetQueries().  The former method is
    /// newer and this option may be removed in the future.
    
    void SetMatrixTable(CRef<objects::CScore_matrix_parameters> matrix)
    {
        if (matrix.Empty()) {
            NCBI_THROW(CBlastException, eBadParameter,
                       "Empty reference for matrix.");
        }
        x_SetOneParam("MatrixTable", matrix);
    }
    
    /* Getting Results */
    
    
    /// This submits the search (if necessary) and polls for results.
    ///
    /// If a new search is configured, and not already submitted, this will
    /// submit it.  It then polls for results until either completion or error
    /// state is reached, or until the search times out.  The polling is done
    /// at an increasing interval, which starts out at 10 seconds, increasing
    /// by 30% after each check to a maximum of 300 seconds per sleep.
    ///
    /// @return true if the search was submitted, otherwise false.
    bool SubmitSync(void)
    {
        return SubmitSync( DefaultTimeout() );
    }
    
    /// This submits the search (if necessary) and polls for results.
    ///
    /// If a new search is configured, and not already submitted, this will
    /// submit it.  It then polls for results until either completion or error
    /// state is reached, or until the search times out.  The polling is done
    /// at an increasing interval, which starts out at 10 seconds, increasing
    /// by 30% after each check to a maximum of 300 seconds per sleep.
    ///
    /// @param timeout Search timeout specified as a number of seconds.
    /// @return true if the search was submitted, otherwise false.
    bool SubmitSync(int timeout);
    
    /// This submits the search (if necessary) and returns immediately.
    ///
    /// If a new search is configured, and not already submitted, this will
    /// submit it.  It then polls for results until either completion or error
    /// state is reached, or until the search times out.  The polling is done
    /// at an increasing interval, which starts out at 10 seconds, increasing
    /// by 30% after each check to a maximum of 300 seconds per sleep.
    ///
    /// @return true if the search was submitted, otherwise false.
    bool Submit(void);
    
    /// Check whether the search has completed.
    ///
    /// This checks the status of the search.  Please delay several seconds
    /// between subsequent calls.
    ///
    /// @return true If search is not still running.
    bool CheckDone(void);
    
    /// This returns a string containing any errors that were produced by the
    /// search.  A successful search should return an empty string here.
    ///
    /// @return An empty string or a newline seperated list of errors.
    string GetErrors(void);
    
    /// This returns any warnings encountered.  These do not necessarily
    /// indicate an error or even a potential error; some warnings are always
    /// returned from certain types of searches.  (This is a debugging feature
    /// and warnings should probably not be returned to the end-user).
    ///
    /// @return Empty string or newline seperated list of warnings.
    string GetWarnings(void);
    
    /* Getting Results */
    
    /// Gets the request id (RID) associated with the search.
    ///
    /// If the search was not successfully submitted, this will be empty.
    const string & GetRID(void)
    {
        return m_RID;
    }
    
    /// Get the seqalign set from the results.
    /// @return Reference to a seqalign set.
    CRef<objects::CSeq_align_set> GetAlignments(void);
    
    /// Get the results of a PHI-Align request, if PHI pattern was set.
    /// @return Reference to PHI alignment set.
    CRef<objects::CBlast4_phi_alignments> GetPhiAlignments(void);
    
    /// Get the query mask from the results.
    /// @return Reference to a Blast4_mask object.
    CRef<objects::CBlast4_mask> GetMask(void);
    
    /// Get the Karlin/Altschul parameter blocks produced by the search.
    /// @return List of references to KA blocks.
    list< CRef<objects::CBlast4_ka_block > > GetKABlocks(void);
    
    /// Get the search statistics block as a list of strings.
    /// 
    /// Search statistics describe the data flow during each step of a BLAST
    /// search.  They are subject to change, and are not formally defined, but
    /// can sometimes provide insight when investigating software problems.
    /// 
    /// @return List of strings, each contains one line of search stats block.
    list< string > GetSearchStats(void);
    
    /// Get the PSSM produced by the search.
    /// @return Reference to a Score-matrix-parameters object.
    CRef<objects::CScore_matrix_parameters> GetPSSM(void);
    
    /// Debugging support can be turned on with eDebug or off with eSilent.
    enum EDebugMode {
        eDebug = 0,
        eSilent
    };
    
    /// Adjust the debugging level.
    ///
    /// This causes debugging trace data to be dumped to standard output,
    /// along with ASN.1 objects used during the search and other text.  It
    /// produces a great deal of output, none of which is expected to be
    /// useful to the end-user.
    void SetVerbose(EDebugMode verb = eDebug)
    {
        m_Verbose = verb;
    }
    
private:
    /// An alias for the most commonly used part of the Blast4 search results.
    typedef objects::CBlast4_get_search_results_reply TGSRR;
    
    /// Various states the search can be in.
    ///
    /// eStart  Not submitted, can still be configured.
    /// eFailed An error was encountered.
    /// eWait   The search is still running.
    /// eDone   Results are available.
    enum EState {
        eStart = 0,
        eFailed,
        eWait,
        eDone
    };
    
    /// Indicates whether to use async mode.
    enum EImmediacy {
        ePollAsync = 0,
        ePollImmed
    };
    
    /// This class attempts to verify whether all necessary configuration is
    /// complete before attempting to submit the search.
    enum ENeedConfig {
        eNoConfig = 0x0,
        eProgram  = 0x1,
        eService  = 0x2,
        eQueries  = 0x4,
        eSubject  = 0x8,
        eNeedAll  = 0xF
    };
    
    /// The default timeout is 3.5 hours.
    const int DefaultTimeout(void)
    {
        return int(3600*3.5);
    }
    
    /// Called by new search constructors: initialize a new search.
    void x_Init(CBlastOptionsHandle * algo_opts,
                const char          * program,
                const char          * service);
    
    /// Called by RID constructor: set up monitoring of existing search.
    void x_Init(const string & RID);
    
    /// Configure new search from options handle passed to constructor.
    void x_SetAlgoOpts(void);
    
    /// Set an integer parameter (not used yet).
    /// @param name Name of option.
    /// @param value Pointer to integer value to use.
    void x_SetOneParam(const char * name, const int * value);
    
    /// Set a list of integers.
    /// @param name Name of option.
    /// @param value Pointer to list of integers to use.
    void x_SetOneParam(const char * name, const list<int> * value);
    
    /// Set a string parameter.
    /// @param name Name of option.
    /// @param value Pointer to pointer to null delimited string.
    void x_SetOneParam(const char * name, const char ** value);
    
    /// Set a matrix parameter.
    /// @param name Name of option (probably MatrixTable).
    /// @param matrix Pointer to Score-matrix-parameters object.
    void x_SetOneParam(const char * name, 
                       objects::CScore_matrix_parameters * matrix);
    
    /// Determine what state the search is in.
    EState x_GetState(void);
    
    /// Poll until done, return the CBlast4_get_search_results_reply.
    /// @return Pointer to GSR reply object or NULL if search failed.
    TGSRR * x_GetGSRR(void);
    
    /// Send a Blast4 request and get a reply.
    /// @return The blast4 server response.
    CRef<objects::CBlast4_reply>
    x_SendRequest(CRef<objects::CBlast4_request_body> body);
    
    /// Try to get the search results.
    /// @return The blast4 server response.
    CRef<objects::CBlast4_reply>
    x_GetSearchResults(void);
    
    /// Verify that search object contains mandatory fields.
    void x_CheckConfig(void);
    
    /// Submit the search and process results (of submit action).
    void x_SubmitSearch(void);
    
    /// Try to get and process results.
    void x_CheckResults(void);
    
    /// Iterate over error list, splitting into errors and warnings.
    void x_SearchErrors(CRef<objects::CBlast4_reply> reply);
    
    /// Poll until results are found, error occurs, or timeout expires.
    void x_PollUntilDone(EImmediacy poll_immed, int seconds);
    
    
    // Data
    
    
    /// Options for new search.
    CRef<blast::CBlastOptionsHandle>            m_CBOH;
    
    /// Request object for new search.
    CRef<objects::CBlast4_queue_search_request> m_QSR;
    
    /// Results of BLAST search.
    CRef<objects::CBlast4_reply>                m_Reply;
    
    /// List of errors encountered.
    vector<string> m_Errs;
    
    /// List of warnings encountered.
    vector<string> m_Warn;

    /// Request ID of submitted or pre-existing search.
    string         m_RID;
    
    /// Count of server glitches (communication errors) to ignore.
    int         m_ErrIgn;
    
    /// Pending state: indicates that search still needs to be queued.
    bool        m_Pending;
    
    /// Verbosity mode: whether to produce debugging info on stdout.
    EDebugMode  m_Verbose;
    
    /// Bitfield to track whether all necessary configuration is done.
    ENeedConfig m_NeedConfig;
};


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2004/06/09 15:55:29  bealer
 * - Fix comments and use enum for return type of x_GetState().
 *
 * Revision 1.7  2004/06/08 17:56:25  bealer
 * - Add documentation for all methods and members.
 *
 * Revision 1.6  2004/05/17 18:07:19  bealer
 * - Add PSI Blast support.
 *
 * Revision 1.5  2004/05/05 15:28:19  bealer
 * - Add GetWarnings() mechanism.
 * - Add PSSM queries (for PSI-Blast).
 * - Add seq-loc-list queries (allows multiple identifier base queries, or
 *   one query based on identifier plus interval.
 * - Add GetPSSM() to retrieve results of PSI-Blast run.
 *
 * Revision 1.4  2004/04/12 16:36:37  bealer
 * - More parameter checking.
 *
 * Revision 1.3  2004/03/23 22:30:27  bealer
 * - Verify that CRemoteBlast objects are configured properly.
 *
 * Revision 1.2  2004/03/19 18:56:17  camacho
 * Correct use of namespaces
 *
 * Revision 1.1  2004/02/18 17:30:22  bealer
 * - Rename of blast4_options, plus changes:
 *   - Add timeout for SubmitSync
 *   - Change bools to enums
 *
 * Revision 1.7  2004/02/09 22:36:06  bealer
 * - Delay examination of CBlastOptionsHandle object until Submit() action.
 *
 * Revision 1.6  2004/02/06 00:15:39  bealer
 * - Add RID capability.
 *
 * Revision 1.5  2004/02/05 19:21:05  bealer
 * - Add retry capability to API code.
 *
 * Revision 1.4  2004/02/05 00:38:07  bealer
 * - Polling optimization.
 *
 * Revision 1.3  2004/02/04 22:31:24  bealer
 * - Add async interface to Blast4 API.
 * - Clean up, simplify code and interfaces.
 * - Add state-based logic to promote robustness.
 *
 * Revision 1.2  2004/01/17 04:28:25  ucko
 * Call x_SetOneParam directly rather than via x_SetParam, which was
 * giving GCC 2.95 trouble.
 *
 * Revision 1.1  2004/01/16 20:40:21  bealer
 * - Add CBlast4Options class (Blast 4 API)
 *
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API___REMOTE_BLAST__HPP */

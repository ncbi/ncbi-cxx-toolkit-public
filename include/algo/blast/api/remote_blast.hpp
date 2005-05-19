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

#include <objects/blast/blast__.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    /// forward declaration of ASN.1 object containing PSSM (scoremat.asn)
    class CPssmWithParameters;
    class CBioseq_set;
    class CSeq_loc;
    class CSeq_id;
    class CSeq_align_set;
END_SCOPE(objects)

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
    CRemoteBlast(const string & RID);
    
    /// Create a blastp (protein) search.
    CRemoteBlast(CBlastProteinOptionsHandle * algo_opts);
    
    /// Create a blastn (nucleotide) search.
    CRemoteBlast(CBlastNucleotideOptionsHandle * algo_opts);
    
    /// Create a blastx (translated query) search.
    CRemoteBlast(CBlastxOptionsHandle * algo_opts);
    
    /// Create a tblastn (translated database) search.
    CRemoteBlast(CTBlastnOptionsHandle * algo_opts);
    
    /// Create a tblastx search, translating both query and database.
    CRemoteBlast(CTBlastxOptionsHandle * algo_opts);
    
    /// Create a Discontiguous Megablast search.
    CRemoteBlast(CDiscNucleotideOptionsHandle * algo_opts);
    
    /// Create a PSI-Blast search (only protein is supported).
    CRemoteBlast(CPSIBlastOptionsHandle * algo_opts);
    
    /// Create a search using any kind of options handle.
    CRemoteBlast(CBlastOptionsHandle * any_opts);
    
    /// Destruct the search object.
    ~CRemoteBlast();
    
    /// This restricts the subject database to this list of GIs (this is not
    /// supported yet on the server end).
    void SetGIList(list<Int4> & gi_list);
    
    /// Set the name of the database to search against.
    void SetDatabase(const string & x);
    
    /// Alternate interface to set database name.
    void SetDatabase(const char * x);
    
    /// Restrict search to sequences matching this Entrez query.
    void SetEntrezQuery(const char * x);
    
    /// Set the query as a Bioseq_set.
    void SetQueries(CRef<objects::CBioseq_set> bioseqs);
    
    /// Set the query as a list of Seq_locs.
    /// @param seqlocs One interval Seq_loc or a list of whole Seq_locs.
    void SetQueries(list< CRef<objects::CSeq_loc> > & seqlocs);
    
    /// Set a PSSM query (as for PSI blast), which must include a bioseq set.
    void SetQueries(CRef<objects::CPssmWithParameters> pssm);
    
    
    /// Set the PSSM matrix for a PSI-Blast search.
    /// 
    /// A PSI-Blast search can be configured by specifying the PSSM (including
    /// an internal query) in SetQueries, or by using this option to set the
    /// matrix and using a Bioseq_set with SetQueries().  The former method is
    /// newer and this option may be removed in the future.
    /// @deprecated
    
    void SetMatrixTable(CRef<objects::CPssmWithParameters> matrix);
    
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
    bool SubmitSync(void);
    
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
    /// This checks the status of the search.  Please delay at least
    /// 10 seconds between subsequent calls.  If this function returns
    /// true, it will already have gotten the results as part of its
    /// processing.  With the common technique of polling with
    /// CheckDone before calling GetAlignments (or other results
    /// access operations), the first CheckDone call after results are
    /// available will perform the CPU, network, and memory intensive
    /// processing, and the GetAlignments() (for example) call will
    /// simply return a pointer to part of this data.
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
    const string & GetRID(void);
    
    /// Get the seqalign set from the results.
    ///
    /// This method returns the alignment data as a seq align set.  If
    /// this search contains multiple queries, this method will return
    /// all data as a single set.  Most users will probably prefer to
    /// call GetSeqAlignSets() in this case.
    ///
    /// @return Reference to a seqalign set.
    CRef<objects::CSeq_align_set> GetAlignments(void);
    
    /// Get the seqalign vector from the results.
    ///
    /// This method returns the alignment data from the search as a
    /// TSeqAlignVector, which is a vector of CSeq_align_set objects.
    /// For multiple query searches, this method will normally return
    /// each alignment as a seperate element of the TSeqAlignVector.
    /// Note that in some cases, the TSeqAlignVector will not have an
    /// entry for some queries.  If the vector contains fewer
    /// alignments than there were queries, it may be necessary for
    /// the calling code to match queries to alignments by comparing
    /// Seq_ids.  This normally happens only if the same query is
    /// specified multiple times, or if one of the searches does not
    /// find any matches.
    ///
    /// @return A seqalign vector.
    ///
    /// @todo Separate the results for each query into discontinuous seq-aligns
    /// separated by the subject sequence. Also, using the upcoming feature of
    /// retrieving the query sequences, insert empty seqaligns into vector
    /// elements where there are no results for a given query (use
    /// x_CreateEmptySeq_align_set from blast_seqalign.cpp)
    TSeqAlignVector GetSeqAlignSets();
    
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
    CRef<objects::CPssmWithParameters> GetPSSM(void);
    
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
    void SetVerbose(EDebugMode verb = eDebug);
    
    /// Get a set of Bioseqs given an input set of Seq-ids.
    ///
    /// This retrieves the Bioseqs corresponding to the given Seq-ids
    /// from the blast4 server.  Normally this will be much faster
    /// than consulting ID1 seperately for each sequence.  Sometimes
    /// there are multiple sequences for a given Seq-id.  In such
    /// cases, there are always 'non-ambiguous' ids available.  This
    /// interface does not currently address this issue, and will
    /// simply return the Bioseqs corresponding to one of the
    /// sequences.  Errors will be returned if the operation cannot be
    /// completed (or started).  In the case of a sequence that cannot
    /// be found, the error will indicate the index of (and Seq-id of)
    /// the missing sequence; processing will continue, and the
    /// sequences that can be found will be returned along with the
    /// error.
    /// 
    /// @param seqids   A vector of Seq-ids for which Bioseqs are requested.
    /// @param database A list of databases from which to get the sequences.
    /// @param seqtype  The residue type, 'p' from protein, 'n' for nucleotide.
    /// @param bioseqs  The vector used to return the requested Bioseqs.
    /// @param errors   A null-seperated list of errors.
    /// @param warnings A null-seperated list of warnings.
    static void
    GetSequences(vector< CRef<objects::CSeq_id> > & seqids,    // in
                 const string            & database,  // in
                 char                      seqtype,   // 'p' or 'n'
                 vector< CRef<objects::CBioseq> > & bioseqs,   // out
                 string                  & errors,    // out
                 string                  & warnings); // out
        
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
    const int x_DefaultTimeout(void);
    
    /// Called by new search constructors: initialize a new search.
    void x_Init(CBlastOptionsHandle * algo_opts,
                const string        & program,
                const string        & service);
    
    /// Called by new search constructors: initialize a new search.
    void x_Init(CBlastOptionsHandle * algo_opts);
    
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
                       objects::CPssmWithParameters * matrix);
    
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
    
    static CRef<objects::CBlast4_request>
    x_BuildGetSeqRequest(vector< CRef<objects::CSeq_id> > & seqids,   // in
                         const string            & database, // in
                         char                      seqtype,  // 'p' or 'n'
                         string                  & errors);  // out
    
    static void
    x_GetSeqsFromReply(CRef<objects::CBlast4_reply>       reply,
                       vector< CRef<objects::CBioseq> > & bioseqs,   // out
                       string                  & errors,    // out
                       string                  & warnings); // out

    
    /// Prohibit copy construction.
    CRemoteBlast(const CRemoteBlast &);
    
    /// Prohibit assignment.
    CRemoteBlast & operator=(const CRemoteBlast &);
    
    
    // Data
    
    /// Options for new search.
    CRef<blast::CBlastOptionsHandle>   m_CBOH;
    
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
 * Revision 1.20  2005/05/19 15:39:05  camacho
 * Added @todo item for GetSeqAlignSets
 *
 * Revision 1.19  2005/05/09 20:08:48  bealer
 * - Add program and service strings to CBlastOptions for remote blast.
 * - New CBlastOptionsHandle constructor for CRemoteBlast.
 * - Prohibit copy construction/assignment for CRemoteBlast.
 * - Code in each BlastOptionsHandle derived class to set program+service.
 *
 * Revision 1.18  2005/05/09 19:58:45  camacho
 * Add notice to deprecated method
 *
 * Revision 1.17  2005/04/11 15:21:48  bealer
 * - Add GetSeqAlignSets() functionality to CRemoteBlast.
 *
 * Revision 1.16  2005/01/21 19:56:57  bealer
 * - Expand CheckDone() documentation.
 *
 * Revision 1.15  2004/12/08 20:27:56  camacho
 * Update comment on CRemoteBlast::CheckDone
 *
 * Revision 1.14  2004/10/12 14:18:31  camacho
 * Update for scoremat.asn reorganization
 *
 * Revision 1.13  2004/09/13 20:12:10  bealer
 * - Add GetSequences() API.
 *
 * Revision 1.12  2004/08/03 21:01:49  bealer
 * - Move one line functions around.
 *
 * Revision 1.11  2004/08/02 15:01:36  bealer
 * - Distinguish between blastn and megablast (for remote blast).
 *
 * Revision 1.10  2004/07/29 15:05:17  bealer
 * - Ignore empty entrez query.
 *
 * Revision 1.9  2004/06/21 16:34:53  bealer
 * - Make scope usage more consistent for doxygen's sake.
 *
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

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
    // Protein = blastp/plain
    CRemoteBlast(const string & RID)
    {
        x_Init(RID);
    }
    
    // Protein = blastp/plain
    CRemoteBlast(CBlastProteinOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "blastp", "plain");
    }
    
    // Nucleotide = blastn/plain
    CRemoteBlast(CBlastNucleotideOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "blastn", "plain");
    }
    
//     // Nucleotide = blastn/plain
//     CRemoteBlast(CMegaNucleotideOptionsHandle * algo_opts)
//     {
//         x_Init(algo_opts, "blastn", "megablast");
//     }
    
    // Blastx = blastx/plain
    CRemoteBlast(CBlastxOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "blastx", "plain");
    }
    
    // TBlastn = tblastn/plain
    CRemoteBlast(CTBlastnOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "tblastn", "plain");
    }
    
    // TBlastx = tblastx/plain
    CRemoteBlast(CTBlastxOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "tblastx", "plain");
    }
    
    // DiscNucl = blastn/plain
    CRemoteBlast(CDiscNucleotideOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "blastn", "megablast");
    }
    
//     // Prot PHI = blastp/phi
//     CRemoteBlast(CBlastPHINucleotideOptionsHandle * algo_opts)
//     { x_Init(algo_opts, "blastn", "phi"); }
    
//     // Nucl PHI = blastn/phi
//     CRemoteBlast(CBlastPHIProteinOptionsHandle * algo_opts)
//     { x_Init(algo_opts, "blastp", "phi"); }
    
    ~CRemoteBlast()
    {
    }
    
    /******************* GI List ***********************/
    void SetGIList(list<Int4> & gi_list)
    {
        if (gi_list.empty()) {
            NCBI_THROW(CBlastException, eBadParameter,
                       "Empty gi_list specified.");
        }
        x_SetOneParam("GiList", & gi_list);
    }
    
    /******************* DB/subject *******************/
    void SetDatabase(const string & x)
    {
        if (x.empty()) {
            NCBI_THROW(CBlastException, eBadParameter,
                       "Empty string specified for database.");
        }
        SetDatabase(x.c_str());
    }
    
    void SetDatabase(const char * x);
    
    /******************* Entrez Query *******************/
    void SetEntrezQuery(const char * x)
    {
        if (!x) {
            NCBI_THROW(CBlastException, eBadParameter,
                       "NULL specified for entrez query.");
        }
        x_SetOneParam("EntrezQuery", &x);
    }
    
    /******************* Queries *******************/
    void SetQueries(CRef<objects::CBioseq_set>                bioseqs);
    void SetQueries(list< CRef<objects::CSeq_loc> >         & seqlocs);
    void SetQueries(CRef<objects::CScore_matrix_parameters>   pssm);
    
    /******************* Queries *******************/
    void SetMatrixTable(CRef<objects::CScore_matrix_parameters> matrix)
    {
        if (matrix.Empty()) {
            NCBI_THROW(CBlastException, eBadParameter,
                       "Empty reference for matrix.");
        }
        x_SetOneParam("MatrixTable", matrix);
    }
    
    /******************* Getting Results *******************/
    
    // Submit and poll results until a completion (or error) state is
    // reached.  This sleeps 10 seconds the after the submission, 13
    // seconds after the first check, increasing by 30% after each
    // check to a maximum of 300 seconds per sleep.
    
    // Returns true if the search was _submitted_ (there may be
    // errors, call CheckErrors).
    
    bool SubmitSync(void)
    {
        return SubmitSync( DefaultTimeout() );
    }
    
    bool SubmitSync(int);
    
    
    // Submit and return immediately; returns true if request was
    // submitted (true iff an RID was returned).
    
    bool Submit(void);
    
    
    // This returns true if the search is done; it does a network
    // operation, so it should not be called in a loop without a
    // few seconds of delay.
    
    bool CheckDone(void);
    
    // This returns any errors encountered as a string (the empty
    // string implies that everything is running smoothly).
    
    string GetErrors(void);
    
    // This returns any warnings encountered as a string.  These do
    // not necessarily indicate an error or potential error; some
    // warnings are always returned from certain types of searches.
    // (This is more of a debugging feature rather than something to
    // return to the end-user.)
    
    string GetWarnings(void);
    
    /******************* Getting Results *******************/
    
    // This is valid if submit returns true.
    
    const string & GetRID(void)
    {
        return m_RID;
    }
    
    // This is valid if CheckDone returns true.  If CheckErrors
    // returns errors, the rest of these will return nulls.
    
    CRef<objects::CSeq_align_set>            GetAlignments(void);
    CRef<objects::CBlast4_phi_alignments>    GetPhiAlignments(void);
    CRef<objects::CBlast4_mask>              GetMask(void);
    list< CRef<objects::CBlast4_ka_block > > GetKABlocks(void);
    list< string >                           GetSearchStats(void);
    CRef<objects::CScore_matrix_parameters>  GetPSSM(void);
    
    // Verbose mode
    enum EDebugMode {
        eDebug = 0,
        eSilent
    };
    
    // Debugging method: set to true to output progress info to stdout.
    void SetVerbose(EDebugMode verb = eDebug)
    {
        m_Verbose = verb;
    }
    
private:
    typedef objects::CBlast4_get_search_results_reply TGSRR;
    
    // State helpers (for readability)
    enum EState {
        eStart = 0,
        eFailed,
        eWait,
        eDone
    };
    
    // Immediacy flag
    enum EImmediacy {
        ePollAsync = 0,
        ePollImmed
    };
    
    // Required Parameters
    enum ENeedConfig {
        eNoConfig = 0x0,
        eProgram  = 0x1,
        eService  = 0x2,
        eQueries  = 0x4,
        eSubject  = 0x8,
        eNeedAll  = 0xF
    };
    
    // Initialize request (called by constructors)
    
    const int DefaultTimeout(void)
    {
        return int(3600*3.5);
    }
    
    void x_Init(CBlastOptionsHandle * algo_opts,
                const char          * program,
                const char          * service);
    
    void x_Init(const string & RID);
    void x_SetAlgoOpts(void);
    
    // Parameter setting
    void x_SetOneParam(const char * name, const int * x); // not used (yet)
    void x_SetOneParam(const char * name, const list<int> * x);
    void x_SetOneParam(const char * name, const char ** x);
    void x_SetOneParam(const char * name, 
                       objects::CScore_matrix_parameters * matrix);
    
    int x_GetState(void);
    
    TGSRR * x_GetGSRR(void);
    
    // Submission/Result progression
    CRef<objects::CBlast4_reply>
    x_SendRequest(CRef<objects::CBlast4_request_body> body);
    
    CRef<objects::CBlast4_reply>
    x_GetSearchResults(void);
    
    void x_CheckConfig(void);
    void x_SubmitSearch(void);
    void x_CheckResults(void);
    void x_SearchErrors(CRef<objects::CBlast4_reply> reply);
    void x_PollUntilDone(EImmediacy poll_immed, int seconds);
    
    
    // Data
    
    CRef<blast::CBlastOptionsHandle>            m_CBOH;
    CRef<objects::CBlast4_queue_search_request> m_QSR;
    CRef<objects::CBlast4_reply>                m_Reply;
    
    vector<string> m_Errs;
    vector<string> m_Warn;
    string m_RID;
    
    int         m_ErrIgn;
    bool        m_Pending;
    EDebugMode  m_Verbose;
    ENeedConfig m_NeedConfig;
};


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */


/*
 * ===========================================================================
 * $Log$
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

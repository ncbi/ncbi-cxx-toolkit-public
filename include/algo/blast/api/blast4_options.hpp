#ifndef ALGO_BLAST_API___BLAST4_OPTIONS__HPP
#define ALGO_BLAST_API___BLAST4_OPTIONS__HPP

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

/// @file blast4_options.hpp
/// Declares the CBlast4Options class.


#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blastx_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/blast/Blast4_queue_search_reply.hpp>
#include <objects/blast/Blast4_queue_search_reques.hpp>
#include <objects/blast/Blast4_reply.hpp>
#include <objects/blast/Blast4_reply_body.hpp>
#include <objects/blast/Blas_get_searc_resul_reply.hpp>
#include <objects/blast/Blast4_subject.hpp>
#include <objects/blast/Blast4_phi_alignments.hpp>
#include <objects/blast/Blast4_mask.hpp>
#include <objects/blast/Blast4_ka_block.hpp>
#include <objects/scoremat/Score_matrix_parameters.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// API for Blast4 Requests
///
/// API Class to facilitate submission of Blast4 requests.
/// Provides an interface to build a Blast4 request given an
/// of a subclass of CBlastOptions.

class NCBI_XBLAST_EXPORT CBlast4Options
{
public:
    // Protein = blastp/plain
    CBlast4Options(CBlastProteinOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "blastp", "plain");
    }
    
    // Nucleotide = blastn/plain
    CBlast4Options(CBlastNucleotideOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "blastn", "plain");
    }
    
//     // Nucleotide = blastn/plain
//     CBlast4Options(CMegaNucleotideOptionsHandle * algo_opts)
//     {
//         x_Init(algo_opts, "blastn", "megablast");
//     }
    
    // Blastx = blastx/plain
    CBlast4Options(CBlastxOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "blastx", "plain");
    }
    
    // DiscNucl = blastn/plain
    CBlast4Options(CDiscNucleotideOptionsHandle * algo_opts)
    {
        x_Init(algo_opts, "blastn", "megablast");
    }
    
//     // Prot PHI = blastp/phi
//     CBlast4Options(CBlastPHINucleotideOptionsHandle * algo_opts)
//     { x_Init(algo_opts, "blastn", "phi"); }
    
//     // Nucl PHI = blastn/phi
//     CBlast4Options(CBlastPHIProteinOptionsHandle * algo_opts)
//     { x_Init(algo_opts, "blastp", "phi"); }
    
    ~CBlast4Options()
    {
    }
    
    /******************* GI List ***********************/
    void SetGIList(list<Int4> & gi_list)
    {
        x_SetOneParam("GiList", &gi_list);
    }
    
    /******************* DB/subject *******************/
    void SetDatabase(const char * x)
    {
        CRef<objects::CBlast4_subject> subject_p(new objects::CBlast4_subject);
        subject_p->SetDatabase(x);
        m_Qsr->SetSubject(*subject_p);
    }
    
    /******************* Entrez Query *******************/
    void SetEntrezQuery(const char * x)
    {
        x_SetOneParam("EntrezQuery", &x);
    }
    
    /******************* Queries *******************/
    void SetQueries(CRef<objects::CBioseq_set> bioseqs)
    {
        m_Qsr->SetQueries(*bioseqs);
    }
    
    /******************* Queries *******************/
    void SetMatrixTable(CRef<objects::CScore_matrix_parameters> matrix)
    {
        x_SetOneParam("MatrixTable", matrix);
    }
    
    /******************* Getting Results *******************/
    void GetReply(string & err);
    
    const string & m_GetRID(void)
    {
        return m_RID;
    }
    
    CRef<objects::CSeq_align_set>            GetAlignments(void);
    CRef<objects::CBlast4_phi_alignments>    GetPhiAlignments(void);
    CRef<objects::CBlast4_mask>              GetMask(void);
    list< CRef<objects::CBlast4_ka_block > > GetKABlocks(void);
    list< string >                           GetSearchStats(void);
    
    // Debugging method: set to true to output progress info to stdout
    
    void SetVerbose(bool verbose)
    {
        m_Verbose = verbose;
    }
    
private:
    CRef<objects::CBlast4_queue_search_request> m_Qsr;
    CRef<objects::CBlast4_reply>                m_Reply;
    
    string m_RID;
    bool   m_Verbose;
    
    void x_Init(CBlastOptionsHandle * algo_opts, const char * program, const char * service);
    
    void x_QueueSearch(string & RID, string & err);
    
    void x_PollResults(string & RID, string & err);

#if 0 // seems to confuse GCC 2.95 :-/    
    template<class T>
    void x_SetParam(const char * name, T & value)
    {
        x_SetOneParam(name, & value);
    }
#endif
    
    void x_SetOneParam(const char * name, const int * x);
    void x_SetOneParam(const char * name, const list<int> * x);
    void x_SetOneParam(const char * name, const char ** x);
    void x_SetOneParam(const char * name, objects::CScore_matrix_parameters * matrix);
};


END_SCOPE(blast)
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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

#endif  /* ALGO_BLAST_API___BLAST4_OPTIONS__HPP */

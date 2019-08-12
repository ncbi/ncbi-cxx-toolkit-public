#ifndef ALGO_BLAST_API___SEARCH_STRATEGY__HPP
#define ALGO_BLAST_API___SEARCH_STRATEGY__HPP

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
 * Authors:  Tom Madden
 *
 */

/// @file search_strategy.hpp
/// Declares the CImportStrategy and CExportStrategy

#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_options_builder.hpp>
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

/// This is the "mutable" data for CImportStrategy.
struct CImportStrategyData {

    /// Has the struct been properly filled in?
    bool valid;

    /// BLAST options.
    CRef<blast::CBlastOptionsHandle> m_OptionsHandle;

    /// Filtering ID
    int m_FilteringID;

    /// Range of query.
    TSeqRange m_QueryRange;

    /// Task, such as megablast, blastn, blastp, etc.
    string m_Task;

    unsigned int m_PsiNumOfIterations;
    
    /// Filtering key
    string m_FilteringKey;

    /// Subject Masking Type
    ESubjectMaskingType m_SubjectMaskingType;

    /// Constructor
    CImportStrategyData() {
        valid = false;
        m_OptionsHandle.Reset(0);
        m_FilteringID = -1; // means uninitialized/unknown
        m_QueryRange = TSeqRange::GetEmpty();
        m_PsiNumOfIterations = 0;
        m_FilteringKey = kEmptyStr; // means uninitialized/unknown
        m_SubjectMaskingType = eNoSubjMasking; // means uninitialized/unknown
    }
};


/// Class to return parts of the CBlast4_request, or data associated with
/// a CBlast4_request, such as options.
class NCBI_XBLAST_EXPORT CImportStrategy : public CObject
{
public:
    /// Constructor, imports the CBlast4_request
    CImportStrategy(CRef<objects::CBlast4_request> request,
                    bool ignore_unsupported_options = false);

    /// Builds and returns the OptionsHandle
    CRef<blast::CBlastOptionsHandle> GetOptionsHandle() ;

    /// Fetches task, such as "megablast", "blastn", etc. 
    string GetTask() ;

    /// Fetches service, such as psiblast, plain, megablast
    string GetService() const;

    /// Fetches program, one of blastn, blastp, blastx, tblastn, tblastx
    string GetProgram() const;

    /// Returns ident field from a Blast4-request
    string GetCreatedBy() const;

    /// The start and stop on the query (if applicable)
    TSeqRange GetQueryRange();

    /// The DB filter ID.
    int GetDBFilteringID() ;

    /// The DB filter key.
    string GetDBFilteringKey() ;

    /// Get Subject Masking Type
    ESubjectMaskingType GetSubjectMaskingType();

    /// The queries either as Bioseq, seqloc, or pssm.
    CRef<objects::CBlast4_queries> GetQueries();

    /// Returns the target sequences.  This is then a choice of a
    /// database (for searches over a blast database) or as a 
    /// list of Bioseqs (for bl2seq type searches).
    CRef<objects::CBlast4_subject> GetSubject();

    /// Options specific to blast searches (e.g, threshold, expect value).
    /// @return the algorithm options or NULL if unavailable
    objects::CBlast4_parameters* GetAlgoOptions();

    /// Options for controlling program execution and database filtering.
    /// @return the program options or NULL if unavailable
    objects::CBlast4_parameters* GetProgramOptions();
    
    /// Options for controlling formatting (psi blast iteration number also).
    /// @return the web formatting options or NULL if unavailable
    objects::CBlast4_parameters* GetWebFormatOptions();

    /// Get number of iteration for psi blast, return 0 if num of iterations not available
    unsigned int GetPsiNumOfIterations();

    /// Return the BlastOptions builder used in this class
    CBlastOptionsBuilder& GetOptionsBuilder() {

        if (m_OptionsBuilder.get() == NULL) {
            FetchData();
        }
        _ASSERT(m_OptionsBuilder.get() != NULL);
        return *m_OptionsBuilder.get();
    }

    // Get Tax IDs for filterting, set size 0 if no tax id found
    set<int> GetTaxidList();

    // Get Negative Tax IDs for filterting, set size 0 if no negative tax id found
    set<int> GetNegativeTaxidList();

private:
    /// Fills in CImportStrategyData and m_OptionsBuilder
    void FetchData();

   void  x_GetProgramOptionIntegerList(EBlastOptIdx idx, list<int> & list);
    auto_ptr<CImportStrategyData> m_Data;
    CRef<objects::CBlast4_request> m_Request;
    string m_Service;
    auto_ptr<CBlastOptionsBuilder> m_OptionsBuilder;
    /// ignore unsupported options when creating blast options builder object
    bool m_IgnoreUnsupportedOptions;

    /// Prohibit copy constructor
    CImportStrategy(const CImportStrategy& rhs);
    /// Prohibit assignment operator
    CImportStrategy& operator=(const CImportStrategy& rhs);
};


class NCBI_XBLAST_EXPORT CExportStrategy : public CObject
{
public:
	/// Construct search strategy with :-.
    /// @param opts_handle Blast options handle
	///        (Note: only eRemote or eBoth mode are supported)
    CExportStrategy(CRef<CBlastOptionsHandle>  	opts_handle,
    			    const string & 				client_id = kEmptyStr);

    /// Construct search strategy with :-.
    /// @param queries Queries corresponding to Seq-loc-list or Bioseq-set.
    /// @param opts_handle Blast options handle.
	///        (Note: only eRemote or eBoth mode are supported)
    /// @param db Database used for this search.
    CExportStrategy(CRef<IQueryFactory>         query,
                 	CRef<CBlastOptionsHandle>  	opts_handle,
                 	CRef<CSearchDatabase> 		db,
    			    const string & 				client_id = kEmptyStr,
    			    unsigned int				psi_num_iterations = 0);

    /// Construct search strategy with :-.
    /// @param queries Queries corresponding to Seq-loc-list or Bioseq-set.
    /// @param opts_handle Blast options handle.
	///        (Note: only eRemote or eBoth mode are supported)
    /// @param subjects Subject corresponding to Seq-loc-list or Bioseq-set.
    CExportStrategy(CRef<IQueryFactory>       	query,
                 	CRef<CBlastOptionsHandle> 	opts_handle,
                 	CRef<IQueryFactory>       	subject,
    			    const string & 				client_id = kEmptyStr);

    /// Construct search strategy with :-.
    /// @param pssm Search matrix for a PSSM search.
    /// @param opts_handle Blast options handle.
	///        (Note: only eRemote or eBoth mode are supported)
    /// @param db Database used for this search.
    CExportStrategy(CRef<CPssmWithParameters>	pssm,
                 	CRef<CBlastOptionsHandle>   opts_handle,
                 	CRef<CSearchDatabase> 		db,
    			    const string & 				client_id = kEmptyStr,
    			    unsigned int				psi_num_iterations = 0);

    // Return Search Strategy constructed by calling one of the constructors above
    CRef<objects::CBlast4_request> GetSearchStrategy(void);

    // Export Search Strategy (Blast4-request) in ASN1 format
    void ExportSearchStrategy_ASN1(CNcbiOstream* out);

private:
	// Prohibit copy and assign constructors
	CExportStrategy(const CExportStrategy & );
	CExportStrategy & operator=(const CExportStrategy & );

	void x_Process_BlastOptions(CRef<CBlastOptionsHandle> & opts_handle);
	void x_Process_Query(CRef<IQueryFactory> & query);
	void x_Process_Pssm(CRef<CPssmWithParameters> & pssm);
	void x_Process_SearchDb(CRef<CSearchDatabase> & db);
	void x_Process_Subject(CRef<IQueryFactory> & subject);

	void x_AddParameterToProgramOptions(objects::CBlast4Field & field,
	                   	 	    		const int int_value);
	void x_AddParameterToProgramOptions(objects::CBlast4Field & field,
										const vector<int> & int_list);
	void x_AddParameterToProgramOptions(objects::CBlast4Field & field,
										const vector<Int8> & int_list);
	void x_AddParameterToProgramOptions(objects::CBlast4Field & field,
                                        const string & str);

	void x_AddPsiNumOfIterationsToFormatOptions(unsigned int num_iters);

	CRef<CBlast4_queue_search_request>   	m_QueueSearchRequest;
	string									m_ClientId;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___SEARCH_STRATEGY__HPP */

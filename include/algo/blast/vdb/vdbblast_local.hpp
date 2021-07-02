/* $Id$
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
 */

/** @file vdbblast_local.hpp
 * Declares the CLocalVDBBlast class
 */
#ifndef ALGO_BLAST_VDB___VDBBLAST_LOCAL_HPP
#define ALGO_BLAST_VDB___VDBBLAST_LOCAL_HPP

#include <common/ncbi_export.h>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/setup_factory.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)


class NCBI_VDB2BLAST_EXPORT CLocalVDBBlast : public CObject
{

private:
	CLocalVDBBlast(const CLocalVDBBlast &);
	CLocalVDBBlast & operator=(const CLocalVDBBlast &);
    CRef<CSearchResultSet> RunThreadedSearch();

    void x_AdjustDbSize(void);
    void x_PrepareQuery(vector<CRef<IQueryFactory> > & qf_v);
    void x_PreparePssm(vector<CRef<CPssmWithParameters> > & pssm);

    CRef<CBlastQueryVector> 		m_query_vector;
    CRef<CBlastOptionsHandle>  		m_opt_handle;
    Uint8							m_total_num_seqs;
    Uint8							m_total_length;
    vector<vector<string> >		&	m_chunks_for_thread;
    unsigned int					m_num_threads;
    Int4							m_num_extensions;
    CRef<objects::CPssmWithParameters> m_pssm;

public:

    static const unsigned int		kDisableThreadedSearch = 1;
    struct SLocalVDBStruct {
    	vector<vector<string> > chunks_for_thread;
    	Uint8 total_num_seqs;
    	Uint8 total_length;
    };

	/*
	 * ClocalVDBBlast Constructor
	 * @parm
	 * query_vector		Query vector
	 * chunks			Each chunk contains a space delimited list of dbs to search
	 * options			Blast Options
	 * local_vdb		Preporcess parmeters for running local vdb blast
	 * num_of_threads	1 = Force non-threaded search
	 * 					Note: If the input num of threads > num of dbs, the num of threads
	 * 						  is the same as the num of database.
	 */
    CLocalVDBBlast(CRef<CBlastQueryVector> query_vector,
              	   CRef<CBlastOptionsHandle> options,
              	   CLocalVDBBlast::SLocalVDBStruct	& local_vdb);

    CLocalVDBBlast(CRef<objects::CPssmWithParameters> pssm,
                   CRef<CBlastOptionsHandle> options,
                   SLocalVDBStruct & local_vdb);

    /*
     * Run Local VDB Search
     * @return
     * CRef<CSearchResultSet>		Results from local VDB Search
     */
    CRef<CSearchResultSet> Run();

    enum ESRASearchMode
    {
    	eUnaligned = 0,
    	eAligned = 1,
    	eBoth = 2
    };
    /*
     * Preprocess dbs into chunks and return db stats
     * Split db Into chunks based on num of threads and OID Int4 limits
     * Check all input dbs
     * Save work when instantiating mutliple CLocalVDBBlast with the same db
     * @ return
     * unique db list (remove reduntant dbs in db_names)
     *
     */
    static string PreprocessDBs(CLocalVDBBlast::SLocalVDBStruct & local_vdb, const string db_names,
    		                    unsigned int num_threads= kDisableThreadedSearch,
    		                    ESRASearchMode seach_mode = eAligned);

    Int4 GetNumExtensions();
    ~CLocalVDBBlast(){};

};

END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* ALGO_BLAST_VDB___VDBBLAST_LOCAL_HPP */

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

/** @file rpsblast_local.hpp
 * Declares the CLocalRPSBlast class
 */
#ifndef ALGO_BLAST_API___RPSBLAST_LOCAL_HPP
#define ALGO_BLAST_API___RPSBLAST_LOCAL_HPP

#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/setup_factory.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)


class NCBI_XBLAST_EXPORT CLocalRPSBlast : public CObject
{

private:
	CLocalRPSBlast(const CLocalRPSBlast &);
	CLocalRPSBlast & operator=(const CLocalRPSBlast &);
    CRef<CSearchResultSet> RunThreadedSearch();

    void x_AdjustDbSize(void);


    unsigned int					m_num_of_threads;
    const string  					m_db_name;
    CRef<CBlastOptionsHandle>  		m_opt_handle;
    CRef<CBlastQueryVector>	 		m_query_vector;
    unsigned int					m_num_of_dbs;
    vector<string>					m_rps_databases;
    static const int				kDisableThreadedSearch = 1;
    static const int				kAutoThreadedSearch = 0;

public:

	/*
	 * ClocalRPSBlast Constructor
	 * @parm
	 * query_vector		Query Sequence(s)
	 * db				Name of the database to be used in search
	 * 					(Supply the path to the database if it not in the default
	 * 					 directory)
	 * options			Blast Options
	 * num_of_threads	0, program determines the num of
	 * 									 threads if the input database support
	 * 								     threadable search
	 * 					1 = Force non-threaded search
	 * 					Note: If the input num of threads > max number of threads
	 * 					      supported by a particaular database, the num of threads
	 * 						  is kept to the max num supported by the database.
	 * 						  If the target database does not support threaded search,
	 * 						  value of the num of threads is ignored.
	 */
    CLocalRPSBlast(CRef<CBlastQueryVector> query_vector,
              	  	  const string & db,
              	  	  CRef<CBlastOptionsHandle> options,
              	  	  unsigned int num_of_threads = kAutoThreadedSearch);

    /*
     * Run Local RPS Search
     * @return
     * CRef<CSearchResultSet>		Results from local RPS Search
     */
    CRef<CSearchResultSet> Run();

    ~CLocalRPSBlast(){};


};

END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* ALGO_BLAST_API___RPSBLAST_LOCAL_HPP */


#ifndef ALGO_BLAST_API___RPSBLAST_LOCAL_HPP
#define ALGO_BLAST_API___RPSBLAST_LOCAL_HPP

#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/setup_factory.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)


class NCBI_XBLAST_EXPORT CLocalRPSBlast :public CObject
{
public:

	/*
	 * ClocalRPSBlast Constructor
	 * @parm
	 * query_vector		Query Sequence(s)
	 * db				Name of the database to be used in search
	 * 					(Supply the path to the database if it not in the default
	 * 					 directory)
	 * options			Blast Options
	 * num_of_threads	CThreadable::kMinNumThreads = default value,
	 * 												  program determines the num of
	 * 												  threads if the input database support
	 * 												  threadable search
	 * 					0 = Force non-threaded search
	 * 					Note: If the input num of threads > max number of threads
	 * 					      supported by a particaular database, the num of threads
	 * 						  is kept to the max num supported by the database.
	 * 						  If the target database does not support threaded search,
	 * 						  value of the num of threads is ignored.
	 */
    CLocalRPSBlast(CRef<CBlastQueryVector> query_vector,
              	  	  const string db,
              	  	  CRef<CBlastOptionsHandle> options,
              	  	  const unsigned int num_of_threads = CThreadable::kMinNumThreads);

    /*
     * Run Local RPS Search
     * @return
     * CRef<CSearchResultSet>		Results from local RPS Search
     */
    CRef<CSearchResultSet> Run();

    ~CLocalRPSBlast(){};

private:
	CLocalRPSBlast(const CLocalRPSBlast &);
	CLocalRPSBlast & operator=(const CLocalRPSBlast &);
    CRef<CSearchResultSet> RunThreadedSearch();

    unsigned int					m_num_of_threads;
    const string 					m_db_name;
    CRef<CBlastOptionsHandle>  		m_opt_handle;
    CRef<CBlastQueryVector>	 		m_query_vector;
};

END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* ALGO_BLAST_API___RPSBLAST_LOCAL_HPP */

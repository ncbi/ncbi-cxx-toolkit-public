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
 * Author: Christiam Camacho
 *
 */

/** @file blast_app_util.hpp
 * Utility functions for BLAST command line applications
 */

#ifndef APP__BLAST_APP_UTIL__HPP
#define APP__BLAST_APP_UTIL__HPP

#include <objmgr/object_manager.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <algo/blast/blastinput/blast_args.hpp>

#include <objects/blast/Blast4_request.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <algo/blast/dbindex/dbindex.hpp>           // for CDbIndex_Exception
#include <objtools/blast/blastdb_format/invalid_data_exception.hpp> // for CInvalidDataException
#include <objtools/blast/seqdb_writer/writedb_error.hpp>
#include <algo/blast/format/blastfmtutil.hpp>   // for CBlastFormatUtil
#include <algo/blast/blastinput/blast_scope_src.hpp>    // for SDataLoaderConfig

BEGIN_NCBI_SCOPE

/// Class to mix batch size for BLAST runs
class CBatchSizeMixer 
{
private:
    const double k_MixIn;        // mixing factor between batches
    Int4 m_TargetHits;           // the target hits per batch
    double m_Ratio;              // the hits to batch size ratio
    Int4 m_BatchSize;            // the batch size for next run
    const Int4 k_MinBatchSize;   // the minimum allowable batch size
    const Int4 k_MaxBatchSize;   // the maximum allowable batch size
    const Int4 k_MinTargetHits;  // the minimum target hits per batch

public:
    CBatchSizeMixer(Int4 max_batch_size)
          : k_MixIn        (0.3),
            m_TargetHits   (1000000),
            m_Ratio        (-1.0),
            m_BatchSize    (5000),
            k_MinBatchSize (100),
            k_MaxBatchSize (max_batch_size),
	    k_MinTargetHits(1000000) { }

    void SetTargetHits(Int4 target) {
        m_TargetHits = (target < k_MinTargetHits) ? k_MinTargetHits : target;
        m_BatchSize = m_TargetHits / 200;
    }

    // Return the next batch_size
    Int4 GetBatchSize(Int4 hits = -1); 

  Int4 GetTargetHits(void)
  {
    return m_TargetHits;
  }

  double GetRatio(void)
  {
    return m_Ratio;
  }
};


/** 
 * @brief Initializes a CRemoteBlast instance for usage by command line BLAST
 * binaries
 * 
 * @param queries query sequence(s) or NULL in case of PSSM input [in]
 * @param db_args database/subject arguments [in]
 * @param opts_hndl BLAST options handle [in]
 * @param verbose_output set to true if CRemoteBlast should produce verbose
 * output [in]
 * @param pssm PSSM to use for single iteration remote PSI-BLAST
 * @throws CInputException in case of remote PSI-BL2SEQ, as it's not supported
 */
CRef<blast::CRemoteBlast> 
InitializeRemoteBlast(CRef<blast::IQueryFactory> queries,
                      CRef<blast::CBlastDatabaseArgs> db_args,
                      CRef<blast::CBlastOptionsHandle> opts_hndl,
                      bool verbose_output,
                      const string& client_id = kEmptyStr,
                      CRef<objects::CPssmWithParameters> pssm = 
                      CRef<objects::CPssmWithParameters>());

/// Initializes the subject/database as well as its scope
/// @param db_args database/subject arguments [in]
/// @param opts_hndl BLAST options handle [in]
/// @param is_remote_search true if it's a remote search, otherwise false [in]
/// @param db_adapter Database/subject adapter [out]
/// @param scope subject scope [out]
void
InitializeSubject(CRef<blast::CBlastDatabaseArgs> db_args, 
                  CRef<blast::CBlastOptionsHandle> opts_hndl,
                  bool is_remote_search,
                  CRef<blast::CLocalDbAdapter>& db_adapter, 
                  CRef<objects::CScope>& scope);

/// Initialize the data loader configuration for the query
/// @param query_is_protein is/are the query sequence(s) protein? [in]
/// @param db_adapter the database/subject information [in]
blast::SDataLoaderConfig 
InitializeQueryDataLoaderConfiguration(bool query_is_protein, 
                                       CRef<blast::CLocalDbAdapter> db_adapter);
/// Create a CSeqDB object from the command line arguments provided
/// @param db_args BLAST database arguments [in]
/// @throw CSeqDBException in case of not being able to properly build a CSeqDB
/// object
CRef<CSeqDB> GetSeqDB(CRef<blast::CBlastDatabaseArgs> db_args);

/// Register the BLAST database data loader using the already initialized
/// CSeqDB object
/// @param db_handle properly initialized CSeqDB instance [in]
/// @return name of the BLAST data data loader (to be added to the CScope 
/// object)
string RegisterOMDataLoader(CRef<CSeqDB> db_handle);

/// Command line binary exit code: success
#define BLAST_EXIT_SUCCESS          0
/// Command line binary exit code: error in input query/options
#define BLAST_INPUT_ERROR           1
/// Command line binary exit code: error in database/subject
#define BLAST_DATABASE_ERROR        2
/// Command line binary exit code: error in BLAST engine
#define BLAST_ENGINE_ERROR          3
/// Command line binary exit code: BLAST run out of memory
#define BLAST_OUT_OF_MEMORY         4
/// Command line binary exit code: Network error encountered
#define BLAST_NETWORK_ERROR         5
/// Command line binary exit code: error when writing output
#define BLAST_OUTPUT_ERROR          6
/// Command line binary exit code: unknown error
#define BLAST_UNKNOWN_ERROR         255

/// Standard catch statement for all BLAST command line programs
/// @param exit_code exit code to be returned from main function
#define CATCH_ALL(exit_code)                                                \
    catch (const CInvalidDataException& e) {                                \
        LOG_POST(Error << "BLAST options error: " << e.GetMsg());           \
        exit_code = BLAST_INPUT_ERROR;                                      \
    }                                                                       \
    catch (const blast::CInputException& e) {                               \
        LOG_POST(Error << "BLAST query/options error: " << e.GetMsg());     \
        LOG_POST(Error << "Please refer to the BLAST+ user manual.");       \
        exit_code = BLAST_INPUT_ERROR;                                      \
    }                                                                       \
    catch (const CArgException& e) {                                        \
        LOG_POST(Error << "Command line argument error: " << e.GetMsg());   \
        exit_code = BLAST_INPUT_ERROR;                                      \
    }                                                                       \
    catch (const CSeqDBException& e) {                                      \
        LOG_POST(Error << "BLAST Database error: " << e.GetMsg());          \
        exit_code = BLAST_DATABASE_ERROR;                                   \
    }                                                                       \
    catch (const blastdbindex::CDbIndex_Exception& e) {                     \
        LOG_POST(Error << "Indexed BLAST database error: " << e.GetMsg());  \
        exit_code = BLAST_DATABASE_ERROR;                                   \
    }                                                                       \
    catch (const CIndexedDbException& e) {                                  \
        LOG_POST(Error << "Indexed BLAST database error: " << e.GetMsg());  \
        exit_code = BLAST_DATABASE_ERROR;                                   \
    }                                                                       \
    catch (const CWriteDBException& e) {                                    \
        LOG_POST(Error << "BLAST Database creation error: " << e.GetMsg()); \
        exit_code = BLAST_INPUT_ERROR;                                      \
    }                                                                       \
    catch (const blast::CBlastException& e) {                               \
        const string& msg = e.GetMsg();                                     \
        if (e.GetErrCode() == CBlastException::eInvalidOptions) {           \
            LOG_POST(Error << "BLAST options error: " << e.GetMsg());       \
            exit_code = BLAST_INPUT_ERROR;                                  \
        } else if ((NStr::Find(msg, "Out of memory") != NPOS) ||            \
            (NStr::Find(msg, "Failed to allocate") != NPOS)) {              \
            LOG_POST(Error << "BLAST ran out of memory: " << e.GetMsg());   \
            exit_code = BLAST_OUT_OF_MEMORY;                                \
        } else {                                                            \
            LOG_POST(Error << "BLAST engine error: " << e.GetMsg());        \
            exit_code = BLAST_ENGINE_ERROR;                                 \
        }                                                                   \
    }                                                                       \
    catch (const blast::CBlastSystemException& e) {                         \
        if (e.GetErrCode() == CBlastSystemException::eOutOfMemory) {        \
            LOG_POST(Error << "BLAST ran out of memory: " << e.GetMsg());   \
            exit_code = BLAST_OUT_OF_MEMORY;                                \
        } else if (e.GetErrCode() == CBlastSystemException::eNetworkError) {\
            LOG_POST(Error << "Network error: " << e.GetMsg());             \
            exit_code = BLAST_NETWORK_ERROR;                                \
        } else {                                                            \
            LOG_POST(Error << "System error: " << e.GetMsg());              \
            exit_code = BLAST_UNKNOWN_ERROR;                                \
        }                                                                   \
    }                                                                       \
    catch (const CIOException& e) {                                         \
        if (e.GetErrCode() == CIOException::eFlush) {                       \
            LOG_POST(Error << "BLAST failed to write output: " << e.GetMsg());\
            exit_code = BLAST_OUTPUT_ERROR;                                 \
        }                                                                   \
    }                                                                       \
    catch (const CException& e) {                                           \
        LOG_POST(Error << "Error: " << e.what());                           \
        exit_code = BLAST_UNKNOWN_ERROR;                                    \
    }                                                                       \
    catch (const std::ios::failure&) {                                      \
        LOG_POST(Error << "BLAST failed to write output");                  \
        exit_code = BLAST_OUTPUT_ERROR;                                     \
    }                                                                       \
    catch (const std::bad_alloc&) {                                         \
        LOG_POST(Error << "BLAST ran out of memory");                       \
        exit_code = BLAST_OUT_OF_MEMORY;                                    \
    }                                                                       \
    catch (const std::exception& e) {                                       \
        LOG_POST(Error << "Error: " << e.what());                           \
        exit_code = BLAST_UNKNOWN_ERROR;                                    \
    }                                                                       \
    catch (...) {                                                           \
        LOG_POST(Error << "Unknown exception occurred");                    \
        exit_code = BLAST_UNKNOWN_ERROR;                                    \
    }                                                                       \
    
/// Recover search strategy from input file
/// @param args the command line arguments provided by the application [in]
/// @param cmdline_args output command line arguments. Will have the database
/// arguments set, as well as options handle [in|out]
/// @Return true if recovered from save search strategy
bool
RecoverSearchStrategy(const CArgs& args, blast::CBlastAppArgs* cmdline_args);

/// Save the search strategy corresponding to the current command line search
void
SaveSearchStrategy(const CArgs& args,
                   blast::CBlastAppArgs* cmdline_args,
                   CRef<blast::IQueryFactory> queries,
                   CRef<blast::CBlastOptionsHandle> opts_hndl,
                   CRef<objects::CPssmWithParameters> pssm 
                     = CRef<objects::CPssmWithParameters>(),
                   unsigned int num_iters = 0);

/// This method needs to be invoked for optimizing the retrieval of
/// sequence data from the remote BLAST database data loader (exercised
/// when there are no locally available BLAST databases)
/// @param results BLAST results [in]
/// @param scope CScope object from which the sequence data will be fetched
/// [in]
void 
BlastFormatter_PreFetchSequenceData(const blast::CSearchResultSet&
                                    results, CRef<CScope> scope);

/// Auxiliary function to extract the ancillary data from the PSSM.
/// Used in PSI-BLAST and DELTA-BLAST
///@param pssm Pssm [in]
///@return Ancillary data extracted from Pssm
CRef<blast::CBlastAncillaryData>
ExtractPssmAncillaryData(const objects::CPssmWithParameters& pssm);

void
CheckForFreqRatioFile(const string& rps_dbname, CRef<blast::CBlastOptionsHandle>  & opt_handle, bool isRpsblast);

//Check for empty input stream
// Note that if true, error/eof is set for the stream
bool
IsIStreamEmpty(CNcbiIstream & in);

string
GetCmdlineArgs(const CNcbiArguments & a);

bool
UseXInclude(const blast::CFormattingArgs & f, const string & s);

/// Get name of subject file
/// @parameter args arguments class [in]
/// @return Name of subject file.
string
GetSubjectFile(const CArgs& args);

END_NCBI_SCOPE

#endif /* APP__BLAST_APP_UTIL__HPP */


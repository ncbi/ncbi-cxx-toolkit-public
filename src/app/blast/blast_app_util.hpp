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
#include <objtools/readers/seqdb/seqdb.hpp>
#include <algo/blast/blastinput/blast_args.hpp>

#include <objects/blast/Blast4_request.hpp>
#include <algo/blast/api/uniform_search.hpp>

BEGIN_NCBI_SCOPE

/// Create a CSeqDB object from the command line arguments provided
/// @param db_args BLAST database arguments [in]
/// @throw CSeqDBException in case of not being able to properly build a CSeqDB
/// object
CRef<CSeqDB> GetSeqDB(CRef<blast::CBlastDatabaseArgs> db_args);

/// Register the BLAST database data loader using the already initialized
/// CSeqDB object
/// @param objmgr the object manager [in]
/// @param db_handle properly initialized CSeqDB instance [in]
/// @return name of the BLAST data data loader (to be added to the CScope 
/// object)
string RegisterOMDataLoader(CRef<objects::CObjectManager> objmgr, 
                            CRef<CSeqDB> db_handle);

/// Command line binary exit code: success
#define BLAST_EXIT_SUCCESS          0
/// Command line binary exit code: error in input query/options
#define BLAST_INPUT_ERROR           1
/// Command line binary exit code: error in database/subject
#define BLAST_DATABASE_ERROR        2
/// Command line binary exit code: error in BLAST engine
#define BLAST_ENGINE_ERROR          3
/// Command line binary exit code: unknown error
#define BLAST_UNKNOWN_ERROR         255

/// Standard catch statement for all BLAST command line programs
/// @param exit_code exit code to be returned from main function
#define CATCH_ALL(exit_code)                                                \
    catch (const CInputException& e) {                                      \
        cerr << "BLAST query/options error: " << e.GetMsg() << endl;        \
        exit_code = BLAST_INPUT_ERROR;                                      \
    }                                                                       \
    catch (const CArgException& e) {                                        \
        cerr << "Command line argument error: " << e.GetMsg() << endl;      \
        exit_code = BLAST_INPUT_ERROR;                                      \
    }                                                                       \
    catch (const CSeqDBException& e) {                                      \
        cerr << "BLAST Database error: " << e.GetMsg() << endl;             \
        exit_code = BLAST_DATABASE_ERROR;                                   \
    }                                                                       \
    catch (const CBlastException& e) {                                      \
        cerr << "BLAST engine error: " << e.GetMsg() << endl;               \
        exit_code = BLAST_ENGINE_ERROR;                                     \
    }                                                                       \
    catch (const CException& e) {                                           \
        cerr << "Error: " << e.what() << endl;                              \
        exit_code = BLAST_UNKNOWN_ERROR;                                    \
    }                                                                       \
    catch (const exception& e) {                                            \
        cerr << "Error: " << e.what() << endl;                              \
        exit_code = BLAST_UNKNOWN_ERROR;                                    \
    }                                                                       \
    catch (...) {                                                           \
        cerr << "Unknown exception occurred" << endl;                       \
        exit_code = BLAST_UNKNOWN_ERROR;                                    \
    }                                                                       \
    
/// Recover search strategy from input file
/// @param cmdline_args command line arguments. Will have the database
/// arguments set, as well as options handle [in|out]
void
RecoverSearchStrategy(const CArgs& args, blast::CBlastAppArgs* cmdline_args);

/// Save the search strategy corresponding to the current command line search
void
SaveSearchStrategy(const CArgs& args,
                   blast::CBlastAppArgs* cmdline_args,
                   CRef<blast::IQueryFactory> queries,
                   CRef<blast::CBlastOptionsHandle> opts_hndl,
                   CRef<blast::CSearchDatabase> search_db,
                   objects::CPssmWithParameters* pssm = NULL);

END_NCBI_SCOPE

#endif /* APP__BLAST_APP_UTIL__HPP */


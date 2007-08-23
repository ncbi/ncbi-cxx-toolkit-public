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

CRef<CSeqDB> GetSeqDB(CRef<blast::CBlastDatabaseArgs> db_args);
string RegisterOMDataLoader(CRef<objects::CObjectManager> objmgr, 
                            CRef<CSeqDB> db_handle);

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


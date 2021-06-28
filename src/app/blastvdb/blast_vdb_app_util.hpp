/*  $Idg $
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
 * Author: Amelia Fong
 *
 */

/** @file blast_vdb_app_util.hpp
 * Utility functions for BLAST  VDB command line applications
 */

#ifndef APP__BLAST_VDB_APP_UTIL__HPP
#define APP__BLAST_VDB_APP_UTIL__HPP

#include <objmgr/scope.hpp>
#include <algo/blast/api/blast_results.hpp>
#include <sra/data_loaders/csra/csraloader.hpp>
#include <sra/data_loaders/wgs/wgsloader.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

BEGIN_NCBI_SCOPE

// Sort Seq Ids and then fetch sequences into scope
// Used by vdb blast apps
// Note that the bioseq handles keep the cached seqs in CScope
// When the bioseq handles go out of program scope, the cached seqs
// will also get released from the CScope
// @param results	BLAST results [in]
// @param scope 	CScope object from which the data will be fetched from [in]
// @param handles 	bioseq handles for which the sequnces are cached [out]

void
SortAndFetchSeqData(const blast::CSearchResultSet& results, CRef<objects::CScope> scope, objects::CScope::TBioseqHandles & handles);

CRef<objects::CScope>
GetVDBScope(string dbAllNames);

END_NCBI_SCOPE

#endif /* APP__BLAST_VDB_APP_UTIL__HPP */


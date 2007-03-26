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

/** @file blast_app_util.cpp
 *  Utility functions for BLAST command line applications
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include "blast_app_util.hpp"
#include <objtools/data_loaders/blastdb/bdbloader.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(blast);

CRef<CSeqDB> GetSeqDB(CRef<CBlastDatabaseArgs> db_args)
{
    const CSeqDB::ESeqType seq_type = db_args->IsProtein()
        ? CSeqDB::eProtein
        : CSeqDB::eNucleotide;

    // Process the optional gi list file
    CRef<CSeqDBGiList> gi_list;
    string gi_list_restriction = db_args->GetGiListFileName();
    if ( !gi_list_restriction.empty() ) {
        gi_list_restriction.assign(SeqDB_ResolveDbPath(gi_list_restriction));
        if ( !gi_list_restriction.empty() ) {
            gi_list.Reset(new CSeqDBFileGiList(gi_list_restriction));

        }
    }
    return CRef<CSeqDB>(new CSeqDB(db_args->GetDatabaseName(), 
                                   seq_type, gi_list));
}

string RegisterOMDataLoader(CRef<CObjectManager> objmgr, 
                            CRef<CSeqDB> db_handle)
{
    // the blast formatter requires that the database coexist in
    // the same scope with the query sequences
    
    CBlastDbDataLoader::RegisterInObjectManager(*objmgr, db_handle,
                                                CObjectManager::eDefault);

    return CBlastDbDataLoader::GetLoaderNameFromArgs(db_handle);
}

END_NCBI_SCOPE

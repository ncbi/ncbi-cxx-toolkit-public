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
* Author:  Christiam Camacho, Kevin Bealer
*
* ===========================================================================
*/

/// @file remote_services.cpp
/// Implementation of CRemoteServices class

#include <ncbi_pch.hpp>
#include <algo/blast/api/remote_services.hpp>
#include <objects/blast/blastclient.hpp>
#include <util/util_exception.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

bool
CRemoteServices::IsValidBlastDb(const string& dbname, bool is_protein)
{
    CRef<CBlast4_database> blastdb(new CBlast4_database);
    blastdb->SetName(dbname);
    blastdb->SetType(is_protein 
                     ? eBlast4_residue_type_protein 
                     : eBlast4_residue_type_nucleotide);
    CRef<CBlast4_database_info> result = GetDatabaseInfo(blastdb);
    return result.NotEmpty();
}

CRef<objects::CBlast4_database_info>
CRemoteServices::x_FindDbInfoFromAvailableDatabases
    (CRef<objects::CBlast4_database> blastdb)
{
    _ASSERT(blastdb.NotEmpty());

    CRef<CBlast4_database_info> retval;

    ITERATE(CBlast4_get_databases_reply::Tdata, dbinfo, m_AvailableDatabases) {
        if ((*dbinfo)->GetDatabase() == *blastdb) {
            retval = *dbinfo;
            break;
        }
    }

    return retval;
}

vector< CRef<objects::CBlast4_database_info> >
CRemoteServices::GetOrganismSpecificRepeatsDatabases()
{
    if (m_AvailableDatabases.empty()) {
        x_GetAvailableDatabases();
    }
    vector< CRef<objects::CBlast4_database_info> > retval;

    ITERATE(CBlast4_get_databases_reply::Tdata, dbinfo, m_AvailableDatabases) {
        if ((*dbinfo)->GetDatabase().GetName().find("repeat_") != NPOS) {
            retval.push_back(*dbinfo);
        }
    }

    return retval;
}

void
CRemoteServices::x_GetAvailableDatabases()
{
    CBlast4Client client;
    CRef<CBlast4_get_databases_reply> databases;
    try { 
        databases = client.AskGet_databases(); 
        m_AvailableDatabases = databases->Set();
    }
    catch (const CEofException &) {
        NCBI_THROW(CRemoteBlastException, eServiceNotAvailable,
                   "No response from server, cannot complete request.");
    }
}


CRef<objects::CBlast4_database_info>
CRemoteServices::GetDatabaseInfo(CRef<objects::CBlast4_database> blastdb)
{
    if (blastdb.Empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "NULL argument specified: blast database description");
    }

    if (m_AvailableDatabases.empty()) {
        x_GetAvailableDatabases();
    }

    return x_FindDbInfoFromAvailableDatabases(blastdb);
}


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */


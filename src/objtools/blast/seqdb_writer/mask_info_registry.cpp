/* ===========================================================================
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
 * Author:  Christiam Camacho
 *
 */

/** @file mask_info_registry.cpp
 * Implements CMaskInfoRegistry class
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include "mask_info_registry.hpp"
#include <objtools/blast/seqdb_writer/writedb_error.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
#endif /* SKIP_DOXYGEN_PROCESSING */

int 
CMaskInfoRegistry::x_AssignId(int start, int end)
{
    return x_FindNextValidIdWithinRange(start, end);
}

int 
CMaskInfoRegistry::x_AssignId(int start, int end, bool use_start)
{
    if (use_start) {
        if (m_UsedIds.find(start) != m_UsedIds.end()) {
            string msg("Masking algorithm with default arguments " 
                       "already provided");
            NCBI_THROW(CWriteDBException, eArgErr, msg);
        }
        return start;
    } else {
        return x_FindNextValidIdWithinRange(start+1, end);
    }
}

int
CMaskInfoRegistry::x_FindNextValidIdWithinRange(int start, int stop)
{
    for (int id = start; id < stop && 
                         id < (int)eBlast_filter_program_max; id++) {
        if (m_UsedIds.find(id) == m_UsedIds.end()) {
            return id;
        }
    }
    string msg("Too many IDs in range " + NStr::IntToString(start));
    msg += "-" + NStr::IntToString(stop);
    NCBI_THROW(CWriteDBException, eArgErr, msg);
}

int
CMaskInfoRegistry::Add(EBlast_filter_program program, 
                       const string& options /* = string() */)
{
    int algo_id = (int)program;

    switch (program) {
    case eBlast_filter_program_dust:
        algo_id = x_AssignId(program, eBlast_filter_program_seg, 
                             options.empty());
        break;

    case eBlast_filter_program_seg:
        algo_id = x_AssignId(program, eBlast_filter_program_windowmasker, 
                             options.empty());
        break;

    case eBlast_filter_program_windowmasker:
        algo_id = x_AssignId(program, eBlast_filter_program_repeat, 
                             options.empty());
        break;
    
    case eBlast_filter_program_repeat:
        algo_id = x_AssignId(program, eBlast_filter_program_other);
        break;

    case eBlast_filter_program_other:
        algo_id = x_AssignId(program, eBlast_filter_program_max);
        break;

    case eBlast_filter_program_not_set:
    case eBlast_filter_program_max:
    default:
        string msg("Invalid filtering program: ");
        msg += NStr::IntToString((int)program);
        NCBI_THROW(CWriteDBException, eArgErr, msg);
    }

    m_UsedIds.insert(algo_id);
    return algo_id;
}

END_NCBI_SCOPE

/* @} */


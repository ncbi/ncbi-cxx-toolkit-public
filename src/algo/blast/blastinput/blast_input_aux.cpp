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

/** @file blast_input_aux.cpp
 *  Auxiliary functions for BLAST input library
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include "blast_input_aux.hpp"
#include <algo/blast/blastinput/psiblast_args.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CArgDescriptions* 
SetUpCommandLineArguments(TBlastCmdLineArgs& args)
{
    auto_ptr<CArgDescriptions> retval(new CArgDescriptions);

    // Create the groups so that the ordering is established
    retval->SetCurrentGroup("Input query options");
    retval->SetCurrentGroup("General search options");
    retval->SetCurrentGroup("BLAST database options");
    retval->SetCurrentGroup("BLAST-2-Sequences options");
    retval->SetCurrentGroup("Formatting options");
    retval->SetCurrentGroup("Query filtering options");
    retval->SetCurrentGroup("Restrict search or results");
    retval->SetCurrentGroup("Discontiguous MegaBLAST options");
    retval->SetCurrentGroup("Statistical options");
    retval->SetCurrentGroup("Search strategy options");
    retval->SetCurrentGroup("Extension options");
    retval->SetCurrentGroup("");


    NON_CONST_ITERATE(TBlastCmdLineArgs, arg, args) {
        (*arg)->SetArgumentDescriptions(*retval);
    }
    return retval.release();
}

int
GetQueryBatchSize(EBlastProgramType program)
{
    int retval = 0;

    // used for experimentation purposes
    char* batch_sz_str = getenv("BATCH_SIZE");
    if (batch_sz_str) {
        retval = NStr::StringToInt(batch_sz_str);
        _TRACE("DEBUG: Using query batch size " << retval);
        return retval;
    }

    switch (program) {
    case eBlastTypeBlastn:
        retval = 1000000;
        break;
    case eBlastTypeTblastn:
        retval = 20000;
        break;
    // if the query will be translated, round the chunk size up to the next
    // multiple of 3, that way, when the nucleotide sequence(s) get(s)
    // split, context N%6 in one chunk will have the same frame as context N%6
    // in the next chunk
    case eBlastTypeBlastx:
    case eBlastTypeTblastx:
        // N.B.: the splitting is done on the nucleotide query sequences, then
        // each of these chunks is translated
        retval = 10002;
        break;
    case eBlastTypeBlastp:
    default:
        retval = 10000;
        break;
    }

    _TRACE("Using query batch size " << retval);
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

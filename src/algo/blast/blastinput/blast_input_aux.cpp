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
    NON_CONST_ITERATE(TBlastCmdLineArgs, arg, args) {
        (*arg)->SetArgumentDescriptions(*retval);
    }
    return retval.release();
}

int
GetQueryBatchSize(const string& program)
{
    if (program == "blastn") {
        return 40000;
    } else if (program == "megablast") {
        return 5000000;
    } else if (program == "tblastn") {
        return 20000;
    } else if (program == "psiblast" || 
               program == "psitblastn" || 
               program == "phiblast") {
        return CPsiBlastAppArgs::kMaxNumQueries;
    } else {
        return 10000;
    }
}

END_SCOPE(blast)
END_NCBI_SCOPE

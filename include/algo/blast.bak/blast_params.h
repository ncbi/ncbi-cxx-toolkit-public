/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: blast_params.h

Author: Ilya Dondoshansky

Contents: BLAST parameters dependent on the ncbiobj library

******************************************************************************
 * $Revision$
 * */

#ifndef __BLASTPARAMS__
#define __BLASTPARAMS__

#include <blast_def.h>
#include <blast_options.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Find a genetic code string in ncbistdaa encoding, given an integer 
 * genetic code value.
 */
Int2 BLAST_GeneticCodeFind(Int4 gc, Uint1Ptr PNTR genetic_code);


/** Initialize database parameters,including calculation of the 
 * database genetic code string.
 * @param program Type of BLAST program [in]
 * @param db_options Database options [in]
 * @param db_params Returned parameters structure [out]
 */
Int2 BlastDatabaseParametersNew(Uint1 program, 
        const BlastDatabaseOptionsPtr db_options,
        BlastDatabaseParametersPtr PNTR db_params);

BlastDatabaseParametersPtr 
BlastDatabaseParametersFree(BlastDatabaseParametersPtr db_params);

#ifdef __cplusplus
}
#endif
#endif /* !__BLASTPARAMS__ */


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

/** @file gencode_singleton.h
 * Defines the interface to interact with the genetic code singleton object
 */

#ifndef ALGO_BLAST_CORE__GENCODE_SINGLETON__H
#define ALGO_BLAST_CORE__GENCODE_SINGLETON__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_export.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the genetic code singleton.
 * @note this function is *NOT* thread safe, call it from the main thread */
NCBI_XBLAST_EXPORT void 
GenCodeSingletonInit();

/** Uninitialize the genetic code singleton */
NCBI_XBLAST_EXPORT void 
GenCodeSingletonFini();

/** Add a genetic code entry to the singleton
 * @param gen_code_id genetic code id [in]
 * @param gen_code_str genetic code string [in]
 * @return 0 if SUCESS or already there, otherwise BLASTERR_MEMORY
 */
NCBI_XBLAST_EXPORT Int2 
GenCodeSingletonAdd(Uint4 gen_code_id, const Uint1* gen_code_str);

/** Returns the genetic code string for the requested genetic code id
 * @param gen_code_id genetic code id [in]
 * @return the genetic code string or NULL if this genetic code was not added
 * to the singleton 
 * @note it's the API layer's responsibility to add least add
 * BLAST_GENETIC_CODE to the singleton (for backwards compatibility and to meet
 * the engine's expectations)
 */
NCBI_XBLAST_EXPORT Uint1* 
GenCodeSingletonFind(Uint4 gen_code_id);

#ifdef __cplusplus
}
#endif
#endif /* ALGO_BLAST_CORE__GENCODE_SINGLETON__H */


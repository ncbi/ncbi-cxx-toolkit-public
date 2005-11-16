/* $Id$
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
 * Author: Ilya Dondoshansky
 *
 */

/** @file blast_dust.h
 * DUST filtering functions. (shouldn't this be merged with blast_filter?)
 * @todo FIXME: include reference?
 */

#ifndef __BLAST_DUST__
#define __BLAST_DUST__

#include <algo/blast/core/blast_def.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Perform DUST low complexity filtering for a sequence buffer.
 * @param sequence Buffer for which to find low complexity locations [in]
 * @param length Length of the buffer [in]
 * @param offset At what offset in the buffer to start looking? [in]
 * @param level Level to be used in DUST algorithm [in]
 * @param window Window size for DUST [in]
 * @param linker Distance at which to link segments  [in]
 * @param dust_loc The locations found by dust [out]
 */
NCBI_XBLAST_EXPORT
Int2 SeqBufferDust (Uint1* sequence, Int4 length, Int4 offset,
                    Int2 level, Int2 window, Int2 linker,
                    BlastSeqLoc** dust_loc);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_FILTER__ */

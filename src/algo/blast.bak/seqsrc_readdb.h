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
* Author:  Christiam Camacho
*
* File Description:
*   Implementation of the BlastSeqSrc interface using readdb
*
*/

#ifndef SEQSRC_READDB_H
#define SEQSRC_READDB_H

#ifndef NCBI_C_TOOLKIT
#define NCBI_C_TOOLKIT
#endif

#include <readdb.h>
#include <blast_seqsrc.h>

#define SEQIDLEN_MAX 255

#ifdef __cplusplus
extern "C" {
#endif

/** Encapsulates the arguments needed to initialize a BLAST database using
 * readdb */
typedef struct ReaddbNewArgs {
    char* dbname;     /**< Database name */
    Boolean is_protein; /**< Is this database protein? */
} ReaddbNewArgs;

/** Readdb sequence source constructor 
 * @param bssp BlastSeqSrc structure (already allocated) to populate [in]
 * @param args Pointer to ReaddbNewArgs structure above [in]
 * @return Updated bssp structure (with all function pointers initialized
 */
BlastSeqSrc* ReaddbSeqSrcNew(BlastSeqSrc* bssp, void* args);

/** Readdb sequence source destructor: frees its internal data structure and the
 * BlastSeqSrc structure itself.
 * @param bssp BlastSeqSrc structure to free [in]
 * @return NULL
 */
BlastSeqSrc* ReaddbSeqSrcFree(BlastSeqSrc* bssp);

#ifdef __cplusplus
}
#endif

#endif /* SEQSRC_READDB_H */

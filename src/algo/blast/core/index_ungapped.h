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
 *  Author: Aleksandr Morgulis
 *
 */

/** @file index_ungapped.h
 * Declarations of structures needed to implement diagonal hash to
 * support ungapped extensions for indexed version of megablast.
 */

#ifndef ALGO_BLAST_CORE__INDEX_UNGAPPED__H
#define ALGO_BLAST_CORE__INDEX_UNGAPPED__H

#include <algo/blast/core/ncbi_std.h>

/** How many keys are there in diagonal hash table. */
#define IR_HASH_SIZE (4*1024)

/** Compute diagonal identifier from subject and query offsets.

    @param qoff Query offset.
    @param soff Subject offset.

    @return Integer diagonal identifier.
  */
#define IR_DIAG(qoff,soff) (0x10000000 + (soff) - (qoff))

/** Compute the hash key from a diagonal identifier.

    @param diag Diagonal identifier.

    @return Hash key corresponding to the diagonal.
  */
#define IR_KEY(diag)       ((diag)%IR_HASH_SIZE)

/** Find a hash table entry for the given diagonal.

    @param hash Pointer to hash table instance.
    @param diag Diagonal identifier.
    @param key  Hash table key corresponding to the diagonal.

    @return Pointer to ir_hash_entry representing the diagonal.
  */
#define IR_LOCATE(hash,diag,key) (                                      \
        ((hash)->entries[(key)].diag_data.qend == 0 ||                  \
         (diag)==(hash)->entries[(key)].diag_data.diag) ?               \
                ((hash)->entries + (key))      :                        \
                (ir_locate(hash,diag,key)) )

/** Part of the hash table entry describing the diagonal. */
typedef struct ir_diag_data_
{
    Uint4 diag;         /**< Diagonal identifier. */
    Uint4 qend;         /**< Right end (in the query) of 
                             the last seen seed on the diagonal. */
} ir_diag_data;

/** Hash table entry. */
typedef struct ir_hash_entry_
{
    ir_diag_data diag_data;             /** Diagonal information. */
    struct ir_hash_entry_ * next;       /** Next entry for the same hash key. */
} ir_hash_entry;

/** Free memory block structure. */
typedef struct ir_fp_entry_
{
    ir_hash_entry * entries;            /** Storage for hash table entries. */
    struct ir_fp_entry_ * next;         /** Next free memory block. */
} ir_fp_entry;

/** Hash table structure. */
typedef struct ir_diag_hash_ 
{
    ir_hash_entry * entries;            /**< Array of hash table entries 
                                             (one per hash table key). */
    ir_fp_entry * free_pool;            /**< Pointer to the first free memory block. */
    ir_hash_entry * free;               /**< Start of the free list of hash table entries. */
} ir_diag_hash;

/** Hash table constructor.

    @return A pointer to a fresh copy of a diagonal hash table, or NULL.
  */
extern ir_diag_hash * ir_hash_create( void );

/** Hash table destructor.

    @param hash Pointer to the hash table instance to destroy.

    @return NULL.
*/
extern ir_diag_hash * ir_hash_destroy( ir_diag_hash * hash );

/** Find a hash table entry for the given diagonal.

    @param hash Pointer to hash table instance.
    @param diag Diagonal identifier.
    @param key  Hash table key corresponding to the diagonal.

    @return Pointer to ir_hash_entry representing the diagonal.
  */
extern ir_hash_entry * ir_locate( 
        ir_diag_hash * hash, Uint4 diag, Uint4 key );

#endif

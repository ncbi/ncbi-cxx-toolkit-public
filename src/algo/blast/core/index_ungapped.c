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

/** @file index_ungapped.c
 * Definitions of functions needed to implement diagonal hash to
 * support ungapped extensions for indexed version of megablast.
 */

#include "index_ungapped.h"

#define FP_ENTRY_SIZE (1024*1024)

/** Free memory block destructor.

    @param e Pointer to the block being destroyed.

    @return NULL.
  */
static ir_fp_entry * ir_fp_entry_destroy( ir_fp_entry * e )
{
    if( e != 0 ) {
        free( e->entries );
        free( e );
        e = 0;
    }

    return e;
}

/** Free memory block constructor.

    @return Pointer to a newly allocated free memory block.
*/
static ir_fp_entry * ir_fp_entry_create( void )
{
    ir_fp_entry * result = (ir_fp_entry *) calloc(1, sizeof( ir_fp_entry ) );

    if( result != 0 ) {
        ir_hash_entry * entries = (ir_hash_entry *) calloc(
                FP_ENTRY_SIZE, sizeof( ir_hash_entry ) );
        if( entries == 0 ) return ir_fp_entry_destroy( result );
        result->next = 0;
        result->entries = entries;

        {
            size_t i = 0;
            for( ; i < FP_ENTRY_SIZE - 1; ++i ) {
                entries[i].next = entries + i + 1;
            }
        }
    }

    return result;
}

ir_diag_hash * ir_hash_create( void )
{
    ir_diag_hash * result = 0;
    result = (ir_diag_hash *) calloc(1, sizeof( ir_diag_hash ) );

    if( result != 0 ) {
        ir_hash_entry * entries = (ir_hash_entry *) calloc(
                IR_HASH_SIZE, sizeof( ir_hash_entry ) );
        if( entries == 0 ) return ir_hash_destroy( result );
        result->entries = entries;
        result->free = 0;
        result->free_pool = 0;
    }

    return result;
}

ir_diag_hash * ir_hash_destroy( ir_diag_hash * hash )
{
    if( hash != 0 ) {
        ir_fp_entry * fpe = hash->free_pool;
        ir_fp_entry * fpn;

        while( fpe != 0 ) {
                fpn = fpe->next;
                fpe = ir_fp_entry_destroy( fpe );
                fpe = fpn;
        }

        free( hash->entries );
        free( hash );
        hash = 0;
    }

    return hash;
}

ir_hash_entry * ir_locate(
        ir_diag_hash * hash, Uint4 diag, Uint4 key )
{
    ir_hash_entry * e = hash->entries + key;
    ir_hash_entry * ce = e->next;

    while( ce != 0 ) {
        if( ce->diag_data.diag == diag ) {
            ir_diag_data tmp = ce->diag_data;
            ce->diag_data = e->diag_data;
            e->diag_data  = tmp;
            return e;
        }

        ce = ce->next;
    }

    if( hash->free == 0 ) {
        ir_fp_entry * fp = ir_fp_entry_create();
        if (fp == 0) {
          return (ir_hash_entry *) 0;
        }
        fp->next = hash->free_pool;
        hash->free_pool = fp;
        hash->free = fp->entries;
    }

    ce = hash->free;
    hash->free = ce->next;
    ce->next = e->next;
    e->next = ce;
    ce->diag_data.diag = diag;
    return ce;
}


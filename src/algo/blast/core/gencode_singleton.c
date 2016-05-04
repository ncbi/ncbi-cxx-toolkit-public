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

/** @file gencode_singleton.c
 * Implementation of the genetic code singleton
 */

#include <algo/blast/core/gencode_singleton.h>
#include "blast_dynarray.h"

/** The singleton instance */
static SDynamicSGenCodeNodeArray* g_theInstance = NULL;

void
GenCodeSingletonInit()
{
    if (g_theInstance == NULL) {
        g_theInstance = DynamicSGenCodeNodeArrayNew();
    }
    ASSERT(g_theInstance);
}

void
GenCodeSingletonFini()
{
    g_theInstance = DynamicSGenCodeNodeArrayFree(g_theInstance);
}

Int2
GenCodeSingletonAdd(Uint4 gen_code_id, const Uint1* gen_code_str)
{
    SGenCodeNode node;
    node.gc_id = gen_code_id;
    node.gc_str = (Uint1*)gen_code_str;
    ASSERT(g_theInstance);
    return DynamicSGenCodeNodeArray_Append(g_theInstance, node);
}

Uint1*
GenCodeSingletonFind(Uint4 gen_code_id)
{
    ASSERT(g_theInstance);
    return DynamicSGenCodeNodeArray_Find(g_theInstance, gen_code_id);
}

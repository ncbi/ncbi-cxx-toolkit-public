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
 * ===========================================================================
 *
 * Author: Ilya Dondoshansky
 *
 */

/** @file gapinfo.c
 * Initialization and freeing of structures containing traceback information.
 */


#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/gapinfo.h>

GapStateArrayStruct* 
GapStateFree(GapStateArrayStruct* state_struct)

{
	GapStateArrayStruct* next;

	while (state_struct)
	{
		next = state_struct->next;
		sfree(state_struct->state_array);
		sfree(state_struct);
		state_struct = next;
		
	}

	return NULL;
}

/*
	Allocates an EditScriptPtr and puts it on the end of
	the chain of EditScriptPtr's.  Returns a pointer to the
	new one.

*/
GapEditScript* 
GapEditScriptNew(GapEditScript* old)

{
	GapEditScript* new;

	new = (GapEditScript*) calloc(1, sizeof(GapEditScript));

	if (old == NULL)
		return new;

	while (old->next != NULL)
	{
		old = old->next;
	}

	old->next = new;

	return new;
}

GapEditScript*
GapEditScriptDelete(GapEditScript* old)
{
	GapEditScript* next;

	while (old)
	{
		next = old->next;
		sfree(old);
		old = next;
	}
	return old;
}

GapEditBlock*
GapEditBlockNew(Int4 start1, Int4 start2)

{
	GapEditBlock* edit_block;

	edit_block = (GapEditBlock*) calloc(1, sizeof(GapEditBlock));
	edit_block->start1 = start1;
	edit_block->start2 = start2;

	return edit_block;
}

GapEditBlock*
GapEditBlockDelete(GapEditBlock* edit_block)

{
	if (edit_block == NULL)
		return NULL;

	edit_block->esp = GapEditScriptDelete(edit_block->esp);

	sfree(edit_block);

	return edit_block;
}


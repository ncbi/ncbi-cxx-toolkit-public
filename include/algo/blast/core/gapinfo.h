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

File name: gapinfo.h

Author: Ilya Dondoshansky

Contents: Structures definitions from gapxdrop.h in ncbitools

******************************************************************************
 * $Revision$
 * */

#ifndef __GAPINFO__
#define __GAPINFO__

#ifdef __cplusplus
extern "C" {
#endif
#include <blast_def.h>

#define GAPALIGN_SUB ((Uint1)0)  /*op types within the edit script*/
#define GAPALIGN_INS ((Uint1)1)
#define GAPALIGN_DEL ((Uint1)2)
#define GAPALIGN_DECLINE ((Uint1)3)

typedef struct GapEditScript {
        Uint1 op_type;  /* GAPALIGN_SUB, GAPALIGN_INS, or GAPALIGN_DEL */
        Int4 num;       /* Number of operations */
        struct GapEditScript PNTR next;
} GapEditScript, PNTR GapEditScriptPtr;

typedef struct GapEditBlock {
    Int4 start1,  start2,       /* starts of alignments. */
        length1, length2,       /* total lengths of the sequences. */
        original_length1,	/* Untranslated lengths of the sequences. */
        original_length2;	
    Int2 frame1, frame2;	    /* frames of the sequences. */
    Boolean translate1,	translate2; /* are either of these be translated. */
    Boolean reverse;	/* reverse sequence 1 and 2 when producing SeqALign? */
    Boolean is_ooframe; /* Is this out_of_frame edit block? */
    Boolean discontinuous; /* Is this OK to produce discontinuous SeqAlign? */
    GapEditScriptPtr esp;
} GapEditBlock, PNTR GapEditBlockPtr;

/*
	Structure to keep memory for state structure.
*/
typedef struct GapStateArrayStruct {
	Int4 	length,		/* length of the state_array. */
		used;		/* how much of length is used. */
	Uint1Ptr state_array;	/* array to be used. */
	struct GapStateArrayStruct PNTR next;
} GapStateArrayStruct, PNTR GapStateArrayStructPtr;

GapEditScriptPtr 
GapEditScriptNew (GapEditScriptPtr old);

GapEditScriptPtr GapEditScriptDelete (GapEditScriptPtr esp);

GapEditBlockPtr GapEditBlockNew (Int4 start1, Int4 start2);
GapEditBlockPtr GapEditBlockDelete (GapEditBlockPtr edit_block);
GapStateArrayStructPtr 
GapStateFree(GapStateArrayStructPtr state_struct);

#ifdef __cplusplus
}
#endif
#endif /* !__GAPINFO__ */

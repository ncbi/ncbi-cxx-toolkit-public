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

File name: blast_hsp.h

Author: Ilya Dondoshansky

Contents: Definitions of structures in which HSPs are saved

Detailed Contents: 

******************************************************************************
 * $Revision$
 * */
#ifndef __BLAST_HSP__
#define __BLAST_HSP__

#include <ncbi.h>
#include <blastkar.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BLAST_Seg {
                Int2            frame;
                Int4            offset; /* start of hsp */
                Int4            length; /* length of hsp */
                Int4            end;    /* end of HSP */
                Int4            offset_trim;    /* start of trimmed hsp */
                Int4            end_trim;       /* end of trimmed HSP */
                /* Where the gapped extension (with X-dropoff) started. */
                Int4            gapped_start;
        } BLAST_Seg, PNTR BLAST_SegPtr;

#define BLAST_NUMBER_OF_ORDERING_METHODS 2

/*
        The following structure is used in "link_hsps" to decide between
        two different "gapping" models.  Here link is used to hook up
        a chain of HSP's (this is a VoidPtr as _blast_hsp is not yet
        defined), num is the number of links, and sum is the sum score.
        Once the best gapping model has been found, this information is
        transferred up to the BLAST_HSP.  This structure should not be
        used outside of the function link_hsps.
*/
typedef struct BLAST_HSP_LINK {
                /* Used to order the HSP's (i.e., hook-up w/o overlapping). */
        VoidPtr link[BLAST_NUMBER_OF_ORDERING_METHODS];
                /* number of HSP in the ordering. */
        Int2    num[BLAST_NUMBER_OF_ORDERING_METHODS];
                /* Sum-Score of HSP. */
        Int4    sum[BLAST_NUMBER_OF_ORDERING_METHODS];
                /* Sum-Score of HSP, multiplied by the appropriate Lambda. */
        Nlm_FloatHi     xsum[BLAST_NUMBER_OF_ORDERING_METHODS];
        Int4 changed;
        } BLAST_HSP_LINK, PNTR BLAST_HSP_LINKPtr;
/*
        BLAST_NUMBER_OF_ORDERING_METHODS tells how many methods are used
        to "order" the HSP's.
*/

typedef struct BLAST_HSP {
                struct _blast_hsp PNTR next, /* the next HSP */
                                  PNTR prev; /* the previous one. */
                BLAST_HSP_LINK  hsp_link;
/* Is this HSp part of a linked set? */
                Boolean         linked_set;
/* which method (max or no max for gaps) was used? */
                Int2            ordering_method;
/* how many HSP's make up this (sum) segment */
                Int4            num;
/* sumscore of a set of "linked" HSP's. */
                BLAST_Score     sumscore;
                /* If TRUE this HSP starts a chain along the "link" pointer. */
                Boolean         start_of_chain;
                BLAST_Score     score;
                Int4            num_ident;
                Nlm_FloatHi     evalue;
                BLAST_Seg query,        /* query sequence info. */
                        subject;        /* subject sequence info. */
                Int2            context;        /* Context number of query */
                GapXEditBlockPtr gap_info; /* ALL gapped alignment is here */
                Int4 num_ref;
                Int4 linked_to;
        } BLAST_HSP, PNTR BLAST_HSPPtr;


#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_HSP__ */

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
* File Name:  $RCSfile$
*
* Author:  Webb Miller and Co.
* Adopted for NCBI standard libraries by Sergey Shavirin
*
* Initial Creation Date: 10/27/1999
*
* $Revision$
*
* File Description: Greedy gapped alignment functions
*/

static char const rcsid[] = "$Id$";

#include <algo/blast/core/greedy_align.h>
#include <algo/blast/core/blast_util.h> /* for NCBI2NA_UNPACK_BASE macros */

enum {
    EDIT_OP_MASK = 0x3,
    EDIT_OP_ERR  = 0x0,
    EDIT_OP_INS  = 0x1,
    EDIT_OP_DEL  = 0x2,
    EDIT_OP_REP  = 0x3
};

enum {         /* half of the (fixed) match score */
    ERROR_FRACTION=2,  /* 1/this */
    MAX_SPACE=1000000,
    sC = 0, sI = 1, sD = 2, LARGE=100000000
};


/* -------- From original file edit.c ------------- */

static Uint4 edit_val_get(edit_op_t op)
{
    return op >> 2;
}
static Uint4 edit_opc_get(edit_op_t op)
{
    return op & EDIT_OP_MASK;
}

static edit_op_t edit_op_cons(Uint4 op, Uint4 val)
{
    return (val << 2) | (op & EDIT_OP_MASK);
}

static edit_op_t edit_op_inc(edit_op_t op, Uint4 n)
{
    return edit_op_cons(edit_opc_get(op), edit_val_get(op) + n);
}

static edit_op_t edit_op_inc_last(MBGapEditScript *es, Uint4 n)
{
    edit_op_t *last;
    ASSERT (es->num > 0);
    last = &(es->op[es->num-1]);
    *last = edit_op_inc(*last, n);
    return *last;
}

static Int4 edit_script_ready(MBGapEditScript *es, Uint4 n)
{
    edit_op_t *p;
    Uint4 m = n + n/2;
    
    if (es->size <= n) {
        p = realloc(es->op, m*sizeof(edit_op_t));
        if (p == 0) {
            return 0;
        } else {
            es->op = p;
            es->size = m;
        }
    }
    return 1;
}

static Int4 edit_script_readyplus(MBGapEditScript *es, Uint4 n)
{
    if (es->size - es->num <= n)
        return edit_script_ready(es, n + es->num);
    return 1;
}

static Int4 edit_script_put(MBGapEditScript *es, Uint4 op, Uint4 n)
{
    if (!edit_script_readyplus(es, 2))
        return 0;
    es->last = op;
    ASSERT(op != 0);
    es->op[es->num] = edit_op_cons(op, n);
    es->num += 1;
    es->op[es->num] = 0; /* sentinal */

    return 1;
}

static MBGapEditScript *edit_script_init(MBGapEditScript *es)
{
	es->op = 0;
	es->size = es->num = 0;
	es->last = 0;
	edit_script_ready(es, 8);
	return es;
}
static edit_op_t *edit_script_first(MBGapEditScript *es)
{
    return es->num > 0 ? &es->op[0] : 0;
}

static edit_op_t *edit_script_next(MBGapEditScript *es, edit_op_t *op)
{
    /* XXX - assumes flat address space */
    if (&es->op[0] <= op && op < &es->op[es->num-1])
        return op+1;
    else
        return 0;
}
static Int4 edit_script_more(MBGapEditScript *data, Uint4 op, Uint4 k)
{
    if (op == EDIT_OP_ERR) {
#ifdef ERR_POST_EX_DEFINED
        ErrPostEx(SEV_FATAL, 1, 0, 
                  "edit_script_more: bad opcode %d:%d", op, k);
#endif
        return -1;
    }
    
    if (edit_opc_get(data->last) == op)
        edit_op_inc_last(data, k);
    else
        edit_script_put(data, op, k);

    return 0;
}

MBGapEditScript *MBGapEditScriptAppend(MBGapEditScript *es, MBGapEditScript *et)
{
    edit_op_t *op;
    
    for (op = edit_script_first(et); op; op = edit_script_next(et, op))
        edit_script_more(es, edit_opc_get(*op), edit_val_get(*op));

    return es;
}

MBGapEditScript *MBGapEditScriptNew(void)
{
    MBGapEditScript *es = calloc(1, sizeof(*es));
    if (!es)
        return 0;

    return edit_script_init(es);
}

MBGapEditScript *MBGapEditScriptFree(MBGapEditScript *es)
{
    if (es) {
        if (es->op)
            sfree(es->op);
        memset(es, 0, sizeof(*es));
        sfree(es);
    }
    return 0;
}

static Int4 edit_script_del(MBGapEditScript *data, Uint4 k)
{
    return edit_script_more(data, EDIT_OP_DEL, k);
}

static Int4 edit_script_ins(MBGapEditScript *data, Uint4 k)
{
    return edit_script_more(data, EDIT_OP_INS, k);
}
static Int4 edit_script_rep(MBGapEditScript *data, Uint4 k)
{
    return edit_script_more(data, EDIT_OP_REP, k);
}

static MBGapEditScript *edit_script_reverse_inplace(MBGapEditScript *es)
{
    Uint4 i;
    const Uint4 num = es->num;
    const Uint4 mid = num/2;
    const Uint4 end = num-1;
    
    for (i = 0; i < mid; ++i) {
        const edit_op_t t = es->op[i];
        es->op[i] = es->op[end-i];
        es->op[end-i] = t;
    }
    return es;
}

MBSpace* MBSpaceNew()
{
    MBSpace* p;
    Int4 amount;
    
    p = (MBSpace*) malloc(sizeof(MBSpace));
    amount = MAX_SPACE;
    p->space_array = (ThreeVal*) malloc(sizeof(ThreeVal)*amount);
    if (p->space_array == NULL) {
       sfree(p);
       return NULL;
    }
    p->used = 0; 
    p->size = amount;
    p->next = NULL;

    return p;
}

static void refresh_mb_space(MBSpace* sp)
{
   while (sp) {
      sp->used = 0;
      sp = sp->next;
   }
}

void MBSpaceFree(MBSpace* sp)
{
   MBSpace* next_sp;

   while (sp) {
      next_sp = sp->next;
      sfree(sp->space_array);
      sfree(sp);
      sp = next_sp;
   }
}

static ThreeVal* get_mb_space(MBSpace* S, Int4 amount)
{
    ThreeVal* s;
    if (amount < 0) 
        return NULL;  
    
    while (S->used+amount > S->size) {
       if (S->next == NULL)
          if ((S->next = MBSpaceNew()) == NULL) {
#ifdef ERR_POST_EX_DEFINED
	     ErrPostEx(SEV_WARNING, 0, 0, "Cannot get new space for greedy extension");
#endif
	     return NULL;
          }
       S = S->next;
    }

    s = S->space_array+S->used;
    S->used += amount;
    
    return s;
}
/* ----- */

static Int4 gcd(Int4 a, Int4 b)
{
    Int4 c;
    if (a < b) {
        c = a;
        a = b; b = c;
    }
    while ((c = a % b) != 0) {
        a = b; b = c;
    }

    return b;
}


static Int4 gdb3(Int4* a, Int4* b, Int4* c)
{
    Int4 g;
    if (*b == 0) g = gcd(*a, *c);
    else g = gcd(*a, gcd(*b, *c));
    if (g > 1) {
        *a /= g;
        *b /= g;
        *c /= g;
    }
    return g;
}

static Int4 get_lastC(ThreeVal** flast_d, Int4* lower, Int4* upper, 
                      Int4* d, Int4 diag, Int4 Mis_cost, Int4* row1)
{
    Int4 row;
    
    if (diag >= lower[(*d)-Mis_cost] && diag <= upper[(*d)-Mis_cost]) {
        row = flast_d[(*d)-Mis_cost][diag].C;
        if (row >= MAX(flast_d[*d][diag].I, flast_d[*d][diag].D)) {
            *d = *d-Mis_cost;
            *row1 = row;
            return sC;
        }
    }
    if (flast_d[*d][diag].I > flast_d[*d][diag].D) {
        *row1 = flast_d[*d][diag].I;
        return sI;
    } else {
        *row1 = flast_d[*d][diag].D;
        return sD;
    }
}

static Int4 get_last_ID(ThreeVal** flast_d, Int4* lower, Int4* upper, 
                        Int4* d, Int4 diag, Int4 GO_cost, 
                        Int4 GE_cost, Int4 IorD)
{
    Int4 ndiag; 
    Int4 row;

    if (IorD == sI) { ndiag = diag -1;} else ndiag = diag+1;
    if (ndiag >= lower[(*d)-GE_cost] && ndiag <=upper[(*d)-GE_cost]) 
        row = (IorD == sI)? flast_d[(*d)-GE_cost][ndiag].I: flast_d[(*d)-GE_cost][ndiag].D;
    else row = -100;
    if (ndiag >= lower[(*d)-GO_cost-GE_cost] && ndiag <= upper[(*d)-GO_cost-GE_cost] && row < flast_d[(*d)-GO_cost-GE_cost][ndiag].C) {
        *d = (*d)-GO_cost-GE_cost;
        return sC;
    }
    *d = (*d)-GE_cost;
    return IorD;
}

static Int4 get_lastI(ThreeVal** flast_d, Int4* lower, Int4* upper, 
                      Int4* d, Int4 diag, Int4 GO_cost, Int4 GE_cost)
{
    return get_last_ID(flast_d, lower, upper, d, diag, GO_cost, GE_cost, sI);
}


static int get_lastD(ThreeVal** flast_d, Int4* lower, Int4* upper, 
                     Int4* d, Int4 diag, Int4 GO_cost, Int4 GE_cost)
{
    return get_last_ID(flast_d, lower, upper, d, diag, GO_cost, GE_cost, sD);
}

/* --- From file align.c --- */
/* ----- */

static Int4 get_last(Int4 **flast_d, Int4 d, Int4 diag, Int4 *row1)
{
    if (flast_d[d-1][diag-1] > MAX(flast_d[d-1][diag], flast_d[d-1][diag+1])) {
        *row1 = flast_d[d-1][diag-1];
        return diag-1;
    } 
    if (flast_d[d-1][diag] > flast_d[d-1][diag+1]) {
        *row1 = flast_d[d-1][diag];
        return diag;
    }
    *row1 = flast_d[d-1][diag+1];
    return diag+1;
}

/*
	Version to search a (possibly) packed nucl. sequence against
an unpacked sequence.  s2 is the packed nucl. sequence.
s1 can be packed or unpacked. If rem == 4, then s1 is unpacked.
len2 corresponds to the unpacked (true) length.

 * Basic O(ND) time, O(N) space, alignment function. 
 * Parameters:
 *   s1, len1        - first sequence and its length
 *   s2, len2        - second sequence and its length
 *   reverse         - direction of alignment
 *   xdrop_threshold - 
 *   mismatch_cost   -
 *   e1, e2          - endpoint of the computed alignment
 *   edit_script     -
 *   rem             - offset shift within a packed sequence
 */
Int4 BLAST_GreedyAlign(const Uint1* s1, Int4 len1,
			  const Uint1* s2, Int4 len2,
			  Boolean reverse, Int4 xdrop_threshold, 
			  Int4 match_cost, Int4 mismatch_cost,
			  Int4* e1, Int4* e2, 
			  GreedyAlignMem* gamp, MBGapEditScript *S,
                          Uint1 rem)
{
#define ICEIL(x,y) ((((x)-1)/(y))+1)
    Int4 col,			/* column number */
        d,				/* current distance */
        k,				/* current diagonal */
        flower, fupper,            /* boundaries for searching diagonals */
        row,		        /* row number */
        MAX_D, 			/* maximum cost */
        ORIGIN,
        return_val = 0;
    Int4** flast_d = gamp->flast_d; /* rows containing the last d */
    Int4* max_row;		/* reached for cost d=0, ... len1.  */
    
    Int4 X_pen = xdrop_threshold;
    Int4 M_half = match_cost/2;
    Int4 Op_cost = mismatch_cost + M_half*2;
    Int4 D_diff = ICEIL(X_pen+M_half, Op_cost);
    
    Int4 x, cur_max, b_diag = 0, best_diag = INT4_MAX/2;
    Int4* max_row_free = gamp->max_row_free;
    char nlower = 0, nupper = 0;
    MBSpace* space = gamp->space;
    Int4 max_len = len2;
 
    MAX_D = (Int4) (len1/ERROR_FRACTION + 1);
    ORIGIN = MAX_D + 2;
    *e1 = *e2 = 0;
    
    if (reverse) {
       if (!(rem & 4)) {
          for (row = 0; row < len2 && row < len1 && 
                  (s2[len2-1-row] ==
                   NCBI2NA_UNPACK_BASE(s1[(len1-1-row)/4], 
                                        3-(len1-1-row)%4)); 
               row++)
             /*empty*/ ;
       } else {
          for (row = 0; row < len2 && row < len1 && (s2[len2-1-row] == s1[len1-1-row]); row++)
             /*empty*/ ;
       }
    } else {
       if (!(rem & 4)) {
          for (row = 0; row < len2 && row < len1 && 
                  (s2[row] == 
                   NCBI2NA_UNPACK_BASE(s1[(row+rem)/4], 
                                        3-(row+rem)%4)); 
               row++)
             /*empty*/ ;
       } else {
          for (row = 0; row < len2 && row < len1 && (s2[row] == s1[row]); row++)
             /*empty*/ ;
       }
    }
    *e1 = row;
    *e2 = row;
    if (row == len1) {
        if (S != NULL)
            edit_script_rep(S, row);
	/* hit last row; stop search */
	return 0;
    }
    if (S==NULL) {
       space = 0;
    } else if (!space) {
       gamp->space = space = MBSpaceNew();
    } else { 
        refresh_mb_space(space);
    }
    
    max_row = max_row_free + D_diff;
    for (k = 0; k < D_diff; k++)
	max_row_free[k] = 0;
    
    flast_d[0][ORIGIN] = row;
    max_row[0] = (row + row)*M_half;
    
    flower = ORIGIN - 1;
    fupper = ORIGIN + 1;

    d = 1;
    while (d <= MAX_D) {
	Int4 fl0, fu0;
	flast_d[d - 1][flower - 1] = flast_d[d - 1][flower] = -1;
	flast_d[d - 1][fupper] = flast_d[d - 1][fupper + 1] = -1;
	x = max_row[d - D_diff] + Op_cost * d - X_pen;
	x = ICEIL(x, M_half);	
	cur_max = 0;
	fl0 = flower;
	fu0 = fupper;
	for (k = fl0; k <= fu0; k++) {
	    row = MAX(flast_d[d - 1][k + 1], flast_d[d - 1][k]) + 1;
	    row = MAX(row, flast_d[d - 1][k - 1]);
	    col = row + k - ORIGIN;
	    if (row + col >= x)
		fupper = k;
	    else {
		if (k == flower)
		    flower++;
		else
		    flast_d[d][k] = -1;
		continue;
	    }
            
            if (row > max_len || row < 0) {
                  flower = k+1; nlower = 1;
            } else {
               /* Slide down the diagonal. */
               if (reverse) {
                  if (!(rem & 4)) {
                     while (row < len2 && col < len1 && s2[len2-1-row] == 
                            NCBI2NA_UNPACK_BASE(s1[(len1-1-col)/4],
                                                 3-(len1-1-col)%4)) {
                        ++row;
                        ++col;
                     }
                  } else {
                     while (row < len2 && col < len1 && 
                            s2[len2-1-row] == s1[len1-1-col]) {
                        ++row;
                        ++col;
                     }
                  }
               } else {
                  if (!(rem & 4)) {
                     while (row < len2 && col < len1 && s2[row] == 
                            NCBI2NA_UNPACK_BASE(s1[(col+rem)/4],
                                                 3-(col+rem)%4)) {
                        ++row;
                        ++col;
                     }
                  } else {
                     while (row < len2 && col < len1 && s2[row] == s1[col]) {
                        ++row;
                        ++col;
                     }
                  }
               }
            }
	    flast_d[d][k] = row;
	    if (row + col > cur_max) {
		cur_max = row + col;
		b_diag = k;
	    }
	    if (row == len2) {
		flower = k+1; nlower = 1;
	    }
	    if (col == len1) {
		fupper = k-1; nupper = 1;
	    }
	}
	k = cur_max*M_half - d * Op_cost;
	if (max_row[d - 1] < k) {
	    max_row[d] = k;
	    return_val = d;
	    best_diag = b_diag;
	    *e2 = flast_d[d][b_diag];
	    *e1 = (*e2)+b_diag-ORIGIN;
	} else {
	    max_row[d] = max_row[d - 1];
	}
	if (flower > fupper)
	    break;
	d++;
	if (!nlower) flower--; 
	if (!nupper) fupper++;
	if (S==NULL)
	   flast_d[d] = flast_d[d - 2];
	else {
           /* space array consists of ThreeVal structures which are 
              3 times larger than Int4, so divide requested amount by 3
           */
	   flast_d[d] = (Int4*) get_mb_space(space, (fupper-flower+7)/3);
	   if (flast_d[d] != NULL)
              flast_d[d] = flast_d[d] - flower + 2;
           else
	      return return_val;
        }
    }
    
    if (S!=NULL) { /*trace back*/
        Int4 row1, col1, diag1, diag;
        d = return_val; diag = best_diag;
        row = *e2; col = *e1;
        while (d > 0) {
            diag1 = get_last(flast_d, d, diag, &row1);
            col1 = row1+diag1-ORIGIN;
            if (diag1 == diag) {
                if (row-row1 > 0) edit_script_rep(S, row-row1);
            } else if (diag1 < diag) {
                if (row-row1 > 0) edit_script_rep(S, row-row1);
                edit_script_ins(S,1);
            } else {
                if (row-row1-1> 0) edit_script_rep(S, row-row1-1);
                edit_script_del(S, 1);
            }
            d--; diag = diag1; col = col1; row = row1;
        }
        edit_script_rep(S, flast_d[0][ORIGIN]);
        if (!reverse) 
            edit_script_reverse_inplace(S);
    }
    return return_val;
}


Int4 BLAST_AffineGreedyAlign (const Uint1* s1, Int4 len1, 
				 const Uint1* s2, Int4 len2,
				 Boolean reverse, Int4 xdrop_threshold, 
				 Int4 match_score, Int4 mismatch_score, 
				 Int4 gap_open, Int4 gap_extend,
				 Int4* e1, Int4* e2, 
				 GreedyAlignMem* gamp, MBGapEditScript *S,
                                 Uint1 rem)
{
    Int4 col,			/* column number */
        d,			/* current distance */
        k,			/* current diagonal */
        flower, fupper,         /* boundaries for searching diagonals */
        row,		        /* row number */
        MAX_D, 			/* maximum cost */
        ORIGIN,
        return_val = 0;
    ThreeVal** flast_d;	/* rows containing the last d */
    Int4* max_row_free = gamp->max_row_free;
    Int4* max_row;		/* reached for cost d=0, ... len1.  */
    Int4 Mis_cost, GO_cost, GE_cost;
    Int4 D_diff, gd;
    Int4 M_half;
    Int4 max_cost;
    Int4 *lower, *upper;
    
    Int4 x, cur_max, b_diag = 0, best_diag = INT4_MAX/2;
    char nlower = 0, nupper = 0;
    MBSpace* space = gamp->space;
    Int4 stop_condition;
    Int4 max_d;
    Int4* uplow_free;
    Int4 max_len = len2;
 
    if (match_score % 2 == 1) {
        match_score *= 2;
        mismatch_score *= 2;
        xdrop_threshold *= 2;
        gap_open *= 2;
	gap_extend *= 2;
    }
    M_half = match_score/2;

    if (gap_open == 0 && gap_extend == 0) {
       return BLAST_GreedyAlign(s1, len1, s2, len2, reverse, 
                                   xdrop_threshold, match_score, 
                                   mismatch_score, e1, e2, gamp, S, rem);
    }
    
    Mis_cost = mismatch_score + match_score;
    GO_cost = gap_open;
    GE_cost = gap_extend+M_half;
    gd = gdb3(&Mis_cost, &GO_cost, &GE_cost);
    D_diff = ICEIL(xdrop_threshold+M_half, gd);
    
    
    MAX_D = (Int4) (len1/ERROR_FRACTION + 1);
    max_d = MAX_D*GE_cost;
    ORIGIN = MAX_D + 2;
    max_cost = MAX(Mis_cost, GO_cost+GE_cost);
    *e1 = *e2 = 0;
    
    if (reverse) {
       if (!(rem & 4)) {
          for (row = 0; row < len2 && row < len1 && 
                  (s2[len2-1-row] ==
                   NCBI2NA_UNPACK_BASE(s1[(len1-1-row)/4], 
                                        3-(len1-1-row)%4)); 
               row++)
             /*empty*/ ;
       } else {
          for (row = 0; row < len2 && row < len1 && (s2[len2-1-row] == s1[len1-1-row]); row++)
             /*empty*/ ;
       }
    } else {
       if (!(rem & 4)) {
          for (row = 0; row < len2 && row < len1 && 
                  (s2[row] == 
                   NCBI2NA_UNPACK_BASE(s1[(row+rem)/4], 
                                        3-(row+rem)%4)); 
               row++)
             /*empty*/ ;
       } else {
          for (row = 0; row < len2 && row < len1 && (s2[row] == s1[row]); row++)
             /*empty*/ ;
       }
    }
    *e1 = row;
    *e2 = row;
    if (row == len1 || row == len2) {
        if (S != NULL)
            edit_script_rep(S, row);
	/* hit last row; stop search */
	return row*match_score;
    }
    flast_d = gamp->flast_d_affine;
    if (S==NULL) {
        space = 0;
    } else if (!space) {
       gamp->space = space = MBSpaceNew();
    } else { 
        refresh_mb_space(space);
    }
    max_row = max_row_free + D_diff;
    for (k = 0; k < D_diff; k++)
	max_row_free[k] = 0;
    uplow_free = gamp->uplow_free;
    lower = uplow_free;
    upper = uplow_free+max_d+1+max_cost;
    /* next 3 lines set boundary for -1,-2,...,-max_cost+1*/
    for (k = 0; k < max_cost; k++) {lower[k] =LARGE;  upper[k] = -LARGE;}
    lower += max_cost;
    upper += max_cost; 
    
    flast_d[0][ORIGIN].C = row;
    flast_d[0][ORIGIN].I = flast_d[0][ORIGIN].D = -2;
    max_row[0] = (row + row)*M_half;
    lower[0] = upper[0] = ORIGIN;
    
    flower = ORIGIN - 1;
    fupper = ORIGIN + 1;
    
    d = 1;
    stop_condition = 1;
    while (d <= max_d) {
	Int4 fl0, fu0;
	x = max_row[d - D_diff] + gd * d - xdrop_threshold;
	x = ICEIL(x, M_half);
	if (x < 0) x=0;
	cur_max = 0;
	fl0 = flower;
	fu0 = fupper;
	for (k = fl0; k <= fu0; k++) {
	    row = -2;
	    if (k+1 <= upper[d-GO_cost-GE_cost] && k+1 >= lower[d-GO_cost-GE_cost]) 
                row = flast_d[d-GO_cost-GE_cost][k+1].C;
	    if (k+1  <= upper[d-GE_cost] && k+1 >= lower[d-GE_cost] &&
		row < flast_d[d-GE_cost][k+1].D) 
                row = flast_d[d-GE_cost][k+1].D;
	    row++;
	    if (row+row+k-ORIGIN >= x) 
	      flast_d[d][k].D = row;
	    else flast_d[d][k].D = -2;
	    row = -1; 
	    if (k-1 <= upper[d-GO_cost-GE_cost] && k-1 >= lower[d-GO_cost-GE_cost]) 
                row = flast_d[d-GO_cost-GE_cost][k-1].C;
	    if (k-1  <= upper[d-GE_cost] && k-1 >= lower[d-GE_cost] &&
		row < flast_d[d-GE_cost][k-1].I) 
                row = flast_d[d-GE_cost][k-1].I;
	    if (row+row+k-ORIGIN >= x) 
                flast_d[d][k].I = row;
	    else flast_d[d][k].I = -2;
            
	    row = MAX(flast_d[d][k].I, flast_d[d][k].D);
	    if (k <= upper[d-Mis_cost] && k >= lower[d-Mis_cost]) 
                row = MAX(flast_d[d-Mis_cost][k].C+1,row);
            
	    col = row + k - ORIGIN;
	    if (row + col >= x)
		fupper = k;
	    else {
		if (k == flower)
		    flower++;
		else
		    flast_d[d][k].C = -2;
		continue;
	    }
            if (row > max_len || row < -2) {
               flower = k; nlower = k+1; 
            } else {
               /* slide down the diagonal */
               if (reverse) {
                  if (!(rem & 4)) {
                     while (row < len2 && col < len1 && s2[len2-1-row] == 
                            NCBI2NA_UNPACK_BASE(s1[(len1-1-col)/4],
                                                 3-(len1-1-col)%4)) {
                        ++row;
                        ++col;
                     }
                  } else {
                     while (row < len2 && col < len1 && s2[len2-1-row] ==
                            s1[len1-1-col]) {
                        ++row;
                        ++col;
                     }
                  }
               } else {
                  if (!(rem & 4)) {
                     while (row < len2 && col < len1 && s2[row] == 
                            NCBI2NA_UNPACK_BASE(s1[(col+rem)/4],
                                                 3-(col+rem)%4)) {
                        ++row;
                        ++col;
                     }
                  } else {
                     while (row < len2 && col < len1 && s2[row] == s1[col]) {
                        ++row;
                        ++col;
                     }
                  }
               }
            }
            flast_d[d][k].C = row;
            if (row + col > cur_max) {
               cur_max = row + col;
               b_diag = k;
            }
            if (row == len2) {
               flower = k; nlower = k+1;
            }
            if (col == len1) {
               fupper = k; nupper = k-1;
            }
	}
	k = cur_max*M_half - d * gd;
	if (max_row[d - 1] < k) {
	    max_row[d] = k;
	    return_val = d;
	    best_diag = b_diag;
	    *e2 = flast_d[d][b_diag].C;
	    *e1 = (*e2)+b_diag-ORIGIN;
	} else {
	    max_row[d] = max_row[d - 1];
	}
	if (flower <= fupper) {
            stop_condition++;
            lower[d] = flower; upper[d] = fupper;
	} else {
            lower[d] = LARGE; upper[d] = -LARGE;
	}
	if (lower[d-max_cost] <= upper[d-max_cost]) stop_condition--;
	if (stop_condition == 0) break;
	d++;
	flower = MIN(lower[d-Mis_cost], MIN(lower[d-GO_cost-GE_cost], lower[d-GE_cost])-1);
	if (nlower) flower = MAX(flower, nlower);
	fupper = MAX(upper[d-Mis_cost], MAX(upper[d-GO_cost-GE_cost], upper[d-GE_cost])+1);
	if (nupper) fupper = MIN(fupper, nupper);
	if (d > max_cost) {
	   if (S==NULL) {
	      /*if (d > max_cost)*/
	      flast_d[d] = flast_d[d - max_cost-1];
	   } else {
	      flast_d[d] = get_mb_space(space, fupper-flower+1)-flower;
	      if (flast_d[d] == NULL)
		 return return_val;
           }
	}
    }
    
    if (S!=NULL) { /*trace back*/
        Int4 row1, diag, state;
        d = return_val; diag = best_diag;
        row = *e2; state = sC;
        while (d > 0) {
            if (state == sC) {
                /* diag will not be changed*/
                state = get_lastC(flast_d, lower, upper, &d, diag, Mis_cost, &row1);
                if (row-row1 > 0) edit_script_rep(S, row-row1);
                row = row1;
            } else {
                if (state == sI) {
                    /*row unchanged */
                    state = get_lastI(flast_d, lower, upper, &d, diag, GO_cost, GE_cost);
                    diag--;
                    edit_script_ins(S,1);
                } else {
                    edit_script_del(S,1);
                    state = get_lastD(flast_d, lower, upper, &d, diag, GO_cost, GE_cost);
                    diag++;
                    row--;
                }
            }
        }
        edit_script_rep(S, flast_d[0][ORIGIN].C);
        if (!reverse) 
            edit_script_reverse_inplace(S);
    }
    return_val = max_row[return_val];
    return return_val;
}


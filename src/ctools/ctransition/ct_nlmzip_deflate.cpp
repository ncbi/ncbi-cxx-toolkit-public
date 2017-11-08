/* $Id$ */
/*****************************************************************************

    Name: Nlmzip_deflate.c                                       ur/compr/Nlmzip_deflate.c

    Description: Utility functions for compress data

    Author: Grisha Starchenko
            InforMax, Inc.
            Gaithersburg, USA.

   ***************************************************************************

                          PUBLIC DOMAIN NOTICE
              National Center for Biotechnology Information

    This software/database is a "United States Government Work" under the
    terms of the United States Copyright Act.  It was written as part of    
    the author's official duties as a United States Government employee
    and thus cannot be copyrighted.  This software/database is freely
    available to the public for use. The National Library of Medicine and
    the U.S. Government have not placed any restriction on its use or
    reproduction.

    Although all reasonable efforts have been taken to ensure the accuracy
    and reliability of the software and data, the NLM and the U.S.
    Government do not and cannot warrant the performance or results that
    may be obtained by using this software or data. The NLM and the U.S.
    Government disclaim all warranties, express or implied, including
    warranties of performance, merchantability or fitness for any
    particular purpose.

    Please cite the author in any work or product based on this material.

   ***************************************************************************

    Entry Points:

       void
       Nlmzip_lm_init (pack_level,flags)
         int pack_level;        [ compression Nlmzip_level (I) ]
         unsigned short *flags; [ general purpose bit flag (O) ]

       ulg
       Nlmzip_deflate(void)

    Modification History:
           05 Aug 1995 - grisha   -  original written

    Bugs and restriction on use:

    Notes:

*****************************************************************************/

#include <ncbi_pch.hpp>
#include "ct_nlmzip_i.h"

BEGIN_CTRANSITION_SCOPE


/****************************************************************************/
/* DEFINES */
/****************************************************************************/
/* For portability to 16 bit machines, do not use values above 15. */
#define HASH_BITS  15

/* To save space, we overlay Nlmzip_prev+head with tab_prefix
   and Nlmzip_window with tab_suffix. Check that we can do this:
*/
#if (WSIZE<<1) > (1<<BITS)
#   error: cannot overlay Nlmzip_window with tab_suffix and Nlmzip_prev with tab_prefix0
#endif
#if HASH_BITS > BITS-1
#   error: cannot overlay head with tab_prefix1
#endif

/* HASH_SIZE and WSIZE must be powers of two */
#define HASH_SIZE (Uint4)(1<<HASH_BITS)
#define HASH_MASK (HASH_SIZE-1)
#define WMASK     (WSIZE-1)

#define NIL 0                      /* Tail of hash chains */

/* speed options for the general purpose bit flag */
#define FAST 4
#define SLOW 2

/* Matches of length 3 are discarded if their distance
   exceeds TOO_FAR
*/
#define TOO_FAR 4096

/* Number of bits by which ins_h and del_h must be shifted at each
   input step. It must be such that after MIN_MATCH steps, the oldest
   byte no longer takes part in the hash key, that is:
   H_SHIFT * MIN_MATCH >= HASH_BITS
*/
#define H_SHIFT  ((HASH_BITS+MIN_MATCH-1)/MIN_MATCH)

/* Insert new strings in the hash table only if the match length
   is not greater than this length. This saves time but degrades
   compression. max_insert_length is used only for compression
   levels <= 3.
*/
#define max_insert_length  max_lazy_match

/* result of memcmp for equal strings */
#define EQUAL 0

/* Update a hash value with the given input byte. In  assertion: all
   calls to to UPDATE_HASH are made with consecutive input characters,
   so that a running hash key can be computed from the previous key
   instead of complete recalculation each time.
*/
#define UPDATE_HASH(h,c) (h = (((h)<<H_SHIFT) ^ (c)) & HASH_MASK)

/* Insert string s in the dictionary and set match_head to the
   previous head of the hash chain (the most recent string with
   same hash key). Return the previous length of the hash chain.
   In assertion: all calls to to INSERT_STRING are made with
   consecutive input characters and the first MIN_MATCH bytes of
   s are valid (except for the last MIN_MATCH-1 bytes of the input
   file).
*/
#define INSERT_STRING(s, match_head) \
   (UPDATE_HASH(ins_h, Nlmzip_window[(s) + MIN_MATCH-1]), \
    Nlmzip_prev[(s) & WMASK] = match_head = head[ins_h], \
    head[ins_h] = (s))

/* Flush the current block, with given end-of-data flag.
   In assertion: Nlmzip_strstart is set to the end of the current
   match.
*/
#define FLUSH_BLOCK(eof) \
      Nlmzip_flush_block(Nlmzip_block_start >= 0L \
    ? (char*)&Nlmzip_window[(Uint4)Nlmzip_block_start] \
    : (char*)NULL,(Int4)Nlmzip_strstart-Nlmzip_block_start,(eof))

/****************************************************************************/
/* TYPEDEFS */
/****************************************************************************/

/* A Pos is an index in the character Nlmzip_window. We use short instead of
   int to save space in the various tables. IPos is used only for
   parameter passing.
*/
typedef unsigned short Pos;
typedef Uint4 IPos;

typedef struct config {
   ush good_length; /* reduce lazy search above this match length */
   ush max_lazy;    /* do not perform lazy search above this match length */
   ush nice_length; /* quit search above this match length */
   ush max_chain;
} config;

/****************************************************************************/
/* LOCAL VARIABLES */
/****************************************************************************/
/* Nlmzip_window size, 2*WSIZE except for MMAP or BIG_MEM, where it is the
   input file length plus MIN_LOOKAHEAD.
*/
static ulg window_size = (ulg)2*WSIZE;

static Uint4 ins_h;               /* hash index of string to be inserted */

/* Length of the best match at previous step. Matches not greater
   than this are discarded. This is used in the lazy match evaluation.
*/
static Uint4 prev_length;

static Uint4  match_start;   /* start of matching string */
static int       eofile;        /* flag set at end of input stream */
static Uint4  lookahead;     /* number of valid bytes ahead in Nlmzip_window */

/* To speed up deflation, hash chains are never searched beyond
   this length. A higher limit improves compression ratio but
   degrades the speed.
*/
static Uint4 max_chain_length;

/* Attempt to find a better match only when the current match
   is strictly smaller than this value. This mechanism is used
   only for compression
   levels >= 4.
*/
static Uint4  max_lazy_match;

/* compression Nlmzip_level (1..9) */
static int compr_level;

/* Use a faster search when the previous match is longer than this */
static Uint4  good_match;

/* Stop searching when current match exceeds this */
static int nice_match;

/* Values for max_lazy_match, good_match and max_chain_length,
   depending on the desired pack Nlmzip_level (0..9). The values given
   below have been tuned to exclude worst case performance for
   pathological files. Better values may be found for specific
   files.

   Note: the Nlmzip_deflate() code requires max_lazy >= MIN_MATCH and
   max_chain >= 4 For deflate_fast() (levels <= 3) good is ignored
   and lazy has a different meaning.
*/
static config configuration_table[10] = { /* good lazy nice chain */
   /* 0 */ {0,    0,  0,    0},  /* store only */
   /* 1 */ {4,    4,  8,    4},  /* maximum speed, no lazy matches */
   /* 2 */ {4,    5, 16,    8},
   /* 3 */ {4,    6, 32,   32},
   /* 4 */ {4,    4, 16,   16},  /* lazy matches */
   /* 5 */ {8,   16, 32,   32},
   /* 6 */ {8,   16, 128, 128},
   /* 7 */ {8,   32, 128, 256},
   /* 8 */ {32, 128, 258, 1024},
   /* 9 */ {32, 258, 258, 4096}  /* maximum compression */
};


/****************************************************************************/
/* LOCAL FUNCTION PROTOTYPES */
/****************************************************************************/

static void fill_window   _((void));
static ulg  deflate_fast  _((void));
static int  longest_match _((IPos cur_match));

/****************************************************************************/
/* LOCAL FUNCTION */
/****************************************************************************/

/****************************************************************************/
/*.doc longest_match (external) */
/*+
   Set match_start to the longest match starting at the given string and
   return its length. Matches shorter or equal to prev_length are discarded,
   in which case the result is equal to prev_length and match_start is
   garbage.
   In assertions: cur_match is the head of the hash chain for the current
   string (Nlmzip_strstart) and its distance is <= MAX_DIST, and prev_length >= 1
   The code is optimized for HASH_BITS >= 8 and MAX_MATCH-2 multiple of 16.
   It is easy to get rid of this optimization if necessary.
-*/
/****************************************************************************/
static int
longest_match ( /*FCN*/
  IPos cur_match               /* current match (I) */
){
    Uint4 chain_length = max_chain_length;   /* max hash chain length */
    register uch *scan = Nlmzip_window + Nlmzip_strstart;     /* current string */
    register uch *match;                        /* matched string */
    register int len;                           /* length of current match */
    int best_len = prev_length;                 /* best match length so far */
    register uch *strend   = Nlmzip_window + Nlmzip_strstart + MAX_MATCH;
    register uch scan_end1 = scan[best_len-1];
    register uch scan_end  = scan[best_len];
    IPos limit;

    /* Stop when cur_match becomes <= limit. To simplify the code,
       we prevent matches with the string of Nlmzip_window index 0.
    */
    limit = Nlmzip_strstart > (IPos)MAX_DIST ? Nlmzip_strstart - (IPos)MAX_DIST : NIL;

    /* Do not waste too much time if we already have a good match: */
    if (prev_length >= good_match) {
        chain_length >>= 2;
    }

    do {
        match = Nlmzip_window + cur_match;

        /* Skip to next match if the match length cannot increase
           or if the match length is less than 2:
        */
        if (match[best_len]   != scan_end  ||
            match[best_len-1] != scan_end1 ||
            *match            != *scan     ||
            *++match          != scan[1])      continue;

        /* The check at best_len-1 can be removed because it
           will be made again later. (This heuristic is not
           always a win.) It is not necessary to compare scan[2]
           and match[2] since they are always equal when the
           other bytes match, given that the hash keys are equal
           and that HASH_BITS >= 8.
        */
        scan += 2;
        match++;

        /* We check for insufficient lookahead only every 8th comparison;
           the 256th check will be made at Nlmzip_strstart+258.
        */
        do {
        } while (
                *++scan == *++match && *++scan == *++match
             && *++scan == *++match && *++scan == *++match
             && *++scan == *++match && *++scan == *++match
             && *++scan == *++match && *++scan == *++match
             && scan < strend
          );

        len = MAX_MATCH - (int)(strend - scan);
        scan = strend - MAX_MATCH;

        if ( len > best_len ) {
            match_start = cur_match;
            best_len = len;
            if ( len >= nice_match ) {
                break;
            }
            scan_end1  = scan[best_len-1];
            scan_end   = scan[best_len];
        }
    } while (    (cur_match = Nlmzip_prev[cur_match & WMASK]) > limit
	      && --chain_length != 0 );

    return best_len;
}

/****************************************************************************/
/*.doc fill_window (external) */
/*+
   Fill the Nlmzip_window when the lookahead becomes insufficient.
   Updates Nlmzip_strstart and lookahead, and sets eofile if end of input file.
   In assertion: lookahead < MIN_LOOKAHEAD && Nlmzip_strstart + lookahead > 0
   Out assertions: at least one byte has been read, or eofile is set;
   data reads are performed for at least two bytes (required for the
   translate_eol option).
-*/
/****************************************************************************/
static void
fill_window (void) /*FCN*/
{
    register Uint4 n;
    register Uint4 m;
    Uint4 more;

    /* Amount of free space at the end of the Nlmzip_window.
    */
    more = (Uint4)(window_size - (ulg)lookahead - (ulg)Nlmzip_strstart);

    /* If the Nlmzip_window is almost full and there is insufficient
       lookahead, move the upper half to the lower one to make
       room in the upper half.
    */
    if ( more == (Uint4)~0 ) {
        /* Very unlikely, but possible on 16 bit machine if Nlmzip_strstart == 0
           and lookahead == 1 (input done one byte at time)
        */
        more--;
    } else if ( Nlmzip_strstart >= WSIZE+MAX_DIST ) {
        memcpy((char*)Nlmzip_window, (char*)Nlmzip_window+WSIZE, (Uint4)WSIZE);
        match_start -= WSIZE;
        Nlmzip_strstart    -= WSIZE; /* we now have Nlmzip_strstart >= MAX_DIST: */

        Nlmzip_block_start -= (Int4) WSIZE;

        for ( n = 0; n < HASH_SIZE; n++ ) {
            m = head[n];
            head[n] = (Pos)(m >= WSIZE ? m-WSIZE : NIL);
        }
        for ( n = 0; n < WSIZE; n++ ) {
            m = Nlmzip_prev[n];
            /* If n is not on any hash chain, Nlmzip_prev[n] is
               garbage but its value will never be used.
            */
            Nlmzip_prev[n] = (Pos)(m >= WSIZE ? m-WSIZE : NIL);
        }
        more += WSIZE;
    }

    /* At this point, more >= 2 */
    if ( eofile == 0 ) {
        n = Nlmzip_ReadData (
                Nlmzip_window+Nlmzip_strstart+lookahead,
                more
            );
        if ( n == 0 ) {
            eofile = 1;
        } else {
            lookahead += n;
        }
    }
}

/****************************************************************************/
/*.doc deflate_fast (external) */
/*+
   Processes a new input file and return its compressed length. This
   function does not perform lazy evaluationof matches and inserts
   new strings in the dictionary only for unmatched strings or for short
   matches. It is used only for the fast compression options.
-*/
/****************************************************************************/
static ulg
deflate_fast (void) /*FCN*/
{
    IPos hash_head;             /* head of the hash chain */
    int flush;                  /* set if current block must be flushed */
    Uint4 match_length = 0;  /* length of best match */

    prev_length = MIN_MATCH-1;
    while (lookahead != 0) {
        /* Insert the string Nlmzip_window[Nlmzip_strstart .. Nlmzip_strstart+2] in the
           dictionary, and set hash_head to the head of the hash chain:
        */
        INSERT_STRING(Nlmzip_strstart, hash_head);

        /* Find the longest match, discarding those <= prev_length.
           At this point we have always match_length < MIN_MATCH
        */
        if ( hash_head != NIL && Nlmzip_strstart - hash_head <= MAX_DIST ) {
            /* To simplify the code, we prevent matches with the string
               of Nlmzip_window index 0 (in particular we have to avoid a match
               of the string with itself at the start of the input file).
            */
            match_length = longest_match (hash_head);

            /* longest_match() sets match_start */
            if ( match_length > lookahead ) {
                match_length = lookahead;
            }
        }
        if ( match_length >= MIN_MATCH ) {
            flush = Nlmzip_ct_tally (
                        Nlmzip_strstart-match_start,
                        match_length - MIN_MATCH
                    );

            lookahead -= match_length;

	    /* Insert new strings in the hash table only if the match
               length is not too large. This saves time but degrades
               compression.
            */
            if ( match_length <= max_insert_length ) {
                /* string at Nlmzip_strstart already in hash table */
                match_length--;
                do {
                    /* Nlmzip_strstart never exceeds WSIZE-MAX_MATCH, so
                       there are always MIN_MATCH bytes ahead. If
                       lookahead < MIN_MATCH these bytes are garbage,
                       but it does not matter since the next lookahead
                       bytes will be emitted as literals.
                    */
                    Nlmzip_strstart++;
                    INSERT_STRING(Nlmzip_strstart, hash_head);
                } while ( --match_length != 0 );
	        Nlmzip_strstart++; 
            } else {
	        Nlmzip_strstart += match_length;
	        match_length = 0;
	        ins_h = Nlmzip_window[Nlmzip_strstart];
	        UPDATE_HASH(ins_h, Nlmzip_window[Nlmzip_strstart+1]);
            }
        } else {
            /* No match, output a literal byte */
            flush = Nlmzip_ct_tally (0, Nlmzip_window[Nlmzip_strstart]);
            lookahead--;
	    Nlmzip_strstart++; 
        }
        if ( flush ) {
            FLUSH_BLOCK(0);
            Nlmzip_block_start = Nlmzip_strstart;
        }

        /* Make sure that we always have enough lookahead, except
           at the end of the input file. We need MAX_MATCH bytes
           for the next match, plus MIN_MATCH bytes to insert the
           string following the next match.
        */
        while ( lookahead < MIN_LOOKAHEAD && eofile == 0 ) {
            fill_window();
        }
    }

    return FLUSH_BLOCK(1);            /* done */
}

/****************************************************************************/
/* GLOBAL FUNCTION */
/****************************************************************************/

/****************************************************************************/
/*.doc Nlmzip_lm_init (external) */
/*+
  Initialize the "longest match" routines for a new file
-*/
/****************************************************************************/
void
Nlmzip_lm_init ( /*FCN*/
    int pack_level, /* 0: store, 1: best speed, 9: best compression (I) */
    ush *flags      /* general purpose bit flag (O) */
){
    register Uint4 j;

    if ( pack_level < 1 || pack_level > 9 ) {
        URCOMPRERR ("Invalid compression Nlmzip_level");
    }
    compr_level = pack_level;

    /* Initialize the hash table. */
    memset ((char*)head, 0, HASH_SIZE*sizeof(*head));

    /* Nlmzip_prev will be initialized on the fly */

    /* Set the default configuration parameters:
    */
    max_lazy_match   = configuration_table[pack_level].max_lazy;
    good_match       = configuration_table[pack_level].good_length;
    nice_match       = configuration_table[pack_level].nice_length;
    max_chain_length = configuration_table[pack_level].max_chain;
    if ( pack_level == 1 ) {
        *flags |= FAST;
    } else if ( pack_level == 9 ) {
        *flags |= SLOW;
    }

    Nlmzip_strstart = 0;
    Nlmzip_block_start = 0L;

    lookahead = Nlmzip_ReadData (
                   (unsigned char*)Nlmzip_window,
                   2*WSIZE
                );
    if ( lookahead == 0 ) {
        eofile = 1;
        lookahead = 0;
        return;
    }
    eofile = 0;

    /* Make sure that we always have enough lookahead. This is important
       if input comes from a device such as a tty.
    */
    while ( lookahead < MIN_LOOKAHEAD && eofile == 0 ) {
        fill_window();
    }

    /* If lookahead < MIN_MATCH, ins_h is garbage, but this is
       not important since only literal bytes will be emitted.
    */
    ins_h = 0;
    for ( j = 0; j < MIN_MATCH-1; j++ ) {
        UPDATE_HASH(ins_h, Nlmzip_window[j]);
    }
}

/****************************************************************************/
/*.doc Nlmzip_deflate (external) */
/*+
   Same as deflate_fast(), but achieves better compression. We use a
   lazy evaluation for matches: a match is finally adopted only if
   there is no better match at the next Nlmzip_window position.
-*/
/****************************************************************************/
ulg
Nlmzip_deflate(void) /*FCN*/
{
    IPos hash_head;          /* head of hash chain */
    IPos prev_match;         /* previous match */
    int flush;               /* set if current block must be flushed */
    int match_available = 0; /* set if previous match exists */
    register Uint4 match_length = MIN_MATCH-1; /* length of best match */

    if ( compr_level <= 3 ) {        /* optimized for speed */
        return deflate_fast();
    }

    while ( lookahead != 0 ) {       /* Process the input block. */
        /* Insert the string Nlmzip_window[Nlmzip_strstart .. Nlmzip_strstart+2] in
           the dictionary, and set hash_head to the head of the
           hash chain:
        */
        INSERT_STRING(Nlmzip_strstart, hash_head);

        /* Find the longest match, discarding those <= prev_length.
        */
        prev_length = match_length, prev_match = match_start;
        match_length = MIN_MATCH-1;

        if (    hash_head != NIL
             && prev_length < max_lazy_match
             && Nlmzip_strstart - hash_head <= MAX_DIST ) {
            /* To simplify the code, we prevent matches with the string
               of Nlmzip_window index 0 (in particular we have to avoid a match
               of the string with itself at the start of the input file).
            */
            match_length = longest_match (hash_head);

            /* longest_match() sets match_start */
            if ( match_length > lookahead ) {
                match_length = lookahead;
            }

            /* Ignore a length 3 match if it is too distant: */
            if (    match_length == MIN_MATCH
                 && Nlmzip_strstart-match_start > TOO_FAR ){
                /* If prev_match is also MIN_MATCH, match_start
                   is garbage but we will ignore the current match
                   anyway.
                */
                match_length--;
            }
        }

        /* If there was a match at the previous step and the
           current match is not better, output the previous
           match:
        */
        if (    prev_length >= MIN_MATCH
             && match_length <= prev_length) {
            flush = Nlmzip_ct_tally (
                        Nlmzip_strstart-1-prev_match,
                        prev_length - MIN_MATCH
                    );

            /* Insert in hash table all strings up to the end of
               the match. Nlmzip_strstart-1 and Nlmzip_strstart are already
               inserted.
            */
            lookahead -= prev_length-1;
            prev_length -= 2;
            do {
                /* Nlmzip_strstart never exceeds WSIZE-MAX_MATCH, so
                   there are always MIN_MATCH bytes ahead. If
                   lookahead < MIN_MATCH these bytes are garbage,
                   but it does not matter since the next lookahead
                   bytes will always be emitted as literals.
                */
                Nlmzip_strstart++;
                INSERT_STRING(Nlmzip_strstart, hash_head);
            } while ( --prev_length != 0 );
            match_available = 0;
            match_length = MIN_MATCH-1;
            Nlmzip_strstart++;
            if ( flush ) {
                FLUSH_BLOCK(0);
                Nlmzip_block_start = Nlmzip_strstart;
            }
        } else if ( match_available ) {
            /* If there was no match at the previous position,
               output a single literal. If there was a match
               but the current match is longer, truncate the
               previous match to a single literal.
            */
            if ( Nlmzip_ct_tally (0, Nlmzip_window[Nlmzip_strstart-1]) ) {
                FLUSH_BLOCK(0);
                Nlmzip_block_start = Nlmzip_strstart;
            }
            Nlmzip_strstart++;
            lookahead--;
        } else {
            /* There is no previous match to compare with, wait
               for the next step to decide.
            */
            match_available = 1;
            Nlmzip_strstart++;
            lookahead--;
        }

        /* Make sure that we always have enough lookahead, except
           at the end of the input file. We need MAX_MATCH bytes
           for the next match, plus MIN_MATCH bytes to insert the
           string following the next match.
        */
        while ( lookahead < MIN_LOOKAHEAD && eofile == 0 ) {
             fill_window();
        }
    }
    if ( match_available ) {
        Nlmzip_ct_tally (0, Nlmzip_window[Nlmzip_strstart-1]);
    }

    return FLUSH_BLOCK(1);                  /* done */
}


END_CTRANSITION_SCOPE

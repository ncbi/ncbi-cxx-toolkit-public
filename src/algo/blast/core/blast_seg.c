static char const rcsid[] = "$Id$";
/*
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
* File Name:  blast_seg.c
*
* Author(s): Ilya Dondoshansky
*   
* Version Creation Date: 05/28/2003
*
* $Revision$
*
* File Description: A utility to find low complexity AA regions.
*                   This parallels functionality of seg.c from the C toolkit,
*                   but without using the structures generated from ASN.1 spec.
* ==========================================================================
*/

#include <algo/blast/core/blast_seg.h>
#include <algo/blast/core/lnfact.h>

/*---------------------------------------------------------------(SeqNew)---*/

static Sequence* SeqNew(void)
{
   Sequence* seq;

   seq = (Sequence*) calloc(1, sizeof(Sequence));
   if (seq==NULL)
     {
      /* raise error flag and etc. */
      return(seq);
     }

   seq->parent = (Sequence*) NULL;
   seq->seq = (char*) NULL;
   seq->palpha = (Alpha*) NULL;
   seq->start = seq->length = 0;
   seq->bogus = seq->punctuation = FALSE;
   seq->composition = seq->state = (Int4*) NULL;
   seq->entropy = (double) 0.0;

   return(seq);
}

/*------------------------------------------------------------(AlphaFree)---*/

static void AlphaFree (Alpha* palpha)

  {
   if (!palpha) return;

   sfree (palpha->alphaindex);
   sfree (palpha->alphaflag);
   sfree (palpha->alphachar);
   sfree (palpha);

   return;
 }

/*--------------------------------------------------------------(SeqFree)---*/

static void SeqFree(Sequence* seq)
{
   if (seq==NULL) return;

   sfree(seq->seq);
   AlphaFree(seq->palpha);
   sfree(seq->composition);
   sfree(seq->state);
   sfree(seq);
   return;
}

/*--------------------------------------------------------------(SegFree)---*/

static void SegFree(Seg* seg)
{
   Seg* nextseg;

   while (seg)
     {
      nextseg = seg->next;
      sfree(seg);
      seg = nextseg;
     }

   return;
}

/*--------------------------------------------------------------(hasdash)---*/

static Boolean hasdash(Sequence* win)
{
	register char	*seq, *seqmax;

	seq = win->seq;
	seqmax = seq + win->length;

	while (seq < seqmax) {
		if (*seq++ == '-')
			return TRUE;
	}
	return FALSE;
}

/*------------------------------------------------------------(state_cmp)---*/

static int state_cmp(const void* s1, const void* s2)
{
	int *np1, *np2;

	np1 = (int *) s1;
	np2 = (int *) s2;

	return (*np2 - *np1);
}

/*---------------------------------------------------------------(compon)---*/

static void compon(Sequence* win)

{
	Int4* comp;
	Int4 letter;
	char* seq,* seqmax;
        Int4* alphaindex;
        unsigned char* alphaflag;
        Int4 alphasize;

        alphasize = win->palpha->alphasize;
        alphaindex = win->palpha->alphaindex;
        alphaflag = win->palpha->alphaflag;

	win->composition = comp =
                (Int4*) calloc(alphasize, sizeof(Int4));
	seq = win->seq;
	seqmax = seq + win->length;

	while (seq < seqmax) {
		letter = *seq++;
		if (!alphaflag[letter])
			comp[alphaindex[letter]]++;
      else win->bogus++;
	}

	return;
}

/*--------------------------------------------------------------(stateon)---*/

static void stateon(Sequence* win)

{
	Int4 letter, nel, c;
        Int4 alphasize;

        alphasize = win->palpha->alphasize;

	if (win->composition == NULL)
		compon(win);

	win->state = (Int4*) calloc((alphasize+1), sizeof(win->state[0]));

	for (letter = nel = 0; letter < alphasize; ++letter) {
		if ((c = win->composition[letter]) == 0)
			continue;
		win->state[nel++] = c;
	}
	for (letter = nel; letter < alphasize+1; ++letter)
		win->state[letter] = 0;

	qsort(win->state, nel, sizeof(win->state[0]), state_cmp);

	return;
}

/*--------------------------------------------------------------(openwin)---*/

static Sequence* openwin(Sequence* parent, Int4 start, Int4 length)
{
   Sequence* win;

   if (start<0 || length<0 || start+length>parent->length)
     {
      return((Sequence*) NULL);
     }

   win = (Sequence*) calloc(1, sizeof(Sequence));

/*---                                          ---[set links, up and down]---*/

   win->parent = parent;
   win->palpha = parent->palpha;

/*---                          ---[install the local copy of the sequence]---*/

   win->start = start;
   win->length = length;
#if 0                                                    /* Hi Warren! */
   win->seq = (char*) calloc(length + 1, sizeof(char));
   memcpy(win->seq, (parent->seq)+start, length);
   win->seq[length] = '\0';
#else
	win->seq = parent->seq + start;
#endif

        win->bogus = 0;
	win->punctuation = FALSE;

	win->entropy = -2.;
	win->state = (Int4*) NULL;
	win->composition = (Int4*) NULL;

	stateon(win);

	return win;
}

/*--------------------------------------------------------------(entropy)---*/

static double entropy(Int4* sv)

  {double ent;
   Int4 i, total;

   total = 0;
   for (i=0; sv[i]!=0; i++)
     {
      total += sv[i];
     }
   if (total==0) return(0.);

   ent = 0.0;
   for (i=0; sv[i]!=0; i++)
     {
      ent += ((double)sv[i])*log(((double)sv[i])/(double)total)/LN2;
     }

   ent = fabs(ent/(double)total);

   return(ent);
  }

/*----------------------------------------------------------(decrementsv)---*/

static void decrementsv(Int4* sv, Int4 class)

{
	Int4	svi;

	while ((svi = *sv++) != 0) {
		if (svi == class && *sv < class) {
			sv[-1] = svi - 1;
			break;
		}
	}
}

/*----------------------------------------------------------(incrementsv)---*/

static void incrementsv(Int4* sv, Int4 class)

{
	for (;;) {
		if (*sv++ == class) {
			sv[-1]++;
			break;
		}
	}
}

/*------------------------------------------------------------(shiftwin1)---*/

static Int4 shiftwin1(Sequence* win)
{
	Int4 j, length;
	Int4* comp;
        Int4* alphaindex;
        unsigned char* alphaflag;

	length = win->length;
	comp = win->composition;
        alphaindex = win->palpha->alphaindex;
        alphaflag = win->palpha->alphaflag;

	if ((++win->start + length) > win->parent->length) {
		--win->start;
		return FALSE;
	}

	if (!alphaflag[j = win->seq[0]])
		decrementsv(win->state, comp[alphaindex[j]]--);
        else win->bogus--;

	j = win->seq[length];
	++win->seq;

	if (!alphaflag[j])
		incrementsv(win->state, comp[alphaindex[j]]++);
        else win->bogus++;

	if (win->entropy > -2.)
		win->entropy = entropy(win->state);

	return TRUE;
}

/*-------------------------------------------------------------(closewin)---*/

static void closewin(Sequence* win)
{
   if (win==NULL) return;

   if (win->state!=NULL)       sfree(win->state);
   if (win->composition!=NULL) sfree(win->composition);

   sfree(win);
   return;
}

/*----------------------------------------------------------------(enton)---*/

static void enton(Sequence* win)

  {
   if (win->state==NULL) {stateon(win);}

   win->entropy = entropy(win->state);

   return;
  }

/*---------------------------------------------------------------(seqent)---*/
static double* seqent(Sequence* seq, Int4 window, Int4 maxbogus)
{
   Sequence* win;
   double* H;
   Int4 i, first, last, downset, upset;

   downset = (window+1)/2 - 1;
   upset = window - downset;

   if (window>seq->length)
     {
      return((double*) NULL);
     }

   H = (double*) calloc(seq->length, sizeof(double));

   for (i=0; i<seq->length; i++)
     {
      H[i] = -1.;
     }

   win = openwin(seq, 0, window);
   enton(win);

   first = downset;
   last = seq->length - upset;

   for (i=first; i<=last; i++)
     {
      if (seq->punctuation && hasdash(win))
        {
         H[i] = -1.;
         shiftwin1(win);
         continue;
        }
      if (win->bogus > maxbogus)
        {
         H[i] = -1.;
         shiftwin1(win);
         continue;
        }
      H[i] = win->entropy;
      shiftwin1(win);
     }

   closewin(win);
   return(H);
}

/*------------------------------------------------------------(appendseg)---*/

static void
appendseg(Seg* segs, Seg* seg)

  {Seg* temp;

   temp = segs;
   while (TRUE)
     {
      if (temp->next==NULL)
        {
         temp->next = seg;
         break;
        }
      else
        {
         temp = temp->next;
        }
     }

   return;
  }

/*---------------------------------------------------------------(findlo)---*/

static Int4 findlo(Int4 i, Int4 limit, double hicut, double* H)

  {
   Int4 j;

   for (j=i; j>=limit; j--)
     {
      if (H[j]==-1) break;
      if (H[j]>hicut) break;
     }

   return(j+1);
  }

/*---------------------------------------------------------------(findhi)---*/

static Int4 findhi(Int4 i, Int4 limit, double hicut, double* H)

  {
   Int4 j;

   for (j=i; j<=limit; j++)
     {
      if (H[j]==-1) break;
      if (H[j]>hicut) break;
     }

   return(j-1);
  }

/*---------------------------------------------------------------(lnperm)---*/

static double lnperm(Int4* sv, Int4 tot)

  {
   double ans;
   Int4 i;

   ans = lnfact[tot];

   for (i=0; sv[i]!=0; i++) 
     {
      ans -= lnfact[sv[i]];
     }

   return(ans);
  }

/*----------------------------------------------------------------(lnass)---*/

static double lnass(Int4* sv, Int4 alphasize)

{
	double	ans;
	int	svi, svim1;
	int	class, total;
	int    i;

	ans = lnfact[alphasize];
	if (sv[0] == 0)
		return ans;

	total = alphasize;
	class = 1;
	svi = *sv;
	svim1 = sv[0];
	for (i=0;; svim1 = svi) {
	        if (++i==alphasize) {
		        ans -= lnfact[class];
			break;
		      }
		else if ((svi = *++sv) == svim1) {
			class++;
			continue;
		}
		else {
			total -= class;
			ans -= lnfact[class];
			if (svi == 0) {
				ans -= lnfact[total];
				break;
			}
			else {
				class = 1;
				continue;
			}
		}
	}

	return ans;
}

/*--------------------------------------------------------------(getprob)---*/

static double getprob(Int4* sv, Int4 total, Alpha* palpha)

  {
   double ans, ans1, ans2 = 0, totseq;

   /* #define LN20	2.9957322735539909 */
   /* #define LN4	1.3862943611198906 */

   totseq = ((double) total) * palpha->lnalphasize;

/*
   ans = lnass(sv, palpha->alphasize) + lnperm(sv, total) - totseq;
*/
   ans1 = lnass(sv, palpha->alphasize);
   if (ans1 > -100000.0 && sv[0] != INT4_MIN)
   {
	ans2 = lnperm(sv, total);
   }
   else
   {
#ifdef ERR_POST_EX_DEFINED
        ErrPostEx (SEV_ERROR, 0, 0, "Illegal value returned by lnass");
#endif
   }
   ans = ans1 + ans2 - totseq;
/*
fprintf(stderr, "%lf %lf %lf\n", ans, ans1, ans2);
*/

   return ans;
  }

/*-----------------------------------------------------------------(trim)---*/

static void trim(Sequence* seq, Int4* leftend, Int4* rightend,
                 SegParameters* sparamsp)

  {
   Sequence* win;
   double prob, minprob;
   Int4 shift, len, i;
   Int4 lend, rend;
   Int4 minlen;
   Int4 maxtrim;

/* fprintf(stderr, "%d %d\n", *leftend, *rightend);  */

   lend = 0;
   rend = seq->length - 1;
   minlen = 1;
   maxtrim = sparamsp->maxtrim;
   if ((seq->length-maxtrim)>minlen) minlen = seq->length-maxtrim;

   minprob = 1.;
/*   fprintf(stderr, "\n");                                           -*/
   for (len=seq->length; len>minlen; len--)
     {
/*      fprintf(stderr, "%5d ", len);                                 -*/
      win = openwin(seq, 0, len);
      i = 0;

      shift = TRUE;
      while (shift)
        {
         prob = getprob(win->state, len, sparamsp->palpha);
/*         realprob = exp(prob);                 (for tracing the trim)-*/
/*         fprintf(stderr, "%2e ", realprob);                          -*/
         if (prob<minprob)
           {
            minprob = prob;
            lend = i;
            rend = len + i - 1;
           }
         shift = shiftwin1(win);
         i++;
        }
      closewin(win);
/*      fprintf(stderr, "\n");                                       -*/
     }

/*   fprintf(stderr, "%d-%d ", *leftend, *rightend);                 -*/

   *leftend = *leftend + lend;
   *rightend = *rightend - (seq->length - rend - 1);

/*   fprintf(stderr, "%d-%d\n", *leftend, *rightend);                -*/

   closewin(seq);
   return;
  }

/*---------------------------------------------------------------(SegSeq)---*/

static void SegSeq(Sequence* seq, SegParameters* sparamsp, Seg* *segs,
                   Int4 offset)
{
   Seg* seg,* leftsegs;
   Sequence* leftseq;
   Int4 window;
   double locut, hicut;
   Int4 maxbogus;
   Int4 downset, upset;
   Int4 first, last, lowlim;
   Int4 loi, hii, i;
   Int4 leftend, rightend, lend, rend;
   double* H;

   if (sparamsp->window<=0) return;
   if (sparamsp->locut<=0.) sparamsp->locut = 0.;
   if (sparamsp->hicut<=0.) sparamsp->hicut = 0.;

   window = sparamsp->window;
   locut = sparamsp->locut;
   hicut = sparamsp->hicut;
   downset = (window+1)/2 - 1;
   upset = window - downset;
   maxbogus = sparamsp->maxbogus;
      
   H = seqent(seq, window, maxbogus);
   if (H==NULL) return;

   first = downset;
   last = seq->length - upset;
   lowlim = first;

   for (i=first; i<=last; i++)
     {
      if (H[i] <= locut && H[i] != -1.0)
        {
         loi = findlo(i, lowlim, hicut, H);
         hii = findhi(i, last, hicut, H);

         leftend = loi - downset;
         rightend = hii + upset - 1;

         trim(openwin(seq, leftend, rightend-leftend+1), &leftend, &rightend,
              sparamsp);

         if (i+upset-1<leftend)   /* check for trigger window in left trim */
           {
            lend = loi - downset;
            rend = leftend - 1;

            leftseq = openwin(seq, lend, rend-lend+1);
            leftsegs = (Seg*) NULL;
            SegSeq(leftseq, sparamsp, &leftsegs, offset+lend);
            if (leftsegs!=NULL)
              {
               if (*segs==NULL) *segs = leftsegs;
               else appendseg(*segs, leftsegs);
              }
            closewin(leftseq);

/*          trim(openwin(seq, lend, rend-lend+1), &lend, &rend);
            seg = (Seg*) calloc(1, sizeof(Seg));
            seg->begin = lend;
            seg->end = rend;
            seg->next = (Seg*) NULL;
            if (segs==NULL) segs = seg;
            else appendseg(segs, seg);  */
           }

         seg = (Seg*) calloc(1, sizeof(Seg));
         seg->begin = leftend + offset;
         seg->end = rightend + offset;
         seg->next = (Seg*) NULL;

         if (*segs==NULL) *segs = seg;
         else appendseg(*segs, seg);

         i = MIN(hii, rightend+downset);
         lowlim = i + 1;
/*       i = hii;     this ignores the trimmed residues... */
        }
     }
   sfree(H);
   return;
}

/*------------------------------------------------------------(mergesegs)---*/
/* merge together overlapping segments, 
	hilenmin also does something, but we need to ask Scott Federhen what?
*/

static void mergesegs(Sequence* seq, Seg* segs, Boolean overlaps)
{
   Seg* seg,* nextseg;
   Int4 hilenmin;		/* hilenmin yet unset */
   Int4 len;

   hilenmin = 0;               /* hilenmin - temporary default */

   if (overlaps == FALSE) 
	return;
	
   if (segs==NULL) return;

   if (segs->begin<hilenmin) segs->begin = 0;

   seg = segs;
   nextseg = seg->next;

   while (nextseg!=NULL)
     {
      if (seg->end>=nextseg->begin && seg->end>=nextseg->end)
        {
         seg->next = nextseg->next;
         sfree(nextseg);
         nextseg = seg->next;
         continue;
        }
      if (seg->end>=nextseg->begin)               /* overlapping segments */
        {
         seg->end = nextseg->end;
         seg->next = nextseg->next;
         sfree(nextseg);
         nextseg = seg->next;
         continue;
        }
      len = nextseg->begin - seg->end - 1;
      if (len<hilenmin)                            /* short hient segment */
        {
         seg->end = nextseg->end;
         seg->next = nextseg->next;
         sfree(nextseg);
         nextseg = seg->next;
         continue;
        }
      seg = nextseg;
      nextseg = seg->next;
     }

   len = seq->length - seg->end - 1;
   if (len<hilenmin) seg->end = seq->length - 1;

   return;
}

static Int2 SegsToBlastSeqLoc(Seg* segs, Int4 offset, BlastSeqLoc** seg_locs)
{
   DoubleInt* dip;
   BlastSeqLoc* last_slp = NULL,* head_slp = NULL;

   for ( ; segs; segs = segs->next) {
      dip = (DoubleInt*) calloc(1, sizeof(DoubleInt));
      dip->i1 = segs->begin + offset;
      dip->i2 = segs->end + offset;

      if (!last_slp) {
         last_slp = ListNodeAddPointer(&head_slp, 0, dip);
      } else {
         last_slp = ListNodeAddPointer(&last_slp, 0, dip);
      }
   }

   *seg_locs = head_slp;
   return 0;
}

/*------------------------------------------------------------(AA20alpha)---*/

static Alpha* AA20alpha_std (void)
{
   Alpha* palpha;
   Int4* alphaindex;
   unsigned char* alphaflag;
   char* alphachar;
   Uint1 c, i;

   palpha = (Alpha*) calloc(1, sizeof(Alpha));

   palpha->alphabet = AA20;
   palpha->alphasize = 20;
   palpha->lnalphasize = LN20;

   alphaindex = (Int4*) calloc(CHAR_SET , sizeof(Int4));
   alphaflag = (unsigned char*) calloc(CHAR_SET , sizeof(char));
   alphachar = (char*) calloc(palpha->alphasize , sizeof(char));

   for (c=0, i=0; c<128; c++)
     {
        if (c == 1 || (c >= 3 && c <= 20) || c == 22) {
           alphaflag[c] = FALSE; 
           alphaindex[c] = i; 
           alphachar[i] = c;
           ++i;
        } else {
           alphaflag[c] = TRUE; alphaindex[c] = 20;
        }
     }

   palpha->alphaindex = alphaindex;
   palpha->alphaflag = alphaflag;
   palpha->alphachar = alphachar;

   return (palpha);
}

/*-------------------------------------------------------(SegParametersNewAa)---*/

SegParameters* SegParametersNewAa (void)
{
   SegParameters* sparamsp;

   sparamsp = (SegParameters*) calloc(1, sizeof(SegParameters));

   sparamsp->window = 12;
   sparamsp->locut = 2.2;
   sparamsp->hicut = 2.5;
   sparamsp->period = 1;
   sparamsp->hilenmin = 0;
   sparamsp->overlaps = FALSE;
   sparamsp->maxtrim = 50;
   sparamsp->maxbogus = 2;
   sparamsp->palpha = AA20alpha_std();

   return (sparamsp);
}

/*-------------------------------------------------------(SegParametersCheck)---*/

static void SegParametersCheck (SegParameters* sparamsp)
{
   if (!sparamsp) return;

   if (sparamsp->window <= 0) sparamsp->window = 12;

   if (sparamsp->locut < 0.0) sparamsp->locut = 0.0;
/* if (sparamsp->locut > sparamsp->palpha->lnalphasize)
       sparamsp->locut = sparamsp->palpha->lnalphasize; */

   if (sparamsp->hicut < 0.0) sparamsp->hicut = 0.0;
/* if (sparamsp->hicut > sparamsp->palpha->lnalphasize)
       sparamsp->hicut = sparamsp->palpha->lnalphasize; */

   if (sparamsp->locut > sparamsp->hicut)
       sparamsp->hicut = sparamsp->locut;

   if (sparamsp->maxbogus < 0)
       sparamsp->maxbogus = 0;
   if (sparamsp->maxbogus > sparamsp->window)
       sparamsp->maxbogus = sparamsp->window;

   if (sparamsp->period <= 0) sparamsp->period = 1;
   if (sparamsp->maxtrim < 0) sparamsp->maxtrim = 0;

   return;
}

/*--------------------------------------------------------(SegParametersFree)---*/

void SegParametersFree(SegParameters* sparamsp)
{
   if (!sparamsp) return;
   AlphaFree(sparamsp->palpha);
   sfree(sparamsp);
   return;
}

/*------------------------------------------------------------(AlphaCopy)---*/

static Alpha* AlphaCopy (Alpha* palpha)
{
   Alpha* pbeta;
   Int2 i;

   if (!palpha) return((Alpha*) NULL);

   pbeta = (Alpha*) calloc(1, sizeof(Alpha));
   pbeta->alphabet = palpha->alphabet;
   pbeta->alphasize = palpha->alphasize;
   pbeta->lnalphasize = palpha->lnalphasize;

   pbeta->alphaindex = (Int4*) calloc(CHAR_SET , sizeof(Int4));
   pbeta->alphaflag = (unsigned char*) calloc(CHAR_SET , sizeof(char));
   pbeta->alphachar = (char*) calloc(palpha->alphasize , sizeof(char));

   for (i=0; i<CHAR_SET; i++)
     {
      pbeta->alphaindex[i] = palpha->alphaindex[i];
      pbeta->alphaflag[i] = palpha->alphaflag[i];
     }

   for (i=0; i<palpha->alphasize; i++)
     {
      pbeta->alphachar[i] = palpha->alphachar[i];
     }

   return(pbeta);
}

Int2 SeqBufferSeg (Uint1* sequence, Int4 length, Int4 offset,
                     SegParameters* sparamsp, BlastSeqLoc** seg_locs)
{
   Sequence* seqwin;
   Seg* segs;
   Boolean params_allocated = FALSE;
   
   SegParametersCheck (sparamsp);
   
   /* check seg parameters */
   
   if (!sparamsp) {
      params_allocated = TRUE;
      sparamsp = SegParametersNewAa();
      SegParametersCheck (sparamsp);
      if (!sparamsp) {
#ifdef ERR_POST_EX_DEFINED
         ErrPostEx (SEV_WARNING, 0, 0, "null parameters object");
         ErrShow();
#endif
         return -1;
      }
   }
   
   /* make an old-style genwin sequence window object */
    
   seqwin = SeqNew();
   seqwin->seq = (char*) sequence;
   seqwin->length = length;
   seqwin->palpha = AlphaCopy(sparamsp->palpha);
   
   /* seg the sequence */
   
   segs = (Seg*) NULL;
   SegSeq (seqwin, sparamsp, &segs, 0);

   /* merge the segment if desired. */
   if (sparamsp->overlaps)
      mergesegs(seqwin, segs, sparamsp->overlaps);

   /* convert segs to seqlocs */
   SegsToBlastSeqLoc(segs, offset, seg_locs);   
   
   /* clean up & return */
   seqwin->seq = NULL;
   SeqFree (seqwin);
   SegFree (segs);

   if(params_allocated)
       SegParametersFree(sparamsp);
   
   return 0;
}


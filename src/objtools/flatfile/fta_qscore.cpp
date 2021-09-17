/* fta_qscore.c
 *
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
 * File Name:  fta_qscore.c
 *
 * Author: Mark Cavanaugh
 *
 * File Description:
 * -----------------
 *      Utilities to parse quality score buffer to single or
 * delta SeqGraph.
 *
 */
#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/seqres/Seq_graph.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqres/Byte_graph.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Annot_descr.hpp>

#include "index.h"

#include <objtools/flatfile/flatdefn.h>
#include <algorithm>


#include "ftaerr.hpp"
#include "utilfun.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "fta_qscore.cpp"

BEGIN_NCBI_SCOPE

/* Defines
 */

#define IS_DIGIT_OR_NA(c)     ( (c)=='N' || (c)=='A' || ('0'<=(c) && (c)<='9') )

#define QSBUF_MAXLINE      256          /* Maximum length for a line of data
                                           read from a buffer of quality-score
                                           data, plus one for the terminal
                                           newline character. */
#define QSBUF_MAXACC       51           /* Maximum length for an accession
                                           read from the 'defline' at the
                                           start of a buffer of quality-score
                                           data, plus one for \0 */
#define QSBUF_MAXTITLE     101          /* Maximum length for the title read
                                           from the 'defline' at the start of
                                           a buffer of quality-score data,
                                           plus one for \0 */
#define QSBUF_MAXSCORES    100          /* Maximum number of scores expected
                                           in a line of score data read from
                                           from a quality-score buffer */
#define QS_MIN_VALID_SCORE 0            /* Minimum valid quality score value */
#define QS_MAX_VALID_SCORE 100          /* Maximum valid quality score value */

/***********************************************************
 *
 *  Function:    QSbuf_ReadLine
 *
 *  Description: Read a line of data from a Quality Score buffer,
 *               copying its contents (up to \n or \0) into dest_buf.
 *
 *  Arguments:   qs_buf:   buffer containing Quality Score data
 *               dest_buf: destination buffer for one line of data
 *                         copied from qs_buf
 *               max_len:  max length for a line copied into dest_buf
 *               line:     line number counter (Int4Ptr)
 *
 *  Returns:     TRUE upon success, otherwise FALSE
 *
 ***********************************************************/
static bool QSbuf_ReadLine(char* qs_buf, char* dest_buf, Int2 max_len, int* line)
{
    Int4 i;

    if(qs_buf == NULL || dest_buf == NULL)
        return false;

    for(i = 1; i <= max_len; i++, dest_buf++, qs_buf++)
    {
        *dest_buf = *qs_buf;
        if(*qs_buf == '\n' || *qs_buf == '\0')
            break;
    }
    (*line)++;

    if(i == max_len)
    {
        /* you read qs_buf all the way to max_len
         * unless the last character is \n or \0, there
         * is data remaining that did not fit into max_len
         * characters; max_len is not sufficient to read
         * a line of data; this is an error
         */
        if(*dest_buf != '\n' && *dest_buf != '\0')
        {
            /* error : max_len too short for reading the lines contained
             * in qs_buf
             */
            return false;
        }
    }

    /* you did *not* read all the way to max_len characters
     * (or the line you read fits exactly into max_len characters)
     *
     * if dest_buf ends with \n, convert it to \0
     *
     * if dest_buf does NOT end with \n, then qs_buf ended on
     * a \0, without a newline; this is considered an error
     */
    if(*dest_buf != '\n')
    {
        /* error : missing newline at end of qs_buf
         */
        return false;
    }
    *dest_buf = '\0';
    return true;
}

/***********************************************************
 *
 *  Function:    QSbuf_ParseDefline
 *
 *  Description: Parse a FASTA-like "defline" read from a buffer
 *               of quality score data.
 *
 *  Sample:      >AL031704.13 Phrap Quality (Length:40617, Min: 1, Max: 99)
 *
 *  Arguments:   qs_defline: buffer containing the Quality Score
 *                           header line; must be NULL-terminated
 *               def_acc:    buffer for the accession number parsed
 *                           from the defline; must be allocated by
 *                           the caller, and big enough to hold an
 *                           accession
 *               def_ver:    buffer for the sequence version number
 *                           parsed from the defline; must be allocated
 *                           by the caller, and big enough to hold
 *                           a version number (six digits? 7?)
 *               def_title:  buffer for the 'title' of the quality
 *                           score data parsed from the defline;
 *                           must be allocated by the caller, and big
 *                           enough to hold a typical title length
 *               def_len:    Int4Ptr for the sequence length parsed
 *                           from the defline
 *               def_max:    unsigned char* for the max score parsed from
 *                           the defline
 *               def_min:    unsigned char* for the min score parsed from
 *                           the defline
 *
 *  Note:        Parsing of the length, max, and min values
 *               is a little more relaxed than the other fields,
 *               because the values that result can be compared
 *               to values from the rest of the quality score data.
 *
 *  Returns:     1  : defline successfully parsed
 *               0  : bad args to this function; defline not parsed
 *               <0 : defline cannot be parsed due to data error
 *
 ***********************************************************/
static int QSbuf_ParseDefline(char* qs_defline, char* def_acc,
                              char* def_ver, char* def_title,
                              unsigned int* def_len, unsigned char* def_max,
                              unsigned char* def_min)
{
    char* p;
    char* q;
    char* r;
    Int4    temp;                       /* used for checking the defline min
                                           and max scores;
                                           could exceed bounds of a Uint1
                                           through a data error, hence the
                                           temp var */

    if(def_acc == NULL || def_ver == NULL || def_title == NULL ||
       def_len == NULL || def_max == NULL || def_min == NULL)
        return(0);

    if(qs_defline == NULL || *qs_defline == '\0')
        return(-1);

    /* init the numeric values that will be parsed from the defline
     */
    *def_len = 0;
    *def_max = 0;
    *def_min = 0;

    /* skip leading whitespace
     */
    for(q = qs_defline; isspace(*q);)
        q++;

    if(*q == '\0')
        return(-2);

    /* should be an initial >
     */
    if(*q != '>')
        return(-3);
    q++;

    p = q;

    /* first token to be read is the accession number
     */
    while(isalnum(*q))
        q++;

    if(*q == '\0')
        return(-4);
    if(*q != '.' && !isspace(*q))
        return(-5);
    *q++ = '\0';
    StringCpy(def_acc, p);

    p = q;
    if(*q == '\0')
        return(-6);

    /* accession may be optionally followed by a version number
     */
    if(isdigit(*q))
    {
        while(isdigit(*q))
            q++;

        if(*q == '\0')
            return(-7);
        if(!isspace(*q))
            return(-8);
        *q++ = '\0';
        StringCpy(def_ver, p);

        p = q;
        if(*q == '\0')
            return(-9);
    }

    /* Ignore additional whitespace chars that might follow acc/ver
     */
    while(isspace(*q))
    {
        p++;
        q++;
    }
    if(*q == '\0')
        return(-10);

    /* alphanumeric and whitespace characters that follow are the
     * "title" of the collection of quality score data
     */
    while(isalnum(*q) || isspace(*q))
        q++;

    if(*q == '\0')
        return(-11);
    if(*q != '(')
        return(-12);

    /* trim terminal whitespace characters from the title
     */
    r = q;
    r--;
    while(isspace(*r))
        r--;
    *++r = '\0';

    if(StringHasNoText(p))
        return(-13);
    *q++ = '\0';
    StringCpy(def_title, p);

    if(NStr::CompareNocase(def_title, "Phrap Quality") != 0 &&
        NStr::CompareNocase(def_title, "Gap4") != 0 &&
        NStr::CompareNocase(def_title, "Phred Quality") != 0)
    {
        ErrPostEx(SEV_ERROR, ERR_QSCORE_BadTitle,
                  "Unrecognized title for quality score data : >%s< : should be 'Phrap Quality', 'Gap4', or 'Phred Quality'.",
                  def_title);
        return(-35);
    }

    p = q;
    if(*q == '\0')
        return(-14);

    /* Look for the 'Length:' token and skip past it
     */
    if(StringNICmp(q, "Length:", 7) != 0)
        return(-15);

    q = StringChr(q, ':');
    q++;
    p = q;
    if(*q == '\0')
        return(-16);

    /* Ignore additional whitespace chars that might follow 'Length:' token
     */
    while(isspace(*q))
    {
        p++;
        q++;
    }
    if(*q == '\0')
        return(-17);

    /* get the length value
     */
    while(isdigit(*q))
        q++;

    if(*q == '\0')
        return(-18);
    *q++ = '\0';

    sscanf(p, "%ld", (long *) &temp);
    *def_len = (Uint4) temp;
    p = q;
    if(*q == '\0')
        return(-19);

    /* Ignore additional whitespace chars that might follow length
     */
    while(isspace(*q))
    {
        p++;
        q++;
    }
    if(*q == '\0')
        return(-20);

    /* Look for the 'Min:' token and skip past it
     */
    if(StringNICmp(q, "Min:", 4) != 0)
        return(-21);

    q = StringChr(q, ':');
    q++;
    p = q;
    if(*q == '\0')
        return(-22);

    /* Ignore additional whitespace chars that might follow 'Min:' token
     */
    while(isspace(*q))
    {
        p++;
        q++;
    }
    if(*q == '\0')
        return(-23);

    /* get the minumum score value
     */
    while(isdigit(*q))
        q++;

    if(*q == '\0')
        return(-24);
    *q++ = '\0';

    sscanf(p, "%ld", (long *) &temp);
    if(temp < QS_MIN_VALID_SCORE)
        return(-25);
    if(temp > QS_MAX_VALID_SCORE)
        return(-26);

    *def_min = (Uint1) temp;

    p = q;
    if(*q == '\0')
        return(-27);

    /* Ignore additional whitespace chars that might follow minimum score
     */
    while(isspace(*q))
    {
        p++;
        q++;
    }
    if(*q == '\0')
        return(-28);

    /* Look for the 'Max:' token and skip past it
     */
    if(StringNICmp(q, "Max:", 4) != 0)
        return(-29);

    q = StringChr(q, ':');
    q++;
    p = q;
    if(*q == '\0')
        return(-30);

    /* Ignore additional whitespace chars that might follow 'Max:' token
     */
    while(isspace(*q))
    {
        p++;
        q++;
    }
    if(*q == '\0')
        return(-31);

    /* get the maximum score value
     */
    while(isdigit(*q))
        q++;

    if(*q == '\0')
        return(-32);
    *q++ = '\0';

    sscanf(p, "%ld", (long *) &temp);
    if(temp < QS_MIN_VALID_SCORE)
        return(-33);
    if(temp > QS_MAX_VALID_SCORE)
        return(-34);

    *def_max = (Uint1) temp;

    return(1);
}

/*****************************************************************************
*
*  Function:	QSbuf_ParseScores
*
*  Description:	Parse a line of data from a Quality Score buffer that supposedly
*	        contains a series of integer scores separated by whitespace.
*		Populate an array of Uint1 with their values. This is a destructive
*		parse in the sense that \0 are inserted into score_buf between the
*		integer tokens.
*
*  Arguments:	score_buf: buffer containing integer-value Quality Scores; must be
*		  null-terminated
*		scores: array of Uint1 to hold the scores parsed from score_buf
*		  (pointer to first element of a Uint1 array alloc'd by the caller)
*		max_toks: maximum number of score tokens that are expected in score_buf
*		  should equal or exceed the number of elements in the scores array
*		max_score: maximum score value encountered in score_buf (actually,
*		  the max that is encountered from multiple calls to QSbuf_ParseScores).
*		  caller should initialize to 0
*		min_score: minimum score value encountered in score_buf (actually,
*		  the min that is encountered from multiple calls to QSbuf_ParseScores).
*		  caller should initialize to 255
*		allow_na: when set to true, allow values of 'NA' in score_buf in
*		  addition to integers, and interpret them as scores of zero
*
*  Returns:	the number of scores that were written to the scores array;
*		zero is returned for empty score_buf, or a score_buf that
*		contains no scores; a negative value indicates that there was a
*		problem parsing the data in score_buf
*
*****************************************************************************/
static Int4 QSbuf_ParseScores(char* score_buf, unsigned char* scores,
			      Int4 max_toks, unsigned char* max_score,
			      unsigned char* min_score, bool allow_na)
{
  Char ch;
  char *p, *q;
  int val;
  Int4 num_toks = 0;

  /* empty buffer, nothing to parse */

  if (score_buf == NULL || *score_buf == '\0')
    return 0;

  /* bad arguments */

  if (scores == NULL || max_toks < 1)
    return -1;

  /* Loop through score_buf a character (ch) at a time, until you reach a NULL.

     Skip whitespace characters, and save your current position. Then skip
     digit characters. Insert a NULL at the first non-digit, and you've got a token
     representing an integer score (from the saved position). Increment
     beyond the non-digit, set ch to the resulting character, and then
     try for another token.

     BUT! DDBJ data can contain non-digit tokens consisting of NA instead
     of zero for the score values that fall in the gaps between contigs.
     So use function IS_DIGIT_OR_NA() rather than IS_DIGIT(), then check
     the returned token to see if it is "NA". If so, treat it as zero.
     
  */

  p = score_buf;
  ch = *p;

  while (ch != '\0') {
    while (isspace(ch)) {
      p++;
      ch = *p;
    }
    /* score_buf might be nothing but whitespace, or might end with whitespace */
    if (ch == '\0') {
      break;
    }

    q = p;
    ch = *q;
    while (IS_DIGIT_OR_NA (ch)) {
      q++;
      ch = *q;
    }

    /* if not at buffer end, then check to see if current
       character is whitespace; if not, then there is data
       in the buffer other then digits and whitespace; data error */

    if (ch != '\0') {
      if (!isspace(ch)) {
	fprintf(stderr,"error: score_buf contains an illegal character : >%c<\n", ch);
	return -2;
      }
      *q = '\0';
      q++;
    }

    if (*p == '\0') {
      /* fprintf(stderr,"error: score_buf buffer contains no score values\n"); */
      return -3;
    }
    if (max_toks < ++num_toks) {
      /* fprintf(stderr,"error: score_buf contains more than >%ld< scores : problem at token >%s<\n", max_toks, p); */
      return -4;
    }

    /*
    fprintf(stdout,"score token is >%s<\n", p);
    fflush(stdout);
    */

    if (allow_na && StringCmp(p,"NA") == 0) {
      *scores = 0;
      scores++;
    }
    else {
      if (sscanf (p, "%d", &val) == 1) {

	/* fprintf(stdout,"integer value for score token is %d\n",val); */

	if (val < QS_MIN_VALID_SCORE) {
	  /* fprintf(stderr,"error: score_buf score >%d< is less than the minimum legal value >%d<\n", val, QS_MIN_VALID_SCORE); */
	  return -5;
	}
	else if (val > QS_MAX_VALID_SCORE) {
	  /* fprintf(stderr,"error: score_buf score >%d< is more than the maximum legal value >%d<\n", val, QS_MAX_VALID_SCORE); */
	  return -6;
	}
	*scores = (Uint1) val;
	scores++;

	*max_score = max(*max_score, (Uint1) val);
	*min_score = min(*min_score, (Uint1) val);
      }
      else {
	/* fprintf(stderr,"error: sscan failure : score_buf score >%s< is probably not numeric\n", p); */
	return -7;
      }
    }
    p = q;
    ch = *p;
  }
  return num_toks;
}

/***********************************************************
 *
 *  Function:    Split_Qscore_SeqGraph_By_DeltaSeq
 *
 *  Description: Take a single monolithic Seq-graph of quality
 *               scores and split it into a series of smaller
 *               Seq-graphs, each graph corresponding to one
 *               of the Delta-seq literals of the Bioseq to
 *               which the scores apply.
 *
 *  Arguments:   big_sgp: SeqGraphPtr for a single Seq-graph,
 *                        containing basepair quality scores
 *                        for every base of a sequence, including
 *                        any gaps that might exist between its
 *                        component contigs (scores at the gaps
 *                        are presumably zero)
 *               bsp:     BioseqPtr for the sequence to which
 *                        the qscores apply; the bioseq must be
 *                        a Delta-seq composed of a series of
 *                        Seq-literals, one for each component
 *                        contig of the bioseq
 *
 *  Notes:       This function cannot handle Delta-seq bioseqs
 *               that contain Seq-loc components (as opposed to
 *               Seq-literal).
 *
 *               This function cannot handle Seq-literals with
 *               a length of zero (presumably representing a gap
 *               of unknown size).
 *
 *  Returns:     pointer to a chain of SeqGraph, created by
 *               splitting big_sgp up, based on the bsp Delta-seg;
 *               otherwise NULL
 *
 *  Warning:     This function cannot handle sequences more
 *               than an Int4 in length.
 *
 ***********************************************************/

static void Split_Qscore_SeqGraph_By_DeltaSeq(objects::CSeq_annot::C_Data::TGraph& graphs,
                                              objects::CBioseq& bioseq)
{
    bool         is_gap = false;        /* set to TRUE if the Seq-literal
                                           represents a gap, rather than
                                           sequence data */
    bool         problem = false;       /* set to TRUE if a problem is
                                           encountered while processing
                                           a new_sgp */
    Uint1        max_score = 0;         /* maximum quality score encountered
                                           for a Delta-seq component */
    Uint1        min_score = 0;         /* minimum quality score encountered
                                           for a Delta-seq component */
    Uint4        last_pos = 1;          /* previous position along bsp */
    Uint4        curr_pos = 1;          /* current position along bsp */
    Int2         nonzero_gap = 0;       /* counter of the number of non-zero
                                           scores encountered in big_bs for a
                                           Delta-seq component that represents
                                           a gap; scores *should* be zero
                                           within gaps */

    if (bioseq.GetInst().GetRepr() != objects::CSeq_inst::eRepr_delta ||
        !bioseq.GetInst().IsSetExt())
        return;

    objects::CSeq_graph& big_graph = *(*graphs.begin());
    if (!big_graph.GetGraph().IsByte())
    {
        ErrPostEx(SEV_ERROR, ERR_QSCORE_NonByteGraph,
                  "Seq-graph to be split does not contain byte qscore values : cannot be processed.");
        return;
    }

    std::vector<Char> scores_str(big_graph.GetGraph().GetByte().GetValues().begin(),
                                 big_graph.GetGraph().GetByte().GetValues().end());
    if (scores_str.empty())
    {
        ErrPostEx(SEV_ERROR, ERR_QSCORE_MissingByteStore,
                  "Seq-graph to be split has a NULL ByteStore for the qscore values : cannot be processed.");
        return;
    }

    std::string def_title;
    if (big_graph.IsSetTitle())
        def_title = big_graph.GetTitle();

    nonzero_gap = 0;
    curr_pos = 0;

    objects::CSeq_annot::C_Data::TGraph new_graphs;
    ITERATE(objects::CDelta_ext::Tdata, delta, bioseq.GetInst().GetExt().GetDelta().Get())
    {
        is_gap = false;
        last_pos = curr_pos;
        max_score = QS_MIN_VALID_SCORE;
        min_score = QS_MAX_VALID_SCORE;

        if ((*delta)->IsLoc())
        {
            ErrPostEx(SEV_ERROR, ERR_QSCORE_NonLiteralDelta,
                      "Cannot process Delta-seq bioseqs with Seq-loc components.");
            problem = true;
            break;
        }

        if (!(*delta)->IsLiteral())
        {
            ErrPostEx(SEV_ERROR, ERR_QSCORE_UnknownDelta,
                      "Encountered Delta-seq component of unknown type.");
            problem = true;
            break;
        }

        const objects::CSeq_literal& literal = (*delta)->GetLiteral();

        if (!literal.IsSetLength() || literal.GetLength() < 1)
        {
            ErrPostEx(SEV_ERROR, ERR_QSCORE_ZeroLengthLiteral,
                      "Encountered Delta-seq Seq-literal component with length of zero (or less) : cannot be processed.");
            problem = true;
            break;
        }

        std::vector<Char> new_scores;

        if (!literal.IsSetSeq_data())
        {
            /* this Seq-literal contains no data, so it presumably
             * represents a gap
             */
            is_gap = true;
        }

        /* read the scores from big_bs for this Delta-seq component.
         * remember the min and max scores. check for non-zero score
         * if the component is a gap
         */
        size_t scores_size = literal.GetLength();
        for (size_t i = 0; i < scores_size; i++)
        {
            Uint1 byte = static_cast<Uint1>(scores_str[curr_pos]);
            new_scores.push_back(scores_str[curr_pos]);

/*
            fprintf(stdout, "byte read from ByteStore is >%i<\n", *j);
            fflush(stdout);
*/

            curr_pos++;

            if(byte < min_score)
                min_score = byte;
            else if(byte > max_score)
                max_score = byte;

            if (is_gap && byte != 0)
            {
                if(nonzero_gap < 10)
                {
                    ErrPostEx(SEV_WARNING, ERR_QSCORE_NonZeroInGap,
                              "Encountered non-zero score value %i within Delta-seq gap at position %ld of bioseq",
                              byte, curr_pos);
                    nonzero_gap++;
                }
                else if(nonzero_gap == 10)
                {
                    ErrPostEx(SEV_WARNING, ERR_QSCORE_NonZeroInGap,
                              "Exceeded reporting threshold (10) for non-zero score values in Delta-seq gaps : no further messages will be generated");
                    nonzero_gap++;
                }
            }
        }

        /* don't create a Seq-graph for gaps
         */
        if(is_gap)
            continue;

        /* allocate a SeqGraph and a ByteStore
         */

        CRef<objects::CSeq_graph> graph(new objects::CSeq_graph);
        objects::CSeq_interval& interval = graph->SetLoc().SetInt();

        interval.SetId(*(*bioseq.SetId().begin()));

        /* Write the scores from big_bs to the new ByteStore
         */
        graph->SetNumval(static_cast<TSeqPos>(new_scores.size()));
        graph->SetGraph().SetByte().SetValues().swap(new_scores);

        /* there is no "compression" for the Seq-graph; there's supposed to
         * be a score for every base in the sequence to which the quality
         * score buffer applies
         */
//        graph->SetComp(1);

        /* no scaling of values
         */
//        graph->SetA(1.0);
//        graph->SetB(0);

        /* Establish the byte-graph values
         */
        graph->SetGraph().SetByte().SetMin(min_score);
        graph->SetGraph().SetByte().SetMax(max_score);
        graph->SetGraph().SetByte().SetAxis(0);

/*
        fprintf(stdout, "new_sgp max score is %i\n", max_score);
        fprintf(stdout, "new_sgp min score is %i\n", min_score);
        fflush(stdout);
*/

        if (!def_title.empty())
            graph->SetTitle(def_title);

        interval.SetFrom(last_pos);
        interval.SetTo(curr_pos - 1);

/*
        fprintf(stdout, "new_sgp from is %ld\n", last_pos);
        fprintf(stdout, "new_sgp to is %ld\n", curr_pos - 1);
        fflush(stdout);
*/
        new_graphs.push_back(graph);
    }

    if (!problem)
        graphs.swap(new_graphs);

    if (curr_pos != bioseq.GetLength())
    {
        ErrPostEx(SEV_WARNING, ERR_QSCORE_OutOfScores,
                  "Exhausted available score data in Seq-graph being split at location %ld : total length of Delta-seq bioseq is %ld .",
                  curr_pos, bioseq.GetLength());
    }
}

/***********************************************************
 *
 *  Function:    QSbuf_To_Single_Qscore_SeqGraph
 *
 *  Description: Read a char* buffer that contains basepair
 *               Quality Score data and convert it to an ASN.1
 *               Seq-Graph object. The buffer is assumed to
 *               start with a FASTA-like identifier, and be
 *               followed by lines that contain whitespace-separated
 *               integer values. For example:
 *
 * >AL031704.13 Phrap Quality (Length:40617, Min: 1, Max: 99)
 * 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99 99
 *
 *		 If the allow_na argument is true, then the buffer may also
 *		 contain tokens consisting of 'NA' (not applicable), which
 *		 are interpreted as zero. Some quality scores from DDBJ
 *		 follow this convention.
 *
 *               This function builds a **single** Seq-graph
 *               for the entire quality score buffer, regardless
 *               of whether the sequence to which it applies is
 *               comprised of contigs separated by gaps. Subsequent
 *               processing should break this monolithic Seq-graph
 *               into a series of smaller Seq-graphs, if the
 *               sequence is represented as a Delta-seq with
 *               multiple Seq-literals for the component contigs.
 *               See function Split_Qscore_SeqGraph_By_DeltaSeq() .
 *
 *  Arguments:   qs_buf:  the buffer containing Quality Score
 *                        data; must be NULL-terminated
 *               bsp:     BioseqPtr for the record to which the
 *                        data in the quality score buffer applies;
 *                        the bsp->id and bsp->length slots of
 *                        the Bioseq must be populated; if the
 *                        Bioseq is a Delta-seq, then the bsp->length
 *                        must include the length of the gaps
 *                        between the pieces of sequence data
 *               def_acc: buffer with which the accession number
 *                        read from the defline in the quality
 *                        score buffer is returned to the caller;
 *                        caller must allocate, and it must be
 *                        large enough for an accession; should
 *                        be compared to the accession of the
 *                        record that the caller is processing,
 *                        to make sure they are equal
 *               def_ver: buffer with which the sequence version
 *                        number read from defline in the quality
 *                        score buffer is returned to the caller;
 *                        caller must allocate, and it must be
 *                        large enough for largest version
 *                        (6 digits? 7?); should be compared
 *                        to the version number of the record
 *                        that the caller is processing, to make
 *                        sure they are equal
 *               check_minmax:  when set to true, min/max scores from
 *                        the defline in qs_buf will be compared
 *                        to the min/max scores in the score data;
 *                        if the values are not equal, no SeqGraphPtr
 *                        will be returned
 *               allow_na : when set to true, score values of "NA" in
 *                        qs_buf are allowed, and interpreted as zero
 *
 *  Returns:     pointer to SeqGraph upon success, otherwise NULL
 *
 ***********************************************************/
static void QSbuf_To_Single_Qscore_SeqGraph(char* qs_buf,
                                            objects::CBioseq& bioseq,
                                            char* def_acc,
                                            char* def_ver,
						                    bool check_minmax,
						                    bool allow_na,
                                            objects::CSeq_annot::C_Data::TGraph& graphs)
{
    Int4         qs_line = 0;           /* current line number within qs_buf */
    char*      my_buf = NULL;         /* copy of a line of data from
                                           qs_buf */
    int          def_stat;              /* return status from parsing of the
                                           'defline' in the quality score
                                           buffer */
    char*      def_title;             /* title parsed from the quality
                                           score defline */
    Uint4        def_len;               /* sequence length parsed from the
                                           quality score defline */
    Uint1        def_max = 0;           /* maximum quality score parsed from
                                           the quality score defline */
    Uint1        def_min = 0;           /* minimum quality score parsed from
                                           the quality score defline */
    Uint1        scores[QSBUF_MAXSCORES];       /* array of Uint1 to hold the
                                                   scores read from one line
                                                   of qs_buf data */
    Uint4        total_scores = 0;
    Uint1        max_score = QS_MIN_VALID_SCORE;        /* maximum quality
                                                           score encountered in
                                                           qs_buf score data */
    Uint1        min_score = QS_MAX_VALID_SCORE;        /* minimum quality
                                                           score encountered in
                                                           qs_buf score data */
    bool         problem = false;       /* set to TRUE for various error
                                           conditions encountered in the
                                           qs_buf data; used to free the
                                           Seq-graph and return NULL */

    if(qs_buf == NULL || *qs_buf == '\0' || def_acc == NULL ||
       def_ver == NULL)
    {
        ErrPostEx(SEV_ERROR, ERR_QSCORE_InvalidArgs,
                  "Missing arguments for QSbuf_To_Single_SeqGraph call.");
        return;
    }

    if (bioseq.GetLength() < 1)
    {
        ErrPostEx(SEV_ERROR, ERR_QSCORE_BadBioseqLen,
                  "Invalid Bioseq length : %ld", bioseq.GetLength());
        return;
    }

    if (!bioseq.IsSetId())
    {
        ErrPostEx(SEV_ERROR, ERR_QSCORE_BadBioseqId,
                  "Invalid Bioseq : no Seq-ids found.");
        return;
    }

    /* allocate a buffer for reading qs_buf, one line at a time
     */
    my_buf = (char*) MemNew(QSBUF_MAXLINE);
    if(my_buf == NULL)
    {
        ErrPostEx(SEV_ERROR, ERR_QSCORE_MemAlloc,
                  "MemNew failure for my_buf buffer");
        return;
    }

    /* allocate a buffer for the 'title' read from the defline in qs_buf
     */
    def_title = (char*) MemNew(QSBUF_MAXTITLE);
    if(def_title == NULL)
    {
        ErrPostEx(SEV_ERROR, ERR_QSCORE_MemAlloc,
                  "MemNew failure for def_title buffer");
        return;
    }

    CRef<objects::CSeq_graph> graph;
    std::vector<Char> scores_str;

    while (*qs_buf != '\0')
    {
        if(!QSbuf_ReadLine(qs_buf, my_buf, QSBUF_MAXLINE, &qs_line))
        {
            ErrPostEx(SEV_ERROR, ERR_QSCORE_BadQscoreRead,
                      "QSbuf_ReadLine failure near line %ld of qscore buffer.",
                      qs_line);
            return;
        }

/*
        fprintf(stdout, "line from qs_buf is:\n>%s<\n", my_buf);
        fflush(stdout);
*/

        /* \n has been replaced by \0 in the line returned by QSbuf_ReadLine
         * we want to increment qs_buf to point to the character beyond the \0
         *
         * it's safe to do this only if QSbuf_ReadLine() returns true,
         * which will happen only when the line from qs_buf ends with \n
         * or \n\0
         */
        qs_buf += StringLen(my_buf) + 1;

        if(qs_line == 1)
        {
            /* first line is supposed to be a 'defline' for the quality
             * score data
             */
            if(*my_buf != '>')
            {
                ErrPostEx(SEV_ERROR, ERR_QSCORE_BadDefline,
                          "qscore buffer does not start with required > character.");
                return;
            }

            def_stat = QSbuf_ParseDefline(my_buf, def_acc, def_ver, def_title,
                                          &def_len, &def_max, &def_min);
            if(def_stat != 1)
            {
                ErrPostEx(SEV_ERROR, ERR_QSCORE_BadDefline,
                          "QSbuf_ParseDefline failure : return value is >%d< : probable defline data/format error : defline is >%s<\n",
                          def_stat, my_buf);
                return;
            }

            if(def_acc != NULL && *def_acc == '\0')
            {
                ErrPostEx(SEV_ERROR, ERR_QSCORE_NoAccession,
                          "Could not parse accession from qscore defline : defline is >%s<\n",
                          my_buf);
                return;
            }
            if(def_ver != NULL && *def_ver == '\0')
            {
                ErrPostEx(SEV_ERROR, ERR_QSCORE_NoSeqVer,
                          "Could not parse sequence version number from qscore defline : defline is >%s<\n",
                          my_buf);
                return;
            }
            if(def_title != NULL && *def_title == '\0')
            {
                ErrPostEx(SEV_ERROR, ERR_QSCORE_NoTitle,
                          "Could not parse title from qscore defline : defline is >%s<\n",
                          my_buf);
                return;
            }
            if (def_len != bioseq.GetLength())
            {
                ErrPostEx(SEV_ERROR, ERR_QSCORE_BadLength,
                          "Sequence length from qscore defline does not match bioseq length : %ld (defline) vs %ld (bioseq)",
                          def_len, bioseq.GetLength());
                return;
            }
            if(def_max < def_min || def_min > def_max)
            {
                ErrPostEx(SEV_ERROR, ERR_QSCORE_BadMinMax,
                          "Maximum and minimum scores from qscore defline are contradictory : %ld (max) vs %ld (min)",
                          def_max, def_min);
                return;
            }

            /* allocate a SeqGraph and a ByteStore
             */
            graph.Reset(new objects::CSeq_graph);
            graph->SetTitle(def_title);
        }
        else
        {
            /* a small number of EMBL records have qscore data that
             * is terminated with a double slash; if encountered,
             * generate a warning message and then exit the while loop.
             */

            if(StringCmp(my_buf, "//") == 0)
            {
/*                ErrPostEx(SEV_WARNING, ERR_QSCORE_DoubleSlash,
                          "Encountered unusual double-slash terminator in qscore buffer : assuming it flags the end of qscore data.");*/
                break;
            }

            /* otherwise, this must be a line of quality score data
             */
            int qs_scores = QSbuf_ParseScores(my_buf, &scores[0], QSBUF_MAXSCORES,
                                          &max_score, &min_score, allow_na);
            if(qs_scores < 0)
            {
                ErrPostEx(SEV_ERROR, ERR_QSCORE_BadScoreLine,
                          "QSbuf_ParseScores failure : return value is >%ld< : probable score data/format error : score data near >%s<\n",
                          qs_scores, my_buf);
                return;
            }

            /* write the scores to the ByteStore
             */
            std::copy(scores, scores + qs_scores, std::back_inserter(scores_str));
            total_scores += qs_scores;
        }
    }

    if (graph.Empty())
        return;

    if(total_scores != def_len)
    {
        ErrPostEx(SEV_ERROR, ERR_QSCORE_ScoresVsLen,
                  "number of scores read from qscore buffer does not equal defline sequence length : %ld (scores) vs %ld (defline)",
                  total_scores, def_len);
        problem = true;
    }
    if (total_scores != bioseq.GetLength())
    {
        ErrPostEx(SEV_ERROR, ERR_QSCORE_ScoresVsBspLen,
                  "number of scores read from qscore buffer does not equal supplied bioseq length : %ld (scores) vs %ld (bioseq)",
                  total_scores, bioseq.GetLength());
        problem = true;
    }
    if(check_minmax)
    {
        if(def_max != max_score)
        {
            ErrPostEx(SEV_ERROR, ERR_QSCORE_BadMax,
                      "maximum score from qscore defline does not equal maximum score value : %ld (defline) vs %ld (scores)",
                      def_max, max_score);
            problem = true;
        }
        if(def_min != min_score)
        {
            ErrPostEx(SEV_ERROR, ERR_QSCORE_BadMin,
                      "minimum score from qscore defline does not equal minimum score value : %ld (defline) vs %ld (scores)",
                      def_min, min_score);
            problem = true;
        }
    }

    /* if a problem has been encountered, free the SeqGraph and return NULL
    */
    if (problem)
    {
        MemFree(my_buf);
        MemFree(def_title);
        return;
    }

    /* get a Seq-interval for the SeqGraph, and duplicate the Seq-id
     * of the Bioseq for use in the Seq-interval
     */
    objects::CSeq_loc& loc = graph->SetLoc();

    /* otherwise, you can now put all the pieces of the Seq-graph together
     */
    graph->SetNumval(static_cast<TSeqPos>(scores_str.size()));

    /* there is no "compression" for the Seq-graph; there's supposed to
     * be a score for every base in the sequence to which the quality
     * score buffer applies
     */

    /* no scaling of values
     */

    /* Seq-graph type is "byte"
     */
    graph->SetGraph().SetByte().SetValues().swap(scores_str);

    /* Establish the byte-graph values
     */
    graph->SetGraph().SetByte().SetMin(min_score);
    graph->SetGraph().SetByte().SetMax(max_score);
    graph->SetGraph().SetByte().SetAxis(0);


    /* feature location for the Seq-graph runs from 0
     * to the sequence length - 1
     */
    objects::CSeq_interval& interval = loc.SetInt();
    interval.SetFrom(0);
    interval.SetTo(bioseq.GetLength() - 1);

    loc.SetId(*(bioseq.GetId().front()));

    MemFree(my_buf);
    MemFree(def_title);

    graphs.push_back(graph);
}

/**********************************************************/
// TODO: functionality in this file was never tested
bool QscoreToSeqAnnot(char* qscore, objects::CBioseq& bioseq, char* acc,
                      Int2 ver, bool check_minmax, bool allow_na)
{
    Char        charver[100];

    if (qscore == NULL || ver < 1)
        return true;

    sprintf(charver, "%d", (int) ver);

    objects::CSeq_annot::C_Data::TGraph graphs;
    QSbuf_To_Single_Qscore_SeqGraph(qscore, bioseq, acc, charver, check_minmax, allow_na, graphs);
    if (graphs.empty())
        return false;

    if (bioseq.GetInst().GetRepr() == objects::CSeq_inst::eRepr_delta)
    {
        Split_Qscore_SeqGraph_By_DeltaSeq(graphs, bioseq);
    }

    if (graphs.empty())
        return false;

    CRef<objects::CSeq_annot> annot(new objects::CSeq_annot);
    annot->SetData().SetGraph().swap(graphs);
    annot->SetNameDesc("Graphs");

    bioseq.SetAnnot().push_front(annot);

    return true;
}

END_NCBI_SCOPE

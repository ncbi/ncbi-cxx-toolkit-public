/*
 * $Id$
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
 * Authors:  Colleen Bollin
 *
 */

#include <util/creaders/alnread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _MSC_VER
#define strdup _strdup
#endif

static const int kMaxPrintedIntLen = 10;
#define MAX_PRINTED_INT_LEN_PLUS_ONE 11

/*  ---------------------------------------------------------------------- */
typedef enum {
    ALNFMT_UNKNOWN,
    ALNFMT_NEXUS,
    ALNFMT_PHYLIP,
    ALNFMT_CLUSTAL,
    ALNFMT_FASTAGAP,
} EAlignFormat;

/*  ---------------------------------------------------------------------- */

/* structures used internally */
typedef struct SLineInfo {
    char *             data;
    int                line_num;
    int                line_offset;
    EBool              delete_me;
    struct SLineInfo * next;
} SLineInfo, * TLineInfoPtr;

typedef struct SLineInfoReader {
    TLineInfoPtr first_line;
    TLineInfoPtr curr_line;
    char *       curr_line_pos;
    int          data_pos;
} SLineInfoReader, * TLineInfoReaderPtr;

typedef struct SIntLink {
    int               ival;
    struct SIntLink * next;
} SIntLink, * TIntLinkPtr;

typedef struct SStringCount {
    char *                string;
    int                   num_appearances;
    TIntLinkPtr           line_numbers;
    struct SStringCount * next;
} SStringCount, * TStringCountPtr;

typedef struct SSizeInfo {
    int                size_value;
    int                num_appearances;
    struct SSizeInfo * next;
} SSizeInfo, * TSizeInfoPtr;

typedef struct SLengthList {
    TSizeInfoPtr         lengthrepeats;
    int                  num_appearances;
    struct SLengthList * next;
} SLengthListData, * SLengthListPtr;
 
typedef struct SCommentLoc {
  char *               start;
  char *               end;
  struct SCommentLoc * next;
} SCommentLoc, * TCommentLocPtr;

typedef struct SBracketedCommentList 
{
	TLineInfoPtr                   comment_lines;
	struct SBracketedCommentList * next;
} SBracketedCommentList, * TBracketedCommentListPtr;

typedef struct SAlignRawSeq {
    char *                id;
    TLineInfoPtr          sequence_data;
    TIntLinkPtr           id_lines;
    struct SAlignRawSeq * next;
} SAlignRawSeq, * TAlignRawSeqPtr;

typedef struct SAlignFileRaw {
    TLineInfoPtr         line_list;
    TLineInfoPtr         organisms;
    TAlignRawSeqPtr      sequences;
    int                  num_organisms;
    TLineInfoPtr         deflines;
    int                  num_deflines;
    EBool                marked_ids;
    int                  block_size;
    TIntLinkPtr          offset_list;
    FReportErrorFunction report_error;
    void *               report_error_userdata;
    char *               alphabet;
    int                  expected_num_sequence;
    int                  expected_sequence_len;
    int                  num_segments;
    char                 align_format_found;
} SAlignRawFileData, * SAlignRawFilePtr;

/* Function declarations
 */
static EBool s_AfrpInitLineData( 
    SAlignRawFilePtr afrp, FReadLineFunction readfunc, void* pfile);
static void s_AfrpProcessFastaGap(
    SAlignRawFilePtr afrp, SLengthListPtr patterns, char* plinestr, int overall_line_count);

/* These functions are used for storing and transmitting information
 * about errors encountered while reading the alignment data.
 */

/* This function allocates memory for a new error structure and populates
 * the structure with default values.
 * The new structure will be added to the end of the linked list of error
 * structures pointed to by list.
 */
extern TErrorInfoPtr ErrorInfoNew (TErrorInfoPtr list)
{
    TErrorInfoPtr eip, last;

    eip = (TErrorInfoPtr) malloc ( sizeof (SErrorInfo));
    if (eip == NULL) {
        return NULL;
    }
    eip->category = eAlnErr_Unknown;
    eip->line_num = -1;
    eip->id       = NULL;
    eip->message  = NULL;
    eip->next     = NULL;
    last = list;
    while (last != NULL && last->next != NULL) {
        last = last->next;
    }
    if (last != NULL) {
        last->next = eip;
    }
    return eip;
}

/* This function recursively frees the memory associated with a list of
 * error structures as well as the member variables of the error structures.
 */
extern void ErrorInfoFree (TErrorInfoPtr eip)
{
    if (eip == NULL) {
        return;
    }
    ErrorInfoFree (eip->next);
    free (eip->id);
    free (eip->message);
    free (eip);
}

/* This function creates and sends an error message regarding a NEXUS comment
 * character.
 */
static void 
s_ReportCharCommentError 
(const char * expected,
 char         seen,
 const char * val_name,
 FReportErrorFunction errfunc,
 void *               errdata)
{
    TErrorInfoPtr eip;
    const char * errformat = "Specified %s character does not match NEXUS"
                             " comment in file (specified %s, comment %c)";

    if (errfunc == NULL  ||  val_name == NULL || expected == NULL) {
        return;
    }

    eip = ErrorInfoNew (NULL);
    if (eip != NULL) {
        eip->category = eAlnErr_BadFormat;
        eip->message = (char *) malloc (strlen (errformat) + strlen (val_name)
                                        + strlen (expected) + 2);
        if (eip->message != NULL) {
            sprintf (eip->message, errformat, val_name, expected, seen);
        }
        errfunc (eip, errdata);
    }
}


/* This function creates and sends an error message regarding a character
 * that is unexpected in sequence data.
 */
static void 
s_ReportBadCharError 
(char *  id,
 char    bad_char,
 int     num_bad,
 int     offset,
 int     line_number,
 const char *  reason,
 FReportErrorFunction errfunc,
 void *             errdata)
{
    TErrorInfoPtr eip;
    const char *  err_format =
                          "%d bad characters (%c) found at position %d (%s).";

    if (errfunc == NULL  ||  num_bad == 0  ||  bad_char == 0
        ||  reason == NULL) {
        return;
    }

    eip = ErrorInfoNew (NULL);
    if (eip == NULL) {
        return;
    }

    eip->category = eAlnErr_BadData;
    if (id != NULL) eip->id = strdup (id);
    eip->line_num = line_number;
    eip->message = (char *) malloc (strlen (err_format) + 2 * kMaxPrintedIntLen
                                    + strlen (reason) + 3);
    if (eip->message != NULL)
    {
        sprintf (eip->message, err_format, num_bad, bad_char, offset, reason);
    }
    errfunc (eip, errdata);
}
 

/* This function creates and sends an error message regarding an ID that
 * was found in the wrong location.
 */
static void 
s_ReportInconsistentID 
(char *               id,
 int                  line_number,
 FReportErrorFunction report_error,
 void *              report_error_userdata)
{
    TErrorInfoPtr eip;

    if (report_error == NULL) {
        return;
    }
    eip = ErrorInfoNew (NULL);
    if (eip == NULL) {
        return;
    }
    eip->category = eAlnErr_BadFormat;
    eip->id = strdup (id);
    eip->line_num = line_number;
    eip->message = strdup ("Found unexpected ID");
    report_error (eip, report_error_userdata);
}


/* This function creates and sends an error message regarding a line
 * of sequence data that was expected to have a different length.
 */
static void 
s_ReportInconsistentBlockLine 
(char *               id,
 int                  line_number,
 FReportErrorFunction report_error,
 void *              report_error_userdata)
{
    TErrorInfoPtr eip;

    if (report_error == NULL) {
        return;
    }
    eip = ErrorInfoNew (NULL);
    if (eip == NULL) {
        return;
    }
    eip->category = eAlnErr_BadFormat;
    eip->id = strdup (id);
    eip->line_num = line_number;
    eip->message = strdup ("Inconsistent block line formatting");
    report_error (eip, report_error_userdata);
}


/* This function creates and sends an error message regarding a line of
 * sequence data that was expected to be a different length.
 */
static void 
s_ReportLineLengthError 
(char *               id,
 TLineInfoPtr         lip,
 int                  expected_length,
 FReportErrorFunction report_error,
 void *               report_error_userdata)
{
    TErrorInfoPtr eip;
    char *        msg;
    const char *  format = "Expected line length %d, actual length %d";
    int           len;

    if (lip == NULL  ||  report_error == NULL) {
        return;
    }

    eip = ErrorInfoNew (NULL);
    if (eip == NULL) {
        return;
    }
    eip->category = eAlnErr_BadFormat;
    eip->id = strdup (id);
    eip->line_num = lip->line_num;
    msg = (char *) malloc (strlen (format) + kMaxPrintedIntLen + 1);
    if (msg != NULL) {
        if (lip->data == NULL) {
            len = 0;
        } else {
            len = strlen (lip->data);
        }
        sprintf (msg, format, expected_length, len);
        eip->message = msg;
    }
    report_error (eip, report_error_userdata);
}


/* This function creates and sends an error message regarding a block of
 * sequence data that was expected to contain more lines.
 */
static void 
s_ReportBlockLengthError 
(char *               id,
 int                  line_num,
 int                  expected_num,
 int                  actual_num,
 FReportErrorFunction report_error,
 void *              report_error_userdata)
{
    TErrorInfoPtr eip;
    const char *  err_format = "Expected %d lines in block, found %d";

    if (report_error == NULL) {
        return;
    }

    eip = ErrorInfoNew (NULL);
    if (eip == NULL) {
        return;
    }
    eip->category = eAlnErr_BadFormat;
    eip->id = strdup (id);
    eip->line_num = line_num;
    eip->message = (char *)malloc (strlen (err_format) + 2 * kMaxPrintedIntLen + 1);
    if (eip->message != NULL) {
      sprintf (eip->message, err_format, expected_num, actual_num);
    }
    report_error (eip, report_error_userdata);
}


/* This function creates and sends an error message regarding a block of
 * sequence data that contains duplicate IDs.
 */
static void 
s_ReportDuplicateIDError 
(char *               id,
 int                  line_num,
 FReportErrorFunction report_error,
 void *              report_error_userdata)
{
    TErrorInfoPtr eip;
    const char *  err_format = "Duplicate ID!  Sequences will be concatenated!";

    if (report_error == NULL) {
        return;
    }

    eip = ErrorInfoNew (NULL);
    if (eip == NULL) {
        return;
    }
    eip->category = eAlnErr_BadData;
    eip->id = strdup (id);
    eip->line_num = line_num;
    eip->message = strdup (err_format);
    report_error (eip, report_error_userdata);
}


/* This function creates and sends an error message regarding missing
 * sequence data.
 */
static void
s_ReportMissingSequenceData
(char *               id,
 FReportErrorFunction report_error,
 void *              report_error_userdata)
{
    TErrorInfoPtr eip;

    if (report_error == NULL) {
        return;
    }
    eip = ErrorInfoNew (NULL);
    if (eip == NULL) {
        return;
    }
    eip->category = eAlnErr_Fatal;
    eip->id = strdup (id);
    eip->message = strdup ("No data found");
    report_error (eip, report_error_userdata);
}


/* This function creates and sends an error message indicating that the
 * most common length of the sequences in the file do not match a comment
 * found in the file.
 */
static void 
s_ReportBadSequenceLength 
(char *               id,
 int                  expected_length,
 int                  actual_length,
 FReportErrorFunction report_error,
 void *               report_error_userdata)
{
    TErrorInfoPtr eip;
    const char *  format_str = "Expected sequence length %d, actual length %d";

    if (report_error == NULL) {
        return;
    }
    eip = ErrorInfoNew (NULL);
    if (eip == NULL) {
        return;
    }
    eip->category = eAlnErr_BadFormat;
    eip->id = strdup (id);
    eip->message = (char *)malloc (strlen (format_str) + 50);
    if (eip->message != NULL) {
        sprintf (eip->message, format_str, expected_length, actual_length);
    }
    report_error (eip, report_error_userdata);
}


/* This function creates and sends an error message indicating that the
 * number of sequences read does not match a comment in the alignment file.
 */
static void
s_ReportIncorrectNumberOfSequences
(int                  num_expected,
 int                  num_found,
 FReportErrorFunction report_error,
 void *              report_error_userdata)
{
    TErrorInfoPtr eip;
    const char *  err_format = "Expected %d sequences, found %d";
 
    if (report_error == NULL) {
        return;
    }
    eip = ErrorInfoNew (NULL);
    if (eip == NULL) {
        return;
    }
    eip->category = eAlnErr_BadFormat;
    eip->message = (char *) malloc (strlen (err_format) +
                                    2 * kMaxPrintedIntLen + 1);
                                     
    if (eip->message != NULL)
    {
        sprintf (eip->message, err_format, num_expected, num_found);
    }
    report_error (eip, report_error_userdata);
}


static void
s_ReportIncorrectSequenceLength 
(int                 len_expected,
 int                 len_found,
 FReportErrorFunction report_error,
 void *             report_error_userdata)
{
    TErrorInfoPtr eip;
    const char *  err_format = "Expected sequences of length %d, found %d";

    if (report_error == NULL) {
        return;
    }
    eip = ErrorInfoNew (NULL);
    if (eip == NULL) {
        return;
    }

    eip->category = eAlnErr_BadFormat;
    eip->message = (char *)malloc (strlen (err_format)
                                   + 2 * kMaxPrintedIntLen + 1);
    if (eip->message != NULL)
    {
      sprintf (eip->message, err_format, len_expected, len_found);
    }
    report_error (eip, report_error_userdata);
}


/* This function creates and sends an error message regarding a non-unique
 * organism name.
 */
static void
s_ReportRepeatedOrganismName
(char *               id,
 int                  line_num,
 int                  second_line_num,
 char *               org_name,
 FReportErrorFunction report_error,
 void *              report_error_userdata)
{
    TErrorInfoPtr eip;
    const char *  err_format = "Organism name %s also appears at line %d";

    if (report_error == NULL || org_name == NULL) {
        return;
    }
    eip = ErrorInfoNew (NULL);
    if (eip == NULL) {
        return;
    }
    eip->category = eAlnErr_BadData;
    eip->line_num = line_num;
    if (id != NULL ) {
        eip->id = strdup (id);
    }
    eip->message = (char *)malloc (strlen (err_format) + strlen (org_name)
                           + kMaxPrintedIntLen + 1);
    if (eip->message != NULL) {
        sprintf (eip->message, err_format, org_name, second_line_num);
    }
    report_error (eip, report_error_userdata);
}


/* This function creates and sends an error message indicating that some or
 * all of the organism information for the sequences are missing.
 */
static void
s_ReportMissingOrganismInfo
(FReportErrorFunction report_error,
 void *             report_error_userdata)
{
    TErrorInfoPtr eip;

    if (report_error == NULL) {
        return;
    }
    eip = ErrorInfoNew (NULL);
    if (eip == NULL) {
        return;
    }

    eip->category = eAlnErr_BadData;
    eip->message = strdup ("Missing organism information");
    report_error (eip, report_error_userdata);
}


/* This function creates and sends an error message regarding an ID that is
 * used for more than one sequence.
 */
static void 
s_ReportRepeatedId 
(TStringCountPtr      scp,
 FReportErrorFunction report_error,
 void *              report_error_userdata)
{
    TErrorInfoPtr  eip;
    const char *   err_format = "ID %s appears in the following locations:";
    char *         cp;
    TIntLinkPtr    line_number;

    if (report_error == NULL  ||  scp == NULL  ||  scp->string == NULL) {
        return;
    }

    eip = ErrorInfoNew (NULL);
    if (eip == NULL) {
        return;
    }

    eip->category = eAlnErr_BadData;
    eip->id = strdup (scp->string);
    if (scp->line_numbers != NULL) {
        eip->line_num = scp->line_numbers->ival;
    }
    eip->message = (char *) malloc ( strlen (err_format)
                                    + strlen (scp->string)
                                    + scp->num_appearances * 15
                                    + 1);
    if (eip->message != NULL) {
        sprintf (eip->message, err_format, scp->string);
        cp = eip->message + strlen (eip->message);
        for (line_number = scp->line_numbers;
             line_number != NULL;
             line_number = line_number->next) {
            sprintf (cp, " %d", line_number->ival);
            cp += strlen (cp);
        }
    }
    report_error (eip, report_error_userdata);
}


/* This function creates and sends an error message indicating that the file
 * being read is an ASN.1 file.
 */
static void 
s_ReportASN1Error 
(FReportErrorFunction errfunc,
 void *             errdata)
{
    TErrorInfoPtr eip;
    const char * msg = "This is an ASN.1 file, "
        "which cannot be read by this function.";

    if (errfunc == NULL) {
        return;
    }

    eip = ErrorInfoNew (NULL);
    if (eip != NULL) {
        eip->category = eAlnErr_BadData;
        eip->message = (char *) malloc (strlen (msg) + 1);
        if (eip->message != NULL) {
            sprintf (eip->message, "%s", msg);
        }
        errfunc (eip, errdata);
    }
}


/* This function reports that some sequences are inside brackets (indicating a segmented set)
 * and that some sequences are outside the brackets.
 */
static void 
s_ReportSegmentedAlignmentError 
(TIntLinkPtr          offset_list,
 FReportErrorFunction errfunc,
 void *               errdata)
{
    TErrorInfoPtr eip;
    const char * msg = "This file contains sequences in brackets (indicating "
        "a segmented alignment) as well as sequences not in brackets at lines "
        "%s.  Please either add or remove brackets to correct this problem.";
    int num_lines = 0;
    int         msg_len = 0;
    TIntLinkPtr t;
    char *      line_text_list;
    char *      line_text_list_offset;

    if (errfunc == NULL || offset_list == NULL) {
        return;
    }

    for (t = offset_list; t != NULL; t = t->next)
    {
        num_lines ++;
    }
    msg_len = num_lines * (kMaxPrintedIntLen + 2);
    if (num_lines > 1) 
    {
    	msg_len += 4;
    }
    line_text_list = (char *) malloc (msg_len);
    if (line_text_list == NULL) return;
    line_text_list_offset = line_text_list;
    for (t = offset_list; t != NULL; t = t->next)
    {
        if (t->next == NULL)
        {
            sprintf (line_text_list_offset, "%d", t->ival);
        }
        else if (num_lines == 2) 
        {
        	sprintf (line_text_list_offset, "%d and ", t->ival);
        }
        else if (t->next->next == NULL)
        {
        	sprintf (line_text_list_offset, "%d, and ", t->ival);
        }
        else
        {
        	sprintf (line_text_list_offset, "%d, ", t->ival);
        }
        line_text_list_offset += strlen (line_text_list_offset);
    }

    msg_len += strlen (msg) + 1;

    eip = ErrorInfoNew (NULL);
    if (eip != NULL) {
        eip->category = eAlnErr_BadData;
        eip->message = (char *) malloc (msg_len);
        if (eip->message != NULL) {
            sprintf (eip->message, msg, line_text_list);
        }
        errfunc (eip, errdata);
    }
    free (line_text_list);
}


/* This function reports an error if a line looks like it might contain an organism comment
 * but is somehow improperly formatted
 */
static void s_ReportOrgCommentError 
(char *               linestring,
 FReportErrorFunction errfunc,
 void *               errdata)
{
    TErrorInfoPtr eip;
    const char * msg = "This line may contain an improperly formatted organism description.\n"
                       "Organism descriptions should be of the form [org=tax name] or [organism=tax name].\n";
    
    if (errfunc == NULL || linestring == NULL) {
        return;
    }
                       
    eip = ErrorInfoNew (NULL);
    if (eip != NULL) {
        eip->category = eAlnErr_BadData;
        eip->message = (char *) malloc (strlen (msg) + strlen (linestring) + 1);
        if (eip->message != NULL) {
            strcpy (eip->message, msg);
            strcat (eip->message, linestring);
        }
        errfunc (eip, errdata);
    }
}

 
/* This function reports that the number of segments in an alignment of
 * segmented sets is inconsistent.
 */
static void s_ReportBadNumSegError 
(int                  line_num,
 int                  num_seg,
 int                  num_seg_exp,
 FReportErrorFunction errfunc,
 void *               errdata)
{
    TErrorInfoPtr eip;
    const char * msg = "This segmented set contains a different number of segments (%d) than expected (%d).\n";
    
    if (errfunc == NULL) {
        return;
    }
                       
    eip = ErrorInfoNew (NULL);
    if (eip != NULL) {
        eip->line_num = line_num;
        eip->category = eAlnErr_BadData;
        eip->message = (char *) malloc (strlen (msg) + 2 * kMaxPrintedIntLen + 1);
        if (eip->message != NULL) {
            sprintf (eip->message, msg, num_seg, num_seg_exp);
        }
        errfunc (eip, errdata);
    }
}

 
/* This function allocates memory for a SSequenceInfo structure and
 * initializes the member variables.  It returns a pointer to the newly
 * allocated memory.
 */
extern TSequenceInfoPtr SequenceInfoNew (void)
{
    TSequenceInfoPtr sip;

    sip = (TSequenceInfoPtr) malloc (sizeof (SSequenceInfo));
    if (sip == NULL) {
        return NULL;
    }
    sip->missing       = strdup ("?");
    sip->beginning_gap = strdup (".");
    sip->middle_gap    = strdup ("-");
    sip->end_gap       = strdup (".");
    sip->match         = strdup (".");
    sip->alphabet      = NULL;
    return sip;
}


/* This function frees memory associated with the member variables of
 * the SSequenceInfo structure and with the structure itself.
 */
extern void SequenceInfoFree (TSequenceInfoPtr sip)
{
    if (sip == NULL) {
        return;
    }
    free (sip->missing);
    free (sip->beginning_gap);
    free (sip->middle_gap);
    free (sip->end_gap);
    free (sip->match);
    sip->alphabet = NULL;
    free (sip);
}


/* This function creates and sends an error message regarding an unused line.
 */
static void 
s_ReportUnusedLine
(int                  line_num_start,
 int                  line_num_stop,
 TLineInfoPtr         line_val,
 FReportErrorFunction errfunc,
 void *               errdata)
{
    TErrorInfoPtr eip;
    const char * errformat1 = "Line %d could not be assigned to an interleaved block";
    const char * errformat2 = "Lines %d through %d could not be assigned to an interleaved block";
    const char * errformat3 = "Contents of unused line: %s";
    int skip;

    if (errfunc == NULL  ||  line_val == NULL) {
        return;
    }

    eip = ErrorInfoNew (NULL);
    if (eip != NULL) {
        eip->category = eAlnErr_BadFormat;
        eip->line_num = line_num_start;
        if (line_num_start == line_num_stop) {
              eip->message = (char *) malloc (strlen (errformat1)
                                            + kMaxPrintedIntLen + 1);
            if (eip->message != NULL) {
                sprintf (eip->message, errformat1, line_num_start);
            }
        } else {
            eip->message = (char *) malloc (strlen (errformat2)
                                            + 2 * kMaxPrintedIntLen + 1);
            if (eip->message != NULL) {
                sprintf (eip->message, errformat2, line_num_start,
                         line_num_stop);
            }
        }
        errfunc (eip, errdata);
    }
    /* report contents of unused lines */
    for (skip = line_num_start;
         skip < line_num_stop + 1  &&  line_val != NULL;
         skip++) {
        if (line_val->data == NULL) {
            continue;
        }
        eip = ErrorInfoNew (NULL);
        if (eip != NULL) {
            eip->category = eAlnErr_BadFormat;
            eip->line_num = skip;
            eip->message = (char *) malloc (strlen (errformat3)
                                            + strlen (line_val->data) + 1);
            if (eip->message != NULL) {
                sprintf (eip->message, errformat3, line_val->data);
            }
            errfunc (eip, errdata);
        }
        line_val = line_val->next;
    }
}


/* The following functions are used to manage a linked list of integer
 * values.
 */

/* This function creates a new SIntLink structure with a value of ival.
 * The new structure will be placed at the end of list if list is not NULL.
 * The function will return a pointer to the new structure.
 */
static TIntLinkPtr 
s_IntLinkNew 
(int         ival, 
 TIntLinkPtr list)
{
    TIntLinkPtr ilp, last;

    ilp = (TIntLinkPtr) malloc (sizeof (SIntLink));
    if (ilp == NULL) {
        return NULL;
    }
    ilp->ival = ival;
    ilp->next = NULL;
    last = list;
    while (last != NULL && last->next != NULL) {
        last = last->next;
    }
    if (last != NULL) {
        last->next = ilp;
    }
    return ilp;
}


/* This function recursively frees memory associated with a linked list
 * of SIntLink structures.
 */
static void s_IntLinkFree (TIntLinkPtr ilp)
{
    if (ilp == NULL) {
        return;
    }
    s_IntLinkFree (ilp->next);
    free (ilp);
}


/* These functions are used to accumulate and retrieve information on 
 * how often a size of data (number of lines or number of characters) occurs.
 */

/* This function allocates space for a new SSizeInfo structure and 
 * initializes its member variables.  If list is not NULL, the new structure
 * is added to the end of the list.
 * The function returns a pointer to the newly allocated structure.
 */
static TSizeInfoPtr s_SizeInfoNew (TSizeInfoPtr list)
{
    TSizeInfoPtr sip, last;

    sip = (TSizeInfoPtr) malloc (sizeof (SSizeInfo));
    if (sip == NULL) {
        return NULL;
    }

    sip->size_value      = 0;
    sip->num_appearances = 0;
    sip->next            = NULL;
    last = list;
    while (last != NULL && last->next != NULL) {
        last = last->next;
    }
    if (last != NULL) {
        last->next = sip;
    }
    return sip;
}


/* This function recursively frees the memory associated with a linked list
 * of SSizeInfo structures.
 */
static void s_SizeInfoFree (TSizeInfoPtr list)
{
    if (list == NULL) {
        return;
    }
    s_SizeInfoFree (list->next);
    list->next = NULL;
    free (list);
}


/* This function returns eTrue if the two SSizeInfo structures have
 * the same size_value and number of appearances, eFalse otherwise.
 */
static EBool 
s_SizeInfoIsEqual 
(TSizeInfoPtr s1,
 TSizeInfoPtr s2)
{
    if (s1 == NULL
      ||  s2 == NULL
      ||  s1->size_value != s2->size_value
      ||  s1->num_appearances != s2->num_appearances) {
        return eFalse;
    }
    return eTrue;
}


/* This function searches list for a SSizeInfo structure with the
 * same size_value as size_value.  If it finds such a structure, it
 * adds the value of num_appearances to the num_appearances for that
 * structure, otherwise the function creates a new structure at the end
 * of the list with the specified values of size_value and num_appearances.
 * The function returns a pointer to the list of SSizeInfo structures.
 */
static TSizeInfoPtr s_AddSizeInfoAppearances 
(TSizeInfoPtr list,
 int  size_value,
 int  num_appearances)
{
    TSizeInfoPtr p, last;

    last = NULL;
    for (p = list;  p != NULL  &&  p->size_value != size_value;  p = p->next) {
        last = p;
    }
    if (p == NULL) {
        p = (TSizeInfoPtr) malloc (sizeof (SSizeInfo));
        if (p == NULL) {
            return NULL;
        }
        p->size_value = size_value;
        p->num_appearances = num_appearances;
        p->next = 0;
        if (last == NULL) {
            list = p;
        } else {
            last->next = p;
        }
    } else {
        p->num_appearances += num_appearances;
    }
    return list;
}


/* This function searches list for a SSizeInfo structure with the
 * same size_value as size_value.  If it finds such a structure, it
 * adds one to the num_appearances for that structure, otherwise the 
 * function creates a new structure at the end of the list with the 
 * specified values of size_value and num_appearances.
 * The function returns a pointer to the list of SSizeInfo structures.
 */
static TSizeInfoPtr 
s_AddSizeInfo
(TSizeInfoPtr list,
 int  size_value)
{
    return s_AddSizeInfoAppearances (list, size_value, 1);
}


/* This function searches list for the SSizeInfo structure with the
 * highest value for num_appearances.  If more than one structure exists
 * with the highest value for num_appearances, the function chooses the
 * value with the highest value for size_value.  The function returns a
 * pointer to the structure selected based on the above criteria.
 */
static TSizeInfoPtr s_GetMostPopularSizeInfo (TSizeInfoPtr list)
{
    TSizeInfoPtr p, best;

    if (list == NULL) {
        return NULL;
    }

    best = list;
    for (p = list->next;  p != NULL;  p = p->next) {
      if (p->num_appearances > best->num_appearances
          ||  (p->num_appearances == best->num_appearances
            &&  p->size_value > best->size_value)) {
          best = p;
      }
    }
    return best;
}


/* This function uses s_GetMostPopularSizeInfo function to find the structure
 * in list that has the highest value for num_appearances and size_value.
 * If such a structure is found and has a num_appearances value greater than
 * one, the size_value for that structure will be returned, otherwise the
 * function returns 0.
 */
static int  s_GetMostPopularSize (TSizeInfoPtr list)
{
    TSizeInfoPtr best;

    best = s_GetMostPopularSizeInfo (list);
    if (best == NULL) {
        return 0;
    }
    if (best->num_appearances > 1) {
        return best->size_value; 
    } else {
        return 0;
    }
}


/* The following functions are used to keep track of patterns of line or
 * token lengths, which will be used to identify errors in formatting.
 */
static SLengthListPtr s_LengthListNew (SLengthListPtr list)
{
    SLengthListPtr llp, last;

    llp = (SLengthListPtr) malloc (sizeof (SLengthListData));
    if (llp == NULL) {
        return NULL;
    }

    llp->lengthrepeats   = NULL;
    llp->num_appearances = 0;
    llp->next            = NULL;

    last = list;
    while (last != NULL && last->next != NULL) {
        last = last->next;
    }
    if (last != NULL) {
        last->next = llp;
    }
    return llp;
}


/* This function recursively frees memory for a list of SLengthListData
 * structures and its member variables.
 */
static void s_LengthListFree (SLengthListPtr llp)
{
    if (llp == NULL) {
        return;
    }
    s_LengthListFree (llp->next);
    s_SizeInfoFree (llp->lengthrepeats);
    free (llp);
}


/* This function examines the last SSizeInfo structure in the 
 * lengthrepeats member variable of llp.  If the last structure 
 * in the list has the same size_value value as the function argument 
 * size_value, the value of num_appearances for that SizeInforData structure
 * will be incremented.  Otherwise a new SSizeInfo structure will be
 * appended to the end of the lengthrepeats list with the specified
 * size_value and a num_appearances value of 1.
 */
static void 
s_AddLengthRepeat
(SLengthListPtr llp,
 int  size_value)
{
    TSizeInfoPtr p, last;

    if (llp == NULL) {
        return;
    }

    last = NULL;
    for (p = llp->lengthrepeats;  p != NULL;  p = p->next) {
        last = p;
    }
    if (last == NULL  ||  last->size_value != size_value) {
        p = (TSizeInfoPtr) malloc (sizeof (SSizeInfo));
        if (p == NULL) {
            return;
        }
        p->size_value = size_value;
        p->num_appearances = 1;
        p->next = 0;
        if (last == NULL) {
            llp->lengthrepeats = p;
        } else {
            last->next = p;
        }
    } else {
        last->num_appearances ++;
    }
}


/* This function examines whether two SLengthListData structures "match" - 
 * the structures match if each SSizeInfo structure in llp1->lengthrepeats
 * has the same size_value and num_appearances values as the SSizeInfo
 * structure in the corresponding list position in llp2->lenghrepeats.
 * If the two structures match, the function returns eTrue, otherwise the
 * function returns eFalse.
 */
static EBool 
s_DoLengthPatternsMatch 
(SLengthListPtr llp1,
 SLengthListPtr llp2)
{
    TSizeInfoPtr sip1, sip2;

    if (llp1 == NULL  ||  llp2 == NULL
        ||  llp1->lengthrepeats == NULL
        ||  llp2->lengthrepeats == NULL) {
        return eFalse;
    }
    for (sip1 = llp1->lengthrepeats, sip2 = llp2->lengthrepeats;
         sip1 != NULL  &&  sip2 != NULL;
         sip1 = sip1->next, sip2 = sip2->next) {
        if ( ! s_SizeInfoIsEqual (sip1, sip2)
          ||  (sip1->next == NULL  &&  sip2->next != NULL)
          ||  (sip1->next != NULL  &&  sip2->next == NULL)) {
            return eFalse;
        }
    }
    return eTrue;
}


/* This function examines a list of SLengthListData structures to see if
 * one of them matches llp.  If so, the value of num_appearances in that
 * list is incremented by one and llp is freed, otherwise llp is added
 * to the end of the list.
 * The function returns a pointer to the list of LenghtListData structures.
 */
static SLengthListPtr
s_AddLengthList
(SLengthListPtr list,
 SLengthListPtr llp)
{
    SLengthListPtr prev_llp;

    if (list == NULL) {
        list = llp;
    } else {
        prev_llp = list;
        while ( prev_llp->next  &&  ! s_DoLengthPatternsMatch (prev_llp, llp)) {
            prev_llp = prev_llp->next;
        }
        if (s_DoLengthPatternsMatch (prev_llp, llp)) {
            prev_llp->num_appearances ++;
            s_LengthListFree (llp);
        } else {
            prev_llp->next = llp;
        }
    }
    return list;
}


/* This set of functions is used for storing and analyzing individual lines
 * or tokens from an alignment file.
 */

/* This function allocates memory for a new SLineInfo structure and
 * initializes the structure with a saved copy of string and the specified
 * values of line_num and line_offset.
 * The function returns a pointer to the new SLineInfo structure.
 */
static TLineInfoPtr
s_LineInfoNew
(char * string,
 int    line_num,
 int    line_offset)
{
    TLineInfoPtr lip;

    lip = (TLineInfoPtr) malloc (sizeof (SLineInfo));
    if (lip == NULL) {
        return NULL;
    }
    lip->data = strdup (string);
    lip->line_num = line_num + 1;
    lip->line_offset = line_offset;
    lip->delete_me = eFalse;
    lip->next = NULL;
    return lip;
}


/* This function recursively frees the memory associated with the structures
 * and members of the structures in a linked list of SLineInfo structures.
 */
static void s_LineInfoFree (TLineInfoPtr lip)
{
    TLineInfoPtr next_lip;
    if (lip == NULL) {
        return;
    }
    while (lip != NULL) {
        next_lip = lip->next;
        lip->next = NULL;
        free (lip->data);
        free (lip);
        lip = next_lip; 
    }
}


/* This function deletes from a linked list of SLineInfo structures
 * those structures for which the delete_me flag has been set.  The function
 * returns a pointer to the beginning of the new list.
 */
static TLineInfoPtr s_DeleteLineInfos (TLineInfoPtr list)
{
    TLineInfoPtr prev = NULL;
    TLineInfoPtr lip, nextlip;

    lip = list;
    while (lip != NULL) {
        nextlip = lip->next;
        if (lip->delete_me) {
            if (prev != NULL) {
                prev->next = lip->next;
            } else {
                list = lip->next;
            }
            lip->next = NULL;
            s_LineInfoFree (lip);
        } else {
            prev = lip;
        }
        lip = nextlip;
    }
    return list;
}
     
 
/* This function creates a new SLineInfo structure, populates it with
 * a copy of string and the specified line_num and line_offset values,
 * and appends it to the end of "list" if list is not NULL.
 * The function will return a pointer to the newly created structure
 * if list is NULL, otherwise the function will return list.
 */
static TLineInfoPtr 
s_AddLineInfo
(TLineInfoPtr list,
 char * string,
 int    line_num,
 int    line_offset)
{
    TLineInfoPtr lip, p;

    if (string == NULL) {
        return list;
    }
    lip = s_LineInfoNew (string, line_num, line_offset);
    if (lip == NULL) {
        return NULL;
    }
    if (list == NULL) {
        list = lip;
    } else {
        p = list;
        while (p != NULL  &&  p->next != NULL) {
            p = p->next;
        }
        p->next = lip;
    }
    return list;
}

/* This function creates a new bracketed comment */
static TBracketedCommentListPtr s_BracketedCommentListNew 
(TBracketedCommentListPtr list,
 char * string,
 int    line_num,
 int    line_offset)
{
    TBracketedCommentListPtr comment;
    	
    comment = (TBracketedCommentListPtr) malloc (sizeof (SBracketedCommentList));
    if (comment == NULL) {
    	return NULL;
    }
    comment->comment_lines = s_LineInfoNew (string, line_num, line_offset);
    comment->next = NULL;
    
    if (list != NULL) {
    	while (list->next != NULL) {
    		list = list->next;
    	}
    	list->next = comment;
    }
    
    return comment;
}

/* This function frees a bracketed comment list. */
static void s_BracketedCommentListFree (TBracketedCommentListPtr list)
{
    if (list == NULL) {
  	    return;
    }
    s_BracketedCommentListFree (list->next);
    list->next = NULL;
    s_LineInfoFree (list->comment_lines);
}

/* This function adds a line to a bracketed comment. */
static void s_BracketedCommentListAddLine 
(TBracketedCommentListPtr comment,
 char                   * string,
 int                      line_num,
 int                      line_offset)
{
	if (comment == NULL) {
		return;
	}

    comment->comment_lines = s_AddLineInfo (comment->comment_lines, string, line_num, line_offset);
}

/* This function counts the sequences found in a bracketed comment. */
static int s_CountSequencesInBracketedComment (TBracketedCommentListPtr comment)
{
    TLineInfoPtr lip;
    int          num_segments = 0;
    EBool        skipped_line_since_last_defline = eTrue;
    
	if (comment == NULL || comment->comment_lines == NULL) {
		return 0;
	}
	
	lip = comment->comment_lines;
	/* First line must be left bracket on a line by itself */
	if (lip->data[0] != '[' || strspn (lip->data + 1, " \t\r\n") != strlen (lip->data + 1))
	{
		return 0;
	}
	lip = lip->next;
	while (lip != NULL && lip->next != NULL)
	{
		if (lip->data[0] == '>')
		{
			if (!skipped_line_since_last_defline) 
			{
				return 0;
			}
			else
			{
				num_segments ++;
				skipped_line_since_last_defline = eFalse;
			}
		}
		else 
		{
			skipped_line_since_last_defline = eTrue;
		}
		lip = lip->next;
	}
	/* Last line must be right bracket on a line by itself */
	/* First line must be left bracket on a line by itself */
	if (lip->data[0] != ']' || strspn (lip->data + 1, " \t\r\n") != strlen (lip->data + 1))
	{
		return 0;
	}
	
	return num_segments;
}

/* This function counts the number of sequences that appear in
 * bracketed comments.  If the number of sequences is inconsistent,
 * the function will issue error messages and return a 1, otherwise
 * the function will return the number of sequences that appear in
 * each bracketed comment.
 */
static int s_GetNumSegmentsInAlignment 
(TBracketedCommentListPtr comment_list,
 FReportErrorFunction     errfunc,
 void *                   errdata)
{
    TBracketedCommentListPtr comment;
    TSizeInfoPtr             segcount_list = NULL;
    int                      num_segments = 1;
    int                      num_segments_this_bracket;
    int                      num_segments_expected;
    TSizeInfoPtr             best;
    
	if (comment_list == NULL)
	{
		return num_segments;
	}
	
	for (comment = comment_list; comment != NULL; comment = comment->next)
	{
	    num_segments_this_bracket = s_CountSequencesInBracketedComment (comment);
        segcount_list = s_AddSizeInfoAppearances (segcount_list,
                                                  num_segments_this_bracket,
                                                  1);
        if (comment != comment_list && segcount_list->next != NULL)
        {
            best = s_GetMostPopularSizeInfo (segcount_list);
            num_segments_expected = best->size_value;

        	if (num_segments_expected != num_segments_this_bracket)
        	{
        		s_ReportBadNumSegError (comment->comment_lines->line_num,
        		                        num_segments_this_bracket, num_segments_expected,
        		                        errfunc, errdata);
        	}
        }
	}
	if (segcount_list != NULL && segcount_list->next == NULL && segcount_list->size_value > 0)
	{
		num_segments = segcount_list->size_value;
	}
	s_SizeInfoFree (segcount_list);
	return num_segments;
}

/* This function gets a list of the offsets of the 
 * sequences in bracketed comments.
 */
static TIntLinkPtr GetSegmentOffsetList (TBracketedCommentListPtr comment_list)
{
	TIntLinkPtr              new_offset, offset_list = NULL;
	TBracketedCommentListPtr comment;
	TLineInfoPtr             lip;

    if (comment_list == NULL) 
    {
    	return NULL;
    }
    
    for (comment = comment_list; comment != NULL; comment = comment->next)
    {
    	if (s_CountSequencesInBracketedComment (comment) == 0) 
    	{
    		continue;
    	}
    	for (lip = comment->comment_lines; lip != NULL; lip = lip->next)
    	{
    		if (lip->data != NULL && lip->data[0] == '>') 
    		{
                new_offset = s_IntLinkNew (lip->line_num + 1, offset_list);
                if (offset_list == NULL) offset_list = new_offset;
    		}
        }
    }
    return offset_list;
}

static char * s_TokenizeString (char * str, const char *delimiter, char **last)
{
    int skip;
    int length;

    if (str == NULL) {
        str = *last;
    }
    if (delimiter == NULL) {
        *last = NULL;
        return NULL;
    }

    if (str == NULL || *str == 0) {
        return NULL;
    }
    skip = strspn (str, delimiter);
    str += skip;
    length = strcspn (str, delimiter);
    *last = str + length;
    if (**last != 0) {
        **last = 0;
        (*last) ++;
    }
    return str;
}
  

/* This function creates a new list of SLineInfo structures by tokenizing
 * each data element from line_list into multiple tokens at whitespace.
 * The function returns a pointer to the new list.  The original list is
 * unchanged.
 */
static TLineInfoPtr s_BuildTokenList (TLineInfoPtr line_list)
{
    TLineInfoPtr first_token, lip;
    char *       tmp;
    char *       piece;
    char *       last;
    int          line_pos;

    first_token = NULL;

    for (lip = line_list;  lip != NULL;  lip = lip->next) {
        if (lip->data != NULL  &&  (tmp = strdup (lip->data)) != NULL) {
            piece = s_TokenizeString (tmp, " \t\r", &last);
            while (piece != NULL) {
                line_pos = piece - tmp;
                line_pos += lip->line_offset;
                first_token = s_AddLineInfo (first_token, piece, 
                                             lip->line_num,
                                             line_pos);
                piece = s_TokenizeString (NULL, " \t\r", &last);
            }
            free (tmp);
        }
    }
    return first_token;
}


/* This function takes a list of SLineInfo structures, allocates memory
 * to hold their contents contiguously, and stores their contents, minus
 * the whitespace, in the newly allocated memory.
 * The function returns a pointer to this newly allocated memory.
 */
static char * s_LineInfoMergeAndStripSpaces (TLineInfoPtr list)
{
    TLineInfoPtr lip;
    int          len;
    char *       result;
    char *       cp_to;
    char *       cp_from;

    if (list == NULL) {
        return NULL;
    }
    len = 0;
    for (lip = list;  lip != NULL;  lip = lip->next) {
        if (lip->data != NULL) {
            len += strlen (lip->data);
        }
    }
    result = (char *) malloc (len + 1);
    if (result == NULL) {
        return result;
    }
    cp_to = result;
    for (lip = list;  lip != NULL;  lip = lip->next) {
        if (lip->data != NULL) {
            cp_from = lip->data;
            while (*cp_from != 0) {
                if (! isspace ((unsigned char)*cp_from)) {
                    *cp_to = *cp_from;
                    cp_to ++;
                }
                cp_from ++;
            }
        }
    }
    *cp_to = 0;
    return result;
}


/* The following functions are used to manage the SLineInfoReader
 * structure.  The intention is to allow the user to access the data
 * from a linked list of SLineInfo structures using a given position
 * in the data based on the number of sequence data characters rather than 
 * any particular line number or position in the line.  This is useful
 * for matching up a data position in a record with a match character with
 * the same data position in the first or master record.  This is also useful
 * for determining how to interpret special characters that may have
 * context-sensitive meanings.  For example, a ? could indicate a missing 
 * character if it is inside a sequence but indicate a gap if it is outside
 * a sequence.
 */

/* This function is used to advance the current data position pointer
 * for a SLineInfoReader structure past white space and blank lines
 * in sequence data.
 */
static void s_LineInfoReaderAdvancePastSpace (TLineInfoReaderPtr lirp)
{
    if (lirp->curr_line_pos == NULL) {
        return;
    }
    while ( isspace ((unsigned char) *lirp->curr_line_pos)
           ||  *lirp->curr_line_pos == 0) {
        while ( isspace ((unsigned char)*lirp->curr_line_pos)) {
            lirp->curr_line_pos ++;
        }
        if (*lirp->curr_line_pos == 0) {
            lirp->curr_line = lirp->curr_line->next;
            while (lirp->curr_line != NULL
                   &&  lirp->curr_line->data == NULL) {
                lirp->curr_line = lirp->curr_line->next;
            }
            if (lirp->curr_line == NULL) {
                lirp->curr_line_pos = NULL;
                return;
            } else {
                lirp->curr_line_pos = lirp->curr_line->data;
            }
        }
    }
}


/* This function sets the current data position pointer to the first
 * non-whitespace character in the sequence data.
 */
static void s_LineInfoReaderReset (TLineInfoReaderPtr lirp)
{
    if (lirp == NULL) {
        return;
    }
    lirp->curr_line = lirp->first_line;

    while (lirp->curr_line != NULL  &&  lirp->curr_line->data == NULL) {
        lirp->curr_line = lirp->curr_line->next;
    }
    if (lirp->curr_line == NULL) {
        lirp->curr_line_pos = NULL;
        lirp->data_pos = -1;
    } else {
        lirp->curr_line_pos = lirp->curr_line->data;
        s_LineInfoReaderAdvancePastSpace (lirp);
        if (lirp->curr_line_pos == NULL) {
            lirp->data_pos = -1;
        } else {
            lirp->data_pos = 0;
        }
    }
}

 
/* This function creates a new SLineInfoReader structure and initializes
 * its member variables.  The current data position pointer is set to the
 * first non-whitespace character in the sequence data, and the data position
 * counter is set to zero.  The function returns a pointer to the new
 * LineInfoReader data structure.
 */
static TLineInfoReaderPtr s_LineInfoReaderNew (TLineInfoPtr line_list)
{
    TLineInfoReaderPtr lirp;

    if (line_list == NULL) {
        return NULL;
    }
    lirp = (TLineInfoReaderPtr) malloc (sizeof (SLineInfoReader));
    if (lirp == NULL) {
        return NULL;
    }

    lirp->first_line = line_list;
    s_LineInfoReaderReset (lirp);
    return lirp;
}


/* This function safely interprets the current line number of the
 * SLineInfoReader structure.  If the structure is NULL or the
 * current line is NULL (usually because the data position has been
 * advanced to the end of the available sequence data), the function
 * returns -1, since the current data position does not actually exist.
 * Otherwise, the line number of the character at the current data position
 * is returned.
 */
static int  s_LineInfoReaderGetCurrentLineNumber (TLineInfoReaderPtr lirp)
{
    if (lirp == NULL  ||  lirp->curr_line == NULL) {
        return -1;
    } else {
        return lirp->curr_line->line_num;
    }
}


/* This function safely interprets the position of the current data position
 * of the SLineInfoReader structure.  If the structure is NULL or the
 * current line is NULL or the current line position is NULL (usually because
 * the data position has been advanced to the end of the available sequence
 * data), the function returns -1, since the current data position does not 
 * actually exist.
 * Otherwise, the position within the line of the character at the current 
 * data position is returned.
 */
static int  s_LineInfoReaderGetCurrentLineOffset (TLineInfoReaderPtr lirp)
{
    if (lirp == NULL  ||  lirp->curr_line == NULL 
        ||  lirp->curr_line_pos == NULL) {
        return -1;
    } else {
        return lirp->curr_line->line_offset + lirp->curr_line_pos 
                     - lirp->curr_line->data;
    }
}


/* This function frees the memory associated with the SLineInfoReader
 * structure.  Notice that this function does NOT free the SLineInfo list.
 * This is by design.
 */
static void s_LineInfoReaderFree (TLineInfoReaderPtr lirp)
{
    if (lirp == NULL) {
        return;
    }
    free (lirp);
    lirp = NULL;
}


/* This function retrieves the "pos"th sequence data character from the lines
 * of sequence data.  If the data position requested is greater than the
 * current position, the current data pointer will be advanced until the
 * current position is the requested position or there is no more data.  If
 * there is no more data, the function returns a 0.  If the data position
 * requested is lower than the current position, the current position is reset
 * to the beginning of the sequence and advanced from there.
 * As a result, it is clearly more efficient to read the data in the forward
 * direction, but it is still possible to access the data randomly.
 */
static char 
s_FindNthDataChar
(TLineInfoReaderPtr lirp,
 int  pos)
{
    if (lirp == NULL  ||  lirp->first_line == NULL  ||  pos < 0
        ||  lirp->data_pos == -1) {
        return 0;
    }

    if (lirp->data_pos == pos) {
        if (lirp->curr_line_pos == NULL) {
            return 0;
        } else {
            return *lirp->curr_line_pos;
        }
    }
    if (lirp->data_pos > pos) {
        s_LineInfoReaderReset (lirp);
    }
     
    while (lirp->data_pos < pos  &&  lirp->curr_line != NULL) {
        lirp->curr_line_pos ++;
        /* skip over spaces, progress to next line if necessary */
        s_LineInfoReaderAdvancePastSpace (lirp);
        lirp->data_pos ++;
    }
    if (lirp->curr_line_pos != NULL) {
        return *lirp->curr_line_pos;
    } else {
        return 0;
    }
}


/* The following functions are used to manage the SStringCount structure.
 * These functions are useful for determining whether a string is unique
 * or whether only one string is used for a particular purpose.
 * The structure also tracks the line numbers on which a particular string
 * appeared.
 */

/* This function allocates memory for a new SStringCount structure,
 * initializes its member variables.  The function also places the 
 * structure at the end of list if list is not NULL.
 * The function returns a pointer to the newly allocated SStringCount
 * structure.
 */
static TStringCountPtr s_StringCountNew (TStringCountPtr list)
{
    TStringCountPtr new_item, last;

    new_item = (TStringCountPtr) malloc (sizeof (SStringCount));
    if (new_item == NULL) {
        return NULL;
    }
    new_item->string          = NULL;
    new_item->num_appearances = 0;
    new_item->line_numbers    = NULL;
    new_item->next            = NULL;

    last = list;
    while (last != NULL && last->next != NULL) {
        last = last->next;
    }
    if (last != NULL) {
        last->next = new_item;
    }
    return new_item;
}


/* This function recursively frees data associated with the structures
 * and structure member variables in a linked list of SStringCount
 * structures.
 */
static void s_StringCountFree (TStringCountPtr list)
{
    if (list == NULL) {
        return;
    }
    s_StringCountFree (list->next);
    s_IntLinkFree (list->line_numbers);
    free (list);
}


/* This function searches list to see if the string matches any of the
 * existing entries.  If so, the num_appearances value for that entry is
 * increased and the line_num is added to that entry's list of line numbers.
 * Otherwise a new entry is created at the end of the list.
 * The function returns list if list was not NULL, or a pointer to the
 * newly created SStringCount structure otherwise.
 */
static TStringCountPtr s_AddStringCount (
  char *          string,
  int             line_num,
  TStringCountPtr list
)
{
    TStringCountPtr  add_to, last = NULL;
    TIntLinkPtr      new_offset;

    if (string == NULL) {
        for (add_to = list;
             add_to != NULL  &&  add_to->string != NULL;
             add_to = add_to->next) {
            last = add_to;
        }
    } else {
        for (add_to = list;
             add_to != NULL
               &&  (add_to->string == NULL
                 ||  strcmp (string, add_to->string) != 0);
             add_to = add_to->next) {
            last = add_to;
        }
    }
    
    if (add_to == NULL) {
        add_to = s_StringCountNew (last);
        if (list == NULL) list = add_to;
        if (add_to != NULL) {
            add_to->string = string;
        }
    }
    if (add_to != NULL) {
        add_to->num_appearances ++;
        new_offset = s_IntLinkNew (line_num, add_to->line_numbers);
        if (add_to->line_numbers == NULL) {
            add_to->line_numbers = new_offset;
        }
    }
    return list;   
}

/* The following functions are replacements for strncasecmp and strcasecmp */

/* This function returns -1 if str1 is less than str2 in the first cmp_count
 * characters (using case-insensitive comparisons), 0 if they are equal,
 * and 1 if str1 is greater than str2.
 */
static int s_StringNICmp (char * str1, char *str2, int cmp_count)
{
    char * cp1;
    char * cp2;
    int    char_count, diff;

    if (str1 == NULL && str2 == NULL) {
        return 0;
    }
    if (str1 == NULL) {
        return -1;
    }
    if (str2 == NULL) {
        return 1;
    }
    cp1 = str1;
    cp2 = str2;
    char_count = 0;
    while (*cp1 != 0  &&  *cp2 != 0  &&  char_count < cmp_count) {
        diff = toupper ((unsigned char)(*cp1)) - toupper ((unsigned char)(*cp2));
        if (diff != 0) {
            return diff;
        }
        char_count ++;
        cp1++;
        cp2++;
    }
    if (char_count == cmp_count) {
        return 0;
    } else if (*cp1 == 0  &&  *cp2 != 0) {
        return -1;
    } else if (*cp1 != 0  && *cp2 == 0) {
        return 1;
    } else {
        return 0;
    }
}


/* This function returns -1 if str1 is less than str2 using case-insensitive
 * comparisons), 0 if they are equal, and 1 if str1 is greater than str2.
 */
static int s_StringICmp (char * str1, char *str2)
{
    char * cp1;
    char * cp2;
    int    diff;

    if (str1 == NULL && str2 == NULL) {
        return 0;
    }
    if (str1 == NULL) {
        return -1;
    }
    if (str2 == NULL) {
        return 1;
    }
    cp1 = str1;
    cp2 = str2;
    while (*cp1 != 0  &&  *cp2 != 0) {
        diff = toupper ((unsigned char) *cp1) - toupper ((unsigned char) *cp2);
        if (diff != 0) {
            return diff;
        }
        cp1++;
        cp2++;
    }
    if (*cp1 == 0  &&  *cp2 != 0) {
        return -1;
    } else if (*cp1 != 0  && *cp2 == 0) {
        return 1;
    } else {
        return 0;
    }
}


/* The following functions are used to analyze specific kinds of lines
 * found in alignment files for information regarding the number of
 * expected sequences, the expected length of those sequences, and the
 * characters used to indicate missing, gap, and match characters.
 */

/* This function reads two numbers separated by whitespace from the
 * beginning of the string and uses them to set the expected number of
 * sequences and the expected number of characters per sequence.
 */
static void
s_GetFASTAExpectedNumbers
(char *           str,
 SAlignRawFilePtr afrp)
{
    char *  cp;
    char *  cpend;
    char    replace;
    int     first, second;

    if (str == NULL  ||  afrp == NULL) {
        return;
    }
    cp = str;
    while (! isdigit ((unsigned char)*cp)  &&  *cp != 0) {
        cp++;
    }

    cpend = cp;
    while (isdigit ((unsigned char)*cpend)  &&  *cpend != 0) {
        cpend++;
    }
    if (cp == cpend) {
        return;
    }
    replace = *cpend;
    *cpend = 0;
    first = atol (cp);
    *cpend = replace;

    cp = cpend;
    while (! isdigit ((unsigned char)*cp)  &&  *cp != 0) {
        cp++;
    }

    cpend = cp;
    while (isdigit ((unsigned char)*cpend)  &&  *cpend != 0) {
        cpend++;
    }
    if (cp == cpend) {
        return;
    }
    replace = *cpend;
    *cpend = 0;
    second = atol (cp);
    *cpend = replace;

    if (first > 0  &&  second > 0) {
        afrp->expected_num_sequence = first;
        afrp->expected_sequence_len = second;
    }
  
}


/* This function examines the string str to see if it begins with two
 * numbers separated by whitespace.  The function returns eTrue if so,
 * otherwise it returns eFalse.
 */
static EBool s_IsTwoNumbersSeparatedBySpace (char * str)
{
    char * cp;
    EBool  found_first_number = eFalse;
    EBool  found_dividing_space = eFalse;
    EBool  found_second_number = eFalse;
    EBool  found_second_number_end = eFalse;

    if (str == NULL) {
        return eFalse;
    }
    cp = str;
    while (*cp != 0) {
        if (! isdigit ((unsigned char)*cp)  &&  ! isspace ((unsigned char)*cp)) {
            return eFalse;
        }
        if (! found_first_number) {
            if (! isdigit ((unsigned char)*cp)) {
                return eFalse;
            }
            found_first_number = eTrue;
        } else if (! found_dividing_space) {
            if ( isspace ((unsigned char) *cp)) {
                found_dividing_space = eTrue;
            } else if ( ! isdigit ((unsigned char)*cp)) {
                return eFalse;
            }
        } else if (! found_second_number) {
            if ( isdigit ((unsigned char)*cp)) {
                found_second_number = eTrue;
            } else if (! isspace ((unsigned char) *cp)) {
                return eFalse;
            }
        } else if (! found_second_number_end) {
            if ( isspace ((unsigned char) *cp)) {
                found_second_number_end = eTrue;
            } else if (! isdigit ((unsigned char)*cp)) {
                return eFalse;
            }
        } else if (! isspace ((unsigned char) *cp)) {
            return eFalse;
        }
        cp++;
    }
    if (found_second_number) {
        return eTrue;
    }
    return eFalse;
}


/* This function finds a value name in a string, looks for an equals sign
 * after the value name, and then looks for an integer value after the
 * equals sign.  If the integer value is found, the function copies the
 * integer value into the val location and returns eTrue, otherwise the
 * function returns eFalse.
 */
static EBool 
s_GetOneNexusSizeComment 
(char       * str,
 const char * valname, 
 int        * val)
{
    char   buf[MAX_PRINTED_INT_LEN_PLUS_ONE];
    char * cpstart;
    char * cpend;
    int    maxlen;

    if (str == NULL  ||  valname == NULL  ||  val == NULL) {
        return eFalse;
    }

    cpstart = strstr (str, valname);
    if (cpstart == NULL) {
        return eFalse;
    }
    cpstart += strlen (valname);
    while (*cpstart != 0  &&  isspace ((unsigned char)*cpstart)) {
        cpstart++;
    }
    if (*cpstart != '=') {
        return eFalse;
    }
    cpstart ++;
    while (*cpstart != 0  &&  isspace ((unsigned char)*cpstart)) {
        cpstart++;
    }

    if (! isdigit ((unsigned char)*cpstart)) {
        return eFalse;
    }
    cpend = cpstart + 1;
    while ( *cpend != 0  &&  isdigit ((unsigned char)*cpend)) {
        cpend ++;
    }
    maxlen = cpend - cpstart;
    if (maxlen > kMaxPrintedIntLen) maxlen = kMaxPrintedIntLen;

    strncpy (buf, cpstart, maxlen);
    buf [maxlen] = 0;
    *val = atoi (buf);
    return eTrue;
}


/* This function looks for Nexus-style comments to indicate the number of
 * sequences and the number of characters per sequence expected from this
 * alignment file.  If the function finds these comments, it returns eTrue,
 * otherwise it returns eFalse.
 */
static void 
s_GetNexusSizeComments 
(char *           str,
 EBool *          found_ntax,
 EBool *          found_nchar,
 SAlignRawFilePtr afrp)
{
    int  num_sequences;
    int  num_chars;
  
    if (str == NULL  ||  found_nchar == NULL  
        ||  found_ntax == NULL  ||  afrp == NULL) {
        return;
    }
    if (! *found_ntax  && 
        (s_GetOneNexusSizeComment (str, "ntax", &num_sequences)
        ||   s_GetOneNexusSizeComment (str, "NTAX", &num_sequences))) {
        afrp->expected_num_sequence = num_sequences;
        afrp->align_format_found = eTrue;
        *found_ntax = eTrue;
    }
    if (! *found_nchar  &&
        (s_GetOneNexusSizeComment (str, "nchar", &num_chars)
        ||  s_GetOneNexusSizeComment (str, "NCHAR", &num_chars))) {
        afrp->expected_sequence_len = num_chars;
        afrp->align_format_found = eTrue;
        *found_nchar = eTrue;
    }
}


/* This function looks for characters in Nexus-style comments to 
 * indicate values for specific kinds of characters (match, missing, gap...).
 * If the string str contains val_name followed by an equals sign, the function
 * will return the first non-whitespace character following the equals sign,
 * otherwise the function will return a 0.
 */
static char GetNexusTypechar (char * str, const char * val_name)
{
    char * cp;
    char * cpend;

    if (str == NULL  ||  val_name == NULL) {
        return 0;
    }
    cpend = strstr (str, ";");
    if (cpend == NULL) {
        return 0;
    }
    cp = strstr (str, val_name);
    if (cp == NULL || cp > cpend) {
        return 0;
    }
    cp += strlen (val_name);
    while ( isspace ((unsigned char)*cp)) {
        cp ++;
    }
    if (*cp != '=') {
        return 0;
    }
    cp++;
    while ( isspace ((unsigned char)*cp) || *cp == '\'') {
        cp ++;
    }
    return *cp;
}


/* This function reads a Nexus-style comment line for the characters 
 * specified for missing, match, and gap and compares the characters from
 * the comment with the characters specified in sequence_info.  If any
 * discrepancies are found, the function reports the errors and returns eFalse,
 * otherwise the function returns eTrue.
 */ 
static EBool s_CheckNexusCharInfo 
(char *               str,
 TSequenceInfoPtr     sequence_info,
 FReportErrorFunction errfunc,
 void *              errdata)
{
    char * cp;
    char   c;

    if (str == NULL  ||  sequence_info == NULL) {
        return eFalse;
    }

    cp = strstr (str, "format ");
    if (cp == NULL) {
        cp = strstr (str, "FORMAT ");
    }
    if (cp == NULL) {
        return eFalse;
    }

    if (errfunc == NULL) {
        return eTrue;
    }

    c = GetNexusTypechar (cp + 7, "missing");
    if (c == 0) {
        c = GetNexusTypechar (cp + 7, "MISSING");
    }
    if (c != 0  &&  sequence_info->missing != NULL
        &&  strchr (sequence_info->missing, c) == NULL)
    {
        s_ReportCharCommentError (sequence_info->missing, c, "MISSING",
                                errfunc, errdata);
    }
 
    c = GetNexusTypechar (cp + 7, "gap");
    if (c == 0) {
        c = GetNexusTypechar (cp + 7, "GAP");
    }
    if (c != 0  &&  sequence_info->middle_gap != NULL
        &&  strchr (sequence_info->middle_gap, c) == NULL)
    {
        s_ReportCharCommentError (sequence_info->middle_gap, c, "GAP",
                                errfunc, errdata);
    }
 
    c = GetNexusTypechar (cp + 7, "match");
    if (c == 0) {
        c = GetNexusTypechar (cp + 7, "MATCH");
    }
    if (c != 0  &&  sequence_info->match != NULL
        &&  strchr (sequence_info->match, c) == NULL)
    {
        s_ReportCharCommentError (sequence_info->match, c, "MATCH",
                                errfunc, errdata);
    }
    return eTrue;
} 


static char * s_ReplaceNexusTypeChar (char *str, char c)
{
    if (str == NULL
        || c != *str 
        || *(str + 1) != 0)
    {
        if (str != NULL)
        {
          free (str);
        }
        str = (char *)malloc (2 * sizeof (char));
        if (str != NULL)
        {
          str [0] = c;
          str [1] = 0;
        }
    }
    return str;
}

/* This function reads a Nexus-style comment line for the characters 
 * specified for missing, match, and gap and sets those values in sequence_info.
 * The function returns eTrue if a Nexus comment was found, eFalse otherwise.
 */ 
static EBool s_UpdateNexusCharInfo 
(char *               str,
 TSequenceInfoPtr     sequence_info)
{
    char * cp;
    char   c;

    if (str == NULL  ||  sequence_info == NULL) {
        return eFalse;
    }

    cp = strstr (str, "format ");
    if (cp == NULL) {
        cp = strstr (str, "FORMAT ");
    }
    if (cp == NULL) {
        return eFalse;
    }

    c = GetNexusTypechar (cp + 7, "missing");
    if (c == 0) {
        c = GetNexusTypechar (cp + 7, "MISSING");
    }
    sequence_info->missing = s_ReplaceNexusTypeChar (sequence_info->missing, c);
    
    c = GetNexusTypechar (cp + 7, "gap");
    if (c == 0) {
        c = GetNexusTypechar (cp + 7, "GAP");
    }
    sequence_info->beginning_gap = s_ReplaceNexusTypeChar (sequence_info->beginning_gap, c);
    sequence_info->middle_gap = s_ReplaceNexusTypeChar (sequence_info->middle_gap, c);
    sequence_info->end_gap = s_ReplaceNexusTypeChar (sequence_info->end_gap, c);
 
    c = GetNexusTypechar (cp + 7, "match");
    if (c == 0) {
        c = GetNexusTypechar (cp + 7, "MATCH");
    }
    sequence_info->match = s_ReplaceNexusTypeChar (sequence_info->match, c);

    return eTrue;
} 


/* This function examines the string str to see if it consists entirely of
 * asterisks, colons, periods, and whitespace.  If so, this line is assumed
 * to be a Clustal-style consensus line and the function returns eTrue.
 * otherwise the function returns false;
 */
static EBool s_IsConsensusLine (char * str)
{
    if (str == NULL 
        ||  strspn (str, "*:. \t\r\n") < strlen (str)
        ||  (strchr (str, '*') == NULL 
             &&  strchr (str, ':') == NULL
             &&  strchr (str, '.') == NULL)) {
        return eFalse;
    } else {
        return eTrue;
    } 
}


/* This function identifies lines that begin with a NEXUS keyword and end
 * with a semicolon - they will not contain sequence data.  The function
 * returns eTrue if the line contains only a NEXUS comment, eFalse otherwise.
 */
static EBool s_SkippableNexusComment (char *str)
{
    char * last_semicolon;

    if (str == NULL) {
        return eFalse;
    }
    last_semicolon = strrchr (str, ';');
    if (last_semicolon == NULL
        ||  strspn (last_semicolon + 1, " \t\r") != strlen (last_semicolon + 1)
        ||  strchr (str, ';') != last_semicolon) {
        return eFalse;
    }
    if (s_StringNICmp (str, "format ", 7) == 0
        ||  s_StringNICmp (str, "dimensions ", 11) == 0
        ||  s_StringNICmp (str, "options ", 8) == 0
        ||  s_StringNICmp (str, "begin characters", 16) == 0
        ||  s_StringNICmp (str, "begin data", 10) == 0
        ||  s_StringNICmp (str, "begin ncbi", 10) == 0) {
        return eTrue;
    } else {
        return eFalse;
    }
}


static EBool s_IsOnlyNumbersAndSpaces (char *str)
{
    if (str == NULL) {
        return eFalse;
    }

    while (*str != 0) {
        if (!isspace (*str) && !isdigit(*str)) {
            return eFalse;
        }
        ++str;
    }
    return eTrue;
}


/* This function determines whether the contents of str are "skippable"
 * in that they do not contain sequence data and therefore should not be
 * considered part of any block patterns or sequence data.
 */
static EBool s_SkippableString (char * str)
{
    if (str == NULL
        ||  s_StringNICmp (str, "matrix", 6) == 0
        ||  s_StringNICmp (str, "sequin", 6) == 0
        ||  s_StringNICmp (str, "#NEXUS", 6) == 0
        ||  s_StringNICmp (str, "CLUSTAL W", 9) == 0
        ||  s_SkippableNexusComment (str)
        ||  s_IsTwoNumbersSeparatedBySpace (str)
        ||  s_IsOnlyNumbersAndSpaces (str)
        ||  s_IsConsensusLine (str)
        ||  str [0] == ';') {
        return eTrue;
    } else {
        return eFalse;
    } 
}


/* This function determines whether str contains a indication
 * that this is real alignment format (nexus, clustal, etc.)
 */
static EBool s_IsAlnFormatString (char * str)
{
    if (s_StringNICmp (str, "matrix", 6) == 0
        ||  s_StringNICmp (str, "#NEXUS", 6) == 0
        ||  s_StringNICmp (str, "CLUSTAL W", 9) == 0
        ||  s_SkippableNexusComment (str)
        ||  s_IsTwoNumbersSeparatedBySpace (str)
        ||  s_IsConsensusLine (str)) {
        return eTrue;
    } else {
        return eFalse;
    }
}


/* This function determines whether or not str contains a blank line.
 */
static EBool s_IsBlank (char * str)
{
    size_t len;

    if (str == NULL) {
        return eTrue;
    }
    len = strspn (str, " \t\r");
    if (len == strlen (str)) {
        return eTrue;
    }
    return eFalse;
}


/* This function determines whether or not linestring contains a line
 * indicating the end of sequence data (organism information and definition
 * lines may occur after this line).
 */
static EBool s_FoundStopLine (char * linestring)
{
    if (linestring == NULL) {
        return eFalse;
    }
    if (s_StringNICmp (linestring, "endblock", 8) == 0
        ||  s_StringNICmp (linestring, "end;", 4) == 0) {
        return eTrue;
    }
    return eFalse;
}


/* This function identifies the beginning line of an ASN.1 file, which
 * cannot be read by the alignment reader.
 */
static EBool s_IsASN1 (char * linestring)
{
    if (linestring != NULL  &&  strstr (linestring, "::=") != NULL) {
        return eTrue;
    } else {
        return eFalse;
    }
}


/* The following functions are used to locate and read comments enclosed
 * in brackets.  These comments sometimes include organism information.
 */

/* This function frees memory associated with a SCommentLoc structure. */
static void s_CommentLocFree (TCommentLocPtr clp)
{
    if (clp == NULL) {
        return;
    }
    s_CommentLocFree (clp->next);
    free (clp);
}


/* This function finds the first comment enclosed in brackets and creates
 * a SCommentLoc structure to indicate the position of the comment
 * in the string.  The function returns a pointer to this structure if a
 * comment is found or a NULL if the string does not contain a bracketed 
 * comment.
 */
static TCommentLocPtr s_FindComment (char * string)
{
    char *         cp_start;
    char *         cp_end;
    TCommentLocPtr clp;

    if (string == NULL) {
        return NULL;
    }
    cp_start = strstr (string, "[");
    if (cp_start != NULL) {
        cp_end = strstr (cp_start, "]");
        if (cp_end != NULL) {
            clp = (TCommentLocPtr) malloc (sizeof (SCommentLoc));
            if (clp == NULL) {
                return NULL;
            }
            clp->start = cp_start;
            clp->end = cp_end;
            clp->next = NULL;
            return clp;
        }
    }
    return NULL;
}


/* This function removes a comment from a line. */
static void s_RemoveCommentFromLine (char * linestring)
{
    TCommentLocPtr clp;

    if (linestring == NULL) {
        return;
    }

    clp = s_FindComment (linestring);
    while (clp != NULL) {
        strcpy (clp->start, clp->end + 1);
        s_CommentLocFree (clp);
        clp = s_FindComment (linestring);
    }

    /* if we have read an organism comment and that's all there was on the
     * line, get rid of the arrow character as well so it doesn't end up 
     * in the sequence data
     */
    if ( linestring [0] == '>'  &&  linestring [1] == 0) {
        linestring [0] = 0;
    }

    /* if the line now contains only space, truncate it */
    if (strspn (linestring, " \t\r") == strlen (linestring)) {
        linestring [0] = 0;
    }
    
}


/* This function determines whether or not a comment describes an organism
 * by looking for org= or organism= inside the brackets.
 */
static EBool s_IsOrganismComment (TCommentLocPtr clp)
{
    int    len;
    char * cp;
    char * cp_end;

    if (clp == NULL  ||  clp->start == NULL  ||  clp->end == NULL) {
        return eFalse;
    }
 
    cp = clp->start;
    if (*cp != '[') {
        return eFalse;
    }
    cp ++;
    len = strspn ( clp->start, " \t\r");
    cp = cp + len;
    cp_end = strstr (cp, "=");
    if (cp_end == NULL) {
        return eFalse;
    }
    cp_end --;
    while (cp_end > cp  &&  isspace ((unsigned char)*cp_end)) {
      cp_end --;
    }
    cp_end ++;
    if ((cp_end - cp == 3  &&  s_StringNICmp (cp, "org", 3) == 0)
        ||  (cp_end - cp == 8  &&  s_StringNICmp (cp, "organism", 8) == 0)) {
        return eTrue;
    }
    return eFalse;
}


/* This function finds an organism comment, which includes the first bracketed
 * comment with org= or organism=, plus any additional bracketed comments
 * separated only by whitespace from the org= or organism= comment.
 * The function returns a pointer to a SCommentLoc structure describing
 * the location of the organism comment.
 */
static TCommentLocPtr s_FindOrganismComment (char * string)
{
    TCommentLocPtr clp, next_clp;

    if (string == NULL) {
        return NULL;
    }

    clp = s_FindComment (string);
    while (clp != NULL  &&  ! s_IsOrganismComment (clp)) {
        clp = s_FindComment (clp->end);
    }

    if (clp == NULL) {
        return NULL;
    }

    next_clp = s_FindComment (clp->end);
    while (next_clp != NULL  && 
        (int) strspn (clp->end + 1, " \t\r") == next_clp->start - clp->end - 1
        &&  ! s_IsOrganismComment (next_clp))
    {
        clp->end = next_clp->end;
        next_clp = s_FindComment (clp->end);
    }
    return clp;
}


/* This function removes an organism comment from a line. */
static void s_RemoveOrganismCommentFromLine (char * string)
{
    TCommentLocPtr clp;

    while ((clp = s_FindOrganismComment (string)) != NULL) {
        strcpy (clp->start, clp->end + 1);
        s_CommentLocFree (clp);
    }
}

 
/* This function creates an ordered list of comments within an organism
 * comment and returns a pointer to the first item in the linked list.
 * In an ordered org name, the org= value appears first, followed by other
 * bracketed values in alphabetical order.
 */
static TCommentLocPtr s_CreateOrderedOrgCommentList (TCommentLocPtr org_clp)
{
    TCommentLocPtr clp, prev_clp, next_clp, clp_list, ordered_start;
    int           next_len, this_len, len;
  
    if (org_clp == NULL) {
        return NULL;
    }

    clp_list = s_FindComment (org_clp->start); /* this is the org= */
    prev_clp = NULL;
    ordered_start = s_FindComment (clp_list->end);
    if (s_IsOrganismComment (ordered_start))
    {
      s_CommentLocFree (ordered_start);
      ordered_start = NULL;
    }
    if (ordered_start == NULL) {
        return clp_list;
    }
    clp = s_FindComment (ordered_start->end);
    while (clp != NULL  &&  clp->start < org_clp->end) {
        /* insert new comment into list */
        prev_clp = NULL;
        next_clp = ordered_start;
        next_len = next_clp->end - next_clp->start;
        this_len = clp->end - clp->start;
        len = next_len > this_len ? next_len : this_len;
        while (next_clp != NULL
          &&  strncmp (next_clp->start, clp->start, len) < 0)
        {
            prev_clp = next_clp;
            next_clp = next_clp->next;
            if (next_clp != NULL) {
                next_len = next_clp->end - next_clp->start;
                len = next_len > this_len ? next_len : this_len;
            }
        }
        if (prev_clp == NULL) {
            clp->next = ordered_start;
            ordered_start = clp;
        } else {
            clp->next = prev_clp->next;
            prev_clp->next = clp;
        }
        clp = s_FindComment (clp->end);
    }
    clp_list->next = ordered_start;
    return clp_list;
}


/* This function creates an ordered organism name based on the bracketed
 * comments contained in the location described by org_clp.
 */
static char * s_CreateOrderedOrgName (TCommentLocPtr org_clp)
{
    TCommentLocPtr clp, clp_list;
    char *         ordered_org_name;
    char *         cp;

    if (org_clp == NULL) {
        return NULL;
    }

    ordered_org_name = (char *)malloc (org_clp->end - org_clp->start + 2);
    if (ordered_org_name == NULL) {
        return NULL;
    }
    ordered_org_name [0] = 0;
    clp_list = s_CreateOrderedOrgCommentList (org_clp);
    cp = ordered_org_name;
    for (clp = clp_list; clp != NULL; clp = clp->next) {
        strncpy (cp, clp->start, clp->end - clp->start + 1);
        cp += clp->end - clp->start + 1;
        *cp = 0;
    }
    
    s_CommentLocFree (clp_list);

    return ordered_org_name;
}

static void s_AddDeflineFromOrganismLine 
(char             *defline, 
 int              line_num,
 int              defline_offset,
 SAlignRawFilePtr afrp)
{
    TLineInfoPtr lip;
    int          org_num, defline_num, new_len;
    char         *empty_defline, *new_defline;
    
    if (afrp == NULL || defline == NULL) {
        return;
    }
    
    /* make sure that we are adding the definition line to the correct position
     * in the list - should match last organism name */
    lip = afrp->organisms;
    org_num = 0;
    while (lip != NULL)
    {
        org_num++;
        lip = lip->next;
    }
    
    lip = afrp->deflines;
    defline_num = 0;
    while (lip != NULL  &&  defline_num < org_num) {
        lip = lip->next;
        defline_num ++;
    }
    
    if (defline_num == org_num) {
        /* if previous defline is empty, replace with new defline */
        if (strlen (lip->data) == 0)
        {
            free (lip->data);
            lip->data = defline;
        }
        else
        {
            /* append defline to the end of the existing entry */
            new_len = strlen (lip->data) + strlen (defline) + 2;
            new_defline = (char *) malloc (new_len * sizeof (char));
            if (new_defline != NULL)
            {
                strcpy (new_defline, lip->data);
                strcat (new_defline, " ");
                strcat (new_defline, defline);
                free (lip->data);
                lip->data = new_defline;
                free (defline);
                defline = NULL;
            }
        }
        /* use new line numbers */
        lip->line_num = line_num + 1;
        lip->line_offset = defline_offset;
        lip->delete_me = eFalse;        
    }
    else
    {
        /* add empty deflines to get to the correct position */
        while (defline_num < org_num - 1)
        {
            empty_defline = (char *) malloc (sizeof (char));
            if (empty_defline != NULL)
            {
                *empty_defline = 0;
                afrp->deflines = s_AddLineInfo (afrp->deflines, 
                                                empty_defline, 0,
                                                0);
                afrp->num_deflines ++;
            }
            defline_num++;
        }
        /* now add new defline in correct position */
        afrp->deflines = s_AddLineInfo (afrp->deflines, defline, 
                                        line_num, defline_offset);
        afrp->num_deflines ++;
    }
}

/* This function is used to read any organism names that may appear in
 * string, including any modifiers that may appear after the organism name.
 */
static void s_ReadOrgNamesFromText 
(char *           string,
 int              line_num,
 SAlignRawFilePtr afrp)
{
    TCommentLocPtr clp;
    char *         org_name;
    char *         cp;
    char *         defline;
    char *         comment_end;
    int            defline_offset;
  
    if (string == NULL  ||  afrp == NULL) {
        return;
    }

    clp = s_FindOrganismComment (string);
    if (clp == NULL && (strstr (string, "org=") != NULL || strstr (string, "organism=") != NULL))
    {
      s_ReportOrgCommentError (string, afrp->report_error, afrp->report_error_userdata);
    }
    while (clp != NULL) {
        org_name = s_CreateOrderedOrgName (clp);
        afrp->organisms = s_AddLineInfo (afrp->organisms, org_name, line_num,
                                       clp->start - string);
        free (org_name);
        afrp->num_organisms ++;
        defline = NULL;
        defline_offset = 0;
        if (*clp->end != 0) {
            cp = clp->end + 1;
            cp += strspn (cp, " \t\r\n");
            if (*cp != 0) {
                defline = clp->end + 1;
                defline_offset = clp->end - string + 1;
            }
        }
        s_AddDeflineFromOrganismLine (defline, line_num, defline_offset, afrp);
                                      
        comment_end = clp->end;
        s_CommentLocFree (clp);
        clp = s_FindOrganismComment (comment_end);
    }
}


/* The following group of functions manages the SAlignRawSeq structure,
 * which is used to track the IDs of sequences in the file, the sequence
 * characters for those IDs, and the locations of the IDs and sequence
 * characters.
 */

/* This function allocates memory for an SAlignRawSeq structure,
 * initializes its member variables, and returns a pointer to the newly
 * allocated structure.
 */
static TAlignRawSeqPtr s_AlignRawSeqNew (TAlignRawSeqPtr list)
{
    TAlignRawSeqPtr arsp, last;

    arsp = (TAlignRawSeqPtr)malloc (sizeof (SAlignRawSeq));
    if (arsp == NULL) {
        return NULL;
    }
    arsp->id            = NULL;
    arsp->sequence_data = NULL;
    arsp->id_lines      = NULL;
    arsp->next          = NULL;

    last = list;
    while (last != NULL && last->next != NULL) {
        last = last->next;
    }
    if (last != NULL) {
        last->next = arsp;
    }
    return arsp;
}


/* This function frees the memory associated with an SAlignRawSeq
 * structure's member variables and with the structure itself.
 */
static void s_AlignRawSeqFree (TAlignRawSeqPtr arsp)
{
    if (arsp == NULL) {
        return;
    }
    s_AlignRawSeqFree (arsp->next);
    free (arsp->id);
    s_LineInfoFree (arsp->sequence_data);
    s_IntLinkFree (arsp->id_lines);
    free (arsp);
}


/* This function returns a pointer to the sequence in list with the specified
 * ID, unless there is no such sequence, in which case the function returns
 * NULL.
 */
static TAlignRawSeqPtr 
s_FindAlignRawSeqById 
(TAlignRawSeqPtr list,
 char *          id)
{
    TAlignRawSeqPtr arsp;

    for (arsp = list; arsp != NULL; arsp = arsp->next) {
        if (strcmp (arsp->id, id) == 0) {
            return arsp;
        }
    }
    return NULL;
}


/* This function finds the position of a given ID in the sequence list,
 * unless the ID is not found in the list, in which case the function returns
 * -1.
 */
static int  
s_FindAlignRawSeqOffsetById 
(TAlignRawSeqPtr list, 
 char *          id)
{
    TAlignRawSeqPtr arsp;
    int             offset;

    for (arsp = list, offset = 0; arsp != NULL; arsp = arsp->next, offset++) {
        if (strcmp (arsp->id, id) == 0) {
            return offset;
        }
    }
    return -1;
}


/* This function returns a pointer to the memory in which the ID for the
 * Nth sequence is stored, unless there aren't that many sequences, in which
 * case NULL is returned.
 */
static char * 
s_GetAlignRawSeqIDByOffset 
(TAlignRawSeqPtr list,
 int  offset)
{
    TAlignRawSeqPtr arsp;
    int            index;

    arsp = list;
    index = 0;
    while ( arsp != NULL  &&  index != offset ) {
        arsp = arsp->next;
        index++;
    }
    if (index == offset  &&  arsp != NULL) {
        return arsp->id;
    } else {
        return NULL;
    }
}


/* This function adds data to a sequence by looking for the specified ID in
 * the list.  If the id is not found, a new sequence with that ID is added to
 * the end of the list.
 * The function returns a pointer to the first item in the list.
 */
static TAlignRawSeqPtr
s_AddAlignRawSeqById
(TAlignRawSeqPtr list,
 char *  id,
 char *  data,
 int     id_line_num,
 int     data_line_num,
 int     data_line_offset)
{
    TAlignRawSeqPtr arsp;
    TIntLinkPtr     ilp;

    arsp = s_FindAlignRawSeqById (list, id);
    if (arsp == NULL) {
        arsp = s_AlignRawSeqNew (list);
        if (arsp == NULL) {
            return NULL;
        }
        if (list == NULL) list = arsp;
        arsp->id = strdup (id);
    }
    arsp->sequence_data = s_AddLineInfo (arsp->sequence_data,
                                       data,
                                       data_line_num,
                                       data_line_offset);
    ilp = s_IntLinkNew (id_line_num, arsp->id_lines);
    if (arsp->id_lines == NULL) arsp->id_lines = ilp;
    return list;
}


/* This function adds data to the Nth sequence in the sequence list and
 * returns eTrue, unless there aren't that many sequences in the list, in
 * which case the function returns eFalse.
 */
static EBool 
s_AddAlignRawSeqByIndex 
(TAlignRawSeqPtr list,
 int     index,
 char *  data,
 int     data_line_num,
 int     data_line_offset)
{
    TAlignRawSeqPtr arsp;
    int            curr;

    curr = 0;
    for (arsp = list; arsp != NULL  &&  curr < index; arsp = arsp->next) {
        curr++;
    }
    if (arsp == NULL) {
        return eFalse;
    } else {
        arsp->sequence_data = s_AddLineInfo (arsp->sequence_data,
                                           data,
                                           data_line_num,
                                           data_line_offset);
        return eTrue;
    }
}


/* This function frees memory associated with the SAlignRawFileData structure.
 */
static void s_AlignFileRawFree (SAlignRawFilePtr afrp)
{
    if (afrp == NULL) {
        return;
    }

    s_LineInfoFree (afrp->organisms);
    s_LineInfoFree (afrp->deflines);
    s_LineInfoFree (afrp->line_list);
    s_AlignRawSeqFree (afrp->sequences);
    s_IntLinkFree (afrp->offset_list);
    free (afrp->alphabet);
    free (afrp);
}


/* This function allocates memory for an SAlignRawFileData structure and
 * initializes its member variables.  The function returns a pointer to
 * the newly allocated structure.
 */
static SAlignRawFilePtr s_AlignFileRawNew (void)
{
    SAlignRawFilePtr afrp;

    afrp = (SAlignRawFilePtr)malloc (sizeof (SAlignRawFileData));
    if (afrp == NULL) {
        return NULL;
    }
    afrp->marked_ids            = eFalse;
    afrp->line_list             = NULL;
    afrp->organisms             = NULL;
    afrp->num_organisms         = 0;
    afrp->deflines              = NULL;
    afrp->num_deflines          = 0;
    afrp->block_size            = 0;
    afrp->offset_list           = NULL;
    afrp->sequences             = NULL;
    afrp->report_error          = NULL;
    afrp->report_error_userdata = NULL;
    afrp->alphabet              = NULL;
    afrp->expected_num_sequence = 0;
    afrp->expected_sequence_len = 0;
    afrp->num_segments          = 1;
    afrp->align_format_found    = eFalse;
    return afrp;
}


/* The following functions are used to analyze the structure of a file and
 * assemble the sequences listed in the file.
 * Sequence data in a file is organized in one of two general formats - 
 * interleaved or contiguous.  Interleaved data can be recognized by looking
 * for repeated blocks of the same number of lines within a file separated
 * by blank or skippable lines from other lines in the file.  The first of
 * these blocks must have at least two elements separated by whitespace
 * in each line, the first of these elements is the ID for the sequence in
 * that row and for the sequences in that position within the block for the
 * remainder of the file.
 * Contiguous data can be recognized by either looking for "marked" sequence
 * IDs, which begin with a '>' character, or by looking for repeated patterns
 * of lines with the same numbers of characters.
 */

/* The following functions are used to analyze interleaved data. */

/* This function creates a SLengthListData structure that describes the pattern
 * of character lengths in the string pointed to by cp.
 */
static SLengthListPtr s_GetBlockPattern (char * cp)
{
    SLengthListPtr this_pattern;
    int           len;

    this_pattern = s_LengthListNew (NULL);
    if (this_pattern == NULL) {
        return NULL;
    }

    this_pattern->num_appearances = 1;
    while (*cp != 0) {
        len = strcspn (cp, " \t\r");
        s_AddLengthRepeat (this_pattern, len);
        cp += len;
        cp += strspn (cp, " \t\r");
    }
    return this_pattern;
}


/* This function attempts to predict whether the following lines will be
 * an interleaved block.  If so, the function returns the location of the
 * beginning of the block, otherwise the function returns -1.
 */
static int 
s_ForecastBlockPattern 
(SLengthListPtr pattern_list,
 TIntLinkPtr    next_offset,
 int            line_start,
 int            block_size)
{
    int  line_counter;
    SLengthListPtr llp;

    line_counter = line_start;
    if (next_offset != NULL
        &&  next_offset->ival - line_counter < block_size) {
        return -1;
    }
    
    for (llp = pattern_list;
         llp != NULL
           &&  (next_offset == NULL  ||  line_counter < next_offset->ival - 1)
           &&  line_counter - line_start < block_size;
         llp = llp->next)
    {
        if (llp->lengthrepeats == NULL) {
            return -1;
        }
        line_counter += llp->num_appearances;
    }
    if (line_counter - line_start == block_size) {
        /* we've found a combination of groups of similarly sized lines
         * that add up to the desired block size - is the next line blank,
         * or are there additional non-blank lines?
         */
        if (llp == NULL /* The block ended with the last line in the file */
            || llp->lengthrepeats == NULL) { /* or the next line is blank */
            return line_start;
        }
    }
    return -1;
}


/* This function looks for malformed blocks between the identified blocks
 * indicated by the offset_list.  It returns a pointer to the list with the
 * new locations inserted at the appropriate locations.
 */
static TIntLinkPtr
s_AugmentBlockPatternOffsetList
(SLengthListPtr pattern_list,
 TIntLinkPtr    offset_list,
 int            block_size)
{
    int            line_counter;
    SLengthListPtr llp;
    TIntLinkPtr    next_offset, prev_offset, new_offset;
    int            forecast_pos;

    prev_offset = NULL;
    next_offset = offset_list;
    line_counter = 0;
    llp = pattern_list;
    while (llp != NULL) {
        if (next_offset != NULL  &&  line_counter == next_offset->ival) {
            prev_offset = next_offset;
            next_offset = next_offset->next;
            /* skip past the lines for this block */
            while (line_counter - prev_offset->ival < block_size
                   &&  llp != NULL)
            {
                line_counter += llp->num_appearances;
                llp = llp->next;
            }
        } else {
            forecast_pos = s_ForecastBlockPattern (llp, next_offset,
                                                 line_counter,
                                                 block_size);
            if (forecast_pos > 0) {
                new_offset = s_IntLinkNew (forecast_pos, NULL);
                if (new_offset == NULL) {
                    return NULL;
                }
                if (prev_offset == NULL) {
                    new_offset->next = offset_list;
                    offset_list = new_offset;
                } else {
                    new_offset->next = next_offset;
                    prev_offset->next = new_offset;
                }
                prev_offset = new_offset;
                /* skip past the lines for this block */
                while (line_counter - prev_offset->ival < block_size
                       &&  llp != NULL)
                {
                    line_counter += llp->num_appearances;
                    llp = llp->next;
                }
            } else {
                line_counter += llp->num_appearances;
                llp = llp->next;
            }
        }
    }
    return offset_list;
}


/* This function looks for lines that could not be assigned to an interleaved
 * block.  It returns eTrue if it finds any such lines after the first offset,
 * eFalse otherwise, and reports all instances of unused lines as errors.
 */
static EBool
s_FindUnusedLines 
(SLengthListPtr pattern_list,
 SAlignRawFilePtr afrp)
{
    TIntLinkPtr    offset;
    SLengthListPtr llp;
    int            line_counter;
    int            block_line_counter;
    EBool          rval = eFalse;
    TLineInfoPtr   line_val;
    int            skip;

    if (pattern_list == NULL  ||  afrp == NULL
        ||  afrp->offset_list == NULL  ||  afrp->block_size < 2) {
        return eFalse;
    }
    
    offset = afrp->offset_list;
    llp = pattern_list;
    line_counter = 0;
    line_val = afrp->line_list;
 
    while (llp != NULL  &&  line_val != NULL) {
        while (llp != NULL  &&  line_val != NULL
               &&  (offset == NULL  ||  line_counter < offset->ival)) {
            if (llp->lengthrepeats != NULL) {
                s_ReportUnusedLine (line_counter,
                                    line_counter + llp->num_appearances - 1,
                                    line_val,
                                    afrp->report_error,
                                    afrp->report_error_userdata);
                if (offset != afrp->offset_list) {
                    rval = eTrue;
                }
            }
            line_counter += llp->num_appearances;
            for (skip = 0;
                 skip < llp->num_appearances  &&  line_val != NULL;
                 skip++) {
                line_val = line_val->next;
            }
            llp = llp->next;
        }
        block_line_counter = 0;
        while (block_line_counter < afrp->block_size  &&  llp != NULL) {
            block_line_counter += llp->num_appearances;
            line_counter += llp->num_appearances;
            for (skip = 0;
                 skip < llp->num_appearances  &&  line_val != NULL;
                 skip++) {
                line_val = line_val->next;
            }
            llp = llp->next;
        }
        if (offset != NULL) {
            offset = offset->next;
        }
    }
    return rval;
}


/* This function examines a list of line lengths, looking for interleaved
 * blocks.  If it finds them, it will set the SAlignRawFileData offset_list
 * member variable to point to a list of locations for the blocks.
 */
static void
s_FindInterleavedBlocks 
(SLengthListPtr pattern_list,
 SAlignRawFilePtr afrp)
{
    SLengthListPtr llp, llp_next;
    TSizeInfoPtr   size_list, best_ptr;
    TIntLinkPtr    new_offset;
    int            line_counter;

    afrp->block_size = 0;
    size_list = NULL;
    afrp->offset_list = NULL;
    for (llp = pattern_list; llp != NULL; llp = llp->next) {
        llp_next = llp->next;
        if (llp->num_appearances > 1 
            &&  (llp_next == NULL  ||  llp_next->lengthrepeats == NULL)) {
            size_list = s_AddSizeInfo (size_list, llp->num_appearances);
        }
    }
    best_ptr = s_GetMostPopularSizeInfo (size_list);
    if (best_ptr != NULL  
        &&  (best_ptr->num_appearances > 1  ||  
             (size_list->next == NULL  &&  size_list->size_value > 1))) {
        afrp->block_size = best_ptr->size_value;
        line_counter = 0;
        for (llp = pattern_list; llp != NULL; llp = llp->next) {
            llp_next = llp->next;
            if (llp->num_appearances == afrp->block_size
                &&  (llp_next == NULL  ||  llp_next->lengthrepeats == NULL))
            {
                new_offset = s_IntLinkNew (line_counter, afrp->offset_list);
                if (new_offset == NULL) {
                    return;
                }
                if (afrp->offset_list == NULL) afrp->offset_list = new_offset;
            }
            line_counter += llp->num_appearances;
        }
        afrp->offset_list = s_AugmentBlockPatternOffsetList (pattern_list,
                                                           afrp->offset_list, 
                                                           afrp->block_size);
    }
    if (s_FindUnusedLines (pattern_list, afrp)) {
        s_IntLinkFree (afrp->offset_list);
        afrp->offset_list = NULL;
        afrp->block_size = 0;
    } else {
        afrp->align_format_found = eTrue;
    }
    s_SizeInfoFree (size_list);
    
}

static void s_TrimSpace(char** ppline)
{
    int len = 0;
    char* ptmp = 0;

    if (ppline == NULL  ||  *ppline == NULL) {
        return;
    }
    len = strlen (*ppline);
    ptmp = *ppline + len - 1;
    while (ptmp > *ppline  &&  (*ptmp == ' ' || *ptmp == '\t' || *ptmp == '\r' || *ptmp == '\n'))
    {
  	    *ptmp = 0;
  	    ptmp--;
    }
    len = strspn (*ppline, " \t\r\n");
    if (len > 0) {
        ptmp = *ppline;
        *ppline = strdup(*ppline + len);
        free(ptmp);
    }
}

static EBool
s_AfrpInitLineData(
    SAlignRawFilePtr afrp,
    FReadLineFunction readfunc,
    void* pfile)
{
    int overall_line_count = 0;
    EBool in_taxa_comment = eFalse;
    char* linestring = readfunc (pfile);
    TLineInfoPtr last_line = NULL, next_line = NULL;

    if (s_IsASN1 (linestring)) {
        s_ReportASN1Error (afrp->report_error, afrp->report_error_userdata);
        s_AlignFileRawFree (afrp);
        return eFalse;
    }

    while (linestring != NULL  &&  linestring [0] != EOF) {
        s_TrimSpace (&linestring);
        if (!in_taxa_comment  &&  s_FoundStopLine(linestring)) {
            linestring [0] = 0;
        }
        if (in_taxa_comment) {
            if (strncmp (linestring, "end;", 4) == 0) {
                in_taxa_comment = eFalse;
            } 
            linestring [0] = 0;
        } else if (strncmp (linestring, "begin taxa;", 11) == 0) {
            linestring [0] = 0;
            in_taxa_comment = eTrue;
            afrp->align_format_found = eTrue;
        }
        next_line = s_LineInfoNew (linestring, overall_line_count, 0);
        if (last_line == NULL) {
            afrp->line_list = next_line;
        } else {
            last_line->next = next_line;
        }
        last_line = next_line;

        free (linestring);
        linestring = readfunc (pfile);
        overall_line_count ++;
    }
    return eTrue;
}

static void
s_AfrpProcessFastaGap(
    SAlignRawFilePtr afrp,
    SLengthListPtr patterns,
    char* linestr,
    int overall_line_count)
{
    static EBool last_line_was_marked_id = eFalse;
    static SLengthListPtr last_pattern = NULL;

    TIntLinkPtr new_offset = NULL;
    SLengthListPtr this_pattern = NULL;
    int len = 0;
    char* cp;

    /*  ID line
     */
    if (linestr [0] == '>') {
        /* this could be a block of organism lines in a
            * NEXUS file.  If there is no sequence data between
            * the lines, don't process this file for marked IDs.
            */
        if (last_line_was_marked_id)
        {
            afrp->marked_ids = eFalse;
//            eFormat = ALNFMT_UNKNOWN;
        }
        else
        {
            afrp->marked_ids = eTrue;
//            eFormat = ALNFMT_FASTAGAP;
        }
        new_offset = s_IntLinkNew (overall_line_count + 1,
                                    afrp->offset_list);
        if (afrp->offset_list == NULL) afrp->offset_list = new_offset;
        last_line_was_marked_id = eTrue;
        return;
    }

    /*  Data line
     */
    last_line_was_marked_id = eFalse;
    /* add to length list for interleaved block search */
    len = strcspn (linestr, " \t\r");
    if (len > 0) {
        cp = linestr + len;
        len = strspn (cp, " \t\r");
        if (len > 0) {
            cp = cp + len;
        }
        if (*cp == 0) {
            this_pattern = s_GetBlockPattern (linestr);
        } else {
            this_pattern = s_GetBlockPattern (cp);
        }                    
    } else {
        this_pattern = s_GetBlockPattern (linestr);
    }
            
    if (last_pattern == NULL) {
        patterns = this_pattern;
        last_pattern = this_pattern;
    } else if (s_DoLengthPatternsMatch (last_pattern, this_pattern)) {
        last_pattern->num_appearances ++;
        s_LengthListFree (this_pattern);
    } else {
        last_pattern->next = this_pattern;
        last_pattern = this_pattern;
    }
}

static SAlignRawFilePtr
s_ReadAlignFileRaw
(FReadLineFunction    readfunc,
 void *               userdata,
 TSequenceInfoPtr     sequence_info,
 EBool                use_nexus_file_info,
 FReportErrorFunction errfunc,
 void *               errdata,
 EAlignFormat*        pformat)
{
    char *                   linestring;
    SAlignRawFilePtr         afrp;
    int                      overall_line_count;
    EBool                    found_expected_ntax = eFalse;
    EBool                    found_expected_nchar = eFalse;
    EBool                    found_char_comment = eFalse;
    SLengthListPtr           pattern_list = NULL;
    SLengthListPtr           this_pattern, last_pattern = NULL;
    char *                   cp;
    int                      len;
    TIntLinkPtr              new_offset;
    EBool                    in_bracketed_comment = eFalse;
    TBracketedCommentListPtr comment_list = NULL, last_comment = NULL;
    EBool                    last_line_was_marked_id = eFalse;
    TLineInfoPtr             next_line;

    if (readfunc == NULL  ||  sequence_info == NULL) {
        return NULL;
    }

    afrp = s_AlignFileRawNew ();
    if (afrp == NULL) {
        return NULL;
    }
  
    afrp->alphabet = strdup (sequence_info->alphabet);
    afrp->report_error = errfunc;
    afrp->report_error_userdata = errdata;

    if (eFalse == s_AfrpInitLineData(afrp, readfunc, userdata)) {
        s_AlignFileRawFree (afrp);
        return NULL;
    }
        
    for (next_line = afrp->line_list; next_line != NULL; next_line = next_line->next) {
        linestring = next_line->data;
        overall_line_count = next_line->line_num-1;

        s_ReadOrgNamesFromText (linestring, overall_line_count, afrp);
        if (*pformat == ALNFMT_FASTAGAP) {
            s_AfrpProcessFastaGap(afrp, pattern_list, linestring, overall_line_count);
            continue;
        }
        /* we want to remove the comment from the line for the purpose 
         * of looking for blank lines and skipping,
         * but save comments for storing in array if line is not skippable or
         * blank
         */ 
 
        if (! found_expected_ntax  ||  ! found_expected_nchar) {
            if (s_IsTwoNumbersSeparatedBySpace (linestring)) {
                s_GetFASTAExpectedNumbers (linestring, afrp);
                found_expected_ntax = eTrue;
                found_expected_nchar = eTrue;
                afrp->align_format_found = eTrue;
           } else {
                s_GetNexusSizeComments (linestring, &found_expected_ntax,
                                        &found_expected_nchar, afrp);
            }
        }
        if (! found_char_comment) {
          if (use_nexus_file_info) {
            found_char_comment = s_UpdateNexusCharInfo (linestring, sequence_info);
          } else {
            found_char_comment = s_CheckNexusCharInfo (linestring, sequence_info, 
                                                       afrp->report_error,
                                                       afrp->report_error_userdata);
          }
        }

        /* remove complete single-line bracketed comments from line 
         *before checking for multiline bracketed comments */
        s_RemoveCommentFromLine (linestring);

        if (in_bracketed_comment) {
            len = strspn (linestring, " \t\r\n");
            if (last_comment != NULL) 
            {
            	s_BracketedCommentListAddLine (last_comment, linestring + len,
            	                               overall_line_count, len);
            }
            if (strchr (linestring, ']') != NULL) {
                in_bracketed_comment = eFalse;
            }
            linestring [0] = 0;
        } else if (linestring [0] == '[' && strchr (linestring, ']') == NULL) {
            in_bracketed_comment = eTrue;
            len = strspn (linestring, " \t\r\n");
            last_comment = s_BracketedCommentListNew (comment_list,
                                                      linestring + len,
                                                      overall_line_count, len);
            if (comment_list == NULL) 
            {
            	comment_list = last_comment;
            }
            linestring [0] = 0;
        }

        if (!afrp->align_format_found) {
            afrp->align_format_found = s_IsAlnFormatString (linestring);
        }                  
        if (s_SkippableString (linestring)) {
            linestring[0] = 0;
        }

        /*  "junk" line: Just record the empty pattern to keep line counts in sync.
         */
        if (linestring[0] == 0) {
            last_line_was_marked_id = eFalse;
            this_pattern = s_GetBlockPattern ("");
            if (pattern_list == NULL) {
                pattern_list = this_pattern;
                last_pattern = this_pattern;
            } else {
                last_pattern->next = this_pattern;
                last_pattern = this_pattern;
            }
            continue;
        }

        /* Presumably fasta ID:
         */
        if (linestring [0] == '>') {
            /* this could be a block of organism lines in a
             * NEXUS file.  If there is no sequence data between
             * the lines, don't process this file for marked IDs.
             */
            if (last_line_was_marked_id)
            {
                afrp->marked_ids = eFalse;
                *pformat = ALNFMT_UNKNOWN;
            }
            else
            {
                *pformat = ALNFMT_FASTAGAP;
                s_AfrpProcessFastaGap(afrp, pattern_list, linestring, overall_line_count);
                continue;
            }
            new_offset = s_IntLinkNew (overall_line_count + 1,
                                      afrp->offset_list);
            if (afrp->offset_list == NULL) afrp->offset_list = new_offset;
            last_line_was_marked_id = eTrue;
            continue;
        }

        /* default case: some real data at last ...
         */
        last_line_was_marked_id = eFalse;
        /* add to length list for interleaved block search */
        len = strcspn (linestring, " \t\r");
        if (len > 0) {
            cp = linestring + len;
            len = strspn (cp, " \t\r");
            if (len > 0) {
                cp = cp + len;
            }
            if (*cp == 0) {
                this_pattern = s_GetBlockPattern (linestring);
            } else {
                this_pattern = s_GetBlockPattern (cp);
            }                    
        } else {
            this_pattern = s_GetBlockPattern (linestring);
        }
        
        if (pattern_list == NULL) {
            pattern_list = this_pattern;
            last_pattern = this_pattern;
        } else if (s_DoLengthPatternsMatch (last_pattern, this_pattern)) {
            last_pattern->num_appearances ++;
            s_LengthListFree (this_pattern);
        } else {
            last_pattern->next = this_pattern;
            last_pattern = this_pattern;
        }
    }
    afrp->num_segments = s_GetNumSegmentsInAlignment (comment_list, errfunc, errdata);
    if (afrp->num_segments > 1) 
    {
        if (afrp->offset_list != NULL)
        {
        	s_ReportSegmentedAlignmentError (afrp->offset_list,
        	                                 errfunc, errdata);
            s_AlignFileRawFree (afrp);
            s_LengthListFree (pattern_list);
            s_BracketedCommentListFree (comment_list);
            return NULL;        	
        }
        else
        {
    	    afrp->offset_list = GetSegmentOffsetList (comment_list);
    	    afrp->marked_ids = eTrue;
        }
    }
    if (! afrp->marked_ids) {
        s_FindInterleavedBlocks (pattern_list, afrp);
    }
    s_LengthListFree (pattern_list);
    s_BracketedCommentListFree (comment_list);
    return afrp;
}


/* This function analyzes a block to see if it contains, as the first element
 * of any of its lines, one of the sequence IDs already identified.  If the
 * one of the lines does begin with a sequence ID, all of the lines are
 * assumed to begin with sequence IDs and the function returns eTrue, otherwise
 * the function returns eFalse.
 */
static EBool 
s_DoesBlockHaveIds 
(SAlignRawFilePtr afrp, 
 TLineInfoPtr     first_line,
 int             num_lines_in_block)
{
    TLineInfoPtr    lip;
    char *          linestring;
    char *          this_id;
    TAlignRawSeqPtr arsp;
    size_t          len;
    int             block_offset;

    if (afrp->sequences == NULL) {
         return eTrue;
    }

    for (lip = first_line, block_offset = 0;
         lip != NULL  &&  block_offset < num_lines_in_block;
         lip = lip->next, block_offset++)
    {
        linestring = lip->data;
        if (linestring != NULL) {
            len = strcspn (linestring, " \t\r");
            if (len > 0  &&  len < strlen (linestring)) {
                this_id = (char *) malloc (len + 1);
                if (this_id == NULL) {
                    return eFalse;
                }
                strncpy (this_id, linestring, len);
                this_id [len] = 0;
                arsp = s_FindAlignRawSeqById (afrp->sequences, this_id);
                free (this_id);
                if (arsp != NULL) {
                    return eTrue;
                }
            }
        }
    }
    return eFalse;
}


/* This function analyzes the lines of the block to see if the pattern of
 * the lengths of the whitespace-separated pieces of sequence data matches
 * for all lines within the block.  The function returns eTrue if this is so,
 * otherwise the function returns eFalse.
 */
static EBool 
s_BlockIsConsistent
(SAlignRawFilePtr afrp,
 TLineInfoPtr     first_line,
 int              num_lines_in_block,
 EBool            has_ids,
 EBool            first_block)
{
    TLineInfoPtr   lip;
    SLengthListPtr list, this_pattern, best;
    int            len, block_offset, id_offset;
    char *         tmp_id;
    EBool          rval;
    char *         cp;

    rval = eTrue;
    list = NULL;
    for (lip = first_line, block_offset = 0;
         lip != NULL  &&  block_offset < num_lines_in_block;
         lip = lip->next, block_offset ++)
    {
        cp = lip->data;
        if (has_ids) {
            len = strcspn (cp, " \t\r");
            if (first_block && len == strlen (cp)) {
                /* PHYLIP IDs are exactly 10 characters long
                 * and may not have a space between the ID and
                 * the sequence.
                 */
                len = 10;
            }
            tmp_id = (char *) malloc ( (len + 1) * sizeof (char));
            if (tmp_id == NULL) {
                return eFalse;
            }
            strncpy (tmp_id, cp, len);
            tmp_id [len] = 0;
            id_offset = s_FindAlignRawSeqOffsetById (afrp->sequences, tmp_id);
            if (id_offset != block_offset  &&  ! first_block) {
                rval = eFalse;
                s_ReportInconsistentID (tmp_id, lip->line_num,
                                      afrp->report_error,
                                      afrp->report_error_userdata);
            }
            free (tmp_id);
            cp += len;
            cp += strspn (cp, " \t\r");
        }
        this_pattern = s_GetBlockPattern (cp);
        list = s_AddLengthList (list, this_pattern);
    }

    /* Now find the pattern with the most appearances */
    best = NULL;
    for (this_pattern = list;
         this_pattern != NULL;
         this_pattern = this_pattern->next)
    {
        if (this_pattern->num_appearances == 0) continue;
        if (best == NULL 
          ||  this_pattern->num_appearances > best->num_appearances)
        {
            best = this_pattern;
        }
    }

    /* now identify and report inconsistent lines */
    for (lip = first_line, block_offset = 0;
         lip != NULL  &&  block_offset < num_lines_in_block;
         lip = lip->next, block_offset ++)
    {
        cp = lip->data;
        if (has_ids) {
            len = strcspn (cp, " \t\r");
            if (first_block && len == strlen (cp)) {
                /* PHYLIP IDs are exactly 10 characters long
                 * and may not have a space between the ID and
                 * the sequence.
                 */
                len = 10;
            }        
            tmp_id = (char *) malloc ( (len + 1) * sizeof (char));
            if (tmp_id == NULL) {
                return eFalse;
            }
            strncpy (tmp_id, cp, len);
            tmp_id [len] = 0;
            cp += len;
            cp += strspn (cp, " \t\r");
        } else {
            tmp_id = s_GetAlignRawSeqIDByOffset (afrp->sequences, block_offset);
        }
        this_pattern = s_GetBlockPattern (cp);
        if ( ! s_DoLengthPatternsMatch (this_pattern, best)) {
            rval = eFalse;
            s_ReportInconsistentBlockLine (tmp_id, lip->line_num,
                                         afrp->report_error,
                                         afrp->report_error_userdata);
        }
        s_LengthListFree (this_pattern);
        if (has_ids) {
            free (tmp_id);
        }
    }
    s_LengthListFree (list);
    return rval;
}


/* This function processes a block of lines and adds the sequence data from
 * each line in the block to the appropriate sequence in the list.
 */
static void 
s_ProcessBlockLines 
(SAlignRawFilePtr afrp,
 TLineInfoPtr     lines,
 int              num_lines_in_block,
 EBool            first_block)
{
    TLineInfoPtr    lip;
    char *          linestring;
    char *          cp;
    char *          this_id;
    int             len;
    int             line_number;
    EBool           this_block_has_ids;
    int             pos;
    TAlignRawSeqPtr arsp;

    this_block_has_ids = s_DoesBlockHaveIds (afrp, lines, num_lines_in_block);
    s_BlockIsConsistent (afrp, lines, num_lines_in_block, this_block_has_ids,
                       first_block);
    for (lip = lines, line_number = 0;
         lip != NULL  &&  line_number < num_lines_in_block;
         lip = lip->next, line_number ++)
    {
        linestring = lip->data;
        if (linestring != NULL) {
            pos = 0;
            if (this_block_has_ids) {
                len = strcspn (linestring, " \t\r");
                if (first_block && len == strlen (linestring)) {
                    /* PHYLIP IDs are exactly ten characters long,
                     * and may not have a space before the start of
                     * the sequence data.
                     */
                    len = 10;
                }
                this_id = (char *) malloc (len + 1);
                if (this_id == NULL) {
                    return;
                }
                strncpy (this_id, linestring, len);
                this_id [len] = 0;
                cp = linestring + len;
                pos += len;
                len = strspn (cp, " \t\r");
                cp += len;
                pos += len;
                /* Check for duplicate IDs in the first block */
                if (first_block)
                {
                  arsp = s_FindAlignRawSeqById (afrp->sequences, this_id);
                  if (arsp != NULL)
                  {
                    s_ReportDuplicateIDError (this_id, lip->line_num,
                                              afrp->report_error,
                                              afrp->report_error_userdata);
                  }
                }
                afrp->sequences = s_AddAlignRawSeqById (afrp->sequences,
                                                      this_id, cp,
                                                      lip->line_num,
                                                      lip->line_num,
                                           lip->line_offset + cp - linestring);
                free (this_id);
            } else {
                if (! s_AddAlignRawSeqByIndex (afrp->sequences, line_number,
                                             linestring, 
                                             lip->line_num, lip->line_offset))
                {
                    s_ReportBlockLengthError ("", lip->line_num,
                                            afrp->block_size,
                                            line_number,
                                            afrp->report_error,
                                            afrp->report_error_userdata);
                }
            }
        }
    }
}


/* This function removes comments from the lines of an interleaved block of
 * data.
 */
static void
s_RemoveCommentsFromBlock
(TLineInfoPtr first_line,
 int         num_lines_in_block)
{
    TLineInfoPtr lip;
    int         block_offset;

    for (lip = first_line, block_offset = 0;
         lip != NULL  &&  block_offset < num_lines_in_block;
         lip = lip->next)
    {                   
        s_RemoveCommentFromLine (lip->data);
    }
}


/* This function processes the interleaved block of data found at each
 * location listed in afrp->offset_list.
 */
static void s_ProcessAlignRawFileByBlockOffsets (SAlignRawFilePtr afrp)
{
    int           line_counter;
    TIntLinkPtr   offset_ptr;
    TLineInfoPtr  lip;
    EBool         first_block = eTrue;
    EBool         in_taxa_comment = eFalse;
 
    if (afrp == NULL) {
        return;
    }
 
    line_counter = 0;
    offset_ptr = afrp->offset_list;
    lip = afrp->line_list;
    while (lip != NULL  &&  offset_ptr != NULL
           &&  (in_taxa_comment  ||  ! s_FoundStopLine (lip->data))) {
        if (in_taxa_comment) {
            if (strncmp (lip->data, "end;", 4) == 0) {
                in_taxa_comment = eFalse;
            } 
        } else if (lip->data != NULL
            &&  strncmp (lip->data, "begin taxa;", 11) == 0) {
            in_taxa_comment = eTrue;
        }
        if (line_counter == offset_ptr->ival) {
            s_RemoveCommentsFromBlock (lip, afrp->block_size);
            s_ProcessBlockLines (afrp, lip, afrp->block_size, first_block);
            first_block = eFalse;
            offset_ptr = offset_ptr->next;
        }
        lip = lip->next;
        line_counter ++;
    }
}


/* The following functions are used to analyze contiguous data. */

static void 
s_CreateSequencesBasedOnTokenPatterns 
(TLineInfoPtr     token_list,
 TIntLinkPtr      offset_list,
 SLengthListPtr * anchorpattern,
 SAlignRawFilePtr afrp,
 EBool gen_local_ids)
{
    TLineInfoPtr lip;
    int          line_counter;
    TIntLinkPtr  offset_ptr, next_offset_ptr;
    char *       curr_id;
    TSizeInfoPtr sip;
    int          pattern_line_counter;
    int          curr_seg;

    static int next_local_id = 1;

    if (token_list == NULL  ||  offset_list == NULL
        ||  anchorpattern == NULL 
        ||  afrp == NULL)
    {
        return;
    }
    for (curr_seg = 0; curr_seg < afrp->num_segments; curr_seg ++)
    {
    	if (anchorpattern [curr_seg] == NULL || anchorpattern [curr_seg]->lengthrepeats == NULL)
    	{
    		return;
    	}
    }

    line_counter = 0;
    lip = token_list;
    offset_ptr = offset_list;
    curr_seg = 0;
  
    for (offset_ptr = offset_list;
         offset_ptr != NULL  &&  lip != NULL;
         offset_ptr = offset_ptr->next)
    {
        next_offset_ptr = offset_ptr->next;
        while (line_counter < offset_ptr->ival - 1  &&  lip != NULL) {
            lip = lip->next;
            line_counter ++;
        }
        if (lip != NULL) {
            if (gen_local_ids) {
                char * replacement_id = (char *) malloc(32 +strlen(lip->data));
                sprintf(replacement_id, "lcl|%d %s", next_local_id++, lip->data+1);
                free(lip->data);
                lip->data = replacement_id; 
            }
            curr_id = lip->data;
            lip = lip->next;
            line_counter ++;
            for (sip = anchorpattern[curr_seg]->lengthrepeats;
                 sip != NULL
                   &&  lip != NULL
                   &&  (next_offset_ptr == NULL 
                     ||  line_counter < next_offset_ptr->ival - 1);
                 sip = sip->next)
            {
                for (pattern_line_counter = 0;
                     pattern_line_counter < sip->num_appearances
                         &&  lip != NULL
                         &&  (next_offset_ptr == NULL
                             ||  line_counter < next_offset_ptr->ival - 1);
                     pattern_line_counter ++)
                {
                    if (lip->data [0]  !=  ']'  &&  lip->data [0]  != '[') {
                        if ((int) strlen (lip->data) != sip->size_value) {
                            s_ReportLineLengthError (curr_id, lip, 
                                                     sip->size_value,
                                                     afrp->report_error,
                                                     afrp->report_error_userdata);
                        }
                        afrp->sequences = s_AddAlignRawSeqById (afrp->sequences, 
                                                                curr_id, 
                                                                lip->data,
                                                                lip->line_num,
                                                                lip->line_num,
                                                                lip->line_offset);
                    }
                    lip = lip->next;
                    line_counter ++;
                }
            }
            if (sip != NULL  &&  lip != NULL) {
                s_ReportBlockLengthError (curr_id, lip->line_num,
                                        afrp->block_size,
                                        line_counter - offset_ptr->ival,
                                        afrp->report_error,
                                        afrp->report_error_userdata);
            }
        }
        curr_seg ++;
        if (curr_seg >= afrp->num_segments)
        {
        	curr_seg = 0;
        }
    }        
}


/* The following functions are used for analyzing contiguous data with
 * marked IDs.
 */

/* This function creates a new LengthList pattern for each marked ID.
 * After each new list is created, the function checks to see if the
 * new pattern matches any pattern already in the list of patterns seen.
 * If so, the function deletes the new pattern and increments 
 * num_appearances for the pattern in the list, otherwise the function
 * adds the new pattern to the list.
 * When the list is complete, the function finds the pattern with the 
 * most appearances and returns that pattern as the anchor pattern to use
 * when checking sequence data blocks for consistency with one another.
 */
static SLengthListPtr *
s_CreateAnchorPatternForMarkedIDs 
(SAlignRawFilePtr afrp)
{
    SLengthListPtr * list;
    SLengthListPtr * best;
    SLengthListPtr this_pattern;
    char *         cp;
    TLineInfoPtr   lip;
    int            curr_seg;

    if (afrp == NULL) {
        return NULL;
    }

    /* initialize length lists */
    list = (SLengthListPtr *) malloc (afrp->num_segments * sizeof (SLengthListPtr));
    if (list == NULL) 
    {
    	return NULL;
    }
    for (curr_seg = 0; curr_seg < afrp->num_segments; curr_seg ++)
    {
    	list[curr_seg] = NULL;
    }
    /* initialize best ptrs */
    /* list is one element longer, to hold null terminator */
    best = (SLengthListPtr *) malloc ((afrp->num_segments + 1) * sizeof (SLengthListPtr));
    if (best == NULL) 
    {
    	return NULL;
    }
    for (curr_seg = 0; curr_seg < afrp->num_segments + 1; curr_seg ++)
    {
    	best[curr_seg] = NULL;
    }
    
    /* initialize pattern */
    this_pattern = NULL;

    curr_seg = 0;
    for (lip = afrp->line_list;
         lip != NULL  &&  ! s_FoundStopLine (lip->data);
         lip = lip->next)
    {
        if (lip->data == NULL) continue;
        if (lip->data [0] == ']' || lip->data [0] == '[') continue;
        if (lip->data [0] == '>') {
            if (this_pattern != NULL) {
                list [curr_seg] = s_AddLengthList (list [curr_seg], this_pattern);
                curr_seg ++;
                if (curr_seg >= afrp->num_segments) 
                {
                	curr_seg = 0;
                }
            }
            this_pattern = s_LengthListNew (NULL);
            if (this_pattern == NULL) {
                for (curr_seg = 0; curr_seg < afrp->num_segments; curr_seg ++)
                {
                  s_LengthListFree (list [curr_seg]);
                }
                free (list);
                return NULL;
            }
            this_pattern->num_appearances = 1;
        } else if (this_pattern != NULL) {
            /* This section gets rid of base pair number comments */
            cp = lip->data;
            while ( isspace ((unsigned char)*cp)  ||  isdigit ((unsigned char)*cp)) {
                cp++;
            }
            s_AddLengthRepeat (this_pattern, strlen (cp));
        }
    }
    if (this_pattern != NULL) {
        list[curr_seg] = s_AddLengthList (list [curr_seg], this_pattern);
    }

    /* Now find the pattern with the most appearances for each segment*/
    for (curr_seg = 0; curr_seg < afrp->num_segments; curr_seg++)
    {
        for (this_pattern = list [curr_seg];
             this_pattern != NULL;
             this_pattern = this_pattern->next)
        {
            if (this_pattern->num_appearances == 0) continue;
            if (best [curr_seg] == NULL 
              ||  this_pattern->num_appearances > best[curr_seg]->num_appearances)
            {
                best[curr_seg] = this_pattern;
            }
            
        }

        /* free all patterns before and after anchor pattern */
        if (best [curr_seg] != NULL) {
            s_LengthListFree (best [curr_seg]->next);
            best [curr_seg]->next = NULL;
        }

        if (best [curr_seg] != list [curr_seg]) {
            this_pattern = list [curr_seg];
            while ( this_pattern != NULL  &&  this_pattern->next != best[curr_seg] ) {
                this_pattern = this_pattern->next;
            }
            if (this_pattern != NULL) {
                this_pattern->next = NULL;
                s_LengthListFree (list [curr_seg]);
            }
        }
    }

    for (curr_seg = 0; curr_seg < afrp->num_segments; curr_seg ++)
    {
    	if (best[curr_seg] == NULL) 
    	{
    		for (curr_seg = 0; curr_seg < afrp->num_segments; curr_seg ++)
    		{
    			s_LengthListFree (best [curr_seg]);
    		}
    		return NULL;
    	}
    }
    
    return best;
}


/* This function removes base pair count comments from the data sections
 * for contiguous marked ID sequences.
 */
static void s_RemoveBasePairCountCommentsFromData (SAlignRawFilePtr afrp)
{
    TIntLinkPtr  this_offset, next_offset;
    TLineInfoPtr lip;
    int          line_count;
    char *       cp;

    if (afrp == NULL  ||  afrp->offset_list == NULL) {
        return;
    }
    this_offset = afrp->offset_list;
    next_offset = this_offset->next;
    lip = afrp->line_list;
    line_count = 0;
    while (lip != NULL  &&  this_offset != NULL) {
        if (line_count == this_offset->ival) {
            while (lip != NULL  && 
                   (next_offset == NULL
                   ||  line_count < next_offset->ival - 1)) {
                cp = lip->data;
                if (cp != NULL) {
                    cp += strspn (cp, " \t\r\n1234567890");
                    if (cp != lip->data) {
                        strcpy (lip->data, cp);
                    }
                }
                line_count ++;
                lip = lip->next;
            }
            this_offset = this_offset->next;
            if (this_offset != NULL) {
                next_offset = this_offset->next;
            }
        } else {
            line_count ++;
            lip = lip->next;
        }
    }
}

 
/* This function assumes that the offset_list has already been populated
 * with the locations of the data blocks.  It analyzes the blocks of data
 * to find the most frequently occurring pattern of lengths of data and then
 * uses that pattern to attach the data to the correct IDs and report any
 * errors in formatting.
 */
static void s_ProcessAlignFileRawForMarkedIDs (
    SAlignRawFilePtr afrp,
    EBool gen_local_ids)
{
    SLengthListPtr * anchorpattern;
    
    if (afrp == NULL) {
        return;
    }

    s_RemoveBasePairCountCommentsFromData (afrp);
    anchorpattern = s_CreateAnchorPatternForMarkedIDs (afrp);
    if (anchorpattern == NULL  ||  afrp->offset_list == NULL) {
        return;
    }
    s_CreateSequencesBasedOnTokenPatterns (afrp->line_list, afrp->offset_list,
                                         anchorpattern, afrp, gen_local_ids);
}


/* The following functions are used for analyzing contiguous sequence data
 * without marked IDs.
 */

/* This function left-shifts a string, character by character. */
static void
s_StringLeftShift
(char * cp_from,
 char * cp_to)
{
    if (cp_from == cp_to  ||  cp_from == NULL  ||  cp_to == NULL) {
        return;
    }
    while (*cp_to != 0) {
        *cp_from = *cp_to;
        cp_from++;
        cp_to++;
    }
    *cp_from = 0;
}


/* This function removes bracketed comments from a linked list of 
 * SLineInfo structures.  The function returns a pointer to the
 * list without the comments.
 */
static TLineInfoPtr s_RemoveCommentsFromTokens (TLineInfoPtr list)
{
    TLineInfoPtr  lip;
    int           num_comment_starts;
    char *        cp_r;
    char *        cp;
    EBool         in_comment;

    num_comment_starts = 0;
    in_comment = eFalse;
    for (lip = list;  lip != NULL;  lip = lip->next) {
        if (lip->data == NULL) {
            lip->delete_me = eTrue;
        } else {
            cp_r = NULL;
            for (cp = lip->data; *cp != 0; cp++) {
                if (*cp == ']') {
                    if (cp_r == NULL) {
                        s_StringLeftShift (lip->data, cp + 1);
                        cp = lip->data - 1;
                    } else {
                        s_StringLeftShift (cp_r, cp + 1);
                        cp = cp_r;
                        if (cp_r > lip->data) {
                            cp_r --;
                            while (cp_r >= lip->data  &&  *cp_r != '[') {
                                cp_r --;
                            }
                            if (cp_r < lip->data) {
                                cp_r = NULL;
                            }
                        } else {
                            cp_r = NULL;
                        }
                    }
                    if (num_comment_starts > 0) {
                        num_comment_starts --;
                    }
                } else if (*cp == '[') {
                    cp_r = cp;
                    num_comment_starts ++;
                }
            }
            if (in_comment) {
                if (num_comment_starts == 0) {
                    in_comment = eFalse;
                } else {
                    lip->delete_me = eTrue;
                }
            } else if (num_comment_starts > 0) {
                cp_r = strchr (lip->data, '[');
                if (cp_r != NULL) {
                    *cp_r = 0;
                }
                in_comment = eTrue;
            }
            if (lip->data [0] == 0) {
                lip->delete_me = eTrue;
            }
        }
    }
    list = s_DeleteLineInfos (list);
    return list;
}


/* This function removes Nexus comments from a linked list of SLineInfo
 * structures.  The function returns a pointer to the list without the
 * comments.
 */
static TLineInfoPtr s_RemoveNexusCommentsFromTokens (TLineInfoPtr list) 
{
    TLineInfoPtr lip, start_lip, end_lip;

    lip = list;
    start_lip = NULL;
    end_lip = NULL;
    while (lip != NULL) {
        if (s_StringICmp (lip->data, "#NEXUS") == 0) {
            start_lip = lip;
            end_lip = lip;
            while (end_lip != NULL 
                   &&  s_StringICmp (end_lip->data, "matrix") != 0) {
                end_lip = end_lip->next;
            }
            if (end_lip != NULL) {
                while (start_lip != end_lip) {
                    start_lip->delete_me = eTrue;
                    start_lip = start_lip->next;
                }
                end_lip->delete_me = eTrue;
                lip = end_lip->next;
            } else {
                lip = lip->next;
            }
        } else {
            lip = lip->next;
        }
    }
    list = s_DeleteLineInfos (list);
    return list;
}


/* This function finds the number of characters that occur most frequently
 * in a token and returns a pointer to a SSizeInfo structure that
 * describes the most common length and the number of times it appears.
 */
static TSizeInfoPtr 
s_FindMostFrequentlyOccurringTokenLength
(TSizeInfoPtr list,
 int          not_this_size)
{
    TSizeInfoPtr list_ptr, new_list, best_ptr, return_best;

    new_list = NULL;
    for (list_ptr = list;  list_ptr != NULL;  list_ptr = list_ptr->next) {
        if (not_this_size != list_ptr->size_value) {
            new_list = s_AddSizeInfoAppearances (new_list,
                                               list_ptr->size_value,
                                               list_ptr->num_appearances);
        }
    }
    best_ptr = s_GetMostPopularSizeInfo (new_list);
    return_best = NULL;
    if (best_ptr != NULL) {
        return_best = s_SizeInfoNew (NULL);
        if (return_best != NULL) {
            return_best->size_value = best_ptr->size_value;
            return_best->num_appearances = best_ptr->num_appearances;
        }
    }
    s_SizeInfoFree (new_list);
    return return_best;
}


/* This function examines all instances of an anchor pattern in the data
 * and checks to see if the line immediately after the anchor pattern should
 * be used as part of the anchor pattern.  This function exists because 
 * frequently, but not always, contiguous data will consist of multiple lines
 * of data of the same length (for example, 80 characters), followed by one
 * shorter line with the remaining data.  We must also make sure that we do
 * not accidentally include the ID of the next sequence in the data of the
 * previous sequence.
 */
static void 
s_ExtendAnchorPattern 
(SLengthListPtr anchorpattern,
 TSizeInfoPtr   line_lengths)
{
    TSizeInfoPtr last_line_lengths, sip, sip_next, twoafter;
    int         best_last_line_length;
    int         anchor_line_length;

    if (anchorpattern == NULL 
        ||  anchorpattern->lengthrepeats == NULL
        ||  line_lengths == NULL) {
       return;
    }

    last_line_lengths = NULL;
    anchor_line_length = anchorpattern->lengthrepeats->size_value;

    /* also check to make sure that there's more than one line between
     * this pattern and the next pattern, otherwise the next line is the
     * ID for the next pattern and shouldn't be included in the anchor
     */
    for (sip = line_lengths;  sip != NULL;  sip = sip->next) {
        if (s_SizeInfoIsEqual (sip, anchorpattern->lengthrepeats)) {
            sip_next = sip->next;
            if (sip_next != NULL
                &&  sip_next->size_value > 0
                &&  sip_next->size_value != anchor_line_length
                &&  ((twoafter = sip_next->next) == NULL
                   ||  twoafter->size_value != anchor_line_length))
            {
                last_line_lengths = s_AddSizeInfo (last_line_lengths,
                                                 sip_next->size_value);
            }
        }
    }
    best_last_line_length = s_GetMostPopularSize (last_line_lengths);
    if (best_last_line_length > 0) {
        s_AddLengthRepeat (anchorpattern, best_last_line_length);
    }
    s_SizeInfoFree (last_line_lengths);
} 


/* This function looks for the most frequently occurring pattern, where a 
 * pattern is considered to be N contiguous tokens of M characters.  The
 * function then checks to see if there is usually a token of a particular
 * length that immediately follows this pattern that is not the ID for the
 * next sequence.  If so, this line length is added to the pattern.
 * The function returns a pointer to this pattern.
 */
static SLengthListPtr s_FindMostPopularPattern (TSizeInfoPtr list)
{
    SLengthListPtr patternlist, newpattern;
    TSizeInfoPtr   sip, popular_line_length;
    SLengthListPtr index, best;
    int           not_this_length;

    patternlist = NULL;
    for (sip = list;  sip != NULL;  sip = sip->next) {
        if (sip->size_value > 0) {
            newpattern = s_LengthListNew (NULL);
            if (newpattern == NULL) {
                s_LengthListFree (patternlist);
                return NULL;
            }
            newpattern->num_appearances = 1;
            newpattern->lengthrepeats = s_SizeInfoNew (NULL);
            if (newpattern->lengthrepeats == NULL) {
                s_LengthListFree (patternlist);
                return NULL;
            }
            newpattern->lengthrepeats->size_value = sip->size_value;
            newpattern->lengthrepeats->num_appearances = sip->num_appearances;
            patternlist = s_AddLengthList (patternlist, newpattern);
        }
    }
    if (patternlist == NULL) {
        return NULL;
    }

    best = NULL;
    for (index = patternlist;  index != NULL;  index = index->next) {
        if (index->lengthrepeats->num_appearances < 2) {
            continue;
        }
        if (best==NULL  ||  best->num_appearances < index->num_appearances) {
            best = index;
        } else if (best->num_appearances == index->num_appearances
            &&  best->lengthrepeats->size_value < 
                                  index->lengthrepeats->size_value) {
            best = index;
        }
    }

    /* Free data in list before best pattern */
    index = patternlist;
    while ( index != NULL  &&  index->next != best ) {
         index = index->next;
    }
    if (index != NULL) {
        index->next = NULL;
        s_LengthListFree (patternlist);
    }
    /* Free data in list after best pattern */
    if (best != NULL) {
        s_LengthListFree (best->next);
        best->next = NULL;
    }

    popular_line_length = s_FindMostFrequentlyOccurringTokenLength (list, 0);

    if (best != NULL  &&  best->lengthrepeats != NULL
      &&  popular_line_length != NULL
      &&  best->lengthrepeats->size_value == popular_line_length->size_value)
    {
        not_this_length = popular_line_length->size_value;
        s_SizeInfoFree (popular_line_length);
        popular_line_length = s_FindMostFrequentlyOccurringTokenLength (list,
                                                        not_this_length);
    }

    if (best == NULL
        ||  (popular_line_length != NULL
          &&  popular_line_length->size_value > best->lengthrepeats->size_value
          &&  popular_line_length->num_appearances > best->num_appearances))
    {
        if (best == NULL) {
            best = s_LengthListNew (NULL);
            if (best == NULL) {
                return NULL;
            }
        }
        best->lengthrepeats = s_SizeInfoNew (NULL);
        if (best->lengthrepeats == NULL) {
            return NULL;
        }
        best->lengthrepeats->size_value = popular_line_length->size_value;
        best->lengthrepeats->num_appearances = 1;
    } else {
        /* extend anchor pattern to include best length of last line */
        s_ExtendAnchorPattern (best, list);
    }

    s_SizeInfoFree (popular_line_length);

    return best;
}


/* This function creates an SIntLink list to describe the locations
 * of occurrences of the anchorpattern in the SSizeInfo list.
 * The function returns a pointer to the SIntLink list.
 */
static TIntLinkPtr 
s_CreateOffsetList 
(TSizeInfoPtr list,
 SLengthListPtr anchorpattern)
{
    int          line_counter;
    TIntLinkPtr  offset_list, new_offset;
    TSizeInfoPtr sip;

    if (list == NULL  ||  anchorpattern == NULL) {
        return NULL;
    }
    line_counter = 0;
    offset_list = NULL;
    for (sip = list;  sip != NULL;  sip = sip->next) {
        if (s_SizeInfoIsEqual (sip, anchorpattern->lengthrepeats)) {
            new_offset = s_IntLinkNew (line_counter, offset_list);
            if (new_offset == NULL) {
                s_IntLinkFree (offset_list);
                return NULL;
            }
            if (offset_list == NULL) {
                offset_list = new_offset;
            }
        }
 
        line_counter += sip->num_appearances;
    }
    return offset_list;
}


/* This function determines whether or not the number of expected sequence
 * characters are available starting at a token after line_start and stopping
 * at least one token before the next known sequence data block in the list.
 * If so, the function returns the number of the token at which the sequence
 * data begins.  Otherwise the function returns -1.
 */
static int  
s_ForecastPattern 
(int          line_start,
 int          pattern_length,
 TIntLinkPtr  next_offset,
 int          sip_offset,
 TSizeInfoPtr list)
{
    int  offset, end_offset;
    TSizeInfoPtr sip;
    int  line_counter, num_chars;
  
    if (list == NULL) {
        return -1;
    }

    for (offset = sip_offset; offset < list->num_appearances; offset++) {
        line_counter = line_start + offset;
        num_chars = list->size_value * (list->num_appearances - offset); 
        sip = list;
        while (num_chars < pattern_length
                &&  (next_offset == NULL  ||  line_counter < next_offset->ival)
                &&  sip->next != NULL)
        {
            sip = sip->next;
            for (end_offset = 0;
                 end_offset < sip->num_appearances
                     &&  num_chars < pattern_length
                     &&  (next_offset == NULL
                          ||  line_counter < next_offset->ival);
                 end_offset ++)
            {
                num_chars += sip->size_value;
                line_counter ++;
            }
        }
        if (num_chars == pattern_length) {
            return line_start + offset;
        }
    }
    return -1;
}


/* This function examines the offset list and searches for holes where blocks
 * of sequence data without the exact expected formatting might exist.  The
 * function adds the offsets of any new blocks to the list and returns a
 * pointer to the augmented offset list.
 */
static TIntLinkPtr 
s_AugmentOffsetList 
(TIntLinkPtr    offset_list,
 TSizeInfoPtr   list,
 SLengthListPtr anchorpattern)
{
    int           pattern_length;
    TSizeInfoPtr  sip;
    TIntLinkPtr   prev_offset, next_offset, new_offset;
    int           line_counter, forecast_position, line_skip;
    EBool         skipped_previous = eFalse;
    int           num_chars;
    int           num_additional_offsets = 0;
    int           max_additional_offsets = 5000; /* if it's that bad, forget it */

    if (list == NULL  ||  anchorpattern == NULL) {
        return offset_list;
    }

    pattern_length = 0;
    for (sip = anchorpattern->lengthrepeats;  sip != NULL;  sip = sip->next) {
        pattern_length += (sip->size_value * sip->num_appearances);
    }
    if (pattern_length == 0) {
        return offset_list;
    }

    prev_offset = NULL;
    next_offset = offset_list;
    line_counter = 0;
    sip = list;
    while (sip != NULL  &&  num_additional_offsets < max_additional_offsets) {
        /* if we are somehow out of synch, don't get caught in infinite loop */
        if (next_offset != NULL  &&  line_counter > next_offset->ival) {
            next_offset = next_offset->next;
        } else if (next_offset != NULL  &&  line_counter == next_offset->ival) {
            skipped_previous = eFalse;
            prev_offset = next_offset;
            next_offset = next_offset->next;
            /* advance sip and line counter past the end of this pattern */
            num_chars = 0;
            while (num_chars < pattern_length  &&  sip != NULL) {
                num_chars += sip->size_value * sip->num_appearances;
                line_counter += sip->num_appearances;
                sip = sip->next;
            }
        } else if (skipped_previous) {
            line_skip = 0;
            while (sip != NULL  &&  line_skip < sip->num_appearances 
                  &&  num_additional_offsets < max_additional_offsets
                  &&  (next_offset == NULL
                       ||  line_counter < next_offset->ival)) {
                /* see if we can build a pattern that matches the pattern 
                 * length we want
                 */
                forecast_position = s_ForecastPattern (line_counter,
                                                     pattern_length,
                                                     next_offset, line_skip,
                                                     sip);
                if (forecast_position > 0) {
                    new_offset = s_IntLinkNew (forecast_position, NULL);
                    num_additional_offsets++;
                    if (new_offset == NULL) {
                        return NULL;
                    }
                    if (prev_offset == NULL) {
                        new_offset->next = offset_list;
                        offset_list = new_offset;
                    } else {
                        new_offset->next = next_offset;
                        prev_offset->next = new_offset;
                    }
                    prev_offset = new_offset;
                    /* now advance sip and line counter past the end 
                     * of the pattern we have just created
                     */
                    num_chars = 0;
                    while (num_chars < pattern_length  &&  sip != NULL) {
                        for (line_skip = 0;
                             line_skip < sip->num_appearances
                                 &&  num_chars < pattern_length;
                             line_skip++)
                        {
                            num_chars += sip->size_value;
                            line_counter ++;
                        }
                        if (line_skip == sip->num_appearances) {
                            sip = sip->next;
                            line_skip = 0;
                        }
                    }
                } else {
                    line_counter += sip->num_appearances;
                    sip = sip->next;
                    line_skip = 0;
                }
            }
        } else {
            skipped_previous = eTrue;
            line_counter += sip->num_appearances;
            sip = sip->next;
        }
    }
    if (num_additional_offsets >= max_additional_offsets)
    {
      s_IntLinkFree (offset_list);
      offset_list = NULL;
    }
    return offset_list;
}


/* This function finds the most frequently occurring distance between
 * two sequence data blocks and returns that value.
 */
static int  s_GetMostPopularPatternLength (TIntLinkPtr offset_list)
{
    int          line_counter, best_length;
    TSizeInfoPtr pattern_length_list;
    TIntLinkPtr  offset;

    if (offset_list == NULL) {
        return -1;
    }

    line_counter = -1;
    pattern_length_list = NULL;
    for (offset = offset_list;  offset != NULL;  offset = offset->next) {
        if (line_counter != -1) {
            pattern_length_list = s_AddSizeInfo (pattern_length_list,
                                               offset->ival - line_counter);
        }
        line_counter = offset->ival;
    }
    best_length = s_GetMostPopularSize (pattern_length_list);
    s_SizeInfoFree (pattern_length_list);
    return best_length;
}


/* This function finds the most frequently appearing number of characters 
 * in a block of sequence data and returns that value.
 */
static int 
s_GetBestCharacterLength 
(TLineInfoPtr token_list,
 TIntLinkPtr  offset_list,
 int          block_length)
{
    TLineInfoPtr lip;
    TIntLinkPtr  prev_offset, new_offset;
    int          line_diff, num_chars, best_num_chars;
    TSizeInfoPtr pattern_length_list = NULL;

    if (token_list == NULL  ||  offset_list == NULL  ||  block_length < 1) {
        return -1;
    }
    /* get length of well-formatted block size */
    lip = token_list;
    prev_offset = NULL;
    for (new_offset = offset_list;
         new_offset != NULL  &&  lip != NULL;
         new_offset = new_offset->next)
    {
        if (prev_offset == NULL) {
            /* skip first tokens */
            for (line_diff = 0;
                 line_diff < new_offset->ival  &&  lip != NULL;
                 line_diff ++)
            {
                lip = lip->next;
            }
        }
        if (prev_offset != NULL) {
            num_chars = 0;
            for (line_diff = 0;
                 line_diff < new_offset->ival - prev_offset->ival
                     &&  lip != NULL;
                 line_diff ++)
            {
                if (line_diff < new_offset->ival - prev_offset->ival - 1) {
                    num_chars += strlen (lip->data);
                }
                lip = lip->next;
            }
            if (new_offset->ival - prev_offset->ival == block_length) {
                pattern_length_list = s_AddSizeInfo (pattern_length_list,
                                                   num_chars);
            }
        }
        prev_offset = new_offset;
    }
    best_num_chars = s_GetMostPopularSize (pattern_length_list);
    if (best_num_chars == 0  &&  pattern_length_list != NULL) {
        best_num_chars = pattern_length_list->size_value;
    }
    s_SizeInfoFree (pattern_length_list);
    pattern_length_list = NULL;
    return best_num_chars;
}


static int  
s_CountCharactersBetweenOffsets 
(TLineInfoPtr list,
 int          distance,
 int          desired_num_chars)
{
    int          line_diff, num_chars, total_chars, pattern_length, num_starts;
    TLineInfoPtr lip;
    TIntLinkPtr  length_list, start_list, start_ptr, length;
    int          start_of_unknown;
    int          num_additional_offsets_needed;

    if (list == NULL  ||  distance == 0  ||  desired_num_chars == 0) {
        return 0;
    }

    /* because the first offset is the start of a known pattern, we should
     * skip to the end of that pattern and start looking for additional
     * offsets
     */
    total_chars = 0;
    for (lip = list, line_diff = 0;
         lip != NULL  &&  line_diff < distance
             &&  total_chars < desired_num_chars;
         lip = lip->next, line_diff++) {
        num_chars = strlen (lip->data);
        total_chars += num_chars;
    }
    while (lip != NULL && line_diff < distance  &&  s_IsBlank (lip->data)) {
        lip = lip->next;
        line_diff ++;
    }
    /* skip over line we would need for ID */
    if (lip != NULL) {
        lip = lip->next;
        line_diff ++;
    }
  
    if (lip == NULL  ||  line_diff == distance) {
        return 0;
    }
    num_starts = 1;
    list = lip->next;
    start_of_unknown = line_diff;

    length_list = NULL;
    total_chars = 0;
    for (lip = list;
         lip != NULL  &&  line_diff < distance;
         lip = lip->next, line_diff++)
    {
        num_chars = strlen (lip->data);
        length = s_IntLinkNew (num_chars, length_list);
        if (length_list == NULL) {
            length_list = length;
        }
        total_chars += num_chars;
    }

    /* how many offsets do we need? */
    num_additional_offsets_needed = (total_chars / desired_num_chars);
    if (num_additional_offsets_needed == 0) {
        return 0;
    }

    /* Find all the places you could start and get the exact right number
     * of characters
     */
    start_list = NULL;
    num_starts = 0;
    pattern_length = 0;
    for (start_ptr = length_list, line_diff = start_of_unknown;
         start_ptr != NULL  &&  line_diff < distance
           &&  pattern_length < distance - line_diff ;
         start_ptr = start_ptr->next, line_diff++) {
        num_chars = start_ptr->ival;
        pattern_length = 1;
        length = start_ptr->next;
        while (num_chars < desired_num_chars
               &&  pattern_length + line_diff < distance
               &&  length != NULL)
        {
            num_chars += length->ival;
            pattern_length ++;
            length = length->next;
        }
        if (num_chars == desired_num_chars) {
            length = s_IntLinkNew (line_diff, start_list);
            if (start_list == NULL) {
                start_list = length;
            }
            num_starts ++;
        }
    }

    /* now select best set of start points */
    
    s_IntLinkFree (length_list);
    s_IntLinkFree (start_list);
    return 0;
}


/* This function inserts new block locations into the offset_list
 * by looking for likely starts of abnormal patterns.
 */
static void s_InsertNewOffsets
(TLineInfoPtr token_list,
 TIntLinkPtr  offset_list,
 int          block_length,
 int          best_num_chars,
 char *       alphabet)
{
    TLineInfoPtr lip;
    TIntLinkPtr  prev_offset, new_offset, splice_offset;
    int          line_diff, num_chars, line_start;

    if (token_list == NULL  ||  offset_list == NULL
        ||  block_length < 1  ||  best_num_chars < 1)
    {
        return;
    }

    lip = token_list;
    prev_offset = NULL;
    for (new_offset = offset_list;
         new_offset != NULL  &&  lip != NULL;
         new_offset = new_offset->next) {
        if (prev_offset == NULL) {
            /* just advance through tokens */
            for (line_diff = 0;
                 line_diff < new_offset->ival  &&  lip != NULL;
                 line_diff ++) {
                lip = lip->next;
            }
        } else {
            if (new_offset->ival - prev_offset->ival == block_length) {
                /* just advance through tokens */
                for (line_diff = 0;
                     line_diff < new_offset->ival - prev_offset->ival 
                         &&  lip != NULL;
                     line_diff ++) {
                    lip = lip->next;
                }
            } else {
                /* look for intermediate breaks */
                num_chars = 0;
                for (line_diff = 0;
                     line_diff < new_offset->ival - prev_offset->ival
                         &&  lip != NULL  &&  num_chars < best_num_chars;
                     line_diff ++) {
                    num_chars += strlen (lip->data);
                    lip = lip->next;
                }
                if (lip == NULL) {
                  return;
                }
                /* set new offset at first line of next pattern */
                line_diff ++;
                lip = lip->next;
                if (line_diff < new_offset->ival - prev_offset->ival) {
                    line_start = line_diff + prev_offset->ival;
                    /* advance token pointer to new piece */
                    while (line_diff < new_offset->ival - prev_offset->ival
                           &&  lip != NULL)
                    {
                        lip = lip->next;
                        line_diff ++;
                    }
                    /* insert new offset value */
                    splice_offset = s_IntLinkNew (line_start, NULL);
                    if (splice_offset == NULL) {
                        return;
                    }
                    splice_offset->next = new_offset;
                    prev_offset->next = splice_offset;

                    s_CountCharactersBetweenOffsets (lip,
                                       new_offset->ival - splice_offset->ival,
                                       best_num_chars);
                }
            }
        }
        prev_offset = new_offset;
    }
    
    /* iterate through the last block */
    for (line_diff = 0;
         line_diff < block_length && lip != NULL; 
         line_diff ++) {
        lip = lip->next;
    }

    /* if we have room for one more sequence, or even most of one more sequence, add it */
    if (lip != NULL  &&  ! s_SkippableString (lip->data)) {
        splice_offset = s_IntLinkNew (line_diff + prev_offset->ival, prev_offset);
    }
}


/* This function returns true if the string contains digits, false otherwise */
static EBool s_ContainsDigits (char *data)
{
    char *cp;

    if (data == NULL) return eFalse;
    for (cp = data; *cp != 0; cp++) {
        if (isdigit ((unsigned char)(*cp))) {
            return eTrue;
        }
    }
    return eFalse;
}


/* This function processes the alignment file data by dividing the original
 * lines into pieces based on whitespace and looking for patterns of length 
 * in the data. 
 */
static void s_ProcessAlignFileRawByLengthPattern (SAlignRawFilePtr afrp)
{
    TLineInfoPtr   token_list;
    SLengthListPtr list;
    TLineInfoPtr   lip;
    SLengthListPtr anchorpattern[2];
    TIntLinkPtr    offset_list;
    int            best_length;
    int            best_num_chars;

    if (afrp == NULL  ||  afrp->line_list == NULL) {
        return;
    }

    token_list = s_BuildTokenList (afrp->line_list);
    token_list = s_RemoveCommentsFromTokens (token_list);
    token_list = s_RemoveNexusCommentsFromTokens (token_list);

    list = s_LengthListNew ( NULL );
    for (lip = token_list;
         lip != NULL  &&  ! s_FoundStopLine (lip->data);
         lip = lip->next)
    {
        if (s_SkippableString (lip->data)  ||  s_ContainsDigits(lip->data)) {
            s_AddLengthRepeat (list, 0);
        } else {
            s_AddLengthRepeat (list, strlen (lip->data));
        }
    }

    anchorpattern [0] = s_FindMostPopularPattern (list->lengthrepeats);
    anchorpattern [1] = NULL;
    if (anchorpattern [0] == NULL  ||  anchorpattern[0]->lengthrepeats == NULL) {
        return;
    }

    /* find anchor patterns in original list, 
     * find distances between anchor patterns 
     */
    offset_list = s_CreateOffsetList (list->lengthrepeats, anchorpattern[0]);
    offset_list = s_AugmentOffsetList (offset_list,
                                     list->lengthrepeats,
                                     anchorpattern[0]);

    /* resolve unusual distances between anchor patterns */
    best_length = s_GetMostPopularPatternLength (offset_list);
    if (best_length < 1  &&  offset_list != NULL  && offset_list->next != NULL) {
        best_length = offset_list->next->ival - offset_list->ival;
    }
    best_num_chars = s_GetBestCharacterLength (token_list, offset_list,
                                             best_length);
    s_InsertNewOffsets (token_list, offset_list, best_length, best_num_chars,
                      afrp->alphabet);

    /* use token before each anchor pattern as ID, use tokens for distance
     * between anchor patterns for sequence data
     */
    s_CreateSequencesBasedOnTokenPatterns (token_list, offset_list,
                                       anchorpattern, afrp, eFalse);
  
    s_LengthListFree (anchorpattern[0]);
    s_LengthListFree (list);
    s_LineInfoFree (token_list);
}


/* The following functions are used to convert data from the internal
 * representation into the form that will be passed to the calling
 * program.  Information from the ID strings is parsed to remove
 * definition lines and organism information, the gap characters are
 * standardized to '-', the missing characters are standardizes to 'N',
 * match characters are replaced with characters from the first record,
 * and bad characters are reported.
 */

/* This function allocates memory for a new AligmentFileData structure
 * and initializes its member variables.
 */
extern TAlignmentFilePtr AlignmentFileNew (void)
{
    TAlignmentFilePtr afp;

    afp = (TAlignmentFilePtr) malloc (sizeof (SAlignmentFile));
    if (afp == NULL) {
        return NULL;
    }
    afp->num_sequences = 0;
    afp->num_organisms = 0;
    afp->num_deflines  = 0;
    afp->num_segments  = 0;
    afp->ids           = NULL;
    afp->sequences     = NULL;
    afp->organisms     = NULL;
    afp->deflines      = NULL;
    return afp;
}


/* This function frees the memory associated with an AligmentFileData
 * structure and its member variables.
 */
extern void AlignmentFileFree (TAlignmentFilePtr afp)
{
    int  index;

    if (afp == NULL) {
        return;
    }
    if (afp->ids != NULL) {
        for (index = 0;  index < afp->num_sequences;  index++) {
            free (afp->ids [index]);
        }  
        free (afp->ids);
        afp->ids = NULL;
    }
    if (afp->sequences != NULL) {
        for (index = 0;  index < afp->num_sequences;  index++) {
            free (afp->sequences [index]);
        }  
        free (afp->sequences);
        afp->sequences = NULL;
    }
    if (afp->organisms != NULL) {
        for (index = 0;  index < afp->num_organisms;  index++) {
            free (afp->organisms [index]);
        }  
        free (afp->organisms);
        afp->sequences = NULL;
    }
    if (afp->deflines != NULL) {
        for (index = 0;  index < afp->num_deflines;  index++) {
            free (afp->deflines [index]);
        }  
        free (afp->deflines);
        afp->deflines = NULL;
    }
    free (afp);
}


/* This function parses the identifier string used by the alignment file
 * to identify a sequence to find the portion of the string that is actually
 * an ID, as opposed to organism information or definition line.
 */
static char * s_GetIdFromString (char * str)
{
    char * cp;
    char * id;
    int    len;

    if (str == NULL) {
        return NULL;
    }

    cp = str;
    cp += strspn (str, " >\t");
    len = strcspn (cp, " \t\r\n");
    if (len == 0) {
        return NULL;
    }
    id = (char *)malloc (len + 1);
    if (id == NULL) {
        return NULL;
    }
    strncpy (id, cp, len);
    id [ len ] = 0;
    return id;
}


/* This function pulls defline information from the ID string, if there is
 * any.
 */
static char * s_GetDeflineFromIdString (char * str)
{
    char * cp;
    int    len;

    if (str == NULL) {
        return NULL;
    }

    cp = str;
    cp += strspn (str, " >\t");
    len = strcspn (cp, " \t\r\n");
    if (len == 0) {
        return NULL; 
    }
    cp += len;
    len = strspn (cp, " \t\r\n");
    if (len == 0) {
        return NULL; 
    }
    cp += len;
    if (*cp == 0) {
        return NULL;
    }
    return strdup (cp);
}


/* This function takes the ID strings read from the file and parses them
 * to obtain a defline (if there is extra text after the ID and/or
 * organism information) and to obtain the actual ID for the sequence.
 */
static EBool s_ReprocessIds (SAlignRawFilePtr afrp)
{
    TStringCountPtr list, scp;
    TAlignRawSeqPtr arsp;
    TLineInfoPtr    lip;
    char *          id;
    int             line_num;
    EBool           rval = eTrue;
    char *          defline;

    if (afrp == NULL) {
        return eFalse;
    }

    list = NULL;
    lip = afrp->deflines;
    for (arsp = afrp->sequences; arsp != NULL; arsp = arsp->next) {
        if (arsp->id_lines != NULL) {
            line_num = arsp->id_lines->ival;
        } else {
            line_num = -1;
        }
        s_RemoveOrganismCommentFromLine (arsp->id);
        id = s_GetIdFromString (arsp->id);
        if (lip == NULL) {
            defline = s_GetDeflineFromIdString (arsp->id);
            afrp->deflines = s_AddLineInfo (afrp->deflines, defline,
                                            line_num, 0);
            free (defline);
            afrp->num_deflines ++;
        }
        free (arsp->id);
        arsp->id = id;
        list = s_AddStringCount (arsp->id, line_num, list);
    }

    for (scp = list;  scp != NULL;  scp = scp->next) {
        if (scp->num_appearances > 1) {
            rval = eFalse;
            s_ReportRepeatedId (scp, afrp->report_error,
                              afrp->report_error_userdata);
        }
    }
    /* free string count list */
    s_StringCountFree (list);
    return rval;
}


/* This function reports unacceptable characters in a sequence.  Frequently
 * there will be more than one character of the same kind (for instance,
 * when the user has incorrectly specified a gap character), so repeated
 * characters are reported together.  The function advances the data 
 * position in the SLineInfoReader structure lirp, and returns the
 * current data position for lirp.
 */
static int 
s_ReportRepeatedBadCharsInSequence
(TLineInfoReaderPtr   lirp,
 char *               id,
 const char *         reason,
 FReportErrorFunction report_error,
 void *               report_error_userdata)
{
    int  bad_line_num, bad_line_offset;
    int  num_bad_chars;
    char bad_char, curr_char;
    int  data_position;

    bad_line_num = s_LineInfoReaderGetCurrentLineNumber (lirp);
    bad_line_offset = s_LineInfoReaderGetCurrentLineOffset (lirp);
    bad_char = *lirp->curr_line_pos;
    num_bad_chars = 1;
    data_position = lirp->data_pos + 1;
    while ((curr_char = s_FindNthDataChar (lirp, data_position)) == bad_char) {
        num_bad_chars ++;
        data_position ++;
    }
    s_ReportBadCharError (id, bad_char, num_bad_chars,
                        bad_line_offset, bad_line_num, reason,
                        report_error, report_error_userdata);
    return data_position;
}


/* This function does context-sensitive replacement of the missing,
 * match, and gap characters and also identifies bad characters.
 * Gap characters found in the wrong location in the sequence are
 * considered an error.  Characters that are not missing, match, or 
 * gap characters and are not in the specified sequence alphabet are
 * reported as errors.  Match characters in the first sequence are also
 * reported as errors.
 * The function will return eTrue if any errors were found, or eFalse
 * if there were no errors.
 */
static EBool
s_FindBadDataCharsInSequence
(TAlignRawSeqPtr      arsp,
 TAlignRawSeqPtr      master_arsp,
 TSequenceInfoPtr     sip,
 int                  num_segments,
 FReportErrorFunction report_error,
 void *               report_error_userdata)
{
    TLineInfoReaderPtr lirp, master_lirp;
    int                data_position;
    int                middle_start = 0;
    int                middle_end = 0;
    char               curr_char, master_char;
    EBool              found_middle_start;
    EBool              rval = eFalse;
    EBool              match_not_in_beginning_gap;
    EBool              match_not_in_end_gap;

    if (arsp == NULL  ||  master_arsp == NULL  ||  sip == NULL) {
        return eTrue;
    }
    lirp = s_LineInfoReaderNew (arsp->sequence_data);
    if (lirp == NULL) {
        return eTrue;
    }
    if (arsp != master_arsp) {
        master_lirp = s_LineInfoReaderNew (master_arsp->sequence_data);
        if (master_lirp == NULL) {
            s_LineInfoReaderFree (lirp);
            return eTrue;
        }
    } else {
        master_lirp = NULL;
    }

    if (strcspn (sip->beginning_gap, sip->match) 
                  == strlen (sip->beginning_gap)) {
        match_not_in_beginning_gap = eTrue;
    } else {
        match_not_in_beginning_gap = eFalse;
    }

    if (strcspn (sip->end_gap, sip->match) == strlen (sip->end_gap)) {
        match_not_in_end_gap = eTrue;
    } else {
        match_not_in_end_gap = eFalse;
    }

    /* First, find middle start and end positions and report characters
     * that are not beginning gap before the middle
     */
    found_middle_start = eFalse;
    data_position = 0;
    curr_char = s_FindNthDataChar (lirp, data_position);
    while (curr_char != 0) {
        if (strchr (sip->alphabet, curr_char) != NULL) {
            if (! found_middle_start) {
                middle_start = data_position;
                found_middle_start = eTrue;
            }
            middle_end = data_position + 1;
            data_position ++;
        } else if (! found_middle_start) {
            if (match_not_in_beginning_gap
                &&  strchr (sip->match, curr_char) != NULL)
            {
                middle_start = data_position;
                found_middle_start = eTrue;
                middle_end = data_position + 1;
                data_position ++;
            } else if (strchr (sip->beginning_gap, curr_char) == NULL) {
                /* Report error - found character that is not beginning gap
                   in beginning gap */
                data_position = s_ReportRepeatedBadCharsInSequence (lirp,
                                                                  arsp->id,
                                "expect only beginning gap characters here",
                                report_error, report_error_userdata);
                rval = eTrue;
            } else {
                *lirp->curr_line_pos = '-';
                data_position ++;
            }
        } else {
            if (match_not_in_end_gap
                &&  strchr (sip->match, curr_char) != NULL)
            {
                middle_end = data_position + 1;
            }
            data_position ++;
        }
        curr_char = s_FindNthDataChar (lirp, data_position);
    }

    if (! found_middle_start) {
        if (num_segments > 1)
        {
            return eFalse;
        }
        else
        {
            s_ReportMissingSequenceData (arsp->id,
                                   report_error, report_error_userdata);
            s_LineInfoReaderFree (lirp);
            return eTrue;
          
        }
    }

    /* Now complain about bad middle characters */
    data_position = middle_start;
    while (data_position < middle_end)
    {
        curr_char = s_FindNthDataChar (lirp, data_position);
        while (data_position < middle_end
               &&  strchr (sip->alphabet, curr_char) != NULL) {
            data_position ++;
            curr_char = s_FindNthDataChar (lirp, data_position);
        } 
        if (curr_char == 0  ||  data_position >= middle_end) {
            /* do nothing, done with middle */
        } else if (strchr (sip->missing, curr_char) != NULL) {
            *lirp->curr_line_pos = 'N';
            data_position ++;
        } else if (strchr (sip->match, curr_char) != NULL) {
            master_char = s_FindNthDataChar (master_lirp, data_position);
            if (master_char == 0) {
                /* report error - unable to get master char */
                if (master_arsp == arsp) {
                    data_position = s_ReportRepeatedBadCharsInSequence (lirp,
                                arsp->id,
                                "can't specify match chars in first sequence",
                                report_error, report_error_userdata);
                } else {
                    data_position = s_ReportRepeatedBadCharsInSequence (lirp,
                                arsp->id,
                                "can't find source for match chars",
                                report_error, report_error_userdata);
                }
                rval = eTrue;
            } else {
                *lirp->curr_line_pos = master_char;
                data_position ++;
            }
        } else if (strchr (sip->middle_gap, curr_char) != NULL) {
            *lirp->curr_line_pos = '-';
            data_position ++;
        } else {
            /* Report error - found bad character in middle */
            data_position = s_ReportRepeatedBadCharsInSequence (lirp,
                                      arsp->id,
                                      "expect only sequence, missing, match,"
                                      " and middle gap characters here",
                                      report_error, report_error_userdata);
            rval = eTrue;
        }
    }

    /* Now find and complain about end characters */
    data_position = middle_end;
    curr_char = s_FindNthDataChar (lirp, data_position);
    while (curr_char != 0) {
        if (strchr (sip->end_gap, curr_char) == NULL) {
            /* Report error - found bad character in middle */
            data_position = s_ReportRepeatedBadCharsInSequence (lirp, arsp->id,
                                      "expect only end gap characters here",
                                      report_error, report_error_userdata);
            rval = eTrue;
        } else {
            *lirp->curr_line_pos = '-';
            data_position++;
        }
        curr_char = s_FindNthDataChar (lirp, data_position);
    }
    s_LineInfoReaderFree (lirp);
    s_LineInfoReaderFree (master_lirp);
    return rval;
}


/* This function examines each sequence and replaces the special characters
 * and reports bad characters in each one.  The function will return eTrue
 * if any of the sequences contained bad characters or eFalse if no errors
 * were seen.
 */
static EBool
s_s_FindBadDataCharsInSequenceList
(SAlignRawFilePtr afrp,
 TSequenceInfoPtr sip)
{
    TAlignRawSeqPtr arsp;
    EBool is_bad = eFalse;

    if (afrp == NULL  ||  afrp->sequences == NULL) {
        return eTrue;
    }
    for (arsp = afrp->sequences; arsp != NULL; arsp = arsp->next) {
        if (s_FindBadDataCharsInSequence (arsp, afrp->sequences, sip,
                                        afrp->num_segments,
                                        afrp->report_error,
                                        afrp->report_error_userdata)) {
            is_bad = eTrue;
        }
    }
    return is_bad;
}


/* This function examines the organisms listed for the alignment and determines
 * whether any of the organism names (including the associated comments) are
 * repeated.
 */
static EBool s_AreOrganismsUnique (SAlignRawFilePtr afrp)
{
    TLineInfoPtr    this_org, lip;
    TAlignRawSeqPtr arsp;
    EBool           are_unique;

    if (afrp == NULL  ||  afrp->num_organisms == 0
        ||  afrp->organisms == NULL) {
        return eFalse;
    }
    are_unique = eTrue;
    for (this_org = afrp->organisms;
         this_org != NULL;
         this_org = this_org->next) {
        lip = afrp->organisms;
        arsp = afrp->sequences;
        while (lip != NULL  &&  lip != this_org
               &&  strcmp (lip->data, this_org->data) != 0  &&  arsp != NULL) {
            lip = lip->next;
            arsp = arsp->next;
        }
        if (lip != NULL  &&  lip != this_org) {
            are_unique = eFalse;
            s_ReportRepeatedOrganismName (arsp->id, this_org->line_num,
                                        lip->line_num,
                                        this_org->data,
                                        afrp->report_error,
                                        afrp->report_error_userdata);
        }
    }
    return are_unique;
}


/* This function uses the contents of an SAlignRawFileData structure to
 * create an SAlignmentFile structure with the appropriate information.
 */
static TAlignmentFilePtr
s_ConvertDataToOutput 
(SAlignRawFilePtr afrp,
 TSequenceInfoPtr sip)
{
    TAlignRawSeqPtr   arsp;
    int               index;
    TSizeInfoPtr    * lengths;
    int             * best_length;
    TAlignmentFilePtr afp;
    TLineInfoPtr      lip;
    int               curr_seg;

    if (afrp == NULL  ||  sip == NULL  ||  afrp->sequences == NULL) {
        return NULL;
    }
    afp = AlignmentFileNew ();
    if (afp == NULL) {
        return NULL;
    }

    afp->num_organisms = afrp->num_organisms;
    afp->num_deflines = afrp->num_deflines;
    afp->num_segments = afrp->num_segments;
    afp->num_sequences = 0;
    afp->align_format_found = afrp->align_format_found;
    lengths = NULL;

    for (arsp = afrp->sequences;  arsp != NULL;  arsp = arsp->next) {
        afp->num_sequences++;
    }

    if (afp->num_sequences != afrp->num_organisms
        && afp->num_sequences / afp->num_segments != afrp->num_organisms) {
        s_ReportMissingOrganismInfo (afrp->report_error,
                                   afrp->report_error_userdata);
    } else {
        s_AreOrganismsUnique (afrp);
    }

    afp->sequences = (char **)malloc (afp->num_sequences 
                                           * sizeof (char *));
    if (afp->sequences == NULL) {
        AlignmentFileFree (afp);
        return NULL;
    }
    afp->ids = (char **)malloc (afp->num_sequences * sizeof (char *));
    if (afp->ids == NULL) {
        AlignmentFileFree (afp);
        return NULL;
    }
    if (afp->num_organisms > 0) {
        afp->organisms = (char **) malloc (afp->num_organisms
                                                * sizeof (char *));
        if (afp->organisms == NULL) {
            AlignmentFileFree (afp);
            return NULL;
        }
    }
    if (afp->num_deflines > 0) {
        afp->deflines = (char **)malloc (afp->num_deflines
                                              * sizeof (char *));
        if (afp->deflines == NULL) {
            AlignmentFileFree (afp);
            return NULL;
        }
    }

    /* copy in deflines */
    for (lip = afrp->deflines, index = 0;
         lip != NULL  &&  index < afp->num_deflines;
         lip = lip->next, index++) {
        if (lip->data == NULL) {
            afp->deflines [index] = NULL;
        } else {
            afp->deflines [index] = strdup (lip->data);
        }
    }
    while (index < afp->num_deflines) {
        afp->deflines [index ++] = NULL;
    }

    /* copy in organism information */
    for (lip = afrp->organisms, index = 0;
         lip != NULL  &&  index < afp->num_organisms;
         lip = lip->next, index++) {
        afp->organisms [index] = strdup (lip->data);
    }
  
    /* we need to store length information about different segments separately */
    lengths = (TSizeInfoPtr *) malloc (sizeof (TSizeInfoPtr) * afrp->num_segments);
    if (lengths == NULL) {
    	AlignmentFileFree (afp);
        return NULL;
    }
    best_length = (int *) malloc (sizeof (int) * afrp->num_segments);
    if (best_length == NULL) {
    	free (lengths);
    	AlignmentFileFree (afp);
    	return NULL;
    }
    for (curr_seg = 0; curr_seg < afrp->num_segments; curr_seg ++) {
    	lengths [curr_seg] = NULL;
    	best_length [curr_seg] = 0;
    }
    
    /* copy in sequence data */
    curr_seg = 0;
    for (arsp = afrp->sequences, index = 0;
         arsp != NULL  &&  index < afp->num_sequences;
         arsp = arsp->next, index++) {
        afp->sequences [index] = 
                    s_LineInfoMergeAndStripSpaces (arsp->sequence_data);

        if (afp->sequences [index] != NULL) {
            lengths [curr_seg] = s_AddSizeInfo (lengths [curr_seg], strlen (afp->sequences [index]));
        }
        afp->ids [index] = strdup (arsp->id);
        curr_seg ++;
        if (curr_seg >= afrp->num_segments) {
        	curr_seg = 0;
        }
    }
    for (curr_seg = 0; curr_seg < afrp->num_segments; curr_seg ++)
    {
        best_length [curr_seg] = s_GetMostPopularSize (lengths [curr_seg]);
        if (best_length [curr_seg] == 0  &&  lengths [curr_seg] != NULL) {
            best_length [curr_seg] = lengths [curr_seg]->size_value;
        }   
    }

    curr_seg = 0;
    for (index = 0;  index < afp->num_sequences;  index++) {
        if (afp->sequences [index] == NULL) {
            s_ReportMissingSequenceData (afp->ids [index],
                                       afrp->report_error,
                                       afrp->report_error_userdata);
        } else if ((int) strlen (afp->sequences [index]) != best_length [curr_seg]) {
            s_ReportBadSequenceLength (afp->ids [index], best_length [curr_seg],
                                     strlen (afp->sequences [index]),
                                     afrp->report_error,
                                     afrp->report_error_userdata);
        }
        curr_seg ++;
        if (curr_seg >= afrp->num_segments) {
        	curr_seg = 0;
        }
    }

    if (afrp->expected_num_sequence > 0
      &&  afrp->expected_num_sequence != afp->num_sequences)
    {
        s_ReportIncorrectNumberOfSequences (afrp->expected_num_sequence,
                                          afp->num_sequences,
                                          afrp->report_error,
                                          afrp->report_error_userdata);
    }
    if (afrp->expected_sequence_len > 0
      &&  afrp->expected_sequence_len != best_length [0])
    {
        s_ReportIncorrectSequenceLength (afrp->expected_sequence_len,
                                       best_length [0],
                                       afrp->report_error,
                                       afrp->report_error_userdata);
    }
    
    free (best_length);
    for (curr_seg = 0; curr_seg < afrp->num_segments; curr_seg ++)
    {
        s_SizeInfoFree (lengths [curr_seg]);
    }
    free (lengths);
    
    return afp;
}


/* This is the function called by the calling program to read an alignment
 * file.  The readfunc argument is a function pointer supplied by the 
 * calling program which this library will use to read in data from the
 * file one line at a time.  The fileuserdata argument is a pointer to 
 * data used by the calling program's readfunc function and will be passed
 * back with each call to readfunc.
 * The errfunc argument is a function pointer supplied by the calling
 * program for reporting errors.  The erroruserdata argument is a pointer
 * to data used by the calling program's errfunc function and will be
 * passed back with each call to readfunc.
 * The sequence_info argument contains the sequence alphabet and missing,
 * match, and gap characters to use in interpreting the sequence data.
 */
extern TAlignmentFilePtr 
ReadAlignmentFileEx2 
(FReadLineFunction readfunc,
 void * fileuserdata,
 FReportErrorFunction errfunc,
 void * erroruserdata,
 TSequenceInfoPtr sequence_info,
 EBool use_nexus_file_info,
 EBool gen_local_ids)
{
    SAlignRawFilePtr afrp;
    TAlignmentFilePtr afp;
    EBool use_file = eFalse;
    EAlignFormat format = ALNFMT_UNKNOWN;

    if (sequence_info == NULL  ||  sequence_info->alphabet == NULL) {
        return NULL;
    }
    
    if (use_nexus_file_info != 0)
    {
      use_file = eTrue;
    }
    
    afrp = s_ReadAlignFileRaw ( readfunc, fileuserdata, sequence_info,
                                use_file,
                                errfunc, erroruserdata, &format);
    if (afrp == NULL) {
        return NULL;
    }

    if (afrp->block_size > 1) {
        s_ProcessAlignRawFileByBlockOffsets (afrp);
    } else if (afrp->marked_ids) {
        s_ProcessAlignFileRawForMarkedIDs (
            afrp, gen_local_ids);
    } else {
        s_ProcessAlignFileRawByLengthPattern (afrp);
    }

    s_ReprocessIds (afrp);

    if (s_s_FindBadDataCharsInSequenceList (afrp, sequence_info)) {
        s_AlignFileRawFree (afrp);
        return NULL;
    }

    afp = s_ConvertDataToOutput (afrp, sequence_info);
    s_AlignFileRawFree (afrp);
  
    return afp;
}

extern TAlignmentFilePtr 
ReadAlignmentFileEx 
(FReadLineFunction readfunc,
 void * fileuserdata,
 FReportErrorFunction errfunc,
 void * erroruserdata,
 TSequenceInfoPtr sequence_info,
 EBool use_nexus_file_info)
{
    return ReadAlignmentFileEx2 (readfunc, fileuserdata, errfunc, erroruserdata,
        sequence_info, use_nexus_file_info, eFalse);
}

extern TAlignmentFilePtr 
ReadAlignmentFile 
(FReadLineFunction readfunc,
 void * fileuserdata,
 FReportErrorFunction errfunc,
 void * erroruserdata,
 TSequenceInfoPtr sequence_info)
{
    return ReadAlignmentFileEx2 (readfunc, fileuserdata, errfunc, erroruserdata,
                                sequence_info, eFalse, eFalse);
}

extern TAlignmentFilePtr 
ReadAlignmentFile2 
(FReadLineFunction readfunc,
 void * fileuserdata,
 FReportErrorFunction errfunc,
 void * erroruserdata,
 TSequenceInfoPtr sequence_info,
 EBool gen_local_ids)
{
    return ReadAlignmentFileEx2 (readfunc, fileuserdata, errfunc, erroruserdata,
                                sequence_info, eFalse, gen_local_ids);
}

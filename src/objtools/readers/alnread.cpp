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

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objtools/readers/alnread.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _MSC_VER
#define strdup _strdup
#endif

BEGIN_NCBI_SCOPE

static const size_t kMaxPrintedIntLen = 10;
static const size_t  kMaxPrintedIntLenPlusOne = 11;


/*  ---------------------------------------------------------------------- */
typedef enum {
    ALNFMT_UNKNOWN,
    ALNFMT_NEXUS,
    ALNFMT_PHYLIP,
    ALNFMT_CLUSTAL,
    ALNFMT_FASTAGAP,
} EAlignFormat;

/*  ---------------------------------------------------------------------- */

//  ============================================================================
template<class T>
static T* tAppendNew(
    T* pList,
    T* pNewItem)
//
//  Create pNewItem if pNewItem is not specified.
//  Append pNewItem to pList if pList is specified.
//  Return pointer to pNewItem whether it's been created or not.
//  ============================================================================
{
    T* pLast = pList;
    while (pLast  &&  pLast->next) {
        pLast = pLast->next;
    }
    if (pLast) {
        pLast->next = pNewItem;
    }
    return pNewItem;
}


//  ===========================================================================
struct SLineInfo {
//  ===========================================================================
    SLineInfo(
        const char* data_,
        int line_num_,
        int line_offset_): 
        data(nullptr), line_num(line_num_ + 1), line_offset(line_offset_),
        delete_me(false), next(nullptr) 
    { 
        SetData(data_); 
    };
    
    ~SLineInfo() { 
        delete[] data; 
        delete next;
        next = nullptr;
    };
    
    void SetData(
        const char* data_) 
    {
        if (data) {
            delete[] data;
            data = nullptr;
        }
        if (data_) {
            data = new char[strlen(data_)+1];
            strcpy(data, data_);
        }
    };

    char* data;
    int line_num;
    int line_offset;
    bool delete_me;
    SLineInfo* next;
};
using TLineInfoPtr = SLineInfo*;

//  =============================================================================
struct SLineInfoReader {
//  =============================================================================
    SLineInfoReader(
        SLineInfo& lineList): first_line(&lineList) 
    {
        first_line = &lineList;
        Reset();
    };

    void
    Reset() {
        curr_line = first_line;

        while (curr_line  &&  !curr_line->data) {
            curr_line = curr_line->next;
        }
        if (!curr_line) {
            curr_line_pos = nullptr;
            data_pos = -1;
        } else {
            curr_line_pos = curr_line->data;
            AdvancePastSpace();
            data_pos = (curr_line_pos ? 0 : -1);
        }
    };

    void AdvancePastSpace() {
        if (!curr_line_pos) {
            return;
        }
        while (isspace((unsigned char) *curr_line_pos)  ||  *curr_line_pos == 0) {
            while (isspace((unsigned char) *curr_line_pos)) {
                curr_line_pos ++;
            }
            if (*curr_line_pos == 0) {
                curr_line = curr_line->next;
                while (curr_line  &&  curr_line->data) {
                    curr_line = curr_line->next;
                }
                if (!curr_line) {
                    curr_line_pos = nullptr;
                    return;
                }
                curr_line_pos = curr_line->data;
            }
        }
    }

    TLineInfoPtr first_line;
    TLineInfoPtr curr_line;
    char* curr_line_pos;
    int data_pos;
};
using TLineInfoReaderPtr = SLineInfoReader*;

//  ============================================================================
struct SIntLink {
//  ============================================================================
    SIntLink(
        int val, SIntLink* next_=nullptr): ival(val), next(next_) {};

    ~SIntLink() {
        delete next;
    };

    static SIntLink* 
    AppendNew(
        int ival, 
        SIntLink* list)
    {
        SIntLink* ilp, * last;

        ilp = new SIntLink(ival, nullptr);
        if (ilp == nullptr) {
            return nullptr;
        }
        last = list;
        while (last != nullptr && last->next != nullptr) {
            last = last->next;
        }
        if (last != nullptr) {
            last->next = ilp;
        }
        return ilp;
    }

    static void
    CreateOrAppend(
        int iVal,
        SIntLink*& anchor)
    {
        SIntLink* ilp = SIntLink::AppendNew(iVal, anchor);
        if (anchor == nullptr) {
            anchor = ilp;
        }
    }

    int               ival;
    SIntLink * next;
};
using TIntLinkPtr = SIntLink*;

//  ============================================================================
struct SStringCount {
//  ============================================================================
    SStringCount(): 
        string(nullptr), num_appearances(0), line_numbers(nullptr), next(nullptr)
    {};

    ~SStringCount() {
        //delete[] string;
        delete line_numbers;
        delete next;
    };

    static SStringCount*
    AppendNew(
        SStringCount* list) { return tAppendNew<SStringCount>(list, new SStringCount); };

    char* string;
    int num_appearances;
    TIntLinkPtr line_numbers;
    struct SStringCount* next;
};
using TStringCountPtr = SStringCount*;

//  ============================================================================
struct SSizeInfo {
//  ============================================================================
    SSizeInfo(
        int size_value_=0, int num_appearances_=1, SSizeInfo* next_=nullptr): 
        size_value(size_value_), num_appearances(num_appearances_), next(next_)
    {};

    bool operator ==(const SSizeInfo& rhs) {
        return (size_value == rhs.size_value  &&  
            num_appearances == rhs.num_appearances); };

    ~SSizeInfo() { delete next; };

    static SSizeInfo*
    AppendNew(
        SSizeInfo* list) { return tAppendNew<SSizeInfo>(list, new SSizeInfo); };

    int size_value;
    int num_appearances;
    SSizeInfo* next;
};
using TSizeInfoPtr = SSizeInfo*;

//  =============================================================================
struct SLengthList {
//  =============================================================================
    SLengthList(): lengthrepeats(nullptr), num_appearances(0), next(nullptr)
    {};
    
    ~SLengthList() {
        delete lengthrepeats;
        delete next;
    };

    static SLengthList*
    AppendNew(
        SLengthList* list) { return tAppendNew<SLengthList>(list, new SLengthList); };

    TSizeInfoPtr lengthrepeats;
    int num_appearances;
    SLengthList* next;
};
using TLengthListPtr = SLengthList*;
using TLengthListData = SLengthList;

//  ============================================================================
struct SCommentLoc {
//  ============================================================================
    SCommentLoc(
        char* start_,
        char* end_,
        SCommentLoc* next_=nullptr): start(start_), end(end_), next(next_)
    {};

    ~SCommentLoc() {
        delete next;
    };

    char* start;
    char* end;
    SCommentLoc * next;
};
using TCommentLocPtr = SCommentLoc*;

//  ============================================================================
struct SBracketedCommentList  {
//  ============================================================================
    SBracketedCommentList(
        const char* str, int lineNum, int lineOffset): 
        comment_lines(new SLineInfo(str, lineNum, lineOffset)),
        next(nullptr)
    {};

    static SBracketedCommentList*
    ApendNew(
        SBracketedCommentList* list, const char* str, int lineNum, int lineOffset)
    {
        auto pNewItem = new SBracketedCommentList(str, lineNum, lineOffset);
        return tAppendNew<SBracketedCommentList>(list, pNewItem);
    };

    ~SBracketedCommentList() {
        delete comment_lines;
        delete next;
    };

    SLineInfo* comment_lines;
    SBracketedCommentList* next;
};
using TBracketedCommentListPtr = SBracketedCommentList*;

//  ============================================================================
struct SAlignRawSeq {
//  ============================================================================
    SAlignRawSeq(): 
        id(nullptr), sequence_data(nullptr), id_lines(nullptr), next(nullptr)
    {};
    ~SAlignRawSeq() {
        delete[] id;
        delete sequence_data;
        delete id_lines;
        delete next;
    };

    static SAlignRawSeq*
    AppendNew(
        SAlignRawSeq* list) 
    { return tAppendNew<SAlignRawSeq>(list, new SAlignRawSeq); };

    void
    SetId(
        const char* id_) 
    {
        if (id) {
            delete[] id;
        }
        id = new char[1 + strlen(id_)];
        strcpy(id, id_);
    };

    char *                id;
    TLineInfoPtr          sequence_data;
    TIntLinkPtr           id_lines;
    struct SAlignRawSeq * next;
};
using TAlignRawSeqPtr = SAlignRawSeq*;

//  ============================================================================
struct SAlignFileRaw {
//  ============================================================================
    SAlignFileRaw():
        marked_ids(false), line_list(nullptr), organisms(nullptr),
        deflines(nullptr), num_deflines(0), block_size(0), 
        offset_list(nullptr), sequences(nullptr), report_error(nullptr),
        report_error_userdata(nullptr), alphabet_(""), expected_num_sequence(0),
        expected_sequence_len(0), align_format_found(false)
    {};

    ~SAlignFileRaw() {
        delete organisms;
        delete deflines;
        delete line_list;
        delete sequences;
        delete offset_list;
    };

    TLineInfoPtr         line_list;
    TLineInfoPtr         organisms;
    TAlignRawSeqPtr      sequences;
    TLineInfoPtr         deflines;
    int                  num_deflines;
    bool                 marked_ids;
    int                  block_size;
    TIntLinkPtr          offset_list;
    FReportErrorFunction report_error;
    void *               report_error_userdata;
    string               alphabet_;
    int                  expected_num_sequence;
    int                  expected_sequence_len;
    char                 align_format_found;
};
using TAlignRawFilePtr = SAlignFileRaw*;

/* Function declarations
 */
static bool s_AfrpInitLineData( 
    TAlignRawFilePtr afrp, FLineReader readfunc, istream&);
static void s_AfrpProcessFastaGap(
    TAlignRawFilePtr afrp, TLengthListPtr * patterns, bool * last_line_was_marked_id, char* plinestr, int overall_line_count);

/* These functions are used for storing and transmitting information
 * about errors encountered while reading the alignment data.
 */

string StrPrintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    return NStr::FormatVarargs(format, args);
}

//  ============================================================================
void
sReportError(
    const string& id,
    int lineNumber,
    EAlnErr errCode,
    const string& errMessage,
    FReportErrorFunction errReporter,
    void* errUserData)
//  ============================================================================
{
    if (!errReporter) {
        return;
    }
    CErrorInfo errorInfo(errCode, lineNumber, id, errMessage);
    errReporter(errorInfo, errUserData);
}

/* This function creates and sends an error message regarding a NEXUS comment
 * character.
 */
static void 
s_ReportCharCommentError(
    const string& expected,
    char seen,
    const char* valName,
    FReportErrorFunction errfunc,
    void* errdata)
{
    const char * errFormat = 
        "Specified %s character does not match NEXUS comment in file (specified %s, comment %c)";
    string errMessage = StrPrintf(errFormat, valName, expected.c_str(), seen);

    sReportError(
        nullptr,
        -1,
        eAlnErr_BadFormat,
        errMessage,
        errfunc,
        errdata);
}


/* This function creates and sends an error message regarding a character
 * that is unexpected in sequence data.
 */
static void 
s_ReportBadCharError(
    char* id,
    char bad_char,
    int num_bad,
    int offset,
    int line_number,
    const char* reason,
    FReportErrorFunction errfunc,
    void* errdata)
{
    const char * errFormat = "%d bad characters (%c) found at position %d (%s).";
    string errMessage = StrPrintf(errFormat, num_bad, bad_char, offset, reason);
    
    sReportError(
        id,
        line_number,
        eAlnErr_BadData,
        errMessage,
        errfunc,
        errdata);
}
 

/* This function creates and sends an error message regarding an ID that
 * was found in the wrong location.
 */
static void 
s_ReportInconsistentID(
    char* id,
    int line_number,
    FReportErrorFunction report_error,
    void* report_error_userdata)
{
    sReportError(
        id,
        line_number,
        eAlnErr_BadFormat,
        "Found unexpected ID",
        report_error,
        report_error_userdata);
}


/* This function creates and sends an error message regarding a line
 * of sequence data that was expected to have a different length.
 */
static void 
s_ReportInconsistentBlockLine(
    char* id,
    int line_number,
    FReportErrorFunction report_error,
    void* report_error_userdata)
{
    sReportError(
        id,
        line_number,
        eAlnErr_BadFormat,
        "Inconsistent block line formatting",
        report_error,
        report_error_userdata);
}


/* This function creates and sends an error message regarding a line of
 * sequence data that was expected to be a different length.
 */
static void 
s_ReportLineLengthError(
    char* id,
    TLineInfoPtr lip,
    int expected_length,
    FReportErrorFunction report_error,
    void* report_error_userdata)
{
    if (!lip) {
        return;
    }

    const char * errFormat = "Expected line length %d, actual length %d";
    size_t dataLength = (lip->data ? strlen(lip->data) : 0);
    string errMessage = StrPrintf(errFormat, expected_length, dataLength);

    sReportError(
        id,
        lip->line_num,
        eAlnErr_BadFormat,
        errMessage,
        report_error,
        report_error_userdata);
}


/* This function creates and sends an error message regarding a block of
 * sequence data that was expected to contain more lines.
 */
static void 
s_ReportBlockLengthError(
    char* id,
    int line_num,
    int expected_num,
    int actual_num,
    FReportErrorFunction report_error,
    void* report_error_userdata)
{
    const char* errFormat = "Expected %d lines in block, found %d";
    string errMessage = StrPrintf(errFormat, expected_num, actual_num);

    sReportError(
        id,
        line_num,
        eAlnErr_BadFormat,
        errMessage,
        report_error,
        report_error_userdata);
}


/* This function creates and sends an error message regarding a block of
 * sequence data that contains duplicate IDs.
 */
static void 
s_ReportDuplicateIDError(
    char* id,
    int line_num,
    FReportErrorFunction report_error,
    void* report_error_userdata)
{
    sReportError(
        id,
        line_num,
        eAlnErr_BadData,
        "Duplicate ID!  Sequences will be concatenated!",
        report_error,
        report_error_userdata);
}


/* This function creates and sends an error message regarding missing
 * sequence data.
 */
static void
s_ReportMissingSequenceData(
    const char* id,
    FReportErrorFunction report_error,
    void* report_error_userdata)
{
    sReportError(
        id,
        -1,
        eAlnErr_Fatal,
        "No data found",
        report_error,
        report_error_userdata);
}


/* This function creates and sends an error message indicating that the
 * most common length of the sequences in the file do not match a comment
 * found in the file.
 */
static void 
s_ReportBadSequenceLength(
    const char* id,
    int expected_length,
    int actual_length,
    FReportErrorFunction report_error,
    void* report_error_userdata)
{
    const char*  errFormat = "Expected sequence length %d, actual length %d";
    string errMessage = StrPrintf(errFormat, expected_length, actual_length);

    sReportError(
        id,
        -1,
        eAlnErr_BadFormat,
        errMessage,
        report_error,
        report_error_userdata);
}


/* This function creates and sends an error message indicating that the
 * number of sequences read does not match a comment in the alignment file.
 */
static void
s_ReportIncorrectNumberOfSequences(
    int num_expected,
    int num_found,
    FReportErrorFunction report_error,
    void* report_error_userdata)
{
    const char*  errFormat = "Expected %d sequences, found %d";
    string errMessage = StrPrintf(errFormat, num_expected, num_found);

    sReportError(
        "",
        -1,
        eAlnErr_BadFormat,
        errMessage,
        report_error,
        report_error_userdata);
}


static void
s_ReportIncorrectSequenceLength(
    int len_expected,
    int len_found,
    FReportErrorFunction report_error,
    void* report_error_userdata)
{
    const char*  errFormat = "Expected sequences of length %d, found %d";
    string errMessage = StrPrintf(errFormat, len_expected, len_found);

    sReportError(
        "",
        -1,
        eAlnErr_BadFormat,
        errMessage,
        report_error,
        report_error_userdata);
}



/* This function creates and sends an error message regarding an ID that is
 * used for more than one sequence.
 */
static void 
s_ReportRepeatedId(
    TStringCountPtr scp,
    FReportErrorFunction report_error,
    void* report_error_userdata)
{
    if (!scp  ||  !scp->string) {
        return;
    }

    const char* errFormat = "ID %s appears in the following locations:";
    string errMessage = StrPrintf(errFormat, scp->string);
    for (TIntLinkPtr t = scp->line_numbers; t != NULL; t = t->next) {
        errMessage += StrPrintf(" %d", t->ival);
    }

    sReportError(
        nullptr,
        -1,
        eAlnErr_BadData,
        errMessage,
        report_error,
        report_error_userdata);
}


/* This function creates and sends an error message indicating that the file
 * being read is an ASN.1 file.
 */
static void 
s_ReportASN1Error(
    FReportErrorFunction errfunc,
    void* errdata)
{
    sReportError(
        "",
        -1,
        eAlnErr_BadData,
        "This is an ASN.1 file which cannot be read by this function",
        errfunc,
        errdata);
}


 
/* This function creates and sends an error message regarding an unused line.
 */
static void 
s_ReportUnusedLine(
    int line_num_start,
    int line_num_stop,
    TLineInfoPtr line_val,
    FReportErrorFunction errfunc,
    void* errdata)
{
    const char * errFormat1 = "Line %d could not be assigned to an interleaved block";
    const char * errFormat2 = "Lines %d through %d could not be assigned to an interleaved block";
    const char * errFormat3 = "Contents of unused line: %s";
    int skip;

    if (errfunc == NULL  ||  line_val == NULL) {
        return;
    }

    if (line_num_start == line_num_stop) {
        string errMessage = StrPrintf(errFormat1, line_num_start);
        sReportError(
            nullptr,
            line_num_start,
            eAlnErr_BadFormat,
            errMessage,
            errfunc,
            errdata);
    } else {
        string errMessage = StrPrintf(errFormat2, line_num_start);
        sReportError(
            nullptr,
            line_num_start,
            eAlnErr_BadFormat,
            errMessage,
            errfunc,
            errdata);
    }

    /* report contents of unused lines */
    for (skip = line_num_start; skip < line_num_stop + 1  &&  line_val != nullptr; skip++) {
        if (!line_val->data) {
            continue;
        }
        string errMessage = StrPrintf(errFormat3, line_val->data);
        sReportError(
            nullptr,
            line_num_start,
            eAlnErr_BadFormat,
            errMessage,
            errfunc,
            errdata);
        line_val = line_val->next;
    }
}


/* These functions are used to accumulate and retrieve information on 
 * how often a size of data (number of lines or number of characters) occurs.
 */

/* This function searches list for a SSizeInfo structure with the
 * same size_value as size_value.  If it finds such a structure, it
 * adds the value of num_appearances to the num_appearances for that
 * structure, otherwise the function creates a new structure at the end
 * of the list with the specified values of size_value and num_appearances.
 * The function returns a pointer to the list of SSizeInfo structures.
 */
static TSizeInfoPtr 
s_AddSizeInfoAppearances (
    TSizeInfoPtr list,
    int  size_value,
    int  num_appearances)
{
    TSizeInfoPtr p, last;

    last = nullptr;
    for (p = list;  p != nullptr  &&  p->size_value != size_value;  p = p->next) {
        last = p;
    }
    if (p == nullptr) {
        p = new SSizeInfo(size_value, num_appearances);
        if (last == nullptr) {
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
s_AddSizeInfo(
    TSizeInfoPtr list,
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
static TSizeInfoPtr 
s_GetMostPopularSizeInfo (
    TSizeInfoPtr list)
{
    if (!list) {
        return nullptr;
    }

    auto best = list;
    for (auto p = list->next;  p != nullptr;  p = p->next) {
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
static int  
s_GetMostPopularSize (TSizeInfoPtr list)
{
    TSizeInfoPtr best = s_GetMostPopularSizeInfo (list);
    if (!best) {
        return 0;
    }
    if (best->num_appearances > 1) {
        return best->size_value; 
    } else {
        return 0;
    }
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
s_AddLengthRepeat(
    TLengthListPtr llp,
    int  size_value)
{
    if (!llp) {
        return;
    }

    TSizeInfoPtr last = nullptr, p;
    for (p = llp->lengthrepeats;  p != nullptr;  p = p->next) {
        last = p;
    }
    if (last == nullptr  ||  last->size_value != size_value) {
        p = new SSizeInfo(size_value, 1);
        if (last == nullptr) {
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
 * If the two structures match, the function returns true, otherwise the
 * function returns false.
 */
static bool 
s_DoLengthPatternsMatch(
    TLengthListPtr llp1,
    TLengthListPtr llp2)
{
    TSizeInfoPtr sip1, sip2;

    if (!llp1  ||  !llp2  ||  !llp1->lengthrepeats  ||  !llp2->lengthrepeats) {
        return false;
    }
    for (sip1 = llp1->lengthrepeats, sip2 = llp2->lengthrepeats;
         sip1  &&  sip2;
         sip1 = sip1->next, sip2 = sip2->next) {
        if ( !(*sip1 == *sip2)  ||  (sip1->next  &&  !sip2->next)  ||
                (!sip1->next  &&  sip2->next)) {
            return false;
        }
    }
    return true;
}


/* This function examines a list of SLengthListData structures to see if
 * one of them matches llp.  If so, the value of num_appearances in that
 * list is incremented by one and llp is freed, otherwise llp is added
 * to the end of the list.
 * The function returns a pointer to the list of LenghtListData structures.
 */
static TLengthListPtr
s_AddLengthList(
    TLengthListPtr list,
    TLengthListPtr llp)
{
    TLengthListPtr prev_llp;

    if (!list) {
        list = llp;
    } else {
        prev_llp = list;
        while ( prev_llp->next  &&  ! s_DoLengthPatternsMatch (prev_llp, llp)) {
            prev_llp = prev_llp->next;
        }
        if (s_DoLengthPatternsMatch (prev_llp, llp)) {
            prev_llp->num_appearances ++;
            delete llp;
        } else {
            prev_llp->next = llp;
        }
    }
    return list;
}


/* This function deletes from a linked list of SLineInfo structures
 * those structures for which the delete_me flag has been set.  The function
 * returns a pointer to the beginning of the new list.
 */
static TLineInfoPtr 
s_DeleteLineInfos(
    TLineInfoPtr list)
{
    TLineInfoPtr prev = nullptr, lip, nextlip;

    lip = list;
    while (lip) {
        nextlip = lip->next;
        if (lip->delete_me) {
            if (prev) {
                prev->next = lip->next;
            } else {
                list = lip->next;
            }
            lip->next = nullptr;
            delete lip;
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
s_AddLineInfo(
    TLineInfoPtr list,
    const char* string,
    int line_num,
    int line_offset)
{
    if (!string) {
        return list;
    }
    TLineInfoPtr lip = new SLineInfo(string, line_num, line_offset);
    if (!list) {
        list = lip;
    } else {
        TLineInfoPtr p = list;
        while (p  &&  p->next) {
            p = p->next;
        }
        p->next = lip;
    }
    return list;
}

/* This function adds a line to a bracketed comment. */
static void 
s_BracketedCommentListAddLine(
    TBracketedCommentListPtr comment,
    char* string,
    int line_num,
    int line_offset)
{
    if (!comment) {
        return;
    }
    comment->comment_lines = s_AddLineInfo(
        comment->comment_lines, string, line_num, line_offset);
}


static char * 
s_TokenizeString(
    char * str, 
    const char *delimiter, 
    char **last)
{
    if (!delimiter) {
        *last = nullptr;
        return nullptr;
    }

    if (!str) {
        str = *last;
    }
    if (!str  ||  *str == 0) {
        return nullptr;
    }
    auto skip = strspn(str, delimiter);
    str += skip;
    auto length = strcspn(str, delimiter);
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
static TLineInfoPtr 
s_BuildTokenList(
    TLineInfoPtr line_list)
{
    TLineInfoPtr first_token;
    char *       tmp;
    char *       piece;
    char *       last;
    size_t       line_pos;

    first_token = nullptr;

    for (auto lip = line_list;  lip;  lip = lip->next) {
        if (lip->data) {
            char* tmp = new char[strlen(lip->data) + 1];
            strcpy(tmp, lip->data);
            piece = s_TokenizeString(tmp, " \t\r", &last);
            while (piece) {
                line_pos = piece - tmp;
                line_pos += lip->line_offset;
                first_token = s_AddLineInfo (first_token, piece, 
                                             lip->line_num,
                                             line_pos);
                piece = s_TokenizeString (nullptr, " \t\r", &last);
            }
            delete[] tmp;
        }
    }
    return first_token;
}


/* This function takes a list of SLineInfo structures, allocates memory
 * to hold their contents contiguously, and stores their contents, minus
 * the whitespace, in the newly allocated memory.
 * The function returns a pointer to this newly allocated memory.
 */
static char* 
s_LineInfoMergeAndStripSpaces (
    TLineInfoPtr list)
{
    TLineInfoPtr lip;
    size_t       len;
    char *       result;
    char *       cp_to;
    char *       cp_from;

    if (!list) {
        return nullptr;
    }
    len = 0;
    for (lip = list;  lip;  lip = lip->next) {
        if (lip->data) {
            len += strlen(lip->data);
        }
    }
    result = new char[len + 1];
    cp_to = result;
    for (lip = list;  lip;  lip = lip->next) {
        if (lip->data) {
            cp_from = lip->data;
            while (*cp_from != 0) {
                if (! isspace((unsigned char)*cp_from)) {
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

 
/* This function safely interprets the current line number of the
 * SLineInfoReader structure.  If the structure is NULL or the
 * current line is NULL (usually because the data position has been
 * advanced to the end of the available sequence data), the function
 * returns -1, since the current data position does not actually exist.
 * Otherwise, the line number of the character at the current data position
 * is returned.
 */
static int  
s_LineInfoReaderGetCurrentLineNumber(
    TLineInfoReaderPtr lirp)
{
    if (!lirp  ||  !lirp->curr_line) {
        return -1;
    }
    return lirp->curr_line->line_num;
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
static int  
s_LineInfoReaderGetCurrentLineOffset (
    TLineInfoReaderPtr lirp)
{
    if (!lirp  ||  !lirp->curr_line  ||  !lirp->curr_line_pos) {
        return -1;
    }
    return lirp->curr_line->line_offset + lirp->curr_line_pos 
                    - lirp->curr_line->data;
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
s_FindNthDataChar(
    TLineInfoReaderPtr lirp,
    int  pos)
{
    if (!lirp  ||  !lirp->first_line  ||  pos < 0 ||  lirp->data_pos == -1) {
        return 0;
    }

    if (lirp->data_pos == pos) {
        if (!lirp->curr_line_pos) {
            return 0;
        } else {
            return *lirp->curr_line_pos;
        }
    }
    if (lirp->data_pos > pos) {
        lirp->Reset();
    }
     
    while (lirp->data_pos < pos  &&  lirp->curr_line) {
        lirp->curr_line_pos ++;
        /* skip over spaces, progress to next line if necessary */
        lirp->AdvancePastSpace();
        lirp->data_pos ++;
    }
    if (lirp->curr_line_pos) {
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

/* This function searches list to see if the string matches any of the
 * existing entries.  If so, the num_appearances value for that entry is
 * increased and the line_num is added to that entry's list of line numbers.
 * Otherwise a new entry is created at the end of the list.
 * The function returns list if list was not NULL, or a pointer to the
 * newly created SStringCount structure otherwise.
 */
static TStringCountPtr 
s_AddStringCount (
    char *          string,
    int             line_num,
    TStringCountPtr list
)
{
    TStringCountPtr  add_to, last = nullptr;

    if (!string) {
        for (add_to = list; add_to  &&  add_to->string; add_to = add_to->next) {
            last = add_to;
        }
    } else {
        for (add_to = list;
             add_to &&  (!add_to->string  ||  strcmp(string, add_to->string) != 0);
             add_to = add_to->next) {
            last = add_to;
        }
    }
    
    if (!add_to) {
        add_to = SStringCount::AppendNew(last);
        if (!list) {
            list = add_to;
        }
        if (add_to) {
            add_to->string = string;
        }
    }
    if (add_to) {
        add_to->num_appearances ++;
        SIntLink::CreateOrAppend(line_num, add_to->line_numbers);
    }
    return list;   
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
s_GetFASTAExpectedNumbers(
    char *           str,
    TAlignRawFilePtr afrp)
{
    char *  cp;
    char *  cpend;
    char    replace;
    int     first, second;

    if (!str  ||  !afrp) {
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
 * numbers separated by whitespace.  The function returns true if so,
 * otherwise it returns false.
 */
static bool 
s_IsTwoNumbersSeparatedBySpace (
    const char* str)
{
    const CTempString DIGITS("0123456789");

    if (!str) {
        return false;
    }
    string teststr(str), first, second;
    NStr::SplitInTwo(teststr, " \t", first, second, NStr::fSplit_MergeDelimiters);
    if (second.empty()) {
        return false;
    }
    return (first.find_first_not_of(DIGITS) == string::npos  &&  
        second.find_first_not_of(DIGITS) == string::npos);
}


/* This function finds a value name in a string, looks for an equals sign
 * after the value name, and then looks for an integer value after the
 * equals sign.  If the integer value is found, the function copies the
 * integer value into the val location and returns true, otherwise the
 * function returns false.
 */
static bool 
s_GetOneNexusSizeComment(
    const char * str,
    const char * valname, 
    int        * val)
{
    char   buf[kMaxPrintedIntLenPlusOne];
    const char * cpstart;
    const char * cpend;
    size_t maxlen;

    if (!str  ||  !valname  ||  !val) {
        return false;
    }
    string normalized(str);
    NStr::ToLower(normalized);
    str = normalized.c_str();

    cpstart = strstr (str, valname);
    if (!cpstart) {
        return false;
    }
    cpstart += strlen (valname);
    while (*cpstart != 0  &&  isspace ((unsigned char)*cpstart)) {
        cpstart++;
    }
    if (*cpstart != '=') {
        return false;
    }
    cpstart ++;
    while (*cpstart != 0  &&  isspace ((unsigned char)*cpstart)) {
        cpstart++;
    }

    if (! isdigit ((unsigned char)*cpstart)) {
        return false;
    }
    cpend = cpstart + 1;
    while ( *cpend != 0  &&  isdigit ((unsigned char)*cpend)) {
        cpend ++;
    }
    maxlen = cpend - cpstart;
    if (maxlen > kMaxPrintedIntLen) 
        maxlen = kMaxPrintedIntLen;

    strncpy(buf, cpstart, maxlen);
    buf [maxlen] = 0;
    *val = atoi (buf);
    return true;
}


/* This function looks for Nexus-style comments to indicate the number of
 * sequences and the number of characters per sequence expected from this
 * alignment file.  If the function finds these comments, it returns true,
 * otherwise it returns false.
 */
static void s_GetNexusSizeComments(
    const char *           str,
    bool *          found_ntax,
    bool *          found_nchar,
    TAlignRawFilePtr afrp)
{
    int  num_sequences;
    int  num_chars;
  
    if (!str  ||  !found_nchar  ||  !found_ntax  ||  !afrp) {
        return;
    }
    if (! *found_ntax  && 
        (s_GetOneNexusSizeComment (str, "ntax", &num_sequences))) {
        afrp->expected_num_sequence = num_sequences;
        afrp->align_format_found = true;
        *found_ntax = true;
    }
    if (! *found_nchar  &&
        (s_GetOneNexusSizeComment (str, "nchar", &num_chars))) {
        afrp->expected_sequence_len = num_chars;
        afrp->align_format_found = true;
        *found_nchar = true;
    }
}


/* This function looks for characters in Nexus-style comments to 
 * indicate values for specific kinds of characters (match, missing, gap...).
 * If the string str contains val_name followed by an equals sign, the function
 * will return the first non-whitespace character following the equals sign,
 * otherwise the function will return a 0.
 */
static char GetNexusTypechar(
    const char * str, 
    const char * val_name)
{
    const char * cp;
    const char * cpend;

    if (!str  ||  !val_name) {
        return 0;
    }
    cpend = strstr (str, ";");
    if (!cpend) {
        return 0;
    }
    cp = strstr (str, val_name);
    if (!cp || cp > cpend) {
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
 * discrepancies are found, the function reports the errors and returns false,
 * otherwise the function returns true.
 */ 
static bool s_CheckNexusCharInfo(
    const char* str,
    const CSequenceInfo& sequence_info,
    FReportErrorFunction errfunc,
    void* errdata)
{
    const char * cp;
    char   c;

    if (!str) {
        return false;
    }

    cp = strstr (str, "format ");
    if (!cp) {
        cp = strstr (str, "FORMAT ");
    }
    if (!cp) {
        return false;
    }

    if (!errfunc) {
        return true;
    }

    c = GetNexusTypechar (cp + 7, "missing");
    if (c == 0) {
        c = GetNexusTypechar (cp + 7, "MISSING");
    }
    if (c != 0  &&  sequence_info.Missing().find(c) == string::npos) {
        s_ReportCharCommentError (
            sequence_info.Missing(), c, "MISSING", errfunc, errdata);
    }
 
    c = GetNexusTypechar (cp + 7, "gap");
    if (c == 0) {
        c = GetNexusTypechar (cp + 7, "GAP");
    }
    if (c != 0  &&  sequence_info.MiddleGap().find(c) == string::npos)
    {
        s_ReportCharCommentError (
            sequence_info.MiddleGap(), c, "GAP", errfunc, errdata);
    }
 
    c = GetNexusTypechar (cp + 7, "match");
    if (c == 0) {
        c = GetNexusTypechar (cp + 7, "MATCH");
    }
    if (c != 0  &&  sequence_info.Match().find(c) == string::npos)
    {
        s_ReportCharCommentError (
            sequence_info.Match(), c, "MATCH", errfunc, errdata);
    }
    return true;
} 

/* This function reads a Nexus-style comment line for the characters 
 * specified for missing, match, and gap and sets those values in sequence_info.
 * The function returns true if a Nexus comment was found, false otherwise.
 */ 
static bool 
s_UpdateNexusCharInfo(
    const char* str,
    CSequenceInfo& sequence_info)
{
    const char * cp;
    char   c;

    if (!str) {
        return false;
    }

    cp = strstr (str, "format ");
    if (!cp) {
        cp = strstr (str, "FORMAT ");
    }
    if (!cp) {
        return false;
    }

    c = GetNexusTypechar (cp + 7, "missing");
    if (c == 0) {
        c = GetNexusTypechar (cp + 7, "MISSING");
    }
    sequence_info.SetMissing(c);
    
    c = GetNexusTypechar (cp + 7, "gap");
    if (c == 0) {
        c = GetNexusTypechar (cp + 7, "GAP");
    }
    sequence_info.SetBeginningGap(c);
    sequence_info.SetMiddleGap(c);
    sequence_info.SetEndGap(c);
 
    c = GetNexusTypechar (cp + 7, "match");
    if (c == 0) {
        c = GetNexusTypechar (cp + 7, "MATCH");
    }
    sequence_info.SetMatch(c);
    return true;
} 


/* This function examines the string str to see if it consists entirely of
 * asterisks, colons, periods, and whitespace.  If so, this line is assumed
 * to be a Clustal-style consensus line and the function returns true.
 * otherwise the function returns false;
 */
static bool 
s_IsConsensusLine (
    const char * str_)
{
    if (!str_) {
        return false;
    }
    string str(str_);
    if (str.find_first_not_of("*:. \t\r\n") != string::npos) {
        return false;
    }
    return (str.find_first_of("*:.") != string::npos);
}


/* This function identifies lines that begin with a NEXUS keyword and end
 * with a semicolon - they will not contain sequence data.  The function
 * returns true if the line contains only a NEXUS comment, false otherwise.
 */
static bool 
s_SkippableNexusComment (
    const char *str_)
{

    if (!str_) {
        return false;
    }
    string str = NStr::TruncateSpaces(str_);
    if (!NStr::EndsWith(str, ';')) {
        return false;
    }
    return (NStr::StartsWith(str, "format ")  ||
        NStr::StartsWith(str, "dimensions ")  ||
        NStr::StartsWith(str, "options ")  ||
        NStr::StartsWith(str, "begin characters")  ||
        NStr::StartsWith(str, "begin data") ||
        NStr::StartsWith(str, "begin ncbi"));
}


static bool 
s_IsOnlyNumbersAndSpaces (
    const char *str)
{
    return (string(str).find_first_not_of(" \t0123456789") == string::npos);
}


/* This function determines whether str contains a indication
* that this is real alignment format (nexus, clustal, etc.)
*/
static bool 
s_IsAlnFormatString (
    const char* str_)
{
    if (!str_) {
        return false;
    }
    string str(str_);
    return (NStr::StartsWith(str, "matrix")  ||
        NStr::StartsWith(str, "#NEXUS")  ||
        NStr::StartsWith(str, "CLUSTAL W")  ||
        s_SkippableNexusComment (str_)  ||
        s_IsTwoNumbersSeparatedBySpace (str_)  ||
        s_IsConsensusLine (str_));
}


/* This function determines whether the contents of str are "skippable"
 * in that they do not contain sequence data and therefore should not be
 * considered part of any block patterns or sequence data.
 */
static bool s_SkippableString (char * str_)
{
    if (s_IsAlnFormatString(str_)) {
        return true;
    }
    string str(str_);
    return (NStr::StartsWith(str, "sequin")  ||
        s_IsOnlyNumbersAndSpaces(str_)  ||
        str_[0] == ';');
 }


/* This function determines whether or not linestring contains a line
 * indicating the end of sequence data (organism information and definition
 * lines may occur after this line).
 */
static bool 
s_FoundStopLine (
    const char * str_)
{
    if (!str_) {
        return false;
    }
    string str(str_);
    return (NStr::StartsWith(str, "endblock")  ||  NStr::StartsWith(str, "end;"));
}


/* This function identifies the beginning line of an ASN.1 file, which
 * cannot be read by the alignment reader.
 */
static bool 
s_IsASN1 (
    const string& line)
{
    return (line.find("::=") != string::npos);
}


/* The following functions are used to locate and read comments enclosed
 * in brackets.  These comments sometimes include organism information.
 */

/* This function finds the first comment enclosed in brackets and creates
 * a SCommentLoc structure to indicate the position of the comment
 * in the string.  The function returns a pointer to this structure if a
 * comment is found or a NULL if the string does not contain a bracketed 
 * comment.
 */
static TCommentLocPtr 
s_FindComment(
    char * string)
{
    if (!string) {
        return nullptr;
    }
   
    char* cp_start = strstr (string, "[");
    if (!cp_start) {
        return nullptr;
    }
    char* cp_end = strstr(cp_start, "]");
    if (!cp_end) {
        return nullptr;
    }
    return new SCommentLoc(cp_start, cp_end);
}


/* This function removes a comment from a line. */
static void 
s_RemoveCommentFromLine (
    char * linestring)
{
    TCommentLocPtr clp;
    size_t offset, len;

    if (!linestring) {
        return;
    }

    clp = s_FindComment (linestring);
    while (clp) {
        memmove(clp->start, clp->end+1, strlen(clp->end));
        // Note that strlen(clp->end) = strlen(clp->end+1)+1;
        // This is needed to ensure that the null terminator is copied.
        delete clp;
        clp = s_FindComment (linestring);
    }

    /* if we have read an organism comment and that's all there was on the
     * line, get rid of the arrow character as well so it doesn't end up 
     * in the sequence data
     */
    if ( linestring [0] == '>') {
        offset = 1;
        while (isspace(linestring[offset])) {
            offset++;
        }
        if (linestring[offset] == 0) {
            linestring[0] = 0;
        }
    }
    /* if the line ends with a number preceded by a space, 
     * and is not the PHYLIP header, truncate it at the space */
    offset = len = strlen(linestring);
    while (offset > 0 && isdigit(linestring[offset - 1])) {
        offset--;
    }
    if (offset != 0 && offset != len && isspace(linestring[offset - 1]) &&
        !s_IsTwoNumbersSeparatedBySpace(linestring)) {
        linestring[offset - 1] = 0;
    }


    /* if the line now contains only space, truncate it */
    if (strspn (linestring, " \t\r") == strlen (linestring)) {
        linestring [0] = 0;
    }
    
}


/* This function determines whether or not a comment describes an organism
 * by looking for org= or organism= inside the brackets.
 */
static bool s_IsOrganismComment(
    TCommentLocPtr clp)
{
    int    len;
    char * cp;
    char * cp_end;

    if (!clp  ||  !clp->start  ||  !clp->end) {
        return false;
    }
    string comment(clp->start, clp->end - clp->start + 1);
    if (!NStr::StartsWith(comment, '[')  ||  !NStr::EndsWith(comment, ']')) {
        return false;
    }
    string interior = comment.substr(1, comment.size() -2);
    string key, value;
    NStr::SplitInTwo(interior, "=", key, value);
    NStr::TruncateSpacesInPlace(key);
    NStr::TruncateSpacesInPlace(value);
    return (!value.empty()  &&  (key == "org"  ||  key == "organism"));
}


/* This function finds an organism comment, which includes the first bracketed
 * comment with org= or organism=, plus any additional bracketed comments.
 * The function returns a pointer to a SCommentLoc structure describing
 * the location of the organism comment.
 */
static TCommentLocPtr 
s_FindOrganismComment (
    char * string)
{
    TCommentLocPtr clp, next_clp;

    if (!string) {
        return nullptr;
    }

    clp = s_FindComment (string);
    while (clp  &&  !s_IsOrganismComment (clp)) {
        char * pos = clp->end;
        clp = s_FindComment (pos);
    }

    if (!clp) {
        return nullptr;
    }

    next_clp = s_FindComment (clp->end);
    while (next_clp  &&  !s_IsOrganismComment(next_clp)) {
        clp->end = next_clp->end;
        delete next_clp;
        next_clp = s_FindComment (clp->end);
    }
    delete next_clp;
    return clp;
}


/* This function removes an organism comment from a line. */
static void 
s_RemoveOrganismCommentFromLine (
    char * string)
{
    TCommentLocPtr clp = s_FindOrganismComment (string);

    while (clp) {
        if (clp->end) {
            memmove(clp->start, clp->end+1, strlen(clp->end));
        }
        delete clp;
        clp = s_FindOrganismComment (string);
    }
}

 
static void s_ReadDefline(
    const char* line, 
    int line_num, 
    TAlignRawFilePtr afrp)
{
    if (!line  ||  !afrp) {
        return;
    }

    if (line[0] == '>') {
        const char* defline_offset = strpbrk(line, " \t");

        if (defline_offset) {
            afrp->deflines = s_AddLineInfo(afrp->deflines, defline_offset, 
                    //line_num, reinterpret_cast<long>(defline_offset));
                    line_num, 0);
        }
        else {
            afrp->deflines = s_AddLineInfo(afrp->deflines, "", line_num, 0);
        }
        afrp->num_deflines++;
        return;
    }


    if (NStr::FindNoCase(line, "org=") != NPOS ||
        NStr::FindNoCase(line, "organism=") != NPOS) {
        afrp->deflines = s_AddLineInfo(afrp->deflines, line, 
                line_num, 0);
        afrp->num_deflines++;
    }
}


/* This function returns a pointer to the sequence in list with the specified
 * ID, unless there is no such sequence, in which case the function returns
 * NULL.
 */
static TAlignRawSeqPtr 
s_FindAlignRawSeqById(
    TAlignRawSeqPtr list,
    char * id)
{
    TAlignRawSeqPtr arsp;

    for (arsp = list; arsp; arsp = arsp->next) {
        if (strcmp (arsp->id, id) == 0) {
            return arsp;
        }
    }
    return nullptr;
}


/* This function finds the position of a given ID in the sequence list,
 * unless the ID is not found in the list, in which case the function returns
 * -1.
 */
static int  
s_FindAlignRawSeqOffsetById(
    TAlignRawSeqPtr list, 
    char * id)
{
    TAlignRawSeqPtr arsp;
    int             offset;

    for (arsp = list, offset = 0; arsp; arsp = arsp->next, offset++) {
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
s_GetAlignRawSeqIDByOffset(
    TAlignRawSeqPtr list,
    int  offset)
{
    TAlignRawSeqPtr arsp;
    int            index;

    arsp = list;
    index = 0;
    while ( arsp  &&  index != offset ) {
        arsp = arsp->next;
        index++;
    }
    if (index == offset  &&  arsp) {
        return arsp->id;
    } else {
        return nullptr;
    }
}


/* This function adds data to a sequence by looking for the specified ID in
 * the list.  If the id is not found, a new sequence with that ID is added to
 * the end of the list.
 * The function returns a pointer to the first item in the list.
 */
static TAlignRawSeqPtr
s_AddAlignRawSeqById(
    TAlignRawSeqPtr list,
    char *  id,
    char *  data,
    int     id_line_num,
    int     data_line_num,
    int     data_line_offset)
{
    TAlignRawSeqPtr arsp;
    TIntLinkPtr     ilp;

    arsp = s_FindAlignRawSeqById (list, id);
    if (!arsp) {
        arsp = SAlignRawSeq::AppendNew(list);
        if (!arsp) {
            return nullptr;
        }
        if (!list) {
            list = arsp;
        }
        arsp->SetId(id);
    }
    arsp->sequence_data = s_AddLineInfo (arsp->sequence_data,
                                       data,
                                       data_line_num,
                                       data_line_offset);
    SIntLink::CreateOrAppend(id_line_num, arsp->id_lines);
    return list;
}


/* This function adds data to the Nth sequence in the sequence list and
 * returns true, unless there aren't that many sequences in the list, in
 * which case the function returns false.
 */
static bool 
s_AddAlignRawSeqByIndex(
    TAlignRawSeqPtr list,
    int     index,
    char *  data,
    int     data_line_num,
    int     data_line_offset)
{
    TAlignRawSeqPtr arsp;
    int            curr;

    curr = 0;
    for (arsp = list; arsp  &&  curr < index; arsp = arsp->next) {
        curr++;
    }
    if (!arsp) {
        return false;
    }
    arsp->sequence_data = s_AddLineInfo (arsp->sequence_data,
                                        data,
                                        data_line_num,
                                        data_line_offset);
    return true;
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
static TLengthListPtr 
s_GetBlockPattern (
    const char* cp)
{
    TLengthListPtr this_pattern;
    int           len;

    this_pattern = SLengthList::AppendNew(nullptr);

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
s_ForecastBlockPattern(
    TLengthListPtr pattern_list,
    TIntLinkPtr    next_offset,
    int            line_start,
    int            block_size)
{
    int  line_counter;
    TLengthListPtr llp;

    line_counter = line_start;
    if (next_offset  &&  next_offset->ival - line_counter < block_size) {
        return -1;
    }
    
    for (llp = pattern_list;
            llp
                &&  (!next_offset  ||  line_counter < next_offset->ival - 1)
                &&  line_counter - line_start < block_size;
            llp = llp->next) {
        if (!llp->lengthrepeats) {
            return -1;
        }
        line_counter += llp->num_appearances;
    }
    if (line_counter - line_start == block_size) {
        /* we've found a combination of groups of similarly sized lines
         * that add up to the desired block size - is the next line blank,
         * or are there additional non-blank lines?
         */
        if (!llp  || llp->lengthrepeats == nullptr) {
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
s_AugmentBlockPatternOffsetList(
    TLengthListPtr pattern_list,
    TIntLinkPtr    offset_list,
    int            block_size)
{
    int            line_counter;
    TLengthListPtr llp;
    TIntLinkPtr    next_offset, prev_offset, new_offset;
    int            forecast_pos;

    prev_offset = nullptr;
    next_offset = offset_list;
    line_counter = 0;
    llp = pattern_list;
    while (llp) {
        if (next_offset  &&  line_counter == next_offset->ival) {
            prev_offset = next_offset;
            next_offset = next_offset->next;
            /* skip past the lines for this block */
            while (line_counter - prev_offset->ival < block_size  &&  llp) {
                line_counter += llp->num_appearances;
                llp = llp->next;
            }
        } else {
            forecast_pos = s_ForecastBlockPattern (llp, next_offset,
                                                 line_counter,
                                                 block_size);
            if (forecast_pos > 0) {
                new_offset = new SIntLink(forecast_pos);
                if (!prev_offset) {
                    new_offset->next = offset_list;
                    offset_list = new_offset;
                } else {
                    new_offset->next = next_offset;
                    prev_offset->next = new_offset;
                }
                prev_offset = new_offset;
                /* skip past the lines for this block */
                while (line_counter - prev_offset->ival < block_size  &&  llp) {
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
 * block.  It returns true if it finds any such lines after the first offset,
 * false otherwise, and reports all instances of unused lines as errors.
 */
static bool
s_FindUnusedLines(
    TLengthListPtr pattern_list,
    TAlignRawFilePtr afrp)
{
    TIntLinkPtr    offset;
    TLengthListPtr llp;
    int            line_counter;
    int            block_line_counter;
    bool          rval = false;
    TLineInfoPtr   line_val;
    int            skip;

    if (!pattern_list  ||  !afrp  ||  !afrp->offset_list  ||  afrp->block_size < 2) {
        return false;
    }
    
    offset = afrp->offset_list;
    llp = pattern_list;
    line_counter = 0;
    line_val = afrp->line_list;
 
    while (!llp  &&  !line_val) {
        while (!llp  &&  !line_val  &&  (!offset  ||  line_counter < offset->ival)) {
            if (llp->lengthrepeats) {
                s_ReportUnusedLine (line_counter,
                                    line_counter + llp->num_appearances - 1,
                                    line_val,
                                    afrp->report_error,
                                    afrp->report_error_userdata);
                if (offset != afrp->offset_list) {
                    rval = true;
                }
            }
            line_counter += llp->num_appearances;
            for (skip = 0;
                 skip < llp->num_appearances  &&  line_val;
                 skip++) {
                line_val = line_val->next;
            }
            llp = llp->next;
        }
        block_line_counter = 0;
        while (block_line_counter < afrp->block_size  &&  llp) {
            block_line_counter += llp->num_appearances;
            line_counter += llp->num_appearances;
            for (skip = 0;
                 skip < llp->num_appearances  &&  line_val;
                 skip++) {
                line_val = line_val->next;
            }
            llp = llp->next;
        }
        if (offset) {
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
s_FindInterleavedBlocks(
    TLengthListPtr pattern_list,
    TAlignRawFilePtr afrp)
{
    TLengthListPtr llp, llp_next;
    TSizeInfoPtr   size_list, best_ptr;
    TIntLinkPtr    new_offset;
    int            line_counter;

    afrp->block_size = 0;
    size_list = nullptr;
    afrp->offset_list = nullptr;
    for (llp = pattern_list; llp; llp = llp->next) {
        llp_next = llp->next;
        if (llp->num_appearances > 1  &&  (!llp_next  ||  !llp_next->lengthrepeats)) {
            size_list = s_AddSizeInfo(size_list, llp->num_appearances);
        }
    }
    if (!size_list) {
        return;
    }
    best_ptr = s_GetMostPopularSizeInfo (size_list);
    if (best_ptr  &&  
            (best_ptr->num_appearances > 1  ||  
             (!size_list->next  &&  size_list->size_value > 1))) {
        afrp->block_size = best_ptr->size_value;
        line_counter = 0;
        for (llp = pattern_list; llp; llp = llp->next) {
            llp_next = llp->next;
            if (llp->num_appearances == afrp->block_size  &&
                    (!llp_next  ||  !llp_next->lengthrepeats)) {
                SIntLink::CreateOrAppend(line_counter, afrp->offset_list);
            }
            line_counter += llp->num_appearances;
        }
        afrp->offset_list = s_AugmentBlockPatternOffsetList (pattern_list,
                                                           afrp->offset_list, 
                                                           afrp->block_size);
    }
    if (s_FindUnusedLines (pattern_list, afrp)) {
        delete afrp->offset_list;
        afrp->offset_list = nullptr;
        afrp->block_size = 0;
    } else {
        afrp->align_format_found = true;
    }
    delete size_list;
}

static bool
s_AfrpInitLineData(
    TAlignRawFilePtr afrp,
    FLineReader readfunc,
    istream& istr)
{
    int overall_line_count = 0;
    bool in_taxa_comment = false;
    string linestring;
    bool dataAvailable = readfunc(istr, linestring);
    TLineInfoPtr last_line = nullptr, next_line = nullptr;

    if (s_IsASN1 (linestring)) {
        s_ReportASN1Error (afrp->report_error, afrp->report_error_userdata);
        return false;
    }

    while (dataAvailable) {
        NStr::TruncateSpacesInPlace(linestring);
        if (!in_taxa_comment  &&  s_FoundStopLine(linestring.c_str())) {
            linestring [0] = 0;
        }
        if (in_taxa_comment) {
            if (strncmp (linestring.c_str(), "end;", 4) == 0) {
                in_taxa_comment = false;
            } 
            linestring [0] = 0;
        } else if (strncmp (linestring.c_str(), "begin taxa;", 11) == 0) {
            linestring [0] = 0;
            in_taxa_comment = true;
            afrp->align_format_found = true;
        }
        next_line = new SLineInfo(linestring.c_str(), overall_line_count, 0);
        if (!last_line) {
            afrp->line_list = next_line;
        } else {
            last_line->next = next_line;
        }
        last_line = next_line;

        dataAvailable = readfunc(istr, linestring);
        if (dataAvailable) {
            overall_line_count ++;
        }
    }
    return true;
}

static void
s_AfrpProcessFastaGap(
    TAlignRawFilePtr afrp,
    TLengthListPtr * patterns,
    bool * last_line_was_marked_id,
    char* linestr_,
    int overall_line_count)
{
    const char* linestr = (const char*)linestr_;
    TLengthListPtr this_pattern = nullptr;
    int len = 0;
    const char* cp;
    TLengthListPtr last_pattern;

    if (!patterns  || !last_line_was_marked_id) {
        return;
    }
    last_pattern = *patterns;

    /* find last pattern in list */
    if (last_pattern) {
        while (last_pattern->next) {
            last_pattern = last_pattern->next;
        }
    }

    /*  ID line
     */
    if (linestr [0] == '>') {
        /* this could be a block of organism lines in a
            * NEXUS file.  If there is no sequence data between
            * the lines, don't process this file for marked IDs.
            */
        if (*last_line_was_marked_id) {
            afrp->marked_ids = false;
//            eFormat = ALNFMT_UNKNOWN;
        }
        else {
            afrp->marked_ids = true;
//            eFormat = ALNFMT_FASTAGAP;
        }
        SIntLink::CreateOrAppend(overall_line_count + 1, afrp->offset_list);
        *last_line_was_marked_id = true;
        return;
    }

    /*  Data line
     */
    *last_line_was_marked_id = false;
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
            
    if (!last_pattern) {
        *patterns = this_pattern;
    } else if (s_DoLengthPatternsMatch (last_pattern, this_pattern)) {
        last_pattern->num_appearances ++;
        delete this_pattern;
    } else {
        last_pattern->next = this_pattern;
    }
}

static TAlignRawFilePtr
s_ReadAlignFileRaw(
    FLineReader readfunc,
    istream& istr,
    CSequenceInfo& sequence_info,
    bool use_nexus_file_info,
    FReportErrorFunction errfunc,
    void* errdata,
    EAlignFormat*        pformat)
{
    char *                   linestring;
    TAlignRawFilePtr         afrp;
    int                      overall_line_count;
    bool                     found_expected_ntax = false;
    bool                     found_expected_nchar = false;
    bool                     found_char_comment = false;
    TLengthListPtr           pattern_list = nullptr;
    TLengthListPtr           this_pattern, last_pattern = nullptr;
    char *                   cp;
    size_t                   len;
    bool                     in_bracketed_comment = false;
    TBracketedCommentListPtr comment_list = nullptr, last_comment = nullptr;
    bool                     last_line_was_marked_id = false;
    TLineInfoPtr             next_line;

    if (!readfunc) {
        return nullptr;
    }

    afrp = new SAlignFileRaw();

    afrp->alphabet_ = sequence_info.Alphabet();
    afrp->report_error = errfunc;
    afrp->report_error_userdata = errdata;

    if (!s_AfrpInitLineData(afrp, readfunc, istr)) {
        delete afrp;
        return nullptr;
    }
        
    for (next_line = afrp->line_list; next_line; next_line = next_line->next) {
        linestring = next_line->data;
        overall_line_count = next_line->line_num-1;

        //s_ReadOrgNamesFromText (linestring, overall_line_count, afrp);
        s_ReadDefline(linestring, overall_line_count, afrp);


        if (*pformat == ALNFMT_FASTAGAP) {
            s_AfrpProcessFastaGap(
                afrp, & pattern_list, & last_line_was_marked_id, linestring, 
                overall_line_count);
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
                found_expected_ntax = true;
                found_expected_nchar = true;
                afrp->align_format_found = true;
           } else {
                s_GetNexusSizeComments (linestring, &found_expected_ntax,
                                        &found_expected_nchar, afrp);
            }
        }
        if (! found_char_comment) {
            if (use_nexus_file_info) {
                found_char_comment = s_UpdateNexusCharInfo (linestring, sequence_info);
          } else {
                found_char_comment = s_CheckNexusCharInfo (
                    linestring, sequence_info, afrp->report_error, afrp->report_error_userdata);
          }
        }

        /* remove complete single-line bracketed comments from line 
         *before checking for multiline bracketed comments */
        s_RemoveCommentFromLine (linestring);

        if (in_bracketed_comment) {
            len = strspn (linestring, " \t\r\n");
            if (last_comment) 
            {
                s_BracketedCommentListAddLine (last_comment, linestring + len,
                                               overall_line_count, len);
            }
            if (strchr (linestring, ']') != nullptr) {
                in_bracketed_comment = false;
            }
            linestring [0] = 0;
        } else if (linestring [0] == '[' && strchr (linestring, ']') == nullptr) {
            in_bracketed_comment = true;
            len = strspn (linestring, " \t\r\n");
            last_comment = SBracketedCommentList::ApendNew(
                comment_list, linestring + len, overall_line_count, len);
            if (comment_list == nullptr) {
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
            last_line_was_marked_id = false;
            this_pattern = s_GetBlockPattern ("");
            if (!pattern_list) {
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
            if (last_line_was_marked_id) {
                afrp->marked_ids = false;
                *pformat = ALNFMT_UNKNOWN;
            }
            else {
                *pformat = ALNFMT_FASTAGAP;
                s_AfrpProcessFastaGap(
                    afrp, & pattern_list, & last_line_was_marked_id, linestring, 
                    overall_line_count);
                continue;
            }
            SIntLink::CreateOrAppend(overall_line_count + 1, afrp->offset_list);
            last_line_was_marked_id = true;
            continue;
        }

        /* default case: some real data at last ...
         */
        last_line_was_marked_id = false;
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
        
        if (!pattern_list) {
            pattern_list = this_pattern;
            last_pattern = this_pattern;
        } else if (s_DoLengthPatternsMatch (last_pattern, this_pattern)) {
            last_pattern->num_appearances ++;
            delete this_pattern;
        } else {
            last_pattern->next = this_pattern;
            last_pattern = this_pattern;
        }
    }
    if (! afrp->marked_ids) {
        s_FindInterleavedBlocks (pattern_list, afrp);
    }
    delete pattern_list;
    delete comment_list;
    return afrp;
}


/* This function analyzes a block to see if it contains, as the first element
 * of any of its lines, one of the sequence IDs already identified.  If the
 * one of the lines does begin with a sequence ID, all of the lines are
 * assumed to begin with sequence IDs and the function returns true, otherwise
 * the function returns false.
 */
static bool 
s_DoesBlockHaveIds(
    TAlignRawFilePtr afrp, 
    TLineInfoPtr     first_line,
    int             num_lines_in_block)
{
    TLineInfoPtr    lip;
    char *          linestring;
    char *          this_id;
    TAlignRawSeqPtr arsp;
    size_t          len;
    int             block_offset;

    if (!afrp->sequences) {
        return true;
    }

    for (lip = first_line, block_offset = 0;
            lip  &&  block_offset < num_lines_in_block;
            lip = lip->next, block_offset++) {
        linestring = lip->data;
        if (linestring) {
            len = strcspn (linestring, " \t\r");
            if (len > 0  &&  len < strlen (linestring)) {
                this_id = new char[len+1];
                strncpy (this_id, linestring, len);
                this_id [len] = 0;
                arsp = s_FindAlignRawSeqById (afrp->sequences, this_id);
                delete[] this_id;
                if (arsp) {
                    return true;
                }
            }
        }
    }
    return false;
}


/* This function analyzes the lines of the block to see if the pattern of
 * the lengths of the whitespace-separated pieces of sequence data matches
 * for all lines within the block.  The function returns true if this is so,
 * otherwise the function returns false.
 */
static bool 
s_BlockIsConsistent(
    TAlignRawFilePtr afrp,
    TLineInfoPtr     first_line,
    int              num_lines_in_block,
    bool            has_ids,
    bool            first_block)
{
    TLineInfoPtr   lip;
    TLengthListPtr list, this_pattern, best;
    int            len, block_offset, id_offset;
    char *         tmp_id;
    bool          rval;
    char *         cp;

    rval = true;
    list = nullptr;
    for (lip = first_line, block_offset = 0;
         lip  &&  block_offset < num_lines_in_block;
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
            tmp_id = new char[len + 1];
            strncpy (tmp_id, cp, len);
            tmp_id [len] = 0;
            id_offset = s_FindAlignRawSeqOffsetById (afrp->sequences, tmp_id);
            if (id_offset != block_offset  &&  ! first_block) {
                rval = false;
                s_ReportInconsistentID (tmp_id, lip->line_num,
                                      afrp->report_error,
                                      afrp->report_error_userdata);
            }
            delete[] tmp_id;
            cp += len;
            cp += strspn (cp, " \t\r");
        }
        this_pattern = s_GetBlockPattern (cp);
        list = s_AddLengthList (list, this_pattern);
    }

    /* Now find the pattern with the most appearances */
    best = nullptr;
    for (this_pattern = list; this_pattern; this_pattern = this_pattern->next) {
        if (this_pattern->num_appearances == 0) {
            continue;
        }
        if (!best  ||  this_pattern->num_appearances > best->num_appearances) {
            best = this_pattern;
        }
    }

    /* now identify and report inconsistent lines */
    for (lip = first_line, block_offset = 0;
            lip  &&  block_offset < num_lines_in_block;
            lip = lip->next, block_offset ++) {
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
            tmp_id = new char[len + 1];
            strncpy (tmp_id, cp, len);
            tmp_id [len] = 0;
            cp += len;
            cp += strspn (cp, " \t\r");
        } else {
            tmp_id = s_GetAlignRawSeqIDByOffset (afrp->sequences, block_offset);
        }
        this_pattern = s_GetBlockPattern (cp);
        if ( ! s_DoLengthPatternsMatch (this_pattern, best)) {
            rval = false;
            s_ReportInconsistentBlockLine (tmp_id, lip->line_num,
                                         afrp->report_error,
                                         afrp->report_error_userdata);
        }
        delete this_pattern;
        if (has_ids) {
            delete[] tmp_id;
        }
    }
    delete list;
    return rval;
}


/* This function processes a block of lines and adds the sequence data from
 * each line in the block to the appropriate sequence in the list.
 */
static void 
s_ProcessBlockLines(
    TAlignRawFilePtr afrp,
    TLineInfoPtr     lines,
    int              num_lines_in_block,
    bool            first_block)
{
    TLineInfoPtr    lip;
    char *          linestring;
    char *          cp;
    char *          this_id;
    int             len;
    int             line_number;
    bool           this_block_has_ids;
    TAlignRawSeqPtr arsp;

    this_block_has_ids = s_DoesBlockHaveIds (afrp, lines, num_lines_in_block);
    s_BlockIsConsistent (afrp, lines, num_lines_in_block, this_block_has_ids,
                       first_block);
    for (lip = lines, line_number = 0;
            lip  &&  line_number < num_lines_in_block;
            lip = lip->next, line_number ++) {
        linestring = lip->data;
        if (linestring) {
            if (this_block_has_ids) {
                len = strcspn (linestring, " \t\r");
                if (first_block &&  len == strlen (linestring)) {
                    /* PHYLIP IDs are exactly ten characters long,
                     * and may not have a space before the start of
                     * the sequence data.
                     */
                    len = 10;
                }
                this_id = new char[len + 1];
                strncpy (this_id, linestring, len);
                this_id [len] = 0;
                cp = linestring + len;
                len = strspn (cp, " \t\r");
                cp += len;
                
                /* Check for duplicate IDs in the first block */
                if (first_block) {
                    arsp = s_FindAlignRawSeqById (afrp->sequences, this_id);
                    if (arsp != NULL) {
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
                delete[] this_id;
            } else {
                if (!s_AddAlignRawSeqByIndex (afrp->sequences, line_number,
                        linestring, lip->line_num, lip->line_offset)) {
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
s_RemoveCommentsFromBlock(
    TLineInfoPtr first_line,
    int num_lines_in_block)
{
    TLineInfoPtr lip;
    int block_offset;

    for (lip = first_line, block_offset = 0;
            lip  &&  block_offset < num_lines_in_block;
            lip = lip->next) {                   
        s_RemoveCommentFromLine (lip->data);
    }
}


/* This function processes the interleaved block of data found at each
 * location listed in afrp->offset_list.
 */
static void 
s_ProcessAlignRawFileByBlockOffsets(
    TAlignRawFilePtr afrp)
{
    int           line_counter;
    TIntLinkPtr   offset_ptr;
    TLineInfoPtr  lip;
    bool         first_block = true;
    bool         in_taxa_comment = false;
 
    if (!afrp) {
        return;
    }
 
    line_counter = 0;
    offset_ptr = afrp->offset_list;
    lip = afrp->line_list;
    while (lip  &&  offset_ptr  &&  (in_taxa_comment  ||  !s_FoundStopLine (lip->data))) {
        if (in_taxa_comment) {
            if (strncmp (lip->data, "end;", 4) == 0) {
                in_taxa_comment = false;
            } 
        } else if (lip->data  &&  strncmp (lip->data, "begin taxa;", 11) == 0) {
            in_taxa_comment = true;
        }
        if (line_counter == offset_ptr->ival) {
            s_RemoveCommentsFromBlock (lip, afrp->block_size);
            s_ProcessBlockLines (afrp, lip, afrp->block_size, first_block);
            first_block = false;
            offset_ptr = offset_ptr->next;
        }
        lip = lip->next;
        line_counter ++;
    }
}


/* The following functions are used to analyze contiguous data. */

static void 
s_CreateSequencesBasedOnTokenPatterns(
    TLineInfoPtr     token_list,
    TIntLinkPtr      offset_list,
    TLengthListPtr anchorpattern,
    TAlignRawFilePtr afrp,
    bool gen_local_ids)
{
    TLineInfoPtr lip;
    int          line_counter;
    TIntLinkPtr  offset_ptr, next_offset_ptr;
    char *       curr_id;
    TSizeInfoPtr sip;
    int          pattern_line_counter;

    static int next_local_id = 1;

    if (!token_list  ||  !offset_list  ||  !anchorpattern  ||  !afrp) {
        return;
    }
    if (!anchorpattern  ||  !anchorpattern->lengthrepeats) {
        return;
    }

    line_counter = 0;
    lip = token_list;
  
    for (offset_ptr = offset_list; offset_ptr  &&  lip; offset_ptr = offset_ptr->next) {
        next_offset_ptr = offset_ptr->next;
        while (line_counter < offset_ptr->ival - 1  &&  lip) {
            lip = lip->next;
            line_counter ++;
        }
        if (lip) {
            if (gen_local_ids) {
                char* replacement_id = new char[32 +strlen(lip->data)];
                sprintf(replacement_id, "lcl|%d %s", next_local_id++, lip->data+1);
                delete[] replacement_id; 
            }
            curr_id = lip->data;
            lip = lip->next;
            line_counter ++;
            for (sip = anchorpattern->lengthrepeats;
                    sip  &&  lip  &&  
                      (!next_offset_ptr  ||  line_counter < next_offset_ptr->ival - 1);
                    sip = sip->next) {
                for (pattern_line_counter = 0;
                        pattern_line_counter < sip->num_appearances  &&  lip  &&  
                          (!next_offset_ptr  ||  line_counter < next_offset_ptr->ival - 1);
                        pattern_line_counter ++) {
                    if (lip->data[0]  !=  ']'  &&  lip->data[0]  != '[') {
                        if (strlen (lip->data) != sip->size_value) {
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
            if (sip  &&  lip ) {
                s_ReportBlockLengthError (curr_id, lip->line_num,
                                        afrp->block_size,
                                        line_counter - offset_ptr->ival,
                                        afrp->report_error,
                                        afrp->report_error_userdata);
            }
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
static TLengthListPtr
s_CreateAnchorPatternForMarkedIDs(
    TAlignRawFilePtr afrp)
{
    TLengthListPtr list = nullptr, best = nullptr, this_pattern = nullptr;
    char *         cp;
    TLineInfoPtr   lip;
    int            curr_seg;

    if (!afrp) {
        return nullptr;
    }

    /* initialize pattern */
    this_pattern = nullptr;

    curr_seg = 0;
    for (lip = afrp->line_list; lip; lip = lip->next) {
        if (s_FoundStopLine(lip->data)) {
            break;
        }
        if (!lip->data  ||  lip->data[0] == ']'  ||  lip->data [0] == '[') {
            continue;
        }
        if (lip->data [0] == '>') {
            if (this_pattern) {
                list = s_AddLengthList (list, this_pattern);
            }
            this_pattern = SLengthList::AppendNew(nullptr);
            this_pattern->num_appearances = 1;
        } 
        else if (this_pattern) {
            /* This section gets rid of base pair number comments */
            cp = lip->data;
            while ( isspace ((unsigned char)*cp)  ||  isdigit ((unsigned char)*cp)) {
                cp++;
            }
            s_AddLengthRepeat (this_pattern, strlen (cp));
        }
    }
    if (this_pattern) {
        list = s_AddLengthList (list, this_pattern);
    }

    /* Now find the pattern with the most appearances for each segment*/
    for (this_pattern = list; this_pattern; 
            this_pattern = this_pattern->next) {
        if (this_pattern->num_appearances == 0) {
            continue;
        }
        if (!best  ||  
                this_pattern->num_appearances > best->num_appearances) {
            best = this_pattern;
        }
    }

    best->next = nullptr;

    if (best != list) {
        this_pattern = list;
        while (this_pattern  &&  this_pattern->next != best) {
            this_pattern = this_pattern->next;
        }
        if (this_pattern) {
            this_pattern->next = nullptr;
            delete list;
            list = nullptr;
        }
    }

    return best;
}


/* This function removes base pair count comments from the data sections
 * for contiguous marked ID sequences.
 */
static void 
s_RemoveBasePairCountCommentsFromData (
    TAlignRawFilePtr afrp)
{
    TIntLinkPtr  this_offset, next_offset;
    TLineInfoPtr lip;
    int          line_count;
    char *       cp;

    if (!afrp  ||  !afrp->offset_list) {
        return;
    }
    this_offset = afrp->offset_list;
    next_offset = this_offset->next;
    lip = afrp->line_list;
    line_count = 0;
    while (lip  &&  this_offset) {
        if (line_count == this_offset->ival) {
            while (lip  &&  (next_offset  ||  line_count < next_offset->ival - 1)) {
                cp = lip->data;
                if (cp) {
                    cp += strspn (cp, " \t\r\n1234567890");
                    if (cp != lip->data) {
                        lip->SetData(cp);
                    }
                }
                line_count ++;
                lip = lip->next;
            }
            this_offset = this_offset->next;
            if (this_offset) {
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
    TAlignRawFilePtr afrp,
    bool gen_local_ids)
{
    TLengthListPtr anchorpattern;
    
    if (!afrp) {
        return;
    }

    s_RemoveBasePairCountCommentsFromData (afrp);
    anchorpattern = s_CreateAnchorPatternForMarkedIDs (afrp);
    if (!anchorpattern  ||  !afrp->offset_list) {
        return;
    }
    s_CreateSequencesBasedOnTokenPatterns (afrp->line_list, afrp->offset_list,
                                         anchorpattern, afrp, gen_local_ids);
    delete anchorpattern;
}


/* The following functions are used for analyzing contiguous sequence data
 * without marked IDs.
 */

/* This function left-shifts a string, character by character. */
static void
s_StringLeftShift(
    char * cp_from,
    char * cp_to)
{
    if (cp_to - cp_from < 0) {
        return;
    }
    if (cp_from == cp_to  ||  !cp_from  ||  !cp_to) {
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
static TLineInfoPtr 
s_RemoveCommentsFromTokens (
    TLineInfoPtr list)
{
    TLineInfoPtr  lip;
    int           num_comment_starts;
    char *        cp_r;
    char *        cp;
    bool         in_comment;

    num_comment_starts = 0;
    in_comment = false;
    for (lip = list;  lip;  lip = lip->next) {
        if (!lip->data) {
            lip->delete_me = true;
        } else {
            cp_r = nullptr;
            for (cp = lip->data; *cp != 0; cp++) {
                if (*cp == ']') {
                    if (!cp_r) {
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
                                cp_r = nullptr;
                            }
                        } else {
                            cp_r = nullptr;
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
                    in_comment = false;
                } else {
                    lip->delete_me = true;
                }
            } else if (num_comment_starts > 0) {
                cp_r = strchr (lip->data, '[');
                if (cp_r) {
                    *cp_r = 0;
                }
                in_comment = true;
            }
            if (lip->data [0] == 0) {
                lip->delete_me = true;
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
static TLineInfoPtr 
s_RemoveNexusCommentsFromTokens (
    TLineInfoPtr list) 
{
    TLineInfoPtr lip = list, start_lip = nullptr, end_lip = nullptr;

    while (lip) {
        if (lip->data  &&  NStr::ToUpper(lip->data)  == "#NEXUS") {
            start_lip = lip;
            end_lip = lip;
            while (end_lip  &&  end_lip->data  && NStr::ToLower(end_lip->data)  !=  "matrix") {
                end_lip = end_lip->next;
            }
            if (end_lip) {
                while (start_lip != end_lip) {
                    start_lip->delete_me = true;
                    start_lip = start_lip->next;
                }
                end_lip->delete_me = true;
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
s_FindMostFrequentlyOccurringTokenLength(
    TSizeInfoPtr list,
    int not_this_size)
{
    TSizeInfoPtr new_list = nullptr;

    for (auto list_ptr = list;  list_ptr;  list_ptr = list_ptr->next) {
        if (not_this_size != list_ptr->size_value) {
            new_list = s_AddSizeInfoAppearances (new_list,
                                               list_ptr->size_value,
                                               list_ptr->num_appearances);
        }
    }
    TSizeInfoPtr best_ptr = s_GetMostPopularSizeInfo (new_list);
    TSizeInfoPtr return_best = nullptr;
    if (best_ptr) {
        return_best = SSizeInfo::AppendNew(nullptr);
        return_best->size_value = best_ptr->size_value;
        return_best->num_appearances = best_ptr->num_appearances;
    }
    delete new_list;
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
s_ExtendAnchorPattern(
    TLengthListPtr anchorpattern,
    TSizeInfoPtr line_lengths)
{
    TSizeInfoPtr sip, sip_next;
    int         best_last_line_length;
    int         anchor_line_length;

    if (!anchorpattern  ||  !anchorpattern->lengthrepeats  ||  line_lengths == NULL) {
       return;
    }

    TSizeInfoPtr last_line_lengths = nullptr;
    anchor_line_length = anchorpattern->lengthrepeats->size_value;

    /* also check to make sure that there's more than one line between
     * this pattern and the next pattern, otherwise the next line is the
     * ID for the next pattern and shouldn't be included in the anchor
     */
    for (sip = line_lengths;  sip;  sip = sip->next) {
        if (*sip == *(anchorpattern->lengthrepeats)) {
                sip_next = sip->next;
            if (sip_next  &&  sip_next->size_value > 0  &&
                    sip_next->size_value != anchor_line_length  &&
                    (!sip_next->next  ||  sip_next->next->size_value != anchor_line_length)) {
                last_line_lengths = s_AddSizeInfo (last_line_lengths,
                                                 sip_next->size_value);
            }
        }
    }
    best_last_line_length = s_GetMostPopularSize (last_line_lengths);
    if (best_last_line_length > 0) {
        s_AddLengthRepeat (anchorpattern, best_last_line_length);
    }
    delete last_line_lengths;
} 


/* This function looks for the most frequently occurring pattern, where a 
 * pattern is considered to be N contiguous tokens of M characters.  The
 * function then checks to see if there is usually a token of a particular
 * length that immediately follows this pattern that is not the ID for the
 * next sequence.  If so, this line length is added to the pattern.
 * The function returns a pointer to this pattern.
 */
static TLengthListPtr 
s_FindMostPopularPattern (
    TSizeInfoPtr list)
{
    TLengthListPtr patternlist = nullptr, newpattern;
    TSizeInfoPtr   popular_line_length;
    int           not_this_length;

    for (auto sip = list;  sip;  sip = sip->next) {
        if (sip->size_value > 0) {
            newpattern = SLengthList::AppendNew(nullptr);
            newpattern->num_appearances = 1;
            newpattern->lengthrepeats = SSizeInfo::AppendNew(nullptr);
            newpattern->lengthrepeats->size_value = sip->size_value;
            newpattern->lengthrepeats->num_appearances = sip->num_appearances;
            patternlist = s_AddLengthList (patternlist, newpattern);
        }
    }
    if (!patternlist) {
        delete newpattern;
        return nullptr;
    }

    TLengthListPtr best = nullptr, index;
    for (index = patternlist;  index;  index = index->next) {
        if (index->lengthrepeats->num_appearances < 2) {
            continue;
        }
        if (!best  ||  best->num_appearances < index->num_appearances) {
            best = index;
        } else if (best->num_appearances == index->num_appearances  &&
                best->lengthrepeats->size_value < index->lengthrepeats->size_value) {
            best = index;
        }
    }

    /* Free data in list before best pattern */
    index = patternlist;
    while (index  &&  index->next != best ) {
         index = index->next;
    }
    if (index) {
        index->next = nullptr;
        delete patternlist;
    }
    /* Free data in list after best pattern */
    if (best) {
        delete best->next;
        best->next = nullptr;
    }

    popular_line_length = s_FindMostFrequentlyOccurringTokenLength (list, 0);

    if (best  &&  best->lengthrepeats  &&  popular_line_length  &&
            best->lengthrepeats->size_value == popular_line_length->size_value) {
        not_this_length = popular_line_length->size_value;
        delete popular_line_length;
        popular_line_length = s_FindMostFrequentlyOccurringTokenLength (list,
                                                        not_this_length);
    }

    if (!best  ||  
            (popular_line_length  &&
            popular_line_length->size_value > best->lengthrepeats->size_value  &&
            popular_line_length->num_appearances > best->num_appearances)) {
        if (!best) {
            best = SLengthList::AppendNew(nullptr);
        }
        best->lengthrepeats = SSizeInfo::AppendNew(nullptr);
        best->lengthrepeats->size_value = popular_line_length->size_value;
        best->lengthrepeats->num_appearances = 1;
    } else {
        /* extend anchor pattern to include best length of last line */
        s_ExtendAnchorPattern (best, list);
    }

    delete popular_line_length;
    return best;
}


/* This function creates an SIntLink list to describe the locations
 * of occurrences of the anchorpattern in the SSizeInfo list.
 * The function returns a pointer to the SIntLink list.
 */
static TIntLinkPtr 
s_CreateOffsetList(
    TSizeInfoPtr list,
    TLengthListPtr anchorpattern)
{
    int          line_counter;
    TIntLinkPtr  offset_list, new_offset;
    TSizeInfoPtr sip;

    if (!list  ||  !anchorpattern) {
        return nullptr;
    }
    line_counter = 0;
    offset_list = nullptr;
    for (sip = list;  sip;  sip = sip->next) {
        if (*sip == *(anchorpattern->lengthrepeats)) {
                SIntLink::CreateOrAppend(line_counter, offset_list);
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
s_ForecastPattern(
    int          line_start,
    int          pattern_length,
    TIntLinkPtr  next_offset,
    int          sip_offset,
    TSizeInfoPtr list)
{
  
    if (!list) {
        return -1;
    }

    for (auto offset = sip_offset; offset < list->num_appearances; offset++) {
        auto line_counter = line_start + offset;
        auto num_chars = list->size_value * (list->num_appearances - offset); 
        auto sip = list;
        while (num_chars < pattern_length  &&
                (!next_offset  ||  line_counter < next_offset->ival)  &&  sip->next) {
            sip = sip->next;
            for (auto end_offset = 0;
                    end_offset < sip->num_appearances
                         &&  num_chars < pattern_length
                        &&  (!next_offset  ||  line_counter < next_offset->ival);
                    end_offset++) {
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
s_AugmentOffsetList(
    TIntLinkPtr    offset_list,
    TSizeInfoPtr   list,
    TLengthListPtr anchorpattern)
{
    int           pattern_length;
    TSizeInfoPtr  sip;
    TIntLinkPtr   prev_offset, next_offset, new_offset;
    int           line_counter, forecast_position, line_skip;
    bool         skipped_previous = false;
    int           num_chars;
    int           num_additional_offsets = 0;
    int           max_additional_offsets = 5000; /* if it's that bad, forget it */

    if (!list  ||  !anchorpattern) {
        return offset_list;
    }

    pattern_length = 0;
    for (sip = anchorpattern->lengthrepeats;  sip;  sip = sip->next) {
        pattern_length += (sip->size_value * sip->num_appearances);
    }
    if (pattern_length == 0) {
        return offset_list;
    }

    prev_offset = nullptr;
    next_offset = offset_list;
    line_counter = 0;
    sip = list;
    while (sip  &&  num_additional_offsets < max_additional_offsets) {
        /* if we are somehow out of synch, don't get caught in infinite loop */
        if (next_offset  &&  line_counter > next_offset->ival) {
            next_offset = next_offset->next;
        } else if (next_offset  &&  line_counter == next_offset->ival) {
            skipped_previous = false;
            prev_offset = next_offset;
            next_offset = next_offset->next;
            /* advance sip and line counter past the end of this pattern */
            num_chars = 0;
            while (num_chars < pattern_length  &&  sip) {
                num_chars += sip->size_value * sip->num_appearances;
                line_counter += sip->num_appearances;
                sip = sip->next;
            }
        } else if (skipped_previous) {
            line_skip = 0;
            while (
                    sip  &&  
                    line_skip < sip->num_appearances  &&  
                    num_additional_offsets < max_additional_offsets  &&  
                    (!next_offset  ||  line_counter < next_offset->ival)) {
                /* see if we can build a pattern that matches the pattern 
                 * length we want
                 */
                forecast_position = s_ForecastPattern (line_counter,
                                                     pattern_length,
                                                     next_offset, line_skip,
                                                     sip);
                if (forecast_position > 0) {
                    new_offset = SIntLink::AppendNew(forecast_position, NULL);
                    num_additional_offsets++;
                    if (!prev_offset) {
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
                    while (num_chars < pattern_length  &&  sip) {
                        for (line_skip = 0;
                            line_skip < sip->num_appearances  &&  
                              num_chars < pattern_length;
                            line_skip++) {
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
            skipped_previous = true;
            line_counter += sip->num_appearances;
            sip = sip->next;
        }
    }
    if (num_additional_offsets >= max_additional_offsets) {
      delete offset_list;
      offset_list = nullptr;
    }
    return offset_list;
}


/* This function finds the most frequently occurring distance between
 * two sequence data blocks and returns that value.
 */
static int  
s_GetMostPopularPatternLength(
    TIntLinkPtr offset_list)
{
    int          line_counter, best_length;
    TSizeInfoPtr pattern_length_list;

    if (!offset_list) {
        return -1;
    }

    line_counter = -1;
    pattern_length_list = nullptr;
    for (auto offset = offset_list;  offset;  offset = offset->next) {
        if (line_counter != -1) {
            pattern_length_list = s_AddSizeInfo (pattern_length_list,
                                               offset->ival - line_counter);
        }
        line_counter = offset->ival;
    }
    best_length = s_GetMostPopularSize (pattern_length_list);
    delete pattern_length_list;
    return best_length;
}


/* This function finds the most frequently appearing number of characters 
 * in a block of sequence data and returns that value.
 */
static int 
s_GetBestCharacterLength(
    TLineInfoPtr token_list,
    TIntLinkPtr  offset_list,
    int          block_length)
{
    int          line_diff, num_chars, best_num_chars;
    TSizeInfoPtr pattern_length_list = nullptr;

    if (!token_list  ||  !offset_list  ||  block_length < 1) {
        return -1;
    }
    /* get length of well-formatted block size */
    TLineInfoPtr lip = token_list;
    TIntLinkPtr prev_offset = nullptr, new_offset = offset_list;
    for ( ; new_offset  &&  lip; new_offset = new_offset->next) {
        if (!prev_offset) {
            /* skip first tokens */
            for (line_diff = 0;
                    line_diff < new_offset->ival  &&  lip;
                    line_diff++) {
                lip = lip->next;
            }
        }
        if (prev_offset) {
            num_chars = 0;
            for (line_diff = 0;
                    line_diff < new_offset->ival - prev_offset->ival  &&  lip;
                    line_diff ++) {
                if (line_diff < new_offset->ival - prev_offset->ival - 1) {
                    num_chars += strlen(lip->data);
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
    if (best_num_chars == 0  &&  pattern_length_list) {
        best_num_chars = pattern_length_list->size_value;
    }
    delete pattern_length_list;
    return best_num_chars;
}


static int  
s_CountCharactersBetweenOffsets(
    TLineInfoPtr list,
    int          distance,
    int          desired_num_chars)
{
    int          line_diff;
    size_t       num_chars, total_chars, pattern_length, num_starts;
    TLineInfoPtr lip;
    TIntLinkPtr  length_list, start_list, start_ptr, length;
    int          start_of_unknown;
    int          num_additional_offsets_needed;

    if (!list  ||  distance == 0  ||  desired_num_chars == 0) {
        return 0;
    }

    /* because the first offset is the start of a known pattern, we should
     * skip to the end of that pattern and start looking for additional
     * offsets
     */
    total_chars = 0;
    for (lip = list, line_diff = 0; 
            lip  &&  line_diff < distance  &&  total_chars < desired_num_chars;
            lip = lip->next, line_diff++) {
        num_chars = strlen (lip->data);
        total_chars += num_chars;
    }
    while (lip  &&  lip->data  && line_diff < distance  &&  NStr::IsBlank(lip->data)) {
        lip = lip->next;
        line_diff++;
    }
    /* skip over line we would need for ID */
    if (lip) {
        lip = lip->next;
        line_diff++;
    }
  
    if (!lip  ||  line_diff == distance) {
        return 0;
    }

    list = lip->next;
    start_of_unknown = line_diff;

    length_list = nullptr;
    total_chars = 0;
    for (lip = list; lip  &&  line_diff < distance; lip = lip->next, line_diff++) {
        num_chars = strlen (lip->data);
        SIntLink::CreateOrAppend(num_chars, length_list);
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
    start_list = nullptr;
    num_starts = 0;
    pattern_length = 0;
    for (start_ptr = length_list, line_diff = start_of_unknown;
            start_ptr  &&  line_diff < distance &&  pattern_length < distance - line_diff ;
            start_ptr = start_ptr->next, line_diff++) {
        num_chars = start_ptr->ival;
        pattern_length = 1;
        length = start_ptr->next;
        while (length  &&  num_chars < desired_num_chars  &&  
                pattern_length + line_diff < distance) {
            num_chars += length->ival;
            pattern_length ++;
            length = length->next;
        }
        if (num_chars == desired_num_chars) {
            SIntLink::CreateOrAppend(line_diff, start_list);
            num_starts ++;
        }
    }

    /* now select best set of start points */
    
    delete length_list;
    delete start_list;
    return 0;
}


/* This function inserts new block locations into the offset_list
 * by looking for likely starts of abnormal patterns.
 */
static void s_InsertNewOffsets(
    TLineInfoPtr token_list,
    TIntLinkPtr  offset_list,
    int          block_length,
    int          best_num_chars,
    const char *       alphabet)
{
    TLineInfoPtr lip;
    TIntLinkPtr  prev_offset, new_offset;
    int          line_diff;

    if (!token_list  ||  !offset_list  ||  block_length < 1  ||  best_num_chars < 1) {
        return;
    }

    lip = token_list;
    prev_offset = nullptr;
    for (new_offset = offset_list; new_offset  &&  lip; new_offset = new_offset->next) {
        if (!prev_offset) {
            /* just advance through tokens */
            for (line_diff = 0; lip  &&  line_diff < new_offset->ival; line_diff ++) {
                lip = lip->next;
            }
        } else {
            if (new_offset->ival - prev_offset->ival == block_length) {
                /* just advance through tokens */
                for (line_diff = 0;
                        lip  &&  line_diff < new_offset->ival - prev_offset->ival;
                        line_diff++) {
                    lip = lip->next;
                }
            } else {
                /* look for intermediate breaks */
                int num_chars = 0;
                for (line_diff = 0;
                        lip  &&  line_diff < new_offset->ival - prev_offset->ival
                          &&  num_chars < best_num_chars;
                        line_diff ++) {
                    num_chars += (lip->data ? strlen(lip->data) : 0);
                    lip = lip->next;
                }
                if (!lip) {
                  return;
                }
                /* set new offset at first line of next pattern */
                line_diff ++;
                lip = lip->next;
                if (line_diff < new_offset->ival - prev_offset->ival) {
                    int line_start = line_diff + prev_offset->ival;
                    /* advance token pointer to new piece */
                    while (lip  &&  line_diff < new_offset->ival - prev_offset->ival) {
                        lip = lip->next;
                        line_diff ++;
                    }
                    /* insert new offset value */
                    auto splice_offset = SIntLink::AppendNew(line_start, nullptr);
                    if (!splice_offset) {
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
    for (line_diff = 0; lip  &&  line_diff < block_length ; line_diff ++) {
        lip = lip->next;
    }

    /* if we have room for one more sequence, or even most of one more sequence, add it */
    if (lip  &&  !s_SkippableString (lip->data)) {
        SIntLink::AppendNew(line_diff + prev_offset->ival, prev_offset);
    }
}


/* This function returns true if the string contains digits, false otherwise */
static bool 
s_ContainsDigits (
    const char *data)
{
    const char *cp;

    if (!data) {
        return false;
    }
    for (cp = data; *cp != 0; cp++) {
        if (isdigit ((unsigned char)(*cp))) {
            return true;
        }
    }
    return false;
}


/* This function processes the alignment file data by dividing the original
 * lines into pieces based on whitespace and looking for patterns of length 
 * in the data. 
 */
static void 
s_ProcessAlignFileRawByLengthPattern (
    TAlignRawFilePtr afrp)
{
    TLineInfoPtr   token_list;
    TLengthListPtr list;
    TLineInfoPtr   lip;
    TLengthListPtr anchorpattern;
    TIntLinkPtr    offset_list;
    int            best_length;
    int            best_num_chars;

    if (!afrp  ||  !afrp->line_list) {
        return;
    }

    token_list = s_BuildTokenList (afrp->line_list);
    token_list = s_RemoveCommentsFromTokens (token_list);
    token_list = s_RemoveNexusCommentsFromTokens (token_list);

    list = SLengthList::AppendNew(nullptr);
    for (lip = token_list; lip  &&  !s_FoundStopLine (lip->data); lip = lip->next) {
        if (s_SkippableString (lip->data)  ||  s_ContainsDigits(lip->data)) {
            s_AddLengthRepeat (list, 0);
        } else {
            s_AddLengthRepeat (list, strlen (lip->data));
        }
    }

    anchorpattern = s_FindMostPopularPattern (list->lengthrepeats);
    if (!anchorpattern  ||  !anchorpattern->lengthrepeats) {
        delete list;
        return;
    }

    /* find anchor patterns in original list, 
     * find distances between anchor patterns 
     */
    offset_list = s_CreateOffsetList (list->lengthrepeats, anchorpattern);
    offset_list = s_AugmentOffsetList (offset_list,
                                     list->lengthrepeats,
                                     anchorpattern);

    /* resolve unusual distances between anchor patterns */
    best_length = s_GetMostPopularPatternLength (offset_list);
    if (best_length < 1  &&  offset_list  &&  offset_list->next) {
        best_length = offset_list->next->ival - offset_list->ival;
    }
    best_num_chars = s_GetBestCharacterLength (token_list, offset_list, best_length);
    s_InsertNewOffsets (token_list, offset_list, best_length, best_num_chars,
                      afrp->alphabet_.c_str());

    /* use token before each anchor pattern as ID, use tokens for distance
     * between anchor patterns for sequence data
     */
    s_CreateSequencesBasedOnTokenPatterns (token_list, offset_list,
                                       anchorpattern, afrp, false);
  
    delete anchorpattern;
    delete list;
    delete token_list;
}


/* This function parses the identifier string used by the alignment file
 * to identify a sequence to find the portion of the string that is actually
 * an ID, as opposed to organism information or definition line.
 */
static char * 
s_GetIdFromString (
    const char* str)
{
    const char * cp;
    char * id;
    int    len;

    if (!str) {
        return nullptr;
    }

    cp = str;
    cp += strspn (str, " >\t");
    len = strcspn (cp, " \t\r\n");
    if (len == 0) {
        return nullptr;
    }
    id = new char[len + 1];
    strncpy (id, cp, len);
    id [ len ] = 0;
    return id;
}



/* This function takes the ID strings read from the file and parses them
 * to obtain a defline (if there is extra text after the ID and/or
 * organism information) and to obtain the actual ID for the sequence.
 */
static bool 
s_ReprocessIds (
    TAlignRawFilePtr afrp)
{
    TStringCountPtr list;
    int             line_num;
    bool           rval = true;
    char *          defline;

    if (!afrp) {
        return false;
    }

    list = nullptr;
    for (auto arsp = afrp->sequences; arsp; arsp = arsp->next) {
        if (arsp->id_lines) {
            line_num = arsp->id_lines->ival;
        } else {
            line_num = -1;
        }
        s_RemoveOrganismCommentFromLine (arsp->id);
        char* id = s_GetIdFromString (arsp->id);
        if (id) {
            arsp->SetId(id);
            delete[] id;
        }
        list = s_AddStringCount (arsp->id, line_num, list);
    }

    for (auto scp = list;  scp;  scp = scp->next) {
        if (scp->num_appearances > 1) {
            rval = false;
            s_ReportRepeatedId (scp, afrp->report_error,
                              afrp->report_error_userdata);
        }
    }
    /* free string count list */
    delete list;
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
s_ReportRepeatedBadCharsInSequence(
    TLineInfoReaderPtr   lirp,
    char *               id,
    const char *         reason,
    FReportErrorFunction report_error,
    void *               report_error_userdata)
{
    int bad_line_num = s_LineInfoReaderGetCurrentLineNumber (lirp);
    int bad_line_offset = s_LineInfoReaderGetCurrentLineOffset (lirp);
    char bad_char = *lirp->curr_line_pos;
    int num_bad_chars = 1;
    int data_position = lirp->data_pos + 1;
    while (s_FindNthDataChar (lirp, data_position) == bad_char) {
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
 * The function will return true if any errors were found, or false
 * if there were no errors.
 */
static bool
s_FindBadDataCharsInSequence(
    TAlignRawSeqPtr      arsp,
    TAlignRawSeqPtr      master_arsp,
    const CSequenceInfo& sequenceInfo,
    FReportErrorFunction report_error,
    void *               report_error_userdata)
{
    TLineInfoReaderPtr lirp, master_lirp;
    int                data_position;
    int                middle_start = 0;
    int                middle_end = 0;
    char               curr_char, master_char;
    bool              found_middle_start;
    bool              rval = false;
    bool              match_not_in_beginning_gap;
    bool              match_not_in_end_gap;

    char               beginning_gap = '-';
    char               middle_gap = '-';
    char               end_gap = '-';

    if (sequenceInfo.BeginningGap().find('-') != string::npos) {
        beginning_gap = sequenceInfo.BeginningGap()[0];
    }

    if (sequenceInfo.MiddleGap().find('-') != string::npos){
        middle_gap = sequenceInfo.MiddleGap()[0];
    }

    if (sequenceInfo.EndGap().find('-') != string::npos){
        end_gap = sequenceInfo.EndGap()[0];
    }

    if (!arsp  ||  !master_arsp) {
        return true;
    }
    if (!arsp->sequence_data) {
        return true;
    }
    lirp = new SLineInfoReader(*arsp->sequence_data);
    if (arsp != master_arsp) {
        if (!master_arsp->sequence_data) {
            return true;
        }
        master_lirp = new SLineInfoReader(*master_arsp->sequence_data);
    } else {
        master_lirp = nullptr;
    }

    auto firstMatchB = sequenceInfo.BeginningGap().find_first_of(sequenceInfo.Match());
    match_not_in_beginning_gap = (firstMatchB == sequenceInfo.BeginningGap().size());

    auto firstMatchE = sequenceInfo.EndGap().find_first_of(sequenceInfo.Match());
    match_not_in_end_gap = (firstMatchE == sequenceInfo.EndGap().size());

    /* First, find middle start and end positions and report characters
     * that are not beginning gap before the middle
     */
    found_middle_start = false;
    data_position = 0;
    curr_char = s_FindNthDataChar (lirp, data_position);
    while (curr_char != 0) {
          if (sequenceInfo.Alphabet().find(curr_char) != string::npos) {
            if (! found_middle_start) {
                middle_start = data_position;
                found_middle_start = true;
            }
            middle_end = data_position + 1;
            data_position ++;
        } else if (! found_middle_start) {
            if (match_not_in_beginning_gap
                    &&  sequenceInfo.Match().find(curr_char) != string::npos) {
                middle_start = data_position;
                found_middle_start = true;
                middle_end = data_position + 1;
                data_position ++;
            } else if (sequenceInfo.BeginningGap().find(curr_char) == string::npos) {
            /* Report error - found character that is not beginning gap
                   in beginning gap */
                data_position = s_ReportRepeatedBadCharsInSequence (lirp,
                                                                  arsp->id,
                                "expect only beginning gap characters here",
                                report_error, report_error_userdata);
                rval = true;
            } else {
                *lirp->curr_line_pos = beginning_gap;
                data_position ++;
            }
        } else {
            if (match_not_in_end_gap
                    &&  sequenceInfo.Match().find(curr_char) != string::npos) {
                middle_end = data_position + 1;
            }
            data_position ++;
        }
        curr_char = s_FindNthDataChar (lirp, data_position);
    }


    if (! found_middle_start) {
        delete lirp;
        s_ReportMissingSequenceData (arsp->id,
                                   report_error, report_error_userdata);
        return true;
    }

    /* Now complain about bad middle characters */
    data_position = middle_start;
    while (data_position < middle_end) {
        curr_char = s_FindNthDataChar (lirp, data_position);
        while (data_position < middle_end  &&
                sequenceInfo.Alphabet().find(curr_char) != string::npos) {
            data_position ++;
            curr_char = s_FindNthDataChar (lirp, data_position);
        } 
        if (curr_char == 0  ||  data_position >= middle_end) {
            /* do nothing, done with middle */
        } else if (sequenceInfo.Missing().find(curr_char) != string::npos) {
            *lirp->curr_line_pos = 'N';
            data_position ++;
        } else if (sequenceInfo.Match().find(curr_char) != string::npos) {
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
                rval = true;
            } else {
                *lirp->curr_line_pos = master_char;
                data_position ++;
            }
        } else if (sequenceInfo.MiddleGap().find(curr_char) != string::npos) {
            *lirp->curr_line_pos = middle_gap;
            data_position ++;
        } else {
            /* Report error - found bad character in middle */
            data_position = s_ReportRepeatedBadCharsInSequence (lirp,
                                      arsp->id,
                                      "expect only sequence, missing, match,"
                                      " and middle gap characters here",
                                      report_error, report_error_userdata);
            rval = true;
        }
    }

    /* Now find and complain about end characters */
    data_position = middle_end;
    curr_char = s_FindNthDataChar (lirp, data_position);
    while (curr_char != 0) {
        if (sequenceInfo.EndGap().find(curr_char) == string::npos) {
        /* Report error - found bad character in middle */
            data_position = s_ReportRepeatedBadCharsInSequence (lirp, arsp->id,
                                      "expect only end gap characters here",
                                      report_error, report_error_userdata);
            rval = true;
        } else {
            *lirp->curr_line_pos = end_gap;
            data_position++;
        }
        curr_char = s_FindNthDataChar (lirp, data_position);
    }
    delete lirp;
    delete master_lirp;
    return rval;
}


/* This function examines each sequence and replaces the special characters
 * and reports bad characters in each one.  The function will return true
 * if any of the sequences contained bad characters or false if no errors
 * were seen.
 */
static bool
s_FindBadDataCharsInSequenceList(
    TAlignRawFilePtr afrp,
    const CSequenceInfo& sequenceInfo)
{
    TAlignRawSeqPtr arsp;
    bool is_bad = false;

    if (!afrp  ||  !afrp->sequences) {
        return true;
    }
    for (arsp = afrp->sequences; arsp; arsp = arsp->next) {
        if (s_FindBadDataCharsInSequence (arsp, afrp->sequences, sequenceInfo,
                                        afrp->report_error,
                                        afrp->report_error_userdata)) {
            is_bad = true;
            break;
        }
    }
    return is_bad;
}


/* This function uses the contents of an SAlignRawFileData structure to
 * create an SAlignmentFile structure with the appropriate information.
 */
bool
s_ConvertDataToOutput(
    TAlignRawFilePtr afrp,
    SAlignmentFile& alignInfo)
{
    TAlignRawSeqPtr   arsp;
    int               index;
    //int             * best_length;
    TLineInfoPtr      lip;
    int               curr_seg;

    if (!afrp  ||  !afrp->sequences) {
        return false;
    }

    auto numDeflines = afrp->num_deflines;
    auto numSequences  = 0;
    for (arsp = afrp->sequences;  arsp;  arsp = arsp->next) {
        numSequences++;
    }
    alignInfo.align_format_found = afrp->align_format_found;

    /* copy in deflines */
    alignInfo.mDeflines.resize(numDeflines);
    for (lip = afrp->deflines, index = 0;
            lip  &&  index < afrp->num_deflines;
            lip = lip->next, index++) {
        alignInfo.mDeflines[index] =  {lip->line_num, string(lip->data ? lip->data : "")};
    }

    /* we need to store length information about different segments separately */
    TSizeInfoPtr lengths = nullptr;
    int best_length = 0;
    
    /* copy in sequence data */
    curr_seg = 0;
    alignInfo.mSequences.resize(numSequences);
    alignInfo.mIds.resize(numSequences);
    for (arsp = afrp->sequences, index = 0;
            arsp  &&  index < numSequences;
            arsp = arsp->next, index++) {
        auto sequenceLi = s_LineInfoMergeAndStripSpaces (arsp->sequence_data);
        alignInfo.mSequences [index] = string( sequenceLi ? sequenceLi : "");
        lengths = s_AddSizeInfo(lengths, alignInfo.mSequences[index].size());

        alignInfo.mIds[index] = string(arsp->id ? arsp->id : "");
    }
    best_length = s_GetMostPopularSize (lengths);
    if (best_length == 0  &&  lengths) {
        best_length = lengths->size_value;
    }   

    curr_seg = 0;
    for (index = 0;  index < alignInfo.NumSequences();  index++) {
        if (alignInfo.mSequences [index].empty()) {
            s_ReportMissingSequenceData (alignInfo.mIds [index].c_str(),
                                       afrp->report_error,
                                       afrp->report_error_userdata);
            return false;
        } else if (alignInfo.mSequences[index].size() != best_length) {
            s_ReportBadSequenceLength (alignInfo.mIds [index].c_str(), best_length,
                                     alignInfo.mSequences [index].size(),
                                     afrp->report_error,
                                     afrp->report_error_userdata);
            return false;
        }
    }

    if (afrp->expected_num_sequence > 0  &&  
            afrp->expected_num_sequence != alignInfo.NumSequences()) {
        s_ReportIncorrectNumberOfSequences (afrp->expected_num_sequence,
                                          alignInfo.NumSequences(),
                                          afrp->report_error,
                                          afrp->report_error_userdata);
        return false;
    }
    if (afrp->expected_sequence_len > 0  &&  
            afrp->expected_sequence_len != best_length) {
        s_ReportIncorrectSequenceLength (afrp->expected_sequence_len,
                                       best_length,
                                       afrp->report_error,
                                       afrp->report_error_userdata);
        return false;
    }
    
    delete lengths;
    return true;
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
bool 
ReadAlignmentFile(
    FLineReader readfunc,
    istream& istr,
    FReportErrorFunction errfunc,
    void * erroruserdata,
    CSequenceInfo& sequence_info,
    bool gen_local_ids,
    SAlignmentFile& alignmentInfo)
{
    TAlignRawFilePtr afrp;
    EAlignFormat format = ALNFMT_UNKNOWN;

    if (sequence_info.Alphabet().empty()) {
        return false;
    }
    
    afrp = s_ReadAlignFileRaw ( readfunc, istr, sequence_info,
                                false,
                                errfunc, erroruserdata, &format);
    if (!afrp) {
        return false;
    }

    if (afrp->block_size > 1) {
        s_ProcessAlignRawFileByBlockOffsets (afrp);
    } else if (afrp->marked_ids) {
        s_ProcessAlignFileRawForMarkedIDs (afrp, gen_local_ids);
    } else {
        s_ProcessAlignFileRawByLengthPattern (afrp);
    }

    s_ReprocessIds (afrp);

    if (s_FindBadDataCharsInSequenceList (afrp, sequence_info)) {
        delete afrp;
        return false;
    } 

    bool result = s_ConvertDataToOutput (afrp, alignmentInfo);
    delete afrp;
    return result;
}

END_NCBI_SCOPE

#ifndef _ALN_DATA_HPP_
#define _ALN_DATA_HPP_

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
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

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
        int line_offset_) :
        data(nullptr), line_num(line_num_ + 1), line_offset(line_offset_),
        delete_me(false), next(nullptr)
    {
        SetData(data_);
    };
    SLineInfo(
        const SLineInfo& rhs) :
        data(nullptr), line_num(rhs.line_num), line_offset(rhs.line_offset),
        delete_me(rhs.delete_me), next(nullptr)
    {
        SetData(rhs.data);
    };

    ~SLineInfo();

    static SLineInfo*
    sAddLineInfo(
        SLineInfo* lineInfoPtr,
        const char* pch,
        int lineNum,
        int lineOffset);

    void SetData(
        const char* data_)
    {
        if (data) {
            delete[] data;
            data = nullptr;
        }
        if (data_) {
            data = new char[strlen(data_) + 1];
            strcpy(data, data_);
        }
    };

    static SLineInfo*
    AppendNew(
        SLineInfo* list, const char* data_, int line_num_, int line_offset_ = 0)
    {
        auto pNew = new SLineInfo(data_, line_num_, line_offset_);
        return tAppendNew<SLineInfo>(list, pNew);
    };

    char* data;
    int line_num;
    int line_offset;
    bool delete_me;
    SLineInfo* next;
};
using TLineInfoPtr = SLineInfo * ;

//  =============================================================================
struct SLineInfoReader {
    //  =============================================================================
    SLineInfoReader(
        SLineInfo& lineList) : first_line(&lineList)
    {
        first_line = &lineList;
        Reset();
    };

    void
    Reset();

    void AdvancePastSpace();
 
    TLineInfoPtr first_line;
    TLineInfoPtr curr_line;
    char* curr_line_pos;
    int data_pos;
};
using TLineInfoReaderPtr = SLineInfoReader * ;

//  ============================================================================
struct SStringCount {
    //  ============================================================================
    SStringCount(
        const string& str,
        int lineNum) :
        mString(str)
    {
        mLineNumbers.push_back(lineNum);
    };

    void AddOccurrence(
        int lineNum) {
        mLineNumbers.push_back(lineNum);
    };

    string mString;
    list<int> mLineNumbers;
};

//  ============================================================================
struct SSizeInfo {
    //  ============================================================================
    SSizeInfo(
        int size_value_ = 0, int num_appearances_ = 1, SSizeInfo* next_ = nullptr) :
        size_value(size_value_), num_appearances(num_appearances_), next(next_)
    {};

    bool operator ==(const SSizeInfo& rhs) {
        return (size_value == rhs.size_value  &&
            num_appearances == rhs.num_appearances);
    };

    ~SSizeInfo();
 
    void LongDelete();
 
    int Size() const {
        int size = 0;
        auto siPtr = next;
        while (siPtr) {
            size++;
            siPtr = siPtr->next;
        }
        return size;
    }

    static SSizeInfo*
    AppendNew(
        SSizeInfo* list) {
        return tAppendNew<SSizeInfo>(list, new SSizeInfo);
    };

    static SSizeInfo* 
    AddSizeInfo(
        SSizeInfo* pList,
        int  sizeValue,
        int numAppearances = 1);

    int size_value;
    int num_appearances;
    SSizeInfo* next;
};
using TSizeInfoPtr = SSizeInfo * ;

//  =============================================================================
struct SLengthList {
//  =============================================================================

    SLengthList() : lengthrepeats(nullptr), num_appearances(0), next(nullptr) {};

    ~SLengthList();
 
    void LongDelete();
 
    int Size() const {
        int size = 0;
        auto siPtr = next;
        while (siPtr) {
            size++;
            siPtr = siPtr->next;
        }
        return size;
    }

    static SLengthList*
        AppendNew(
            SLengthList* list) {
        return tAppendNew<SLengthList>(list, new SLengthList);
    };

    TSizeInfoPtr lengthrepeats;
    int num_appearances;
    SLengthList* next;
};
using TLengthListPtr = SLengthList * ;
using TLengthListData = SLengthList;

//  ============================================================================
struct SCommentLoc {
    //  ============================================================================
    SCommentLoc(
        const char* start_ = nullptr,
        const char* end_ = nullptr,
        SCommentLoc* next_ = nullptr) : start(start_), end(end_), next(next_)
    {};

    ~SCommentLoc() {
        delete next;
    };

    const char* start;
    const char* end;
    SCommentLoc * next;
};
using TCommentLocPtr = SCommentLoc * ;

//  ============================================================================
struct SBracketedCommentList {
    //  ============================================================================
    SBracketedCommentList(
        const char* str, int lineNum, int lineOffset) :
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
using TBracketedCommentListPtr = SBracketedCommentList * ;

//  ============================================================================
struct SAlignRawSeq {
    //  ============================================================================
    SAlignRawSeq() :
        sequence_data(nullptr), next(nullptr)
    {};
    ~SAlignRawSeq() {
        delete sequence_data;
        delete next;
    };

    static SAlignRawSeq*
    AppendNew(
        SAlignRawSeq* list)
    {
        return tAppendNew<SAlignRawSeq>(list, new SAlignRawSeq);
    };

    static SAlignRawSeq* sFindSeqById(
        SAlignRawSeq* pList,
        const string& id);

    static SAlignRawSeq*
    sAddSeqById(
        SAlignRawSeq* pList,
        const string&  id,
        char* pData,
        int idLineNum,
        int dataLineNum,
        int dataLineOffset);

    string mId;
    TLineInfoPtr          sequence_data;
    list<int> mIdLines;
    struct SAlignRawSeq * next;
};
using TAlignRawSeqPtr = SAlignRawSeq * ;

//  ============================================================================
struct SAlignFileRaw {
    //  ============================================================================
    SAlignFileRaw() :
        marked_ids(false), line_list(nullptr),
        block_size(0),
        sequences(nullptr),
        alphabet_(""), expected_num_sequence(0),
        expected_sequence_len(0), align_format_found(false)
    {};

    ~SAlignFileRaw() {
        delete line_list;
        delete sequences;
        //delete offset_list;
    };

    TLineInfoPtr         line_list;
    TAlignRawSeqPtr      sequences;
    list<SLineInfo> mDeflines;
    bool                 marked_ids;
    int                  block_size;
    list<int>            mOffsetList;
    string               alphabet_;
    int                  expected_num_sequence;
    int                  expected_sequence_len;
    char                 align_format_found;
};
using TAlignRawFilePtr = SAlignFileRaw * ;

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _ALN_DATA_HPP_

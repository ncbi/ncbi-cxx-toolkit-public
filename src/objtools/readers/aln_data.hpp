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


    ~SLineInfo() {
        //best for a file with 1,000,000 lines, limts recursion depth to 2,000.
        const int BLOCKSIZE(1000);
        SLineInfo* farNext = this;
        for (auto i = 0; i < BLOCKSIZE && farNext; ++i) {
            if (farNext) {
                farNext = farNext->next;
            }
        }
        if (!farNext) {
            delete[] data;
            delete next;
        }
        else {
            auto beyondFarNext = farNext->next;
            farNext->next = nullptr;
            delete next;
            delete beyondFarNext;
        }
    };

    static SLineInfo*
    sAddLineInfo(
        SLineInfo* lineInfoPtr,
        const char* pch,
        int lineNum,
        int lineOffset)
    {
        if (!pch) {
            return lineInfoPtr;
        }
        SLineInfo* liPtr = new SLineInfo(pch, lineNum - 1, lineOffset); //hack alert!
        if (!lineInfoPtr) {
            lineInfoPtr = liPtr;
        }
        else {
            SLineInfo* p = lineInfoPtr;
            while (p  &&  p->next) {
                p = p->next;
            }
            p->next = liPtr;
        }
        return lineInfoPtr;
    };

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
        Reset() {
        curr_line = first_line;

        while (curr_line && !curr_line->data) {
            curr_line = curr_line->next;
        }
        if (!curr_line) {
            curr_line_pos = nullptr;
            data_pos = -1;
        }
        else {
            curr_line_pos = curr_line->data;
            AdvancePastSpace();
            data_pos = (curr_line_pos ? 0 : -1);
        }
    };

    void AdvancePastSpace() {
        if (!curr_line_pos) {
            return;
        }
        while (isspace((unsigned char)*curr_line_pos) || *curr_line_pos == 0) {
            while (isspace((unsigned char)*curr_line_pos)) {
                curr_line_pos++;
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

    ~SSizeInfo() {
        const int BLOCKSIZE(100);
        auto size = Size();
        if (size > BLOCKSIZE) {
            LongDelete();
        }
        else {
            delete next;
        }
    };

    void LongDelete() {
        const int BLOCKSIZE(100);
        SSizeInfo* farNext = this;
        list<SSizeInfo*> shortDeleteBlocks;
        shortDeleteBlocks.push_back(farNext);
        while (farNext) {
            for (auto i = 0; i < BLOCKSIZE && farNext; ++i) {
                if (farNext) {
                    farNext = farNext->next;
                }
            }
            if (farNext) {
                auto beyondFarNext = farNext->next;
                farNext->next = nullptr;
                shortDeleteBlocks.push_back(farNext);
                farNext = beyondFarNext;
            }
        }
        for (auto shortDeletePtr : shortDeleteBlocks) {
            delete shortDeletePtr;
        }
    };


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
        int numAppearances = 1) 
    {
        SSizeInfo* p, *pLast = nullptr;

        for (p = pList;  p  &&  p->size_value != sizeValue;  p = p->next) {
            pLast = p;
        }
        if (p == nullptr) {
            p = new SSizeInfo(sizeValue, numAppearances);
            if (!pLast) {
                pList = p;
            } 
            else {
                pLast->next = p;
            }
        } 
        else {
            p->num_appearances += numAppearances;
        }
        return pList;
    };

    int size_value;
    int num_appearances;
    SSizeInfo* next;
};
using TSizeInfoPtr = SSizeInfo * ;

//  =============================================================================
struct SLengthList {
    //  =============================================================================
    SLengthList() : lengthrepeats(nullptr), num_appearances(0), next(nullptr)
    {};


    ~SLengthList() {
        const int BLOCKSIZE(100);
        auto size = Size();
        if (size > BLOCKSIZE) {
            LongDelete();
        }
        else {
            //delete next;
        }
    };

    void LongDelete() {
        const int BLOCKSIZE(100);
        SLengthList* farNext = next;
        list<SLengthList*> shortDeleteBlocks;
        while (farNext) {
            shortDeleteBlocks.push_back(farNext);
            for (auto i = 0; i < BLOCKSIZE && farNext; ++i) {
                if (farNext) {
                    farNext = farNext->next;
                }
            }
            if (farNext) {
                auto beyondFarNext = farNext->next;
                farNext->next = nullptr;
                farNext = beyondFarNext;
            }
        }
        for (auto shortDeletePtr : shortDeleteBlocks) {
            delete shortDeletePtr;
        }
    };


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
        const string& id)
    {
        for (auto pSeq = pList; pSeq; pSeq = pSeq->next) {
            if (id == pSeq->mId) {
                return pSeq;
            }
        }
        return nullptr;
    };

    static SAlignRawSeq*
    sAddSeqById(
        SAlignRawSeq* pList,
        const string&  id,
        char* pData,
        int idLineNum,
        int dataLineNum,
        int dataLineOffset)
    {
        auto arsp = SAlignRawSeq::sFindSeqById(pList, id);

        if (!arsp) {
            arsp = SAlignRawSeq::AppendNew(pList);
            if (!arsp) {
                return nullptr;
            }
            if (!pList) {
                pList = arsp;
            }
            arsp->mId = id;
        }
        arsp->sequence_data = SLineInfo::sAddLineInfo(
            arsp->sequence_data, pData, dataLineNum, dataLineOffset);
        arsp->mIdLines.push_back(idLineNum);
        return pList;
    }


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

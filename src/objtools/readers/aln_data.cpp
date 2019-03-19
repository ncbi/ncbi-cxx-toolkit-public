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
#include <objtools/readers/reader_error_codes.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/alnread.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include "aln_data.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ----------------------------------------------------------------------------
SLineInfo::~SLineInfo() {
//  ----------------------------------------------------------------------------
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


//  ----------------------------------------------------------------------------
TLineInfoPtr
SLineInfo::sAddLineInfo(
    SLineInfo* lineInfoPtr,
    const char* pch,
    int lineNum,
    int lineOffset)
//  ----------------------------------------------------------------------------
{
    if (!pch) {
        return lineInfoPtr;
    }
    TLineInfoPtr liPtr = new SLineInfo(pch, lineNum - 1, lineOffset); //hack alert!
    if (!lineInfoPtr) {
        lineInfoPtr = liPtr;
    }
    else {
        auto p = lineInfoPtr;
        while (p  &&  p->next) {
            p = p->next;
        }
        p->next = liPtr;
    }
    return lineInfoPtr;
};

//  ----------------------------------------------------------------------------
void
SLineInfoReader::Reset()
//  ----------------------------------------------------------------------------
{
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

//  ----------------------------------------------------------------------------
void 
SLineInfoReader::AdvancePastSpace() 
//  ----------------------------------------------------------------------------
{
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

//  ----------------------------------------------------------------------------
SSizeInfo::~SSizeInfo() 
//  ----------------------------------------------------------------------------
{
    const int BLOCKSIZE(100);
    auto size = Size();
    if (size > BLOCKSIZE) {
        LongDelete();
    }
    else {
        delete next;
    }
};

//  ----------------------------------------------------------------------------
void 
SSizeInfo::LongDelete() 
//  ----------------------------------------------------------------------------
{
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

//  ----------------------------------------------------------------------------
TSizeInfoPtr
SSizeInfo::AddSizeInfo(
    SSizeInfo* pList,
    int  sizeValue,
    int numAppearances) 
//  ----------------------------------------------------------------------------
{
    SSizeInfo *p = nullptr,  *pLast = nullptr;

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

//  ----------------------------------------------------------------------------
SLengthList::~SLengthList() 
//  ----------------------------------------------------------------------------
{
    const int BLOCKSIZE(100);
    auto size = Size();
    if (size > BLOCKSIZE) {
        LongDelete();
    }
    else {
        delete next;
    }
};

//  ----------------------------------------------------------------------------
void 
SLengthList::LongDelete() 
//  ----------------------------------------------------------------------------
{
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

//  ----------------------------------------------------------------------------
SAlignRawSeq* 
SAlignRawSeq::sFindSeqById(
    SAlignRawSeq* pList,
    const string& id)
//  ----------------------------------------------------------------------------
{
    for (auto pSeq = pList; pSeq; pSeq = pSeq->next) {
        if (id == pSeq->mId) {
            return pSeq;
        }
    }
    return nullptr;
};

//  ----------------------------------------------------------------------------
SAlignRawSeq*
SAlignRawSeq::sAddSeqById(
    SAlignRawSeq* pList,
    const string&  id,
    char* pData,
    int idLineNum,
    int dataLineNum,
    int dataLineOffset)
//  ----------------------------------------------------------------------------
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

END_SCOPE(objects)
END_NCBI_SCOPE

#ifndef _ALNREAD_HPP_
#define _ALNREAD_HPP_

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

#if defined (WIN32)
#    define ALIGNMENT_CALLBACK __stdcall
#else
#    define ALIGNMENT_CALLBACK
#endif

BEGIN_NCBI_SCOPE

using FLineReader = bool (ALIGNMENT_CALLBACK *)(istream&, string&);

typedef enum {
    eAlnErr_Unknown = -1,
    eAlnErr_NoError = 0,
    eAlnErr_Fatal,
    eAlnErr_BadData,
    eAlnErr_BadFormat
} EAlnErr;

//  =====================================================================
class CErrorInfo
//  =====================================================================
{
public:
    static const int NO_LINE_NUMBER = -1;

    CErrorInfo(
        EAlnErr category = eAlnErr_Unknown,
        int lineNumber = NO_LINE_NUMBER,
        const string& id = "",
        const string& message = ""):
        mCategory(category),
        mLineNumber(lineNumber),
        mId(id),
        mMessage(message)
    {};

    string
    Message() const { return mMessage; };

    string
    Id() const { return mId; };

    int
    LineNumber() const { return mLineNumber; };

    EAlnErr
    Category() const { return mCategory; };

protected:
    EAlnErr mCategory;
    int mLineNumber;
    string mId;
    string mMessage;
};
using FReportErrorFunction = void (ALIGNMENT_CALLBACK *)(const CErrorInfo&, void*);

//  =============================================================================
class CSequenceInfo {
//  =============================================================================
public:
    CSequenceInfo(
        const string& alphabet,
        const string& match,
        const string& missing,
        const string& beginningGap,
        const string& middleGap,
        const string& endGap):
        mAlphabet(alphabet),
        mMatch(match),
        mMissing(missing),
        mBeginningGap(beginningGap),
        mMiddleGap(middleGap),
        mEndGap(endGap)
    {};

    const string&
    Alphabet() const { return mAlphabet; };

    const string&
    Match() const { return mMatch; };

    void
    SetMatch(
        char c) { mMatch = string(1, c); };

    const string&
    Missing() const { return mMissing; };

    void
    SetMissing(
        char c) { mMissing = string(1, c); };

    const string&
    BeginningGap() const { return mBeginningGap; };

    void
    SetBeginningGap(
        char c) { mBeginningGap = string(1, c); };

    const string&
    MiddleGap() const { return mMiddleGap; };

    void
    SetMiddleGap(
        char c) { mMiddleGap = string(1, c); };

    const string&
    EndGap() const { return mEndGap; };

    void
    SetEndGap(
        char c) { mEndGap = string(1, c); };

protected:
    string mMatch;
    string mAlphabet;
    string mMissing;
    string mBeginningGap;
    string mMiddleGap;
    string mEndGap;
}; 


//  ============================================================================
class SAlignmentFile {
//  ============================================================================
public:
    SAlignmentFile():
        num_sequences(0),
        num_organisms(0),
        num_deflines(0),
        num_segments(0),
        ids(nullptr),
        sequences(nullptr),
        organisms(nullptr),
        deflines(nullptr)
    {};

    ~SAlignmentFile()
    {
        for (int i=0; i < num_sequences; ++i) {
            delete[] ids[i];
            delete[] sequences[i];
        }
        delete[] ids;
        delete[] sequences;

        for (int i=0; i < num_organisms; ++i) {
            delete[] organisms[i];
        }
        delete[] organisms;
        
        for (int i=0; i < num_deflines; ++i) {
            delete[] deflines[i];
        }
        delete[] deflines;
    };

    int num_sequences;
    int num_organisms;
    int num_deflines;
    int num_segments;
    char ** ids;
    char ** sequences;
    char ** organisms;
    char** deflines;
    char align_format_found;
protected:
};

NCBI_XOBJREAD_EXPORT 
bool ReadAlignmentFile(
  FLineReader    readfunc,      /* function for reading lines of 
                                       * alignment file
                                       */
    istream& istr,  // file object for readfunc to operate on
  FReportErrorFunction errfunc,       /* function for reporting errors */
  void *               erroruserdata, /* data to be passed back each time
                                       * errfunc is invoked
                                       */
  CSequenceInfo&       sequence_info, /* structure containing sequence
                                       * alphabet and special characters
                                       */
  bool                gen_local_ids,  /* flag indicating whether input IDs
                                       * should be replaced with unique
                                       * local IDs
                                       */ 
    SAlignmentFile& alignmentInfo
);

END_NCBI_SCOPE

#endif // _ALNREAD_HPP_

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
BEGIN_SCOPE(objects);

class ILineErrorListener;

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
    size_t 
    NumDeflines() const { return mDeflines.size(); };

    size_t
    NumSequences() const { return mSequences.size(); };


    struct SDeflineInfo {
        int line_num;
        string data;
    };
    using TDeflineInfo = SDeflineInfo;

    vector<string> mIds;
    vector<string> mSequences;
    vector<TDeflineInfo> mDeflines;
    char align_format_found;
};

NCBI_XOBJREAD_EXPORT 
bool ReadAlignmentFile(
    istream& istr,
    bool gen_local_ids,
    bool use_nexus_info,
    CSequenceInfo& sequence_info,
    SAlignmentFile& alignmentInfo,
    ILineErrorListener* pErrorListener=nullptr);

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _ALNREAD_HPP_

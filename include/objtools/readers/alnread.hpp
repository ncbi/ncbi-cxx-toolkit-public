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

#include <objtools/readers/aln_formats.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class ILineErrorListener;

//  =============================================================================
struct SLineInfo {
//  =============================================================================
    string mData;
    int mNumLine;
};


//  =============================================================================
class CSequenceInfo {
//  =============================================================================
public:
    CSequenceInfo()
    {};

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

    CSequenceInfo&
    SetAlphabet(
        const string& alphabet) { mAlphabet = alphabet; return *this; }

    const string&
    Match() const { return mMatch; };

    CSequenceInfo&
    SetMatch(
        const string& c) { mMatch = c; return *this; };

    const string&
    Missing() const { return mMissing; };

    CSequenceInfo&
    SetMissing(
        const string& c) { mMissing = c; return *this; };

    const string&
    BeginningGap() const { return mBeginningGap; };

    CSequenceInfo&
    SetBeginningGap(
        const string& c) { mBeginningGap = c; return *this; };

    const string&
    MiddleGap() const { return mMiddleGap; };

    CSequenceInfo&
    SetMiddleGap(
        const string& c) { mMiddleGap = c; return *this; };

    const string&
    EndGap() const { return mEndGap; };

    CSequenceInfo&
    SetEndGap(
        const string& c) { mEndGap = c; return *this; };

protected:
    string mAlphabet;
    string mMatch;
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


    using TLineInfo = SLineInfo;

    vector<TLineInfo> mIds; 
    vector<string> mSequences;
    vector<TLineInfo> mDeflines;
};

NCBI_XOBJREAD_EXPORT 
bool ReadAlignmentFile(
    istream& istr,
    bool gen_local_ids,
    bool use_nexus_info,
    CSequenceInfo& sequence_info,
    SAlignmentFile& alignmentInfo,
    ILineErrorListener* pErrorListener=nullptr);

NCBI_XOBJREAD_EXPORT 
bool ReadAlignmentFile(
    istream& istr,
    CSequenceInfo& sequenceInfo,
    SAlignmentFile& alignmentInfo);

NCBI_XOBJREAD_EXPORT 
bool ReadAlignmentFile(
    istream& istr,
    EAlignFormat&,
    CSequenceInfo& sequenceInfo,
    SAlignmentFile& alignmentInfo);

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _ALNREAD_HPP_

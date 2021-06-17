#ifndef _ALN_PEEK_AHEAD_HPP_
#define _ALN_PEEK_AHEAD_HPP_

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

//  ----------------------------------------------------------------------------
class CLineInput
//  ----------------------------------------------------------------------------
{
public:
    CLineInput() {};
    virtual ~CLineInput() {};

    virtual bool
    ReadLine(
        string& line,
        int& lineNum) = 0;
};

//  ----------------------------------------------------------------------------
class CPeekAheadStream:
    public CLineInput
//  ----------------------------------------------------------------------------
{
public:
    CPeekAheadStream(
        istream& istr): mIstr(istr) {};

    ~CPeekAheadStream() {};

    bool
    PeekLine(
        string& str)
    {
        if (getline(mIstr, str)) {
            mPeeked.push_back({str,mLineNum++});
            return true;
        }
        return false;
    };

    bool
    ReadLine(
        string& str,
        int& lineNum)
    {
        if (!mPeeked.empty()) {
            str = mPeeked.front().mData;
            lineNum = mPeeked.front().mNumLine;
            mPeeked.pop_front();
            return true;
        }

        str.clear();
        if (getline(mIstr, str)) {
            lineNum = mLineNum++;
            return true;
        }

        return false;
    };

protected:
    int mLineNum = 1;
    std::istream& mIstr;
    list<SLineInfo> mPeeked;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _ALN_PEEK_AHEAD_HPP_

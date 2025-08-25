#ifndef _ALN_UTIL_HPP_
#define _ALN_UTIL_HPP_

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
 * Authors: Frank Ludwig
 *
 */
#include <corelib/ncbistd.hpp>
#include <objtools/readers/alnread.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

class CLineInput;
class CSequenceInfo;
struct SAlignFileRaw;

//  ============================================================================
namespace AlnUtil
//  ============================================================================
{
    void
    ProcessDefline(
        const string& defLine,
        string& seqId,
        string& defLineInfo);

    void
    ProcessDataLine(
        const string& dataLine,
        string& seqId,
        string& seqData,
        int& offset);

    void
    ProcessDataLine(
        const string& dataLine,
        string& seqId,
        string& seqData);

    void CheckId(
        const string& seqId,
        const vector<SLineInfo>& orderedIds,
        size_t idCount,
        int lineNum,
        bool firstBlock);

    void
    StripBlanks(
        const string& line,
        string& stipped);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _ALN_UTIL_HPP_

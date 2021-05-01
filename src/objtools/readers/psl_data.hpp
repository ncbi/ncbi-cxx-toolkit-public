/*  $Id$
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
 * Author: Frank Ludwig
 *
 * File Description:
 *   PSL intermediate representation
 *
 */

#ifndef OBJTOOLS_READERS___PSL_DATA__HPP
#define OBJTOOLS_READERS___PSL_DATA__HPP

#include <objects/seqloc/Na_strand.hpp>
//#include <objtools/readers/message_listener.hpp>

#include <objtools/readers/psl_reader.hpp>
#include "reader_message_handler.hpp"

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


//  ----------------------------------------------------------------------------
struct SAlignSegment
//  ----------------------------------------------------------------------------
{
    int mLen;
    int mStartQ;
    int mStartT;
    ENa_strand mStrandQ;
    ENa_strand mStrandT;
};

//  ----------------------------------------------------------------------------
class CPslData
//  ----------------------------------------------------------------------------
{
public:
    CPslData(
        CReaderMessageHandler* = nullptr);

    //
    //  Input/output:
    //
    void Initialize(
        const CPslReader::TReaderLine& );

    void Dump(
        ostream& ostr);

    void ExportToSeqAlign(
        CPslReader::SeqIdResolver,
        CSeq_align& seqAlign);

private:
    void xReset();

    void xConvertBlocksToSegments(
        vector<SAlignSegment>&) const;

    //
    // Data:
    //
private:
    CReaderMessageHandler* mpEL;
    int mFirstDataColumn = -1;

    int mMatches;
    int mMisMatches;
    int mRepMatches;
    int mCountN;
    int mNumInsertQ;
    int mBaseInsertQ;
    int mNumInsertT;
    int mBaseInsertT;
    ENa_strand mStrandT;
    string mNameQ;
    int mSizeQ;
    int mStartQ;
    int mEndQ;
    string mNameT;
    int mSizeT;
    int mStartT;
    int mEndT;
    int mBlockCount;
    vector<int> mBlockSizes;
    vector<int> mBlockStartsQ;
    vector<int> mBlockStartsT;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___PSL_DATA__HPP

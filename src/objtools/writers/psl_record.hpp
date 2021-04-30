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
 * Authors: Frank Ludwig
 *
 * File Description:  Write alignment file
 *
 */

#ifndef OBJTOOLS_WRITERS_PSL_RECORD_HPP
#define OBJTOOLS_WRITERS_PSL_RECORD_HPP

#include <objtools/writers/writer.hpp>
#include <util/sequtil/sequtil.hpp>
#include <objects/seqalign/Spliced_seg.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CDense_seg;
class CScope;
class CSparse_align;
class CSparse_seg;
class CWriterListener;

//  ----------------------------------------------------------------------------
class CPslRecord
//  ----------------------------------------------------------------------------
{
public:
    CPslRecord(
        CWriterListener* pMessageListener = nullptr):
        mpMessageListener(pMessageListener)
    {};

    ~CPslRecord() = default;

    void Initialize(
        CScope& scope,
        const CSpliced_seg& splicedSeg);

    void Initialize(
        CScope& scope,
        const CDense_seg& denseSeg);

    void Initialize(
        CScope& scope,
        const CSeq_align::TScore& scores);

    void Finalize();

    int GetMatches() const { return mMatches; };
    int GetMisMatches() const { return mMisMatches; };
    int GetRepMatches() const { return mRepMatches; };
    int GetCountN() const { return mCountN; };
    int GetNumInsertQ() const { return mNumInsertQ; };
    int GetBaseInsertQ() const { return mBaseInsertQ; };
    int GetNumInsertT() const { return mNumInsertT; };
    int GetBaseInsertT() const { return mBaseInsertT; };
    ENa_strand GetStrandQ() const { return mStrandQ; };
    ENa_strand GetStrandT() const { return mStrandT; };
    const string& GetNameQ() const { return mNameQ; };
    int GetSizeQ() const { return mSizeQ; };
    int GetStartQ() const { return mStartQ; };
    int GetEndQ() const { return mEndQ; };
    const string& GetNameT() const { return mNameT; };
    int GetSizeT() const { return mSizeT; };
    int GetStartT() const { return mStartT; };
    int GetEndT() const { return mEndT; };
    int GetBlockCount() const { return mBlockCount; };
    const vector<int>& GetBlockSizes() const { return mBlockSizes; };
    const vector<int>& GetBlockStartsQ() const { return  mBlockStartsQ; };
    const vector<int>& GetBlockStartsT() const { return mBlockStartsT; };


protected:
    void xValidateSegment(
        CScope&,
        const CSpliced_seg&);
    void xInitializeStrands(
        CScope&,
        const CSpliced_seg&);
    void xInitializeStats(
        CScope&,
        const CSpliced_seg&);
    void xInitializeSequenceInfo(
        CScope&,
        const CSpliced_seg&);
    void xInitializeBlocks(
        CScope&,
        const CSpliced_seg&);
    void xInitializeBlocksStrandPositive(
        CScope&,
        const CSpliced_seg&);
    void xInitializeBlocksStrandNegative(
        CScope&,
        const CSpliced_seg&);

    void xValidateSegment(
        CScope&,
        const CDense_seg&);
    void xInitializeStrands(
        CScope&,
        const CDense_seg&);
    void xInitializeSequenceInfo(
        CScope&,
        const CDense_seg&);
    void xInitializeStatsAndBlocks(
        CScope&,
        const CDense_seg&);

    void xPutMessage(
        const string& message,
        EDiagSev severity);

    CWriterListener* mpMessageListener;
    int mMatches = -1;                          // number of bases that match that aren't repeats
    int mMisMatches = -1;                       // number of bases that don't match
    int mRepMatches = -1;                       // number of bases that match but are part of repeats
    int mCountN = -1;                           // number of "N" bases
    int mNumInsertQ = -1;                       // number of inserts in query
    int mBaseInsertQ = -1;                      // number of bases inserted in query
    int mNumInsertT = -1 ;                      // number of inserts in target
    int mBaseInsertT = -1;                      // number of bases inserted in target
    ENa_strand mStrandQ = eNa_strand_unknown;   // + or - for query strand.
    ENa_strand mStrandT = eNa_strand_unknown;   // if translated, second + or - for target genomic strand
    string mNameQ;                              // query sequence name
    int mSizeQ = -1;                            // query sequence size
    int mStartQ = -1;                           // alignment start position in query
    int mEndQ = -1;                             // alignment end position in query
    string mNameT;                              // target sequence name
    int mSizeT = -1;                            // target sequence size
    int mStartT = -1;                           // alignment start position in target
    int mEndT = -1;                             // alignment end position in target
    int mBlockCount = -1;                       // number of blocks in the alignment
    vector<int> mBlockSizes;                    // list of block sizes
    vector<int> mBlockStartsQ;                  // list of block starting positions in query
    vector<int> mBlockStartsT;                  // list of block starting positions in target
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS_PSL_RECORD_HPP

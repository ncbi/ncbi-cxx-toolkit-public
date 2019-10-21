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

//  ----------------------------------------------------------------------------
class CPslRecord
//  ----------------------------------------------------------------------------
{
public:
    CPslRecord():
        mMatches(0),
        mMisMatches(0),
        mRepMatches(0),
        mCountN(0),
        mNumInsertQ(0),
        mBaseInsertQ(0),
        mNumInsertT(0),
        mBaseInsertT(0),
        mStrandQ(eNa_strand_unknown),
        mStrandT(eNa_strand_unknown),
        mNameQ(""),
        mSizeQ(0),
        mStartQ(0),
        mEndQ(0),
        mNameT(""),
        mSizeT(0),
        mStartT(0),
        mEndT(0),
        mExonCount(0)
    {};

    ~CPslRecord() = default;

    void Initialize(
        CScope&,
        const CSpliced_seg& splicedSeg);

    void Write(
        ostream& ostr,
        bool debug=false) const;

protected:

    string xFieldMatches(bool debug) const;
    string xFieldMisMatches(bool debug) const;
    string xFieldRepMatches(bool debug) const;
    string xFieldCountN(bool debug) const;
    string xFieldNumInsertQ(bool debug) const;
    string xFieldBaseInsertQ(bool debug) const;
    string xFieldNumInsertT(bool debug) const;
    string xFieldBaseInsertT(bool debug) const;
    string xFieldStrand(bool debug) const;
    string xFieldNameQ(bool debug) const;
    string xFieldSizeQ(bool debug) const;
    string xFieldStartQ(bool debug) const;
    string xFieldEndQ(bool debug) const;
    string xFieldNameT(bool debug) const;
    string xFieldSizeT(bool debug) const;
    string xFieldStartT(bool debug) const;
    string xFieldEndT(bool debug) const;
    string xFieldBlockCount(bool debug) const;
    string xFieldBlockSizes(bool debug) const;
    string xFieldStartsQ(bool debug) const;
    string xFieldStartsT(bool debug) const;

    int mMatches;               // number of bases that match that aren't repeats
    int mMisMatches;            // number of bases that don't match
    int mRepMatches;            // number of bases that match but are part of repeats
    int mCountN;                // number of "N" bases
    int mNumInsertQ;            // number of inserts in query
    int mBaseInsertQ;           // number of bases inserted in query
    int mNumInsertT;            // number of inserts in target
    int mBaseInsertT;           // number of bases inserted in target
    ENa_strand mStrandQ;        // + or - for query strand.
    ENa_strand mStrandT;        // if translated, second + or - for target genomic strand
    string mNameQ;              // query sequence name
    int mSizeQ;                 // query sequence size
    int mStartQ;                // alignment start position in query
    int mEndQ;                  // alignment end position in query
    string mNameT;              // target sequence name
    int mSizeT;                 // target sequence size
    int mStartT;                // alignment start position in target
    int mEndT;                  // alignment end position in target
    int mExonCount;             // number of blocks in the alignment
    vector<int> mExonSizes;     // list of block sizes
    vector<int> mExonStartsQ;   // list of block starting positions in query
    vector<int> mExonStartsT;   // list of block starting positions in target
};
 

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS_PSL_RECORD_HPP

#ifndef ALGO_GNOMON___GNOMON__HPP
#define ALGO_GNOMON___GNOMON__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbiobj.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <util/range.hpp>

BEGIN_NCBI_SCOPE


class CClusterSet;
class CFrameShiftInfo;
typedef vector<CFrameShiftInfo> TFrameShifts;


class NCBI_XALGOGNOMON_EXPORT CGnomon
{
public:

    CGnomon();
    ~CGnomon();

    // set our sequence, as a vector of IUPACna characters
    void SetSequence(const vector<char>& seq);

    // set our sequence, as a string of IUPACna characters
    void SetSequence(const string& seq);

    // set our sequence, as a CSeqVector.  We will explicitly convert these
    // to IUPACna.
    void SetSequence(const objects::CSeqVector& vec);

    // set the name of the model data file
    void SetModelData(const string& file);

    // set the name of the a priori information file
    void SetAprioriInfo(const string& file);

    // set the name of the frame shifts file
    void SetFrameShiftInfo(const string& file);

    // set the repeats flag.  By default, we do not use repeat information.
    void SetRepeats(bool val);

    // set the range to scan.  By default, we scan the whole sequence.
    void SetScanRange(const CRange<TSeqPos>& range);

    // set the scan starting position
    void SetScanFrom(TSeqPos from);

    // set the scan stopping position
    void SetScanTo  (TSeqPos to);

    // run the algorithm
    void Run(void);

    CRef<objects::CSeq_annot> GetAnnot(void) const;

private:
    // our sequence
    vector<char> m_Seq;

    // our scan range
    CRange<TSeqPos> m_ScanRange;

    // the model data file
    string m_ModelFile;

    // the a priori information
    auto_ptr<CClusterSet> m_Clusters;

    // the frame shifts information
    auto_ptr<TFrameShifts> m_Fshifts;

    // flag: do we use repeat information?
    bool m_Repeats;

    // our resulting annotation
    CRef<objects::CSeq_annot> m_Annot;

    // copying is prohibited
    CGnomon(const CGnomon&);
    CGnomon& operator=(const CGnomon&);
};



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/03/16 15:37:43  vasilche
 * Added required include
 *
 * Revision 1.1  2003/10/24 15:06:30  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // ALGO_GNOMON___GNOMON__HPP

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
* Author:  Christiam Camacho, Vahram Avagyan
*
*/

#ifndef OBJECTS_SEQ___SEQLOCINFO__HPP
#define OBJECTS_SEQ___SEQLOCINFO__HPP

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <util/range.hpp>       // For TSeqRange

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_loc;
    class CPacked_seqint;
END_SCOPE(objects)

//Note: The following types have been refactored
//      from algo/blast/api/blast_aux.hpp

///structure for seqloc info
class NCBI_SEQ_EXPORT CSeqLocInfo : public CObject {
public:
    enum ETranslationFrame {
        eFramePlus1  =  1,
        eFramePlus2  =  2,
        eFramePlus3  =  3,
        eFrameMinus1 = -1,
        eFrameMinus2 = -2,
        eFrameMinus3 = -3,
        eFrameNotSet = 0
    };

    CSeqLocInfo(objects::CSeq_interval* interval, int frame)
        : m_Interval(interval)
    { SetFrame(frame); }

    CSeqLocInfo(objects::CSeq_id& id, TSeqRange& range, int frame)
        : m_Interval(new objects::CSeq_interval(id, range.GetFrom(),
                                                range.GetToOpen()))
    { SetFrame(frame); }

    const objects::CSeq_interval& GetInterval() const { return *m_Interval; }

    const objects::CSeq_id& GetSeqId() const { return m_Interval->GetId(); }

    void SetInterval(objects::CSeq_interval* interval) 
    { m_Interval.Reset(interval); }

    /// Convert the frame to a strand
    objects::ENa_strand GetStrand() const;

    int GetFrame() const { return (int) m_Frame; }

    void SetFrame(int frame); // Throws exception on out-of-range input

    operator TSeqRange() const {
        return TSeqRange(m_Interval->GetFrom(), m_Interval->GetTo()-1);
    }

    operator pair<TSeqPos, TSeqPos>() const {
        return make_pair<TSeqPos, TSeqPos>(m_Interval->GetFrom(), 
                                           m_Interval->GetTo());
    }

    friend ostream& operator<<(ostream& out, const CSeqLocInfo& rhs) {
        out << "CSeqLocInfo = { " << MSerial_AsnText << *rhs.m_Interval 
            << "ETranslationFrame = " << rhs.m_Frame << "\n}";
        return out;
    }

    bool operator==(const CSeqLocInfo& rhs) const;

    bool operator!=(const CSeqLocInfo& rhs) const {
        return ! (*this == rhs);
    }

private:
    CRef<objects::CSeq_interval> m_Interval; 
    ETranslationFrame m_Frame;         // For translated nucleotide sequence
};

typedef list< CRef<CSeqLocInfo> >   TSeqLocInfoCRefList;

/// Collection of masked regions for a single query sequence
class NCBI_SEQ_EXPORT TMaskedQueryRegions : public TSeqLocInfoCRefList
{
public:
    /// Return a new instance of this object that is restricted to the location
    /// specified
    /// @param location location describing the range to restrict. Note that
    /// only the provided range is examined, the Seq-id and strand of the 
    /// location (if assigned and different from unknown) is copied verbatim 
    /// into the return value of this method [in]
    /// @return empty TMaskedQueryRegions if this object is empty, otherwise 
    /// the intersection of location and this object
    TMaskedQueryRegions 
    RestrictToSeqInt(const objects::CSeq_interval& location) const;

    /// Converts this object to a CPacked_seqint (this is the convention used
    /// to encode masking locations in the network BLAST 4 protocol)
    CRef<objects::CPacked_seqint> ConvertToCPacked_seqint() const;

    /// Returns true if there are masks on the negative strand
    bool HasNegativeStrandMasks() const;
};

/// TMaskedSubjRegions defined as synonym to TMaskedQueryRegions
typedef TMaskedQueryRegions TMaskedSubjRegions;

/// Collection of masked regions for all queries in a BLAST search
/// @note this supports tra
typedef vector< TMaskedQueryRegions > TSeqLocInfoVector;

END_NCBI_SCOPE

#endif  /* OBJECTS_SEQ___SEQLOCINFO__HPP */

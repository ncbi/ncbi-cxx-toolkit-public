#ifndef OBJECTS_ALNMGR___ALNMIX__HPP
#define OBJECTS_ALNMGR___ALNMIX__HPP

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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment mix
*
*/

#include <objects/seqalign/Seq_align.hpp>
#include <objtools/alnmgr/alnmatch.hpp>
#include <objtools/alnmgr/task_progress.hpp>


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


class CScope;
class CAlnMixSeq;
class CAlnMixMerger;


class NCBI_XALNMGR_EXPORT CAlnMix : 
    public CSeq_align::SSeqIdChooser, // Note that SSeqIdChooser derives from 
                                      // CObject, so CAlnMix *is* also a CObject.
    public CTaskProgressReporter
{
public:

    typedef CAlnMixMatches::TCalcScoreMethod TCalcScoreMethod;

    // Constructors
    CAlnMix(void);
    CAlnMix(CScope& scope,
            TCalcScoreMethod calc_score = 0);
                 
    // Destructor
    ~CAlnMix(void);


    enum EAddFlags {
        // Determine score of each aligned segment in the process of mixing
        // (only makes sense if scope was provided at construction time)
        fCalcScore            = 0x01,

        // Force translation of nucleotide rows
        // This will result in an output Dense-seg that has Widths,
        // no matter if the whole alignment consists of nucleotides only.
        fForceTranslation     = 0x02,

        // Used for mapping sequence to itself
        fPreserveRows         = 0x04 
    };
    typedef int TAddFlags;

    // Iteratively building the mix
    void Add(const CDense_seg& ds, TAddFlags flags = 0);
    void Add(const CSeq_align& aln, TAddFlags flags = 0);



    typedef vector<CConstRef<CDense_seg> >         TConstDSs;
    typedef vector<CConstRef<CSeq_align> >         TConstAlns;

    // Input accessors
    CScope&            GetScope         (void) const;
    const TConstDSs&   GetInputDensegs  (void) const;
    const TConstAlns&  GetInputSeqAligns(void) const;

    

    enum EMergeFlags {
        fTruncateOverlaps     = 0x0001, //< otherwise put on separate rows
        fNegativeStrand       = 0x0002,
        fGapJoin              = 0x0004, //< join equal len segs gapped on refseq
        fMinGap               = 0x0008, //< minimize segs gapped on refseq
        fRemoveLeadTrailGaps  = 0x0010, //< Remove all leading or trailing gaps
        fSortSeqsByScore      = 0x0020, //< Better scoring seqs go towards the top
        fSortInputByScore     = 0x0040, //< Process better scoring input alignments first
        fQuerySeqMergeOnly    = 0x0080, //< Only put the query seq on same row, 
                                        //< other seqs from diff densegs go to
                                        //< diff rows
        fFillUnalignedRegions = 0x0100,
        fAllowTranslocation   = 0x0200  //< allow translocations when truncating overlaps
    };
    typedef int TMergeFlags;

    // Merge the mix
    void               Merge            (TMergeFlags flags = 0);



    // Obtain the resulting alignment
    const CDense_seg&  GetDenseg        (void) const;
    const CSeq_align&  GetSeqAlign      (void) const;



private:

    // Prohibit copy constructor and assignment operator
    CAlnMix(const CAlnMix& value);
    CAlnMix& operator=(const CAlnMix& value);

    typedef map<void *, CConstRef<CDense_seg> >           TConstDSsMap;
    typedef map<void *, CConstRef<CSeq_align> >           TConstAlnsMap;

    void x_Init                (void);
    void x_Reset               (void);

    // SChooseSeqId implementation
    virtual void ChooseSeqId(CSeq_id& id1, const CSeq_id& id2);


    CRef<CDense_seg> x_ExtendDSWithWidths(const CDense_seg& ds);


    mutable CRef<CScope>        m_Scope;
    TCalcScoreMethod            x_CalculateScore;
    TConstDSs                   m_InputDSs;
    TConstAlns                  m_InputAlns;
    TConstDSsMap                m_InputDSsMap;
    TConstAlnsMap               m_InputAlnsMap;

    TAddFlags                   m_AddFlags;

    CRef<CAlnMixSequences>      m_AlnMixSequences;
    CRef<CAlnMixMatches>        m_AlnMixMatches;
    CRef<CAlnMixMerger>         m_AlnMixMerger;
};



///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////

inline
CScope& CAlnMix::GetScope() const
{
    return const_cast<CScope&>(*m_Scope);
}


inline
const CAlnMix::TConstDSs& CAlnMix::GetInputDensegs() const
{
    return m_InputDSs;
}


inline
const CAlnMix::TConstAlns& CAlnMix::GetInputSeqAligns() const
{
    return m_InputAlns;
}


///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_ALNMGR___ALNMIX__HPP

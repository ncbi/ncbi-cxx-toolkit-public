#ifndef FEAT_CI__HPP
#define FEAT_CI__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Object manager iterators
*
*/

#include <corelib/ncbistd.hpp>
#include <objmgr/annot_types_ci.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/mapped_feat.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_annot_Handle;

/** @addtogroup ObjectManagerIterators
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///
///  CFeat_CI --
///
///  Enumerate CSeq_feat objects related to a bioseq, seq-loc,
///  or contained in a particular seq-entry or seq-annot 
///  regardless of the referenced locations.

class NCBI_XOBJMGR_EXPORT CFeat_CI : public CAnnotTypes_CI
{
public:
    CFeat_CI(void);

    /// Search features on the whole bioseq
    CFeat_CI(const CBioseq_Handle& bioseq);

    /// Search features on the whole bioseq
    ///
    /// @sa
    ///   SAnnotSelector
    CFeat_CI(const CBioseq_Handle& bioseq,
             const SAnnotSelector& sel);

    /// Search features on part of the bioseq
    CFeat_CI(const CBioseq_Handle& bioseq,
             const CRange<TSeqPos>& range,
             ENa_strand strand = eNa_strand_unknown);

    /// Search features on part of the bioseq
    CFeat_CI(const CBioseq_Handle& bioseq,
             const CRange<TSeqPos>& range,
             const SAnnotSelector& sel);

    /// Search features on part of the bioseq
    CFeat_CI(const CBioseq_Handle& bioseq,
             const CRange<TSeqPos>& range,
             ENa_strand strand,
             const SAnnotSelector& sel);

    /// Search features related to the location
    CFeat_CI(CScope& scope,
             const CSeq_loc& loc);

    /// Search features related to the location
    ///
    /// @sa
    ///   SAnnotSelector
    CFeat_CI(CScope& scope,
             const CSeq_loc& loc,
             const SAnnotSelector& sel);

    /// Iterate all features from the seq-annot regardless of their location
    CFeat_CI(const CSeq_annot_Handle& annot);

    /// Iterate all features from the seq-annot regardless of their location
    ///
    /// @sa
    ///   SAnnotSelector
    CFeat_CI(const CSeq_annot_Handle& annot,
             const SAnnotSelector& sel);

    /// Iterate all features from the seq-entry regardless of their location
    CFeat_CI(const CSeq_entry_Handle& entry);

    /// Iterate all features from the seq-entry regardless of their location
    ///
    /// @sa
    ///   SAnnotSelector
    CFeat_CI(const CSeq_entry_Handle& entry,
             const SAnnotSelector& sel);

    /// Search features with specified id
    typedef CObject_id TFeatureId;
    typedef int TFeatureIdInt;
    typedef string TFeatureIdStr;
    CFeat_CI(const CTSE_Handle& tse,
             const SAnnotSelector& sel,
             const TFeatureId& id);
    CFeat_CI(const CTSE_Handle& tse,
             const SAnnotSelector& sel,
             const TFeatureIdInt& int_id);
    CFeat_CI(const CTSE_Handle& tse,
             const SAnnotSelector& sel,
             const TFeatureIdStr& str_id);

    CFeat_CI(const CFeat_CI& iter);
    virtual ~CFeat_CI(void);
    CFeat_CI& operator= (const CFeat_CI& iter);

    /// Move to the next object in iterated sequence
    CFeat_CI& operator++(void);

    /// Move to the pervious object in iterated sequence
    CFeat_CI& operator--(void);

    /// Check if iterator points to an object
    DECLARE_OPERATOR_BOOL(IsValid());

    void Update(void);
    void Rewind(void);

    const CMappedFeat& operator* (void) const;
    const CMappedFeat* operator-> (void) const;

private:
    CFeat_CI& operator++ (int);
    CFeat_CI& operator-- (int);

    void x_AddFeaturesWithId(const CTSE_Handle& tse,
                             const SAnnotSelector& sel,
                             const TFeatureId& feat_id);
    typedef vector<CSeq_feat_Handle> TSeq_feat_Handles;
    void x_AddFeatures(const SAnnotSelector& sel,
                       const TSeq_feat_Handles& feats);

    CMappedFeat m_MappedFeat;// current feature object returned by operator->()
};



inline
void CFeat_CI::Update(void)
{
    if ( IsValid() ) {
        m_MappedFeat.Set(GetCollector(), GetIterator());
    }
    else {
        m_MappedFeat.Reset();
    }
}


inline
CFeat_CI& CFeat_CI::operator++ (void)
{
    Next();
    Update();
    return *this;
}


inline
CFeat_CI& CFeat_CI::operator-- (void)
{
    Prev();
    Update();
    return *this;
}


inline
void CFeat_CI::Rewind(void)
{
    CAnnotTypes_CI::Rewind();
    Update();
}


inline
const CMappedFeat& CFeat_CI::operator* (void) const
{
    return m_MappedFeat;
}


inline
const CMappedFeat* CFeat_CI::operator-> (void) const
{
    return &m_MappedFeat;
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // FEAT_CI__HPP

#ifndef ALIGN_CI__HPP
#define ALIGN_CI__HPP

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
#include <objmgr/seq_align_handle.hpp>

#include <objects/seqalign/Seq_align.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerIterators
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///
///  CAlign_CI --
///
///  Enumerate CSeq_align objects related to the specified bioseq or seq-loc
///

class NCBI_XOBJMGR_EXPORT CAlign_CI : public CAnnotTypes_CI
{
public:
    /// Create an empty iterator
    CAlign_CI(void);

    /// Create an iterator that enumerates CSeq_align objects 
    /// related to the given bioseq
    CAlign_CI(const CBioseq_Handle& bioseq);

    /// Create an iterator that enumerates CSeq_align objects 
    /// related to the given bioseq
    CAlign_CI(const CBioseq_Handle& bioseq,
              const CRange<TSeqPos>& range,
              ENa_strand strand = eNa_strand_unknown);

    /// Create an iterator that enumerates CSeq_align objects 
    /// related to the given bioseq
    ///
    /// @sa
    ///   SAnnotSelector
    CAlign_CI(const CBioseq_Handle& bioseq,
              const SAnnotSelector& sel);

    /// Create an iterator that enumerates CSeq_align objects 
    /// related to the given bioseq
    ///
    /// @sa
    ///   SAnnotSelector
    CAlign_CI(const CBioseq_Handle& bioseq,
              const CRange<TSeqPos>& range,
              const SAnnotSelector& sel);

    /// Create an iterator that enumerates CSeq_align objects 
    /// related to the given bioseq
    ///
    /// @sa
    ///   SAnnotSelector
    CAlign_CI(const CBioseq_Handle& bioseq,
              const CRange<TSeqPos>& range,
              ENa_strand strand,
              const SAnnotSelector& sel);

    /// Create an iterator that enumerates CSeq_align objects 
    /// related to the given seq-loc
    ///
    /// @sa
    ///   SAnnotSelector
    CAlign_CI(CScope& scope,
              const CSeq_loc& loc,
              const SAnnotSelector& sel);

    /// Create an iterator that enumerates CSeq_align objects 
    /// related to the given seq-loc
    CAlign_CI(CScope& scope,
              const CSeq_loc& loc);

    // Iterate all features from the object regardless of their location

    /// Create an iterator that enumerates CSeq_align objects
    /// from the annotation regardless of their location
    CAlign_CI(const CSeq_annot_Handle& annot);

    /// Create an iterator that enumerates CSeq_align objects
    /// from the annotation regardless of their location
    /// based on selection
    ///
    /// @sa
    ///   SAnnotSelector
    CAlign_CI(const CSeq_annot_Handle& annot,
              const SAnnotSelector& sel);

    /// Create an iterator that enumerates CSeq_align objects
    /// from the seq-entry regardless of their location
    CAlign_CI(const CSeq_entry_Handle& entry);

    /// Create an iterator that enumerates CSeq_align objects
    /// from the seq-entry regardless of their location
    /// based on selection
    ///
    /// @sa
    ///   SAnnotSelector
    CAlign_CI(const CSeq_entry_Handle& entry,
              const SAnnotSelector& sel);

    virtual ~CAlign_CI(void);

    /// Move to the next object in iterated sequence
    CAlign_CI& operator++ (void);

    /// Move to the pervious object in iterated sequence
    CAlign_CI& operator-- (void);

    /// Check if iterator points to an object
    DECLARE_OPERATOR_BOOL(IsValid());

    /// Mapped alignment, not the original one
    const CSeq_align& operator* (void) const;

    /// Mapped alignment, not the original one
    const CSeq_align* operator-> (void) const;

    /// Get original alignment
    const CSeq_align& GetOriginalSeq_align(void) const;

    /// Get original alignment handle
    CSeq_align_Handle GetSeq_align_Handle(void) const;

private:
    CAlign_CI operator++ (int);
    CAlign_CI operator-- (int);

    mutable CConstRef<CSeq_align> m_MappedAlign;
};


inline
CAlign_CI::CAlign_CI(void)
{
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ALIGN_CI__HPP

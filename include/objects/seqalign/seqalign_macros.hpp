#ifndef OBJECTS_SEQALIGN___SEQALIGN_MACROS__HPP
#define OBJECTS_SEQALIGN___SEQALIGN_MACROS__HPP

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
 * Authors:  Jonathan Kans, Michael Kornbluh, Colleen Bollin
 *
 */

/// @file seqalign_macros.hpp
/// Utility macros and typedefs for exploring NCBI objects from seqalign.asn.


#include <objects/misc/sequence_util_macros.hpp>
#include <objects/seqalign/seqalign__.hpp>


/// @NAME Convenience macros for NCBI objects
/// @{


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
/// Macros and typedefs for object subtypes
/////////////////////////////////////////////////////////////////////////////


/// CSeq_align definitions

#define NCBI_SEGTYPE(Type) CSeq_align::C_Segs::e_##Type
typedef CSeq_align::C_Segs::E_Choice TSEGTYPE_TYPE;

//  Dendiag   Denseg    Std     Packed
//  Disc      Spliced   Sparse


// "FOR_EACH_XXX_ON_YYY" does a linear const traversal of STL containers
// "EDIT_EACH_XXX_ON_YYY" does a linear non-const traversal of STL containers

// "SWITCH_ON_XXX_CHOICE" switches on the item subtype

// "ADD_XXX_TO_YYY" adds an element to a specified object
// "ERASE_XXX_ON_YYY" deletes a specified object within an iterator

// Miscellaneous macros for testing objects include
// "XXX_IS_YYY" or "XXX_HAS_YYY"
// "XXX_CHOICE_IS"


/// BOUND_ON_SEQALIGN macros

#define BOUND_ON_SEQALIGN_Type      CSeq_align::TBounds
#define BOUND_ON_SEQALIGN_Test(Var) (Var).IsSetBounds()
#define BOUND_ON_SEQALIGN_Get(Var)  (Var).GetBounds()
#define BOUND_ON_SEQALIGN_Set(Var)  (Var).SetBounds()

// EDIT_EACH_BOUND_ON_SEQALIGN

#define EDIT_EACH_BOUND_ON_SEQALIGN(Itr, Var) \
    EDIT_EACH (BOUND_ON_SEQALIGN, Itr, Var)

/// SEGTYPE_ON_SEQALIGN macros

#define SEGTYPE_ON_SEQALIGN_Test(Var) ((Var).IsSetSegs())
#define SEGTYPE_ON_SEQALIGN_Chs(Var)  (Var).GetSegs().Which()

#define SWITCH_ON_SEGTYPE_ON_SEQALIGN(Var) \
    SWITCH_ON( SEGTYPE_ON_SEQALIGN, Var )

/// DENDIAG_ON_SEQALIGN macros

#define DENDIAG_ON_SEQALIGN_Type        CSeq_align_Base::C_Segs::TDendiag
#define DENDIAG_ON_SEQALIGN_Test(Var)   (Var).IsSetSegs() && (Var).GetSegs().IsDendiag()
#define DENDIAG_ON_SEQALIGN_Get(Var)    (Var).GetSegs().GetDendiag()
#define DENDIAG_ON_SEQALIGN_Set(Var)    (Var).SetSegs().SetDendiag()

/// EDIT_EACH_DENDIAG_ON_SEQALIGN

#define EDIT_EACH_DENDIAG_ON_SEQALIGN(Itr, Var) \
EDIT_EACH (DENDIAG_ON_SEQALIGN, Itr, Var)

/// STDSEG_ON_SEQALIGN macros

#define STDSEG_ON_SEQALIGN_Type        CSeq_align_Base::C_Segs::TStd
#define STDSEG_ON_SEQALIGN_Test(Var)   (Var).IsSetSegs() && (Var).GetSegs().IsStd()
#define STDSEG_ON_SEQALIGN_Get(Var)    (Var).GetSegs().GetStd()
#define STDSEG_ON_SEQALIGN_Set(Var)    (Var).SetSegs().SetStd()

/// EDIT_EACH_STDSEG_ON_SEQALIGN

#define EDIT_EACH_STDSEG_ON_SEQALIGN(Itr, Var) \
EDIT_EACH (STDSEG_ON_SEQALIGN, Itr, Var)

/// RECURSIVE_SEQALIGN_ON_SEQALIGN macros

#define RECURSIVE_SEQALIGN_ON_SEQALIGN_Type        CSeq_align_Base::C_Segs::TDisc::Tdata
#define RECURSIVE_SEQALIGN_ON_SEQALIGN_Test(Var)   (Var).IsSetSegs() && (Var).GetSegs().IsDisc()
#define RECURSIVE_SEQALIGN_ON_SEQALIGN_Get(Var)    (Var).GetSegs().GetDisc().Get()
#define RECURSIVE_SEQALIGN_ON_SEQALIGN_Set(Var)    (Var).SetSegs().SetDisc().Set()

/// EDIT_EACH_RECURSIVE_SEQALIGN_ON_SEQALIGN

#define EDIT_EACH_RECURSIVE_SEQALIGN_ON_SEQALIGN(Itr, Var) \
EDIT_EACH (RECURSIVE_SEQALIGN_ON_SEQALIGN, Itr, Var)

/// SEQID_ON_DENDIAG macros

#define SEQID_ON_DENDIAG_Type        CDense_diag_Base::TIds
#define SEQID_ON_DENDIAG_Test(Var)   (Var).IsSetIds()
#define SEQID_ON_DENDIAG_Get(Var)    (Var).GetIds()
#define SEQID_ON_DENDIAG_Set(Var)    (Var).SetIds()

/// EDIT_EACH_SEQID_ON_DENDIAG

#define EDIT_EACH_SEQID_ON_DENDIAG(Itr, Var) \
EDIT_EACH (SEQID_ON_DENDIAG, Itr, Var)

/// SEQID_ON_DENSEG macros

#define SEQID_ON_DENSEG_Type        CDense_seg::TIds
#define SEQID_ON_DENSEG_Test(Var)   (Var).IsSetIds()
#define SEQID_ON_DENSEG_Get(Var)    (Var).GetIds()
#define SEQID_ON_DENSEG_Set(Var)    (Var).SetIds()

/// EDIT_EACH_SEQID_ON_DENSEG

#define EDIT_EACH_SEQID_ON_DENSEG(Itr, Var) \
EDIT_EACH (SEQID_ON_DENSEG, Itr, Var)


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJECTS_SEQALIGN___SEQALIGN_MACROS__HPP */

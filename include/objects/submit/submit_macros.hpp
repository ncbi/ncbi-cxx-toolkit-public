#ifndef OBJECTS_SUBMIT___SUBMIT_MACROS__HPP
#define OBJECTS_SUBMIT___SUBMIT_MACROS__HPP

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

/// @file submit_macros.hpp
/// Utility macros and typedefs for exploring NCBI objects from submit.asn.


#include <objects/misc/sequence_util_macros.hpp>
#include <objects/submit/submit__.hpp>


/// @NAME Convenience macros for NCBI objects
/// @{


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
/// Macros and typedefs for object subtypes
/////////////////////////////////////////////////////////////////////////////


/// CSeq_submit definitions

#define NCBI_SEQSUBMIT(Type) CSeq_submit::TData::e_##Type
typedef CSeq_submit::TData::E_Choice TSEQSUBMIT_CHOICE;

//   Entrys     Annots



// "FOR_EACH_XXX_ON_YYY" does a linear const traversal of STL containers
// "EDIT_EACH_XXX_ON_YYY" does a linear non-const traversal of STL containers

// "SWITCH_ON_XXX_CHOICE" switches on the item subtype

// "ADD_XXX_TO_YYY" adds an element to a specified object
// "ERASE_XXX_ON_YYY" deletes a specified object within an iterator

// Miscellaneous macros for testing objects include
// "XXX_IS_YYY" or "XXX_HAS_YYY"
// "XXX_CHOICE_IS"


///
/// CSeq_submit macros

/// SEQSUBMIT_CHOICE macros

#define SEQSUBMIT_CHOICE_Test(Var) ((Var).IsSetData() && \
                                    (Var).GetData().Which() != CSeq_submit::TData::e_not_set)
#define SEQSUBMIT_CHOICE_Chs(Var)  (Var).GetData().Which()

/// SEQSUBMIT_CHOICE_IS

#define SEQSUBMIT_CHOICE_IS(Var, Chs) \
CHOICE_IS (SEQSUBMIT_CHOICE, Var, Chs)

/// SEQSUBMIT_IS_ENTRYS

#define SEQSUBMIT_IS_ENTRYS(Var) \
SEQSUBMIT_CHOICE_IS (Var, NCBI_SEQSUBMIT(Entrys))

/// SEQSUBMIT_IS_ANNOTS

#define SEQSUBMIT_IS_ANNOTS(Var) \
SEQSUBMIT_CHOICE_IS (Var, NCBI_SEQSUBMIT(Annots))

/// SWITCH_ON_SEQSUBMIT_CHOICE

#define SWITCH_ON_SEQSUBMIT_CHOICE(Var) \
SWITCH_ON (SEQSUBMIT_CHOICE, Var)

/// SEQENTRY_ON_SEQSUBMIT macros

#define SEQENTRY_ON_SEQSUBMIT_Type      CSeq_submit::TData::TEntrys
#define SEQENTRY_ON_SEQSUBMIT_Test(Var) ((Var).IsSetData() && (Var).GetData().IsEntrys())
#define SEQENTRY_ON_SEQSUBMIT_Get(Var)  (Var).GetData().GetEntrys()
#define SEQENTRY_ON_SEQSUBMIT_Set(Var)  (Var).SetData().SetEntrys()

/// FOR_EACH_SEQENTRY_ON_SEQSUBMIT
/// EDIT_EACH_SEQENTRY_ON_SEQSUBMIT
// CSeq_submit& as input, dereference with [const] CSeq_entry& se = **itr;

#define FOR_EACH_SEQENTRY_ON_SEQSUBMIT(Itr, Var) \
FOR_EACH (SEQENTRY_ON_SEQSUBMIT, Itr, Var)

#define EDIT_EACH_SEQENTRY_ON_SEQSUBMIT(Itr, Var) \
EDIT_EACH (SEQENTRY_ON_SEQSUBMIT, Itr, Var)

/// ADD_SEQENTRY_TO_SEQSUBMIT

#define ADD_SEQENTRY_TO_SEQSUBMIT(Var, Ref) \
ADD_ITEM (SEQENTRY_ON_SEQSUBMIT, Var, Ref)

/// ERASE_SEQENTRY_ON_SEQSUBMIT

#define ERASE_SEQENTRY_ON_SEQSUBMIT(Itr, Var) \
LIST_ERASE_ITEM (SEQENTRY_ON_SEQSUBMIT, Itr, Var)


/// SEQANNOT_ON_SEQSUBMIT macros

#define SEQANNOT_ON_SEQSUBMIT_Type      CSeq_submit::TData::TAnnots
#define SEQANNOT_ON_SEQSUBMIT_Test(Var) ((Var).IsSetData() && (Var).GetData().IsAnnots())
#define SEQANNOT_ON_SEQSUBMIT_Get(Var)  (Var).GetData().GetAnnots()
#define SEQANNOT_ON_SEQSUBMIT_Set(Var)  (Var).SetData().SetAnnots()

/// SEQSUBMIT_HAS_SEQANNOT

#define SEQSUBMIT_HAS_SEQANNOT(Var) \
ITEM_HAS (SEQANNOT_ON_SEQSUBMIT, Var)

/// FOR_EACH_SEQANNOT_ON_SEQSUBMIT
/// EDIT_EACH_SEQANNOT_ON_SEQSUBMIT
// CBioseq_set& as input, dereference with [const] CSeq_annot& annot = **itr;

#define FOR_EACH_SEQANNOT_ON_SEQSUBMIT(Itr, Var) \
FOR_EACH (SEQANNOT_ON_SEQSUBMIT, Itr, Var)

#define EDIT_EACH_SEQANNOT_ON_SEQSUBMIT(Itr, Var) \
EDIT_EACH (SEQANNOT_ON_SEQSUBMIT, Itr, Var)

/// ADD_SEQANNOT_TO_SEQSUBMIT

#define ADD_SEQANNOT_TO_SEQSUBMIT(Var, Ref) \
ADD_ITEM (SEQANNOT_ON_SEQSUBMIT, Var, Ref)

/// ERASE_SEQANNOT_ON_SEQSUBMIT

#define ERASE_SEQANNOT_ON_SEQSUBMIT(Itr, Var) \
LIST_ERASE_ITEM (SEQANNOT_ON_SEQSUBMIT, Itr, Var)


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJECTS_SUBMIT___SUBMIT_MACROS__HPP */

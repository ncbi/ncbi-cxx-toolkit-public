#ifndef OBJECTS_SEQLOC___SEQLOC_MACROS__HPP
#define OBJECTS_SEQLOC___SEQLOC_MACROS__HPP

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

/// @file seqloc_macros.hpp
/// Utility macros and typedefs for exploring NCBI objects from seqloc.asn.


#include <objects/misc/sequence_util_macros.hpp>
#include <objects/seqloc/seqloc__.hpp>


/// @NAME Convenience macros for NCBI objects
/// @{


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
/// Macros and typedefs for object subtypes
/////////////////////////////////////////////////////////////////////////////


/// CSeq_id definitions

#define NCBI_SEQID(Type) CSeq_id::e_##Type
typedef CSeq_id::E_Choice TSEQID_CHOICE;

//   Local       Gibbsq     Gibbmt      Giim
//   Genbank     Embl       Pir         Swissprot
//   Patent      Other      General     Gi
//   Ddbj        Prf        Pdb         Tpg
//   Tpe         Tpd        Gpipe       Named_annot_track

#define NCBI_ACCN(Type) CSeq_id::eAcc_##Type
typedef CSeq_id::EAccessionInfo TACCN_CHOICE;


/// CSeq_loc definitions

#define NCBI_SEQLOC(Type) CSeq_loc::e_##Type
typedef CSeq_loc::E_Choice TSEQLOC_TYPE;

//  Null        Empty    Whole    Int     Packed_int   Pnt
//  Packed_pnt  Mix      Equiv    Bond    Feat


// "FOR_EACH_XXX_ON_YYY" does a linear const traversal of STL containers
// "EDIT_EACH_XXX_ON_YYY" does a linear non-const traversal of STL containers

// "SWITCH_ON_XXX_CHOICE" switches on the item subtype

// "ADD_XXX_TO_YYY" adds an element to a specified object
// "ERASE_XXX_ON_YYY" deletes a specified object within an iterator

// Miscellaneous macros for testing objects include
// "XXX_IS_YYY" or "XXX_HAS_YYY"
// "XXX_CHOICE_IS"


///
/// CSeq_id macros

/// SEQID_CHOICE macros

#define SEQID_CHOICE_Test(Var) (Var).Which() != CSeq_id::e_not_set
#define SEQID_CHOICE_Chs(Var)  (Var).Which()

/// SEQID_CHOICE_IS

#define SEQID_CHOICE_IS(Var, Chs) \
CHOICE_IS (SEQID_CHOICE, Var, Chs)

/// SWITCH_ON_SEQID_CHOICE

#define SWITCH_ON_SEQID_CHOICE(Var) \
SWITCH_ON (SEQID_CHOICE, Var)


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJECTS_SEQLOC___SEQLOC_MACROS__HPP */

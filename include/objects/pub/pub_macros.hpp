#ifndef OBJECTS_PUB___PUB_MACROS__HPP
#define OBJECTS_PUB___PUB_MACROS__HPP

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

/// @file pub_macros.hpp
/// Utility macros and typedefs for exploring NCBI objects from pub.asn.


#include <objects/misc/sequence_util_macros.hpp>
#include <objects/pub/pub__.hpp>


/// @NAME Convenience macros for NCBI objects
/// @{


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
/// Macros and typedefs for object subtypes
/////////////////////////////////////////////////////////////////////////////


/// CPub definitions

#define NCBI_PUB(Type) CPub::e_##Type
typedef CPub::E_Choice TPUB_CHOICE;

//   Gen         Sub       Medline     Muid       Article
//   Journal     Book      Proc        Patent     Pat_id
//   Man         Equiv     Pmid


// "FOR_EACH_XXX_ON_YYY" does a linear const traversal of STL containers
// "EDIT_EACH_XXX_ON_YYY" does a linear non-const traversal of STL containers

// "SWITCH_ON_XXX_CHOICE" switches on the item subtype

// "ADD_XXX_TO_YYY" adds an element to a specified object
// "ERASE_XXX_ON_YYY" deletes a specified object within an iterator

// Miscellaneous macros for testing objects include
// "XXX_IS_YYY" or "XXX_HAS_YYY"
// "XXX_CHOICE_IS"


///
/// CPubequiv macros

/// PUB_ON_PUBEQUIV macros

#define PUB_ON_PUBEQUIV_Type      CPub_equiv::Tdata
#define PUB_ON_PUBEQUIV_Test(Var) (Var).IsSet()
#define PUB_ON_PUBEQUIV_Get(Var)  (Var).Get()
#define PUB_ON_PUBEQUIV_Set(Var)  (Var).Set()

#define FOR_EACH_PUB_ON_PUBEQUIV(Itr, Var) \
FOR_EACH (PUB_ON_PUBEQUIV, Itr, Var)

#define EDIT_EACH_PUB_ON_PUBEQUIV(Itr, Var) \
EDIT_EACH (PUB_ON_PUBEQUIV, Itr, Var)

/// ADD_PUB_TO_PUBEQUIV

#define ADD_PUB_TO_PUBEQUIV(Var, Ref) \
ADD_ITEM (PUB_ON_PUBEQUIV, Var, Ref)

/// ERASE_PUB_ON_PUBEQUIV

#define ERASE_PUB_ON_PUBEQUIV(Itr, Var) \
LIST_ERASE_ITEM (PUB_ON_PUBEQUIV, Itr, Var)


///
/// CPubdesc macros

/// PUB_ON_PUBDESC macros

#define PUB_ON_PUBDESC_Type      CPub_equiv::Tdata
#define PUB_ON_PUBDESC_Test(Var) (Var).IsSetPub() && (Var).GetPub().IsSet()
#define PUB_ON_PUBDESC_Get(Var)  (Var).GetPub().Get()
#define PUB_ON_PUBDESC_Set(Var)  (Var).SetPub().Set()

/// PUBDESC_HAS_PUB

#define PUBDESC_HAS_PUB(Var) \
ITEM_HAS (PUB_ON_PUBDESC, Var)

/*
#define PUBDESC_HAS_PUB(Pbd) \
((Pbd).IsSetPub())
*/

/// FOR_EACH_PUB_ON_PUBDESC
/// EDIT_EACH_PUB_ON_PUBDESC
// CPubdesc& as input, dereference with [const] CPub& pub = **itr;

#define FOR_EACH_PUB_ON_PUBDESC(Itr, Var) \
FOR_EACH (PUB_ON_PUBDESC, Itr, Var)

#define EDIT_EACH_PUB_ON_PUBDESC(Itr, Var) \
EDIT_EACH (PUB_ON_PUBDESC, Itr, Var)

/// ADD_PUB_TO_PUBDESC

#define ADD_PUB_TO_PUBDESC(Var, Ref) \
ADD_ITEM (PUB_ON_PUBDESC, Var, Ref)

/// ERASE_PUB_ON_PUBDESC

#define ERASE_PUB_ON_PUBDESC(Itr, Var) \
LIST_ERASE_ITEM (PUB_ON_PUBDESC, Itr, Var)


///
/// CPub macros

/// AUTHOR_ON_PUB macros

#define AUTHOR_ON_PUB_Type      CAuth_list::C_Names::TStd
#define AUTHOR_ON_PUB_Test(Var) (Var).IsSetAuthors() && \
                                    (Var).GetAuthors().IsSetNames() && \
                                    (Var).GetAuthors().GetNames().IsStd()
#define AUTHOR_ON_PUB_Get(Var)  (Var).GetAuthors().GetNames().GetStd()
#define AUTHOR_ON_PUB_Set(Var)  (Var).SetAuthors().SetNames().SetStd()

/// PUB_HAS_AUTHOR

#define PUB_HAS_AUTHOR(Var) \
ITEM_HAS (AUTHOR_ON_PUB, Var)

/// FOR_EACH_AUTHOR_ON_PUB
/// EDIT_EACH_AUTHOR_ON_PUB
// CPub& as input, dereference with [const] CAuthor& auth = **itr;

#define FOR_EACH_AUTHOR_ON_PUB(Itr, Var) \
FOR_EACH (AUTHOR_ON_PUB, Itr, Var)

#define EDIT_EACH_AUTHOR_ON_PUB(Itr, Var) \
EDIT_EACH (AUTHOR_ON_PUB, Itr, Var)

/// ADD_AUTHOR_TO_PUB

#define ADD_AUTHOR_TO_PUB(Var, Ref) \
ADD_ITEM (AUTHOR_ON_PUB, Var, Ref)

/// ERASE_AUTHOR_ON_PUB

#define ERASE_AUTHOR_ON_PUB(Itr, Var) \
LIST_ERASE_ITEM (AUTHOR_ON_PUB, Itr, Var)


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJECTS_PUB___PUB_MACROS__HPP */

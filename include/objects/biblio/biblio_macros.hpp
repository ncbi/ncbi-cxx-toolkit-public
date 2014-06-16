#ifndef OBJECTS_BIBLIO___BIBLIO_MACROS__HPP
#define OBJECTS_BIBLIO___BIBLIO_MACROS__HPP

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

/// @file biblio_macros.hpp
/// Utility macros and typedefs for exploring NCBI objects from biblio.asn.


#include <objects/misc/sequence_util_macros.hpp>
#include <objects/biblio/biblio__.hpp>


/// @NAME Convenience macros for NCBI objects
/// @{


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
/// Macros and typedefs for object subtypes
/////////////////////////////////////////////////////////////////////////////


// "FOR_EACH_XXX_ON_YYY" does a linear const traversal of STL containers
// "EDIT_EACH_XXX_ON_YYY" does a linear non-const traversal of STL containers

// "SWITCH_ON_XXX_CHOICE" switches on the item subtype

// "ADD_XXX_TO_YYY" adds an element to a specified object
// "ERASE_XXX_ON_YYY" deletes a specified object within an iterator

// Miscellaneous macros for testing objects include
// "XXX_IS_YYY" or "XXX_HAS_YYY"
// "XXX_CHOICE_IS"

/// 
/// CCit_art macros

#define ARTICLEID_ON_CITART_Type      CCit_art::TIds::Tdata
#define ARTICLEID_ON_CITART_Test(Var) ( (Var).IsSetIds() && (Var).GetIds().IsSet() )
#define ARTICLEID_ON_CITART_Get(Var)  (Var).GetIds().Get()
#define ARTICLEID_ON_CITART_Set(Var)  (Var).SetIds().Set()

#define FOR_EACH_ARTICLEID_ON_CITART(Itr, Var) \
FOR_EACH (ARTICLEID_ON_CITART, Itr, Var)

#define EDIT_EACH_ARTICLEID_ON_CITART(Itr, Var) \
EDIT_EACH (ARTICLEID_ON_CITART, Itr, Var)

#define ERASE_ARTICLEID_ON_CITART(Itr, Var) \
LIST_ERASE_ITEM (ARTICLEID_ON_CITART, Itr, Var)

#define ARTICLEID_ON_CITART_IS_SORTED(Itr, Var) \
IS_SORTED (ARTICLEID_ON_CITART, Itr, Var)

#define SORT_ARTICLEID_ON_CITART(Itr, Var) \
DO_LIST_SORT (ARTICLEID_ON_CITART, Itr, Var)

#define ARTICLEID_ON_CITART_IS_UNIQUE(Itr, Var) \
IS_UNIQUE (ARTICLEID_ON_CITART, Itr, Var)

#define UNIQUE_ARTICLEID_ON_CITART(Itr, Var) \
DO_UNIQUE (ARTICLEID_ON_CITART, Itr, Var)

#define REMOVE_IF_EMPTY_ARTICLEID_ON_CITART(Var) \
REMOVE_IF_EMPTY_FIELD (ARTICLEID_ON_CITART,, Var)

#define ARTICLEID_ON_CITART_IS_EMPTY(Var) \
FIELD_IS_EMPTY (ARTICLEID_ON_CITART, Var)

///
/// CAuth_list macros

#define AUTHOR_ON_AUTHLIST_Type      CAuth_list::C_Names::TStd
#define AUTHOR_ON_AUTHLIST_Test(Var) (Var).IsSetNames() && \
                                     (Var).GetNames().IsStd()
#define AUTHOR_ON_AUTHLIST_Get(Var)  (Var).GetNames().GetStd()
#define AUTHOR_ON_AUTHLIST_Set(Var)  (Var).SetNames().SetStd()

#define EDIT_EACH_AUTHOR_ON_AUTHLIST(Itr, Var) \
EDIT_EACH (AUTHOR_ON_AUTHLIST, Itr, Var)

/// ERASE_AUTHOR_ON_AUTHLIST

#define ERASE_AUTHOR_ON_AUTHLIST(Itr, Var) \
LIST_ERASE_ITEM (AUTHOR_ON_AUTHLIST, Itr, Var)

/// AUTHOR_ON_AUTHLIST_IS_EMPTY

#define AUTHOR_ON_AUTHLIST_IS_EMPTY(Var) \
    FIELD_IS_EMPTY( AUTHOR_ON_AUTHLIST, Var )


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJECTS_BIBLIO___BIBLIO_MACROS__HPP */

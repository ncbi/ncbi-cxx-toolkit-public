#ifndef OBJECTS_GENERAL___GENERAL_MACROS__HPP
#define OBJECTS_GENERAL___GENERAL_MACROS__HPP

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

/// @file general_macros.hpp
/// Utility macros and typedefs for exploring NCBI objects from general.asn.


#include <objects/misc/sequence_util_macros.hpp>
#include <objects/general/general__.hpp>


/// @NAME Convenience macros for NCBI objects
/// @{


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
/// Macros and typedefs for object subtypes
/////////////////////////////////////////////////////////////////////////////


/// CUser_field definitions

#define NCBI_USERFIELD(Type) CUser_field::TData::e_##Type
typedef CUser_field::C_Data::E_Choice TUSERFIELD_CHOICE;

//   Str        Int         Real     Bool      Os
//   Object     Strs        Ints     Reals     Oss
//   Fields     Objects


/// CPerson_id definitions

#define NCBI_PERSONID(Type) CPerson_id::e_##Type
typedef CPerson_id::E_Choice TPERSONID_TYPE;

//   Dbtag      Name        Ml       Str       Consortium



// "FOR_EACH_XXX_ON_YYY" does a linear const traversal of STL containers
// "EDIT_EACH_XXX_ON_YYY" does a linear non-const traversal of STL containers

// "SWITCH_ON_XXX_CHOICE" switches on the item subtype

// "ADD_XXX_TO_YYY" adds an element to a specified object
// "ERASE_XXX_ON_YYY" deletes a specified object within an iterator

// Miscellaneous macros for testing objects include
// "XXX_IS_YYY" or "XXX_HAS_YYY"
// "XXX_CHOICE_IS"

///
/// CUser_object macros

/// USERFIELD_ON_USEROBJECT macros

#define USERFIELD_ON_USEROBJECT_Type      CUser_object::TData
#define USERFIELD_ON_USEROBJECT_Test(Var) (Var).IsSetData()
#define USERFIELD_ON_USEROBJECT_Get(Var)  (Var).GetData()
#define USERFIELD_ON_USEROBJECT_Set(Var)  (Var).SetData()

/// USEROBJECT_HAS_USERFIELD

#define USEROBJECT_HAS_USERFIELD(Var) \
ITEM_HAS (USERFIELD_ON_USEROBJECT, Var)

/// FOR_EACH_USERFIELD_ON_USEROBJECT
/// EDIT_EACH_USERFIELD_ON_USEROBJECT
// CUser_object& as input, dereference with [const] CUser_field& fld = **itr;

#define FOR_EACH_USERFIELD_ON_USEROBJECT(Itr, Var) \
FOR_EACH (USERFIELD_ON_USEROBJECT, Itr, Var)

#define EDIT_EACH_USERFIELD_ON_USEROBJECT(Itr, Var) \
EDIT_EACH (USERFIELD_ON_USEROBJECT, Itr, Var)

/// ADD_USERFIELD_TO_USEROBJECT

#define ADD_USERFIELD_TO_USEROBJECT(Var, Ref) \
ADD_ITEM (USERFIELD_ON_USEROBJECT, Var, Ref)

/// ERASE_USERFIELD_ON_USEROBJECT

#define ERASE_USERFIELD_ON_USEROBJECT(Itr, Var) \
VECTOR_ERASE_ITEM (USERFIELD_ON_USEROBJECT, Itr, Var)

#define USERFIELD_ON_USEROBJECT_IS_SORTED(Var, Func) \
IS_SORTED (USERFIELD_ON_USEROBJECT, Var, Func)

#define SORT_USERFIELD_ON_USEROBJECT(Var, Func) \
DO_VECTOR_SORT (USERFIELD_ON_USEROBJECT, Var, Func)

///
/// CUser_field macros

/// USERFIELD_CHOICE macros

#define USERFIELD_CHOICE_Test(Var) (Var).IsSetData() && Var.GetData().Which() != CUser_field::TData::e_not_set
#define USERFIELD_CHOICE_Chs(Var)  (Var).GetData().Which()

/// USERFIELD_CHOICE_IS

#define USERFIELD_CHOICE_IS(Var, Chs) \
CHOICE_IS (USERFIELD_CHOICE, Var, Chs)

/// SWITCH_ON_USERFIELD_CHOICE

#define SWITCH_ON_USERFIELD_CHOICE(Var) \
SWITCH_ON (USERFIELD_CHOICE, Var)


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJECTS_GENERAL___GENERAL_MACROS__HPP */

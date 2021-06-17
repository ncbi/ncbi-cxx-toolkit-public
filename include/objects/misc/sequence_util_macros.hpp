#ifndef OBJECTS_MISC___SEQUENCE_UTIL_MACROS__HPP
#define OBJECTS_MISC___SEQUENCE_UTIL_MACROS__HPP

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

/// @file sequence_util_macros.hpp
/// Generic utility macros and templates for exploring NCBI objects.


#include <serial/iterator.hpp>


/// @NAME Convenience macros for NCBI objects
/// @{


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/// NCBI_SERIAL_TEST_EXPLORE base macro tests to see if loop should be entered
// If okay, calls CTypeConstIterator for recursive exploration

#define NCBI_SERIAL_TEST_EXPLORE(Test, Type, Var, Cont) \
if (! (Test)) {} else for (CTypeConstIterator<Type> Var (Cont); Var; ++Var)

#define NCBI_SERIAL_NC_EXPLORE(Test, Type, Var, Cont) \
if (! (Test)) {} else for (CTypeIterator<Type> Var (Cont); Var; ++Var)


/////////////////////////////////////////////////////////////////////////////
/// Macros to iterate over standard template containers (non-recursive)
/////////////////////////////////////////////////////////////////////////////


/// NCBI_CS_ITERATE base macro tests to see if loop should be entered
// If okay, calls ITERATE for linear STL iteration

#define NCBI_CS_ITERATE(Test, Type, Var, Cont) \
if (! (Test)) {} else ITERATE (Type, Var, Cont)

/// NCBI_NC_ITERATE base macro tests to see if loop should be entered
// If okay, calls ERASE_ITERATE for linear STL iteration

#define NCBI_NC_ITERATE(Test, Type, Var, Cont) \
if (! (Test)) {} else ERASE_ITERATE (Type, Var, Cont)

/// NCBI_SWITCH base macro tests to see if switch should be performed
// If okay, calls switch statement

#define NCBI_SWITCH(Test, Chs) \
if (! (Test)) {} else switch(Chs)


/// FOR_EACH base macro calls NCBI_CS_ITERATE with generated components

#define FOR_EACH(Base, Itr, Var) \
NCBI_CS_ITERATE (Base##_Test(Var), Base##_Type, Itr, Base##_Get(Var))

/// EDIT_EACH base macro calls NCBI_NC_ITERATE with generated components

#define EDIT_EACH(Base, Itr, Var) \
NCBI_NC_ITERATE (Base##_Test(Var), Base##_Type, Itr, Base##_Set(Var))

/// ADD_ITEM base macro

#define ADD_ITEM(Base, Var, Ref) \
(Base##_Set(Var).push_back(Ref))

/// LIST_ERASE_ITEM base macro

#define LIST_ERASE_ITEM(Base, Itr, Var) \
(Base##_Set(Var).erase(Itr))

/// VECTOR_ERASE_ITEM base macro

#define VECTOR_ERASE_ITEM(Base, Itr, Var) \
(VECTOR_ERASE (Itr, Base##_Set(Var)))

/// ITEM_HAS base macro

#define ITEM_HAS(Base, Var) \
(Base##_Test(Var))

/// FIELD_IS_EMPTY base macro

#define FIELD_IS_EMPTY(Base, Var) \
    (Base##_Test(Var) && Base##_Get(Var).empty() )

/// RAW_FIELD_IS_EMPTY base macro

#define RAW_FIELD_IS_EMPTY(Var, Fld) \
    ( (Var).IsSet##Fld() && (Var).Get##Fld().empty() )

/// FIELD_IS_EMPTY_OR_UNSET base macro

#define FIELD_IS_EMPTY_OR_UNSET(Base, Var) \
    ( ! Base##_Test(Var) || Base##_Get(Var).empty() )

/// RAW_FIELD_IS_EMPTY_OR_UNSET macro

#define RAW_FIELD_IS_EMPTY_OR_UNSET(Var, Fld) \
    ( ! (Var).IsSet##Fld() || (Var).Get##Fld().empty() )

/// RAW_FIELD_GET_SIZE_IF_SET

// returns the size of the field if it's set; else zero.
#define RAW_FIELD_GET_SIZE_IF_SET(Var, Fld) \
    ( (Var).IsSet##Fld() ? (Var).Get##Fld().size() : 0 )

/// SET_FIELD_IF_UNSET macro

// (The do-while is just so the user has to put a semi-colon after it)
#define SET_FIELD_IF_UNSET(Var, Fld, Val) \
    do { if( ! (Var).IsSet##Fld() ) { (Var).Set##Fld(Val); ChangeMade(CCleanupChange::eChangeQualifiers); } } while(false)

/// RESET_FIELD_IF_EMPTY base macro

// (The do-while is just so the user has to put a semi-colon after it)
#define REMOVE_IF_EMPTY_FIELD(Base, Var) \
    do { if( FIELD_IS_EMPTY(Base, Var) ) { Base##_Reset(Var); ChangeMade(CCleanupChange::eChangeQualifiers); } } while(false)

/// GET_STRING_OR_BLANK base macro

#define GET_STRING_OR_BLANK(Base, Var) \
    (Base##_Test(Var) ? Base##_Get(Var) : kEmptyStr )

/// CHOICE_IS base macro

#define CHOICE_IS(Base, Var, Chs) \
(Base##_Test(Var) && Base##_Chs(Var) == Chs)


/// SWITCH_ON base macro calls NCBI_SWITCH with generated components

#define SWITCH_ON(Base, Var) \
NCBI_SWITCH (Base##_Test(Var), Base##_Chs(Var))


// is_sorted template

template <class Iter, class Comp>
bool seq_mac_is_sorted(Iter first, Iter last, Comp comp)
{
    if (first == last)
        return true;
    
    Iter next = first;
    for (++next; next != last; first = next, ++next) {
        if (comp(*next, *first))
            return false;
    }
    
    return true;
}


// is_unique template assumes that the container is already sorted

template <class Iterator, class Predicate>
bool seq_mac_is_unique(Iterator iter1, Iterator iter2, Predicate pred)
{
    Iterator prev = iter1;
    if (iter1 == iter2) {
        return true;
    }
    for (++iter1;  iter1 != iter2;  ++iter1, ++prev) {
        if (pred(*iter1, *prev)) {
            return false;
        }
    }
    return true;
}


/// IS_SORTED base macro

#define IS_SORTED(Base, Var, Func) \
((! Base##_Test(Var)) || \
seq_mac_is_sorted (Base##_Set(Var).begin(), \
                   Base##_Set(Var).end(), \
                   Func))

/// DO_LIST_SORT base macro

#define DO_LIST_SORT(Base, Var, Func) \
(Base##_Set(Var).sort (Func))

/// DO_VECTOR_SORT base macro

#define DO_VECTOR_SORT(Base, Var, Func) \
(stable_sort (Base##_Set(Var).begin(), \
              Base##_Set(Var).end(), \
              Func))

/// DO_LIST_SORT_HACK base macro

// This is more complex than some of the others
// to get around the WorkShop compiler's lack of support
// for member template functions.
// This should only be needed when you're sorting
// by a function object rather than a plain function.
#define DO_LIST_SORT_HACK(Base, Var, Func) \
    do { \
        vector< Base##_Type::value_type > vec; \
        copy( Base##_Get(Var).begin(), Base##_Get(Var).end(), back_inserter(vec) ); \
        stable_sort( vec.begin(), vec.end(), Func ); \
        Base##_Set(Var).clear(); \
        copy( vec.begin(), vec.end(), back_inserter(Base##_Set(Var)) ); \
    } while(false) // The purpose of the one-time do-while is to force a semicolon


/// IS_UNIQUE base macro

#define IS_UNIQUE(Base, Var, Func) \
((! Base##_Test(Var)) || \
seq_mac_is_unique (Base##_Set(Var).begin(), \
                   Base##_Set(Var).end(), \
                   Func))

/// DO_UNIQUE base macro

#define DO_UNIQUE(Base, Var, Func) \
{ \
    Base##_Type::iterator it = unique (Base##_Set(Var).begin(), \
                                       Base##_Set(Var).end(), \
                                       Func); \
    it = Base##_Set(Var).erase(it, Base##_Set(Var).end()); \
}

// keeps only the first of all the ones that match
#define UNIQUE_WITHOUT_SORT(Base, Var, FuncType, CleanupChangeType) \
{ \
    if( Base##_Test(Var) ) { \
      set<Base##_Type::value_type, FuncType> valuesAlreadySeen; \
      Base##_Type non_duplicate_items; \
      FOR_EACH(Base, iter, Var ) { \
          if( valuesAlreadySeen.find(*iter) == valuesAlreadySeen.end() ) { \
              non_duplicate_items.push_back( *iter ); \
              valuesAlreadySeen.insert( *iter ); \
          } \
      } \
      if( Base##_Get(Var).size() != non_duplicate_items.size() ) { \
          ChangeMade(CleanupChangeType); \
      } \
      Base##_Set(Var).swap( non_duplicate_items ); \
    } \
}

// careful of side effects
#define BEGIN_COMMA_END(container) \
    (container).begin(), (container).end()


// "FOR_EACH_XXX_ON_YYY" does a linear const traversal of STL containers
// "EDIT_EACH_XXX_ON_YYY" does a linear non-const traversal of STL containers

// "SWITCH_ON_XXX_CHOICE" switches on the item subtype

// "ADD_XXX_TO_YYY" adds an element to a specified object
// "ERASE_XXX_ON_YYY" deletes a specified object within an iterator

// Miscellaneous macros for testing objects include
// "XXX_IS_YYY" or "XXX_HAS_YYY"
// "XXX_CHOICE_IS"


///
/// list <string> macros

/// STRING_IN_LIST macros

#define STRING_IN_LIST_Type      list <string>
#define STRING_IN_LIST_Test(Var) (! (Var).empty())
#define STRING_IN_LIST_Get(Var)  (Var)
#define STRING_IN_LIST_Set(Var)  (Var)

/// LIST_HAS_STRING

#define LIST_HAS_STRING(Var) \
ITEM_HAS (STRING_IN_LIST, Var)

/// FOR_EACH_STRING_IN_LIST
/// EDIT_EACH_STRING_IN_LIST
// list <string>& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_STRING_IN_LIST(Itr, Var) \
FOR_EACH (STRING_IN_LIST, Itr, Var)

#define EDIT_EACH_STRING_IN_LIST(Itr, Var) \
EDIT_EACH (STRING_IN_LIST, Itr, Var)

/// ADD_STRING_TO_LIST

#define ADD_STRING_TO_LIST(Var, Ref) \
ADD_ITEM (STRING_IN_LIST, Var, Ref)

/// ERASE_STRING_IN_LIST

#define ERASE_STRING_IN_LIST(Itr, Var) \
LIST_ERASE_ITEM (STRING_IN_LIST, Itr, Var)

/// STRING_IN_LIST_IS_SORTED

#define STRING_IN_LIST_IS_SORTED(Var, Func) \
IS_SORTED (STRING_IN_LIST, Var, Func)

/// SORT_STRING_IN_LIST

#define SORT_STRING_IN_LIST(Var, Func) \
DO_LIST_SORT (STRING_IN_LIST, Var, Func)

/// STRING_IN_LIST_IS_UNIQUE

#define STRING_IN_LIST_IS_UNIQUE(Var, Func) \
IS_UNIQUE (STRING_IN_LIST, Var, Func)

/// UNIQUE_STRING_IN_LIST

#define UNIQUE_STRING_IN_LIST(Var, Func) \
DO_UNIQUE (STRING_IN_LIST, Var, Func)


///
/// vector <string> macros

/// STRING_IN_VECTOR macros

#define STRING_IN_VECTOR_Type      vector <string>
#define STRING_IN_VECTOR_Test(Var) (! (Var).empty())
#define STRING_IN_VECTOR_Get(Var)  (Var)
#define STRING_IN_VECTOR_Set(Var)  (Var)

/// VECTOR_HAS_STRING

#define VECTOR_HAS_STRING(Var) \
ITEM_HAS (STRING_IN_VECTOR, Var)

/// FOR_EACH_STRING_IN_VECTOR
/// EDIT_EACH_STRING_IN_VECTOR
// vector <string>& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_STRING_IN_VECTOR(Itr, Var) \
FOR_EACH (STRING_IN_VECTOR, Itr, Var)

#define EDIT_EACH_STRING_IN_VECTOR(Itr, Var) \
EDIT_EACH (STRING_IN_VECTOR, Itr, Var)

/// ADD_STRING_TO_VECTOR

#define ADD_STRING_TO_VECTOR(Var, Ref) \
ADD_ITEM (STRING_IN_VECTOR, Var, Ref)

/// ERASE_STRING_IN_VECTOR

#define ERASE_STRING_IN_VECTOR(Itr, Var) \
VECTOR_ERASE_ITEM (STRING_IN_VECTOR, Itr, Var)

/// STRING_IN_VECTOR_IS_SORTED

#define STRING_IN_VECTOR_IS_SORTED(Var, Func) \
IS_SORTED (STRING_IN_VECTOR, Var, Func)

/// SORT_STRING_IN_VECTOR

#define SORT_STRING_IN_VECTOR(Var, Func) \
DO_VECTOR_SORT (STRING_IN_VECTOR, Var, Func)

/// STRING_IN_VECTOR_IS_UNIQUE

#define STRING_IN_VECTOR_IS_UNIQUE(Var, Func) \
IS_UNIQUE (STRING_IN_VECTOR, Var, Func)

/// UNIQUE_STRING_IN_VECTOR

#define UNIQUE_STRING_IN_VECTOR(Var, Func) \
DO_UNIQUE (STRING_IN_VECTOR, Var, Func)


///
/// <string> macros

/// CHAR_IN_STRING macros

#define CHAR_IN_STRING_Type      string
#define CHAR_IN_STRING_Test(Var) (! (Var).empty())
#define CHAR_IN_STRING_Get(Var)  (Var)
#define CHAR_IN_STRING_Set(Var)  (Var)

/// STRING_HAS_CHAR

#define STRING_HAS_CHAR(Var) \
ITEM_HAS (CHAR_IN_STRING, Var)

/// FOR_EACH_CHAR_IN_STRING
/// EDIT_EACH_CHAR_IN_STRING
// string& as input, dereference with [const] char& ch = *itr;

#define FOR_EACH_CHAR_IN_STRING(Itr, Var) \
FOR_EACH (CHAR_IN_STRING, Itr, Var)

#define EDIT_EACH_CHAR_IN_STRING(Itr, Var) \
EDIT_EACH (CHAR_IN_STRING, Itr, Var)

/// ADD_CHAR_TO_STRING

#define ADD_CHAR_TO_STRING(Var, Ref) \
ADD_ITEM (CHAR_IN_STRING, Var, Ref)

/// ERASE_CHAR_IN_STRING

#define ERASE_CHAR_IN_STRING(Itr, Var) \
LIST_ERASE_ITEM (CHAR_IN_STRING, Itr, Var)

/// CHAR_IN_STRING_IS_SORTED

#define CHAR_IN_STRING_IS_SORTED(Var, Func) \
IS_SORTED (CHAR_IN_STRING, Var, Func)

/// SORT_CHAR_IN_STRING

#define SORT_CHAR_IN_STRING(Var, Func) \
DO_LIST_SORT (CHAR_IN_STRING, Var, Func)

/// CHAR_IN_STRING_IS_UNIQUE

#define CHAR_IN_STRING_IS_UNIQUE(Var, Func) \
IS_UNIQUE (CHAR_IN_STRING, Var, Func)

/// UNIQUE_CHAR_IN_STRING

#define UNIQUE_CHAR_IN_STRING(Var, Func) \
DO_UNIQUE (CHAR_IN_STRING, Var, Func)

/// CHAR_IN_STRING_IS_EMPTY

#define CHAR_IN_STRING_IS_EMPTY(Var) \
    FIELD_IS_EMPTY(CHAR_IN_STRING, Var, Func)


///
/// Generic FIELD macros

/// FIELD_IS base macro

#define FIELD_IS(Var, Fld) \
    ((Var).Is##Fld())

/// FIELD_CHAIN_OF_2_IS base macro

#define FIELD_CHAIN_OF_2_IS(Var, Fld1, Fld2) \
    ( (Var).Is##Fld1() && \
      (Var).Get##Fld1().Is##Fld2() )

/// FIELD_IS_SET_AND_IS base macro

#define FIELD_IS_SET_AND_IS(Var, Fld, Chs) \
    ( (Var).IsSet##Fld() && (Var).Get##Fld().Is##Chs() )

/// FIELD_IS_AND_IS_SET base macro

#define FIELD_IS_AND_IS_SET(Var, Chs, Fld)                          \
    ( (Var).Is##Chs() && (Var).Get##Chs().IsSet##Fld() )

/// FIELD_IS_SET base macro

#define FIELD_IS_SET(Var, Fld) \
    ((Var).IsSet##Fld())

/// FIELD_CHAIN_OF_2_IS_SET

#define FIELD_CHAIN_OF_2_IS_SET(Var, Fld1, Fld2) \
    ( (Var).IsSet##Fld1() && \
      (Var).Get##Fld1().IsSet##Fld2() )

/// FIELD_CHAIN_OF_3_IS_SET

#define FIELD_CHAIN_OF_3_IS_SET(Var, Fld1, Fld2, Fld3) \
    ( (Var).IsSet##Fld1() && \
      (Var).Get##Fld1().IsSet##Fld2() && \
      (Var).Get##Fld1().Get##Fld2().IsSet##Fld3() )

/// FIELD_CHAIN_OF_4_IS_SET

#define FIELD_CHAIN_OF_4_IS_SET(Var, Fld1, Fld2, Fld3, Fld4) \
    ( (Var).IsSet##Fld1() && \
      (Var).Get##Fld1().IsSet##Fld2() && \
      (Var).Get##Fld1().Get##Fld2().IsSet##Fld3() && \
      (Var).Get##Fld1().Get##Fld2().Get##Fld3().IsSet##Fld4() )


/// FIELD_CHAIN_OF_5_IS_SET

#define FIELD_CHAIN_OF_5_IS_SET(Var, Fld1, Fld2, Fld3, Fld4, Fld5) \
    ( (Var).IsSet##Fld1() && \
    (Var).Get##Fld1().IsSet##Fld2() && \
    (Var).Get##Fld1().Get##Fld2().IsSet##Fld3() && \
    (Var).Get##Fld1().Get##Fld2().Get##Fld3().IsSet##Fld4() && \
    (Var).Get##Fld1().Get##Fld2().Get##Fld3().Get##Fld4().IsSet##Fld5() )

/// GET_FIELD base macro

#define GET_FIELD(Var, Fld) \
    ((Var).Get##Fld())

/// GET_FIELD_OR_DEFAULT base macro

#define GET_FIELD_OR_DEFAULT(Var, Fld, Dflt) \
    ((Var).IsSet##Fld() ? (Var).Get##Fld() : Dflt )
    
/// GET_FIELD_CHOICE_OR_DEFAULT base macro

#define GET_FIELD_CHOICE_OR_DEFAULT(Var, Fld, Dflt) \
    ((Var).Is##Fld() ? (Var).Get##Fld() : Dflt )


/// GET_MUTABLE base macro

#define GET_MUTABLE(Var, Fld) \
    ((Var).Set##Fld())

/// SET_FIELD base macro

#define SET_FIELD(Var, Fld, Val) \
    ((Var).Set##Fld(Val))

/// RESET_FIELD base macro

#define RESET_FIELD(Var, Fld) \
    ((Var).Reset##Fld())


/// STRING_FIELD_MATCH base macro

#define STRING_FIELD_MATCH(Var, Fld, Str) \
    ((Var).IsSet##Fld() && NStr::EqualNocase((Var).Get##Fld(), Str))

/// STRING_FIELD_MATCH_BUT_ONLY_CASE_INSENSITIVE base macro

#define STRING_FIELD_MATCH_BUT_ONLY_CASE_INSENSITIVE(Var, Fld, Str) \
    ((Var).IsSet##Fld() && NStr::EqualNocase((Var).Get##Fld(), (Str)) && \
        (Var).Get##Fld() != (Str) )

/// STRING_FIELD_CHOICE_MATCH base macro

#define STRING_FIELD_CHOICE_MATCH( Var, Fld, Chs, Value) \
    ( (Var).IsSet##Fld() && (Var).Get##Fld().Is##Chs() && \
      NStr::EqualNocase((Var).Get##Fld().Get##Chs(), (Value)) )


/// GET_STRING_FLD_OR_BLANK base macro

#define GET_STRING_FLD_OR_BLANK(Var, Fld) \
    ( (Var).IsSet##Fld() ? (Var).Get##Fld() : kEmptyStr )

/// STRING_FIELD_NOT_EMPTY base macro

#define STRING_FIELD_NOT_EMPTY(Var, Fld) \
    ( (Var).IsSet##Fld() && ! (Var).Get##Fld().empty() )

/// STRING_FIELD_APPEND base macro
/// Appends Str to Var's Fld, putting Delim between if Fld was previously non-empty
/// Nothing is done if Str is empty.
#define STRING_FIELD_APPEND(Var, Fld, Delim, Str)       \
    do {                                                \
        const string sStr = (Str);                      \
        if( ! sStr.empty() ) {                          \
            if( ! (Var).IsSet##Fld() ) {                \
                (Var).Set##Fld("");                     \
            }                                           \
            string & field = (Var).Set##Fld();          \
            if( ! field.empty() ) {                     \
                field += (Delim);                       \
            }                                           \
            field += sStr;                              \
        }                                               \
    } while(false)


/// STRING_SET_MATCH base macro (for list or vectors)

#define STRING_SET_MATCH(Var, Fld, Str) \
    ((Var).IsSet##Fld() && NStr::FindNoCase((Var).Get##Fld(), Str) != NULL)

/// FIELD_OUT_OF_RANGE base macro

#define FIELD_OUT_OF_RANGE(Var, Fld, Lower, Upper) \
    ( (Var).IsSet##Fld() && ( (Var).Get##Fld() < (Lower) || (Var).Get##Fld() > (Upper) ) )

/// FIELD_EQUALS base macro

#define FIELD_EQUALS( Var, Fld, Value ) \
    ( (Var).IsSet##Fld() && (Var).Get##Fld() == (Value) )

/// FIELD_CHOICE_EQUALS base macro

#define FIELD_CHOICE_EQUALS( Var, Fld, Chs, Value) \
    ( (Var).IsSet##Fld() && (Var).Get##Fld().Is##Chs() && \
      (Var).Get##Fld().Get##Chs() == (Value) )

#define FIELD_CHOICE_EMPTY( Var, Fld, Chs) \
    ( ! (Var).IsSet##Fld() || ! (Var).Get##Fld().Is##Chs() || \
      (Var).Get##Fld().Get##Chs().empty() )

/// CALL_IF_SET base macro

#define CALL_IF_SET( Func, Var, Fld ) \
    { \
        if( (Var).IsSet##Fld() ) { \
            Func( GET_MUTABLE( (Var), Fld) ); \
        } \
    }

/// CALL_IF_SET_CHAIN_2 base macro

#define CALL_IF_SET_CHAIN_2( Func, Var, Fld1, Fld2 ) \
    { \
        if( (Var).IsSet##Fld1() ) { \
            CALL_IF_SET( Func, (Var).Set##Fld1(), Fld2 ); \
        } \
    }

/// CLONE_IF_SET base macro
/// (Useful to copy and object from a variable of one
/// type to a variable of another type)
/// If SrcVar doesn't have SrcFld set, it's reset on the DestVar also.

#define CLONE_IF_SET_ELSE_RESET(DestVar, DestFld, SrcVar, SrcFld)       \
    do {                                                                \
        if( FIELD_IS_SET(SrcVar, SrcFld) ) {                            \
            (DestVar).Set##DestFld( *SerialClone( GET_FIELD(SrcVar, SrcFld) ) ); \
        } else {                                                        \
            (DestVar).Reset##DestFld();                                 \
        }                                                               \
    } while(0)

/// ASSIGN_IF_SET_ELSE_RESET

#define ASSIGN_IF_SET_ELSE_RESET(DestVar, DestFld, SrcVar, SrcFld)       \
    do {                                                                \
        if( FIELD_IS_SET(SrcVar, SrcFld) ) {                            \
            (DestVar).Set##DestFld( GET_FIELD(SrcVar, SrcFld) ); \
        } else {                                                        \
            (DestVar).Reset##DestFld();                                 \
        }                                                               \
    } while(0)

/// TEST_FIELD_CHOICE

#define TEST_FIELD_CHOICE( Var, Fld, Chs ) \
    ( (Var).IsSet##Fld() && (Var).Get##Fld().Which() == (Chs) )

END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJECTS_MISC___SEQUENCE_UTIL_MACROS__HPP */

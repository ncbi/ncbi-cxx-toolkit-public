#ifndef OBJECTS_SEQBLOCK___SEQBLOCK_MACROS__HPP
#define OBJECTS_SEQBLOCK___SEQBLOCK_MACROS__HPP

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

/// @file seqblock_macros.hpp
/// Utility macros and typedefs for exploring NCBI objects from seqblock.asn.


#include <objects/misc/sequence_util_macros.hpp>
#include <objects/seqblock/seqblock__.hpp>


/// @NAME Convenience macros for NCBI objects
/// @{


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// "FOR_EACH_XXX_ON_YYY" does a linear const traversal of STL containers
// "EDIT_EACH_XXX_ON_YYY" does a linear non-const traversal of STL containers

// "SWITCH_ON_XXX_CHOICE" switches on the item subtype

// "ADD_XXX_TO_YYY" adds an element to a specified object
// "ERASE_XXX_ON_YYY" deletes a specified object within an iterator

// Miscellaneous macros for testing objects include
// "XXX_IS_YYY" or "XXX_HAS_YYY"
// "XXX_CHOICE_IS"


///
/// CGB_block macros

/// EXTRAACCN_ON_GENBANKBLOCK macros

#define EXTRAACCN_ON_GENBANKBLOCK_Type      CGB_block::TExtra_accessions
#define EXTRAACCN_ON_GENBANKBLOCK_Test(Var) (Var).IsSetExtra_accessions()
#define EXTRAACCN_ON_GENBANKBLOCK_Get(Var)  (Var).GetExtra_accessions()
#define EXTRAACCN_ON_GENBANKBLOCK_Set(Var)  (Var).SetExtra_accessions()

/// GENBANKBLOCK_HAS_EXTRAACCN

#define GENBANKBLOCK_HAS_EXTRAACCN(Var) \
ITEM_HAS (EXTRAACCN_ON_GENBANKBLOCK, Var)

/// FOR_EACH_EXTRAACCN_ON_GENBANKBLOCK
/// EDIT_EACH_EXTRAACCN_ON_GENBANKBLOCK
// CGB_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_EXTRAACCN_ON_GENBANKBLOCK(Itr, Var) \
FOR_EACH (EXTRAACCN_ON_GENBANKBLOCK, Itr, Var)

#define EDIT_EACH_EXTRAACCN_ON_GENBANKBLOCK(Itr, Var) \
EDIT_EACH (EXTRAACCN_ON_GENBANKBLOCK, Itr, Var)

/// ADD_EXTRAACCN_TO_GENBANKBLOCK

#define ADD_EXTRAACCN_TO_GENBANKBLOCK(Var, Ref) \
ADD_ITEM (EXTRAACCN_ON_GENBANKBLOCK, Var, Ref)

/// ERASE_EXTRAACCN_ON_GENBANKBLOCK

#define ERASE_EXTRAACCN_ON_GENBANKBLOCK(Itr, Var) \
LIST_ERASE_ITEM (EXTRAACCN_ON_GENBANKBLOCK, Itr, Var)

/// EXTRAACCN_ON_GENBANKBLOCK_IS_SORTED

#define EXTRAACCN_ON_GENBANKBLOCK_IS_SORTED(Var, Func) \
IS_SORTED (EXTRAACCN_ON_GENBANKBLOCK, Var, Func)

/// SORT_EXTRAACCN_ON_GENBANKBLOC

#define SORT_EXTRAACCN_ON_GENBANKBLOCK(Var, Func) \
DO_LIST_SORT (EXTRAACCN_ON_GENBANKBLOCK, Var, Func)

/// EXTRAACCN_ON_GENBANKBLOCK_IS_UNIQUE

#define EXTRAACCN_ON_GENBANKBLOCK_IS_UNIQUE(Var, Func) \
IS_UNIQUE (EXTRAACCN_ON_GENBANKBLOCK, Var, Func)

/// UNIQUE_EXTRAACCN_ON_GENBANKBLOCK

#define UNIQUE_EXTRAACCN_ON_GENBANKBLOCK(Var, Func) \
DO_UNIQUE (EXTRAACCN_ON_GENBANKBLOCK, Var, Func)


/// KEYWORD_ON_GENBANKBLOCK macros

#define KEYWORD_ON_GENBANKBLOCK_Type      CGB_block::TKeywords
#define KEYWORD_ON_GENBANKBLOCK_Test(Var) (Var).IsSetKeywords()
#define KEYWORD_ON_GENBANKBLOCK_Get(Var)  (Var).GetKeywords()
#define KEYWORD_ON_GENBANKBLOCK_Set(Var)  (Var).SetKeywords()

/// GENBANKBLOCK_HAS_KEYWORD

#define GENBANKBLOCK_HAS_KEYWORD(Var) \
ITEM_HAS (KEYWORD_ON_GENBANKBLOCK, Var)

/// FOR_EACH_KEYWORD_ON_GENBANKBLOCK
/// EDIT_EACH_KEYWORD_ON_GENBANKBLOCK
// CGB_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_KEYWORD_ON_GENBANKBLOCK(Itr, Var) \
FOR_EACH (KEYWORD_ON_GENBANKBLOCK, Itr, Var)

#define EDIT_EACH_KEYWORD_ON_GENBANKBLOCK(Itr, Var) \
EDIT_EACH (KEYWORD_ON_GENBANKBLOCK, Itr, Var)

/// ADD_KEYWORD_TO_GENBANKBLOCK

#define ADD_KEYWORD_TO_GENBANKBLOCK(Var, Ref) \
ADD_ITEM (KEYWORD_ON_GENBANKBLOCK, Var, Ref)

/// ERASE_KEYWORD_ON_GENBANKBLOCK

#define ERASE_KEYWORD_ON_GENBANKBLOCK(Itr, Var) \
LIST_ERASE_ITEM (KEYWORD_ON_GENBANKBLOCK, Itr, Var)

/// KEYWORD_ON_GENBANKBLOCK_IS_SORTED

#define KEYWORD_ON_GENBANKBLOCK_IS_SORTED(Var, Func) \
IS_SORTED (KEYWORD_ON_GENBANKBLOCK, Var, Func)

/// SORT_KEYWORD_ON_GENBANKBLOCK

#define SORT_KEYWORD_ON_GENBANKBLOCK(Var, Func) \
DO_LIST_SORT (KEYWORD_ON_GENBANKBLOCK, Var, Func)

/// KEYWORD_ON_GENBANKBLOCK_IS_UNIQUE

#define KEYWORD_ON_GENBANKBLOCK_IS_UNIQUE(Var, Func) \
IS_UNIQUE (KEYWORD_ON_GENBANKBLOCK, Var, Func)

/// UNIQUE_KEYWORD_ON_GENBANKBLOCK

#define UNIQUE_KEYWORD_ON_GENBANKBLOCK(Var, Func) \
DO_UNIQUE (KEYWORD_ON_GENBANKBLOCK, Var, Func)

/// UNIQUE_WITHOUT_SORT_KEYWORD_ON_GENBANKBLOCK

#define UNIQUE_WITHOUT_SORT_KEYWORD_ON_GENBANKBLOCK(Var, FuncType) \
UNIQUE_WITHOUT_SORT( KEYWORD_ON_GENBANKBLOCK, Var, FuncType, \
    CCleanupChange::eCleanQualifiers )

///
/// CEMBL_block macros

/// EXTRAACCN_ON_EMBLBLOCK macros

#define EXTRAACCN_ON_EMBLBLOCK_Type      CEMBL_block::TExtra_acc
#define EXTRAACCN_ON_EMBLBLOCK_Test(Var) (Var).IsSetExtra_acc()
#define EXTRAACCN_ON_EMBLBLOCK_Get(Var)  (Var).GetExtra_acc()
#define EXTRAACCN_ON_EMBLBLOCK_Set(Var)  (Var).SetExtra_acc()

/// EMBLBLOCK_HAS_EXTRAACCN

#define EMBLBLOCK_HAS_EXTRAACCN(Var) \
ITEM_HAS (EXTRAACCN_ON_EMBLBLOCK, Var)

/// FOR_EACH_EXTRAACCN_ON_EMBLBLOCK
/// EDIT_EACH_EXTRAACCN_ON_EMBLBLOCK
// CEMBL_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_EXTRAACCN_ON_EMBLBLOCK(Itr, Var) \
FOR_EACH (EXTRAACCN_ON_EMBLBLOCK, Itr, Var)

#define EDIT_EACH_EXTRAACCN_ON_EMBLBLOCK(Itr, Var) \
EDIT_EACH (EXTRAACCN_ON_EMBLBLOCK, Itr, Var)

/// ADD_EXTRAACCN_TO_EMBLBLOCK

#define ADD_EXTRAACCN_TO_EMBLBLOCK(Var, Ref) \
ADD_ITEM (EXTRAACCN_ON_EMBLBLOCK, Var, Ref)

/// ERASE_EXTRAACCN_ON_EMBLBLOCK

#define ERASE_EXTRAACCN_ON_EMBLBLOCK(Itr, Var) \
LIST_ERASE_ITEM (EXTRAACCN_ON_EMBLBLOCK, Itr, Var)

/// EXTRAACCN_ON_EMBLBLOCK_IS_SORTED

#define EXTRAACCN_ON_EMBLBLOCK_IS_SORTED(Var, Func) \
IS_SORTED (EXTRAACCN_ON_EMBLBLOCK, Var, Func)

/// SORT_EXTRAACCN_ON_EMBLBLOCK

#define SORT_EXTRAACCN_ON_EMBLBLOCK(Var, Func) \
DO_LIST_SORT (EXTRAACCN_ON_EMBLBLOCK, Var, Func)

/// EXTRAACCN_ON_EMBLBLOCK_IS_UNIQUE

#define EXTRAACCN_ON_EMBLBLOCK_IS_UNIQUE(Var, Func) \
IS_UNIQUE (EXTRAACCN_ON_EMBLBLOCK, Var, Func)

/// UNIQUE_EXTRAACCN_ON_EMBLBLOCK

#define UNIQUE_EXTRAACCN_ON_EMBLBLOCK(Var, Func) \
DO_UNIQUE (EXTRAACCN_ON_EMBLBLOCK, Var, Func)


/// KEYWORD_ON_EMBLBLOCK macros

#define KEYWORD_ON_EMBLBLOCK_Type      CEMBL_block::TKeywords
#define KEYWORD_ON_EMBLBLOCK_Test(Var) (Var).IsSetKeywords()
#define KEYWORD_ON_EMBLBLOCK_Get(Var)  (Var).GetKeywords()
#define KEYWORD_ON_EMBLBLOCK_Set(Var)  (Var).SetKeywords()

/// EMBLBLOCK_HAS_KEYWORD

#define EMBLBLOCK_HAS_KEYWORD(Var) \
ITEM_HAS (KEYWORD_ON_EMBLBLOCK, Var)

/// FOR_EACH_KEYWORD_ON_EMBLBLOCK
/// EDIT_EACH_KEYWORD_ON_EMBLBLOCK
// CEMBL_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_KEYWORD_ON_EMBLBLOCK(Itr, Var) \
FOR_EACH (KEYWORD_ON_EMBLBLOCK, Itr, Var)

#define EDIT_EACH_KEYWORD_ON_EMBLBLOCK(Itr, Var) \
EDIT_EACH (KEYWORD_ON_EMBLBLOCK, Itr, Var)

/// ADD_KEYWORD_TO_EMBLBLOCK

#define ADD_KEYWORD_TO_EMBLBLOCK(Var, Ref) \
ADD_ITEM (KEYWORD_ON_EMBLBLOCK, Var, Ref)

/// ERASE_KEYWORD_ON_EMBLBLOCK

#define ERASE_KEYWORD_ON_EMBLBLOCK(Itr, Var) \
LIST_ERASE_ITEM (KEYWORD_ON_EMBLBLOCK, Itr, Var)

/// KEYWORD_ON_EMBLBLOCK_IS_SORTED

#define KEYWORD_ON_EMBLBLOCK_IS_SORTED(Var, Func) \
IS_SORTED (KEYWORD_ON_EMBLBLOCK, Var, Func)

/// SORT_KEYWORD_ON_EMBLBLOCK

#define SORT_KEYWORD_ON_EMBLBLOCK(Var, Func) \
DO_LIST_SORT (KEYWORD_ON_EMBLBLOCK, Var, Func)

/// KEYWORD_ON_EMBLBLOCK_IS_UNIQUE

#define KEYWORD_ON_EMBLBLOCK_IS_UNIQUE(Var, Func) \
IS_UNIQUE (KEYWORD_ON_EMBLBLOCK, Var, Func)

/// UNIQUE_KEYWORD_ON_EMBLBLOCK

#define UNIQUE_KEYWORD_ON_EMBLBLOCK(Var, Func) \
DO_UNIQUE (KEYWORD_ON_EMBLBLOCK, Var, Func)


/// UNIQUE_WITHOUT_SORT_KEYWORD_ON_EMBLBLOCK

#define UNIQUE_WITHOUT_SORT_KEYWORD_ON_EMBLBLOCK(Var, FuncType) \
UNIQUE_WITHOUT_SORT(KEYWORD_ON_EMBLBLOCK, Var, FuncType, \
    CCleanupChange::eCleanQualifiers)

///
/// CPDB_block macros

/// COMPOUND_ON_PDBBLOCK macros

#define COMPOUND_ON_PDBBLOCK_Type      CPDB_block::TCompound
#define COMPOUND_ON_PDBBLOCK_Test(Var) (Var).IsSetCompound()
#define COMPOUND_ON_PDBBLOCK_Get(Var)  (Var).GetCompound()
#define COMPOUND_ON_PDBBLOCK_Set(Var)  (Var).SetCompound()

/// PDBBLOCK_HAS_COMPOUND

#define PDBBLOCK_HAS_COMPOUND(Var) \
ITEM_HAS (COMPOUND_ON_PDBBLOCK, Var)

/// FOR_EACH_COMPOUND_ON_PDBBLOCK
/// EDIT_EACH_COMPOUND_ON_PDBBLOCK
// CPDB_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_COMPOUND_ON_PDBBLOCK(Itr, Var) \
FOR_EACH (COMPOUND_ON_PDBBLOCK, Itr, Var)

#define EDIT_EACH_COMPOUND_ON_PDBBLOCK(Itr, Var) \
EDIT_EACH (COMPOUND_ON_PDBBLOCK, Itr, Var)

/// ADD_COMPOUND_TO_PDBBLOCK

#define ADD_COMPOUND_TO_PDBBLOCK(Var, Ref) \
ADD_ITEM (COMPOUND_ON_PDBBLOCK, Var, Ref)

/// ERASE_COMPOUND_ON_PDBBLOCK

#define ERASE_COMPOUND_ON_PDBBLOCK(Itr, Var) \
LIST_ERASE_ITEM (COMPOUND_ON_PDBBLOCK, Itr, Var)


/// SOURCE_ON_PDBBLOCK macros

#define SOURCE_ON_PDBBLOCK_Type      CPDB_block::TSource
#define SOURCE_ON_PDBBLOCK_Test(Var) (Var).IsSetSource()
#define SOURCE_ON_PDBBLOCK_Get(Var)  (Var).GetSource()
#define SOURCE_ON_PDBBLOCK_Set(Var)  (Var).SetSource()

/// PDBBLOCK_HAS_SOURCE

#define PDBBLOCK_HAS_SOURCE(Var) \
ITEM_HAS (SOURCE_ON_PDBBLOCK, Var)

/// FOR_EACH_SOURCE_ON_PDBBLOCK
/// EDIT_EACH_SOURCE_ON_PDBBLOCK
// CPDB_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_SOURCE_ON_PDBBLOCK(Itr, Var) \
FOR_EACH (SOURCE_ON_PDBBLOCK, Itr, Var)

#define EDIT_EACH_SOURCE_ON_PDBBLOCK(Itr, Var) \
EDIT_EACH (SOURCE_ON_PDBBLOCK, Itr, Var)

/// ADD_SOURCE_TO_PDBBLOCK

#define ADD_SOURCE_TO_PDBBLOCK(Var, Ref) \
ADD_ITEM (SOURCE_ON_PDBBLOCK, Var, Ref)

/// ERASE_SOURCE_ON_PDBBLOCK

#define ERASE_SOURCE_ON_PDBBLOCK(Itr, Var) \
LIST_ERASE_ITEM (SOURCE_ON_PDBBLOCK, Itr, Var)


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJECTS_SEQBLOCK___SEQBLOCK_MACROS__HPP */

#ifndef SEQ_LOC_UTIL__HPP
#define SEQ_LOC_UTIL__HPP

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
* Author:  Clifford Clausen, Aaron Ucko, Aleksey Grichenko
*
* File Description:
*   Seq-loc utilities requiring CScope
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <util/range_coll.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Forward declarations
class CSeq_loc;
class CScope;
class CSeq_id_Handle;
class CSeq_id;

BEGIN_SCOPE(sequence)

// Flags for seq-loc operations.
// ePreserveStrand (default) treats ranges on plus and minus strands as
// different sub-sets.
// eIgnoreStrand does not make difference between plus and
// minus strands.
// Both modes set strand of each range in the result to the strand
// of the first original range covering the same interval.
// Strands may be reset by sorting (see below).
enum EStrandFlag {
    ePreserveStrand,  // preserve original strand
    eIgnoreStrand     // ignore strands
};

// eNoMerge (default) results in no merging or sorting of the result.
// eMerge merges overlapping ranges without sorting. When merging
// different strands, strand from the first interval is used.
// eMergeAndSort merges overlapping ranges and sorts them.
// All strands are set to unknow (in eIgnoreStrand mode) or
// plus and minus (in ePerserveStrand mode).
// Merging does not preserve the original seq-loc structure,
// it will be converted to mix.
enum EMergeFlag {
    eNoMerge,       // no merging or sorting
    eMerge,         // merge ranges without sorting
    eMergeAndSort   // merge and sort resulting seq-loc
};


// Base class for mapping IDs to the best synonym. Should provide
// GetBestSynonym() method which returns the ID which should replace
// the original one in the destination seq-loc.
class CSynonymMapper_Base
{
public:
    CSynonymMapper_Base(void) {}
    virtual ~CSynonymMapper_Base(void) {}

    virtual CSeq_id_Handle GetBestSynonym(const CSeq_id& id) = 0;
};


class CLengthGetter_Base
{
public:
    CLengthGetter_Base(void) {}
    virtual ~CLengthGetter_Base(void) {}

    virtual TSeqPos GetLength(const CSeq_id& id) = 0;
};


// If a scope is provided, all operations perform synonyms check and
// use the first of IDs in the result. The scope will also be used
// to get real length of a whole location.
NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc> Seq_loc_Add(const CSeq_loc& loc1,
                           const CSeq_loc& loc2,
                           EMergeFlag  merge_flag = eNoMerge,
                           EStrandFlag strand_flag = ePreserveStrand,
                           CScope* scope = 0);

NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc> Seq_loc_Subtract(const CSeq_loc& loc1,
                                const CSeq_loc& loc2,
                                EMergeFlag  merge_flag = eNoMerge,
                                EStrandFlag strand_flag = ePreserveStrand,
                                CScope* scope = 0);

// Merge flag should be eMerge or eMergeAndSort. If it's eNoMerge
// the method returns a copy of the original location.
NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc> Seq_loc_Merge(const CSeq_loc& loc,
                             EMergeFlag  merge_flag = eNoMerge,
                             EStrandFlag strand_flag = ePreserveStrand,
                             CScope* scope = 0);

// Synonym mapper should provide a method to get the best synonym
// for each ID. The best synonym is used in the destination seq-loc.
// Length getter is used to get length of a whole location.
NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc> Seq_loc_Add(const CSeq_loc& loc1,
                           const CSeq_loc& loc2,
                           CSynonymMapper_Base& syn_mapper,
                           EMergeFlag  merge_flag = eNoMerge,
                           EStrandFlag strand_flag = ePreserveStrand);

NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc> Seq_loc_Subtract(const CSeq_loc& loc1,
                                const CSeq_loc& loc2,
                                CSynonymMapper_Base& syn_mapper,
                                CLengthGetter_Base&  len_getter,
                                EMergeFlag  merge_flag = eNoMerge,
                                EStrandFlag strand_flag = ePreserveStrand);

NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc> Seq_loc_Merge(const CSeq_loc& loc,
                             CSynonymMapper_Base& syn_mapper,
                             EMergeFlag  merge_flag = eNoMerge,
                             EStrandFlag strand_flag = ePreserveStrand);

END_SCOPE(sequence)
END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.1  2004/10/20 18:09:43  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* SEQ_LOC_UTIL__HPP */

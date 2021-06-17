#ifndef SEQ_ALIGN_UTIL__HPP
#define SEQ_ALIGN_UTIL__HPP

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
* Author:  Aleksey Grichenko
*
* File Description:
*   Seq-align utilities
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqalign/Seq_align.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Forward declarations
class CSeq_loc;
class CScope;

BEGIN_SCOPE(sequence)

/** @addtogroup ObjUtilSeqAlign
 *
 * @{
 */

/** @name Seq-align mapping
 * Seq-align mapping
 * @{
 */

/// Remap seq-align row to the seq-loc.
/// Treats the given row as being relative to the location, maps it
/// to the sequence(s) referenced by this location.
/// @param align
///   The seq-align object to be mapped (the object will be modified!).
/// @param row
///   Row to be mapped.
/// @param loc
///   Seq-loc to which the row should be mapped.
/// @param scope
///   Optional scope may be required by CSeq_loc_Mapper to process
///   some locations (e.g. whole locations).
/// @result
///   Reference to the new seq-align with the mapped row.
NCBI_XOBJUTIL_EXPORT
CRef<CSeq_align> RemapAlignToLoc(const CSeq_align& align,
                                 CSeq_align::TDim  row,
                                 const CSeq_loc&   loc,
                                 CScope*           scope = NULL);


/// Given a spliced-seg alignment with MismatchedBases user object, compose
/// product sequence data. Return empty string if the alignment is not a
/// spliced-seg or if it does not contain MismatchedBases.
NCBI_XOBJUTIL_EXPORT
string GetProductString(const CSeq_align& align, CScope& scope);


/* @} */


END_SCOPE(sequence)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* SEQ_ALIGN_UTIL__HPP */

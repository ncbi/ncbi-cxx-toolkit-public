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

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/seq_align_util.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(sequence)


CRef<CSeq_align> RemapAlignToLoc(const CSeq_align& align,
                                 CSeq_align::TDim  row,
                                 const CSeq_loc&   loc,
                                 CScope*           scope)
{
    if ( loc.IsWhole() ) {
        CRef<CSeq_align> copy(new CSeq_align);
        copy->Assign(align);
        return copy;
    }
    const CSeq_id* orig_id = loc.GetId();
    if ( !orig_id ) {
        NCBI_THROW(CAnnotMapperException, eBadLocation,
                   "Location with multiple ids can not be used to "
                   "remap seq-aligns.");
    }
    CRef<CSeq_id> id(new CSeq_id);
    id->Assign(*orig_id);

    // Create source seq-loc
    CSeq_loc src_loc(*id, 0, GetLength(loc, scope) - 1);
    ENa_strand strand = loc.GetStrand();
    if (strand != eNa_strand_unknown) {
        src_loc.SetStrand(strand);
    }
    CSeq_loc_Mapper mapper(src_loc, loc, scope);
    return mapper.Map(align, row);
}


END_SCOPE(sequence)
END_SCOPE(objects)
END_NCBI_SCOPE

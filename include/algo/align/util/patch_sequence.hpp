#ifndef ALGO_ALIGN_UTIL___PATCHSEQUENCE__HPP
#define ALGO_ALIGN_UTIL___PATCHSEQUENCE__HPP

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
 * Authors:  Eyal Mozes
 *
 * File Description:
 *
 */

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CSeq_id_Handle;
    class CSeq_align;
    class CSeq_inst;
    class CScope;
END_SCOPE(objects)


/// Patch the Seq-inst of alignment's target sequence by substituting
/// query sequence.
CRef<objects::CSeq_inst> NCBI_XALGOALIGN_EXPORT
PatchTargetSequence(CRef<objects::CSeq_align> alignment,
                    objects::CScope &scope);

/// Patch the Seq-inst of alignment's target sequence by substituting
/// all query sequences. All alignments on the list must have the same
/// target sequence.
CRef<objects::CSeq_inst> NCBI_XALGOALIGN_EXPORT
PatchTargetSequence(const list< CRef<objects::CSeq_align> > &alignments,
                    objects::CScope &scope);

void NCBI_XALGOALIGN_EXPORT
AddNovelPatch(const objects::CSeq_id_Handle &idh);

END_NCBI_SCOPE


#endif  // ALGO_ALIGN_UTIL___PATCHSEQUENCE__HPP

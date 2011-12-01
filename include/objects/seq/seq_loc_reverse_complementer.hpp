#ifndef SEQ_LOC_REVERSE_COMPLEMENTER__HPP
#define SEQ_LOC_REVERSE_COMPLEMENTER__HPP

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
* Author:  Clifford Clausen, Aaron Ucko, Aleksey Grichenko, Michael Kornbluh
*
* File Description:
*   Get reverse complement of a CSeq_loc.
*/

#include <objects/seqloc/Seq_loc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/// Wraps up any functionality needed that might be outside
/// the scope of this library.  The user is encouraged
/// to override the functions if they have something
/// more accurate.  Do NOT let this class be abstract.
class CReverseComplementHelper {
public:
    // currently no public functions but that may change in the future.
    // For example, if you need a more complex GetStrand(), then 
    // that might be added for the user to override.
private:
    // This is unused and it's just here so the class isn't empty 
    // in case the compiler doesn't like empty classes.
    int m_dummy;
};

/// Get reverse complement of the seq-loc (?).
/// This holds the implementation used by SeqLocRevCmpl in a way
/// that prevents dependency on objmgr.  objmgr dependencies, if they
/// arise in the future, can be taken care of via overridable methods
/// of CReverseComplementHelper.
NCBI_SEQ_EXPORT
CSeq_loc* GetReverseComplement(const CSeq_loc& loc, CReverseComplementHelper* helper);

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* SEQ_LOC_REVERSE_COMPLEMENTER__HPP */

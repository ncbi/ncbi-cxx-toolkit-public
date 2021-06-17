#ifndef GUI_OBJECTS___SEQUTILS__HPP
#define GUI_OBJECTS___SEQUTILS__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/bioseq_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


///
/// Return a (corrected) set of flags identifying the sequence type.
/// This function is intended to produce a set of corrected sequence types
/// indicating chromsome / not chromosome, genomic/mRNA/protein.
///
/// The returned flags are a subset of CSeq_id::EAccessionInfo types:
///
///  CSeq_id::fAcc_prot
///  CSeq_id::fAcc_nuc
///  CSeq_id::eAcc_mrna
///  CSeq_id::fAcc_genomic
///  CSeq_id::eAcc_unknown
///
///  @param bsh Bioseq-handle of the sequence to report on
///  @return Set of bitwise flags corresponding to sequence subtypes as noted
///  above
///
Uint4 GetSequenceType(const CBioseq_Handle& bsh);

END_SCOPE(objects)
END_NCBI_SCOPE


#endif  // GUI_OBJECTS___SEQUTILS__HPP

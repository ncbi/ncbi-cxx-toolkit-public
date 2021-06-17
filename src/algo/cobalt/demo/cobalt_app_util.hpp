#ifndef ALGO_COBALT___COBALT_APP_UTIL__HPP
#define ALGO_COBALT___COBALT_APP_UTIL__HPP

/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: cobalt_app_util.hpp

Author: Greg Boratyn

Contents: Utility functions for COBALT command line applications

******************************************************************************/

#include <objects/seqalign/Seq_align.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/readers/fasta.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

/// Reads fasta sequences from stream, adds them to scope,
/// and returns them as the list of Seq_locs
/// @param instream Input Stream [in|out]
/// @param seqs List of the read sequences [out]
/// @param scope Scope that contains the read sequences [in|out]
/// @param flags Fasta Reader flags [in]
void GetSeqLocFromStream(CNcbiIstream& instream,
                         vector< CRef<objects::CSeq_loc> >& seqs,
                         CRef<objects::CScope>& scope,
                         objects::CFastaReader::TFlags flags);


/// Reads fasta sequences as multiple sequence alignment
/// @param instream Input stream [in|out]
/// @param scope Scope that contains the read sequences [in|out]
/// @param flags Fasta Reader flags [in]
/// @param id_generator Generator for sequece ids [in|out]
/// @return Alignment in ASN.1 format
CRef<objects::CSeq_align> GetAlignmentFromStream(CNcbiIstream& instream,
                                        CRef<objects::CScope>& scope,
                                        objects::CFastaReader::TFlags flags,
                                        objects::CSeqIdGenerator& id_generator);


END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif /* ALGO_COBALT___COBALT_APP_UTIL_HPP */

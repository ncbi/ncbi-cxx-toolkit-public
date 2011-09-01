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
* Author:  Greg Boratyn
*
* File Description:
*   Utilities for Cobalt unit tests
*
*
* ===========================================================================
*/

#include <objmgr/scope.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objtools/readers/fasta.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <algo/cobalt/cobalt.hpp>

#include <vector>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/// Read fasta sequences from a file
/// @param filename File name [in]
/// @param seqs The read sequences [out]
/// @param scope The sequences read are added to this scope [in|out]
/// @param id_generator Sequence id generator, must be used when reading more
/// than one set of sequences, so that unique sequence ids are assigned [in|out]
/// @return 0 on success, -1 on error
int ReadFastaQueries(const string& filename, vector< CRef<CSeq_loc> >& seqs,
                      CRef<CScope>& scope,
                      CSeqIdGenerator* id_generator = NULL);

/// Read multiple sequence alignment from a file
/// @param filename File name [in]
/// @param align The alignment read [out]
/// @param scope The sequences in the alignment are added to this scope [in|out]
/// @param id_generator Sequence id generator, must be used when reading more
/// than one set of sequences, so that unique sequence ids are assigned [in|out]
/// @return 0 on success, -1 on error
int ReadMsa(const string& filename, CRef<CSeq_align>& align,
            CRef<CScope> scope, CSeqIdGenerator* id_generator = NULL);


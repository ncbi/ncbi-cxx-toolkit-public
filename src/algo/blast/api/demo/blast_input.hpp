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
 * Author:  Christiam Camacho
 *
 */

/** @file blast_input.hpp
 * Reading FASTA sequences from input file
 */

#ifndef ALGO_BLAST_API___BLAST_INPUT__HPP
#define ALGO_BLAST_API___BLAST_INPUT__HPP

#include <corelib/ncbistd.hpp>
#include <algo/blast/api/sseqloc.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CObjectManager;
END_SCOPE(objects)

BEGIN_SCOPE(blast)


/// Reads FASTA file and creates a TSeqLocVector type for the read sequence(s). 
/// Restricts sequences to an interval, if requested. 
/// @param in Input file stream [in]
/// @param objmgr Object manager reference [in]
/// @param strand What strand to use if it is a nucleotide sequence [in]
/// @param from Starting offset of an interval [in]
/// @param to Ending offset of an interval (end of sequence if 0) [in]
/// @param counter What index to start assigning local ids from? First unused 
///                index on exit. [in] [out]
/// @param get_lcase_mask Should lower case be masked? [in]
/// @return Vector of sequence location structures.
TSeqLocVector
BLASTGetSeqLocFromStream(CNcbiIstream& in, objects::CObjectManager& objmgr, 
                         objects::ENa_strand strand,
                         TSeqPos from, TSeqPos to, int* counter,
                         bool get_lcase_mask = false);


END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_API___BLAST_INPUT__HPP */

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
* File Description:
*   Utility function to convert internal BLAST result structures into
*   CSeq_align_set objects
*
*/

#ifndef BLASTSEQALIGN__HPP
#define BLASTSEQALIGN__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/seqalign__.hpp>

// Blast++ includes
#include <algo/blast/api/blast_option.hpp>

// NewBlast includes
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/gapinfo.h>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/** Converts BlastResults structure into CSeq_align_set class (handles 
 * query concatenation).
 * @param results results from running the BLAST algorithm [in]
 * @param prog type of BLAST program [in]
 * @param query_seqids list of sequence identifiers (to allow query
 * concatenation) [in]
 * @param bssp handle to BLAST Sequence Source ADT [in]
 * @param subject_seqid sequence identifier (for 2 sequences search) [in]
 * @param score_options contains scoring options [in]
 * @param sbp scoring and statistical information [in]
 * @return set of CSeq_align objects
 */
CRef<CSeq_align_set>
BLAST_Results2CppSeqAlign(const BlastResults* results, 
                          CBlastOption::EProgram prog,
                          vector< CConstRef<CSeq_id> >& query_seqids, 
                          const BlastSeqSrc* bssp, 
                          const CSeq_id* subject_seqid,
                          const BlastScoringOptions* score_options, 
                          const BlastScoreBlk* sbp);


END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.10  2003/08/12 19:18:45  dondosha
* Use TSeqLocVector type in functions
*
* Revision 1.9  2003/08/11 19:55:04  camacho
* Early commit to support query concatenation and the use of multiple scopes.
* Compiles, but still needs work.
*
* Revision 1.8  2003/08/11 15:18:50  dondosha
* Added algo/blast/core to all #included headers
*
* Revision 1.7  2003/08/11 13:58:51  dicuccio
* Added export specifiers.  Fixed problem with unimplemented private copy ctor
* (truly make unimplemented)
*
* Revision 1.6  2003/08/08 19:42:14  dicuccio
* Compilation fixes: #include file relocation; fixed use of 'list' and 'vector'
* as variable names
*
* Revision 1.5  2003/08/01 17:40:56  dondosha
* Use renamed functions and structures from local blastkar.h
*
* Revision 1.4  2003/07/31 19:45:33  camacho
* Eliminate Ptr notation
*
* Revision 1.3  2003/07/25 22:12:46  camacho
* Use BLAST Sequence Source to retrieve sequence identifier
*
* Revision 1.2  2003/07/23 21:28:23  camacho
* Use new local gapinfo structures
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* BLASTSEQALIGN__HPP */

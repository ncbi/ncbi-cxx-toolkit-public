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

// NewBlast includes
#include <algo/blast/api/blast_option.hpp>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/gapinfo.h>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_align_set;
END_SCOPE(objects)

BEGIN_SCOPE(blast)
USING_SCOPE(objects);

/** Converts BlastResults structure into CSeq_align_set class (handles 
 * query concatenation).
 * @param results results from running the BLAST algorithm [in]
 * @param prog type of BLAST program [in]
 * @param query All query sequences [in]
 * @param bssp handle to BLAST Sequence Source ADT [in]
 * @param subject Subject sequence (for 2 sequences search) [in]
 * @param score_options contains scoring options [in]
 * @param sbp scoring and statistical information [in]
 * @param is_gapped Was this a gapped search? [in]
 * @return set of CSeq_align objects
 */
CRef<CSeq_align_set>
BLAST_Results2CSeqAlign(const BlastResults* results, 
                          CBlastOption::EProgram prog,
                          TSeqLocVector &query, 
                          const BlastSeqSrc* bssp, 
                          const SSeqLoc* subject,
                          const BlastScoringOptions* score_options, 
                          const BlastScoreBlk* sbp, bool is_gapped);


END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.13  2003/08/18 20:58:56  camacho
* Added blast namespace, removed *__.hpp includes
*
* Revision 1.12  2003/08/18 17:07:41  camacho
* Introduce new SSeqLoc structure (replaces pair<CSeq_loc, CScope>).
* Change in function to read seqlocs from files.
*
* Revision 1.11  2003/08/15 15:54:55  dondosha
* Pass seqloc-scope pairs to results2seqalign conversion functions
*
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

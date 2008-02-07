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
* Authors:  Kamen Todorov
*
* File Description:
*   Alignment builders
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>

#include <objtools/alnmgr/aln_builders.hpp>
#include <objtools/alnmgr/aln_rng_coll_oper.hpp>
#include <objtools/alnmgr/aln_serial.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


void
MergePairwiseAlns(CPairwiseAln& existing,
                  const CPairwiseAln& addition,
                  const CAlnUserOptions::TMergeFlags& flags)
{
    CPairwiseAln difference(existing.GetFirstId(), existing.GetSecondId());
    SubtractAlnRngCollections(addition, // minuend
                              existing, // subtrahend
                              difference);
#ifdef _TRACE_MergeAlnRngColl
    cerr << endl;
    cerr << "existing:" << endl << existing << endl;
    cerr << "addition:" << endl << addition << endl;
    cerr << "difference = addition - existing:" << endl << difference << endl;
#endif
    ITERATE(CPairwiseAln, rng_it, difference) {
        existing.insert(*rng_it);
    }
#ifdef _TRACE_MergeAlnRngColl
    cerr << "result = existing + difference:" << endl << existing << endl;
#endif
}


END_NCBI_SCOPE

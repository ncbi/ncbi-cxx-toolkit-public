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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Serialization
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>
#include <objtools/alnmgr/serial.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


ostream& operator<<(ostream& out, CPairwiseAln::TAlnRng aln_rng)
{
    return out << "["
        << aln_rng.GetFirstFrom() << ", " 
        << aln_rng.GetSecondFrom() << ", "
        << aln_rng.GetLength() << ", " 
        << (aln_rng.IsDirect() ? "direct" : "reverse") 
        << "]";
}


ostream& operator<<(ostream& out, CPairwiseAln::EFlags flags)
{
    out << " Flags = " << NStr::UIntToString(flags, 0, 2)
        << ":" << endl;
    
    if (flags & CPairwiseAln::fKeepNormalized) out << "fKeepNormalized" << endl;
    if (flags & CPairwiseAln::fAllowMixedDir) out << "fAllowMixedDir" << endl;
    if (flags & CPairwiseAln::fAllowOverlap) out << "fAllowOverlap" << endl;
    if (flags & CPairwiseAln::fAllowAbutting) out << "fAllowAbutting" << endl;
    if (flags & CPairwiseAln::fNotValidated) out << "fNotValidated" << endl;
    if (flags & CPairwiseAln::fInvalid) out << "fInvalid" << endl;
    if (flags & CPairwiseAln::fUnsorted) out << "fUnsorted" << endl;
    if (flags & CPairwiseAln::fDirect) out << "fDirect" << endl;
    if (flags & CPairwiseAln::fReversed) out << "fReversed" << endl;
    if ((flags & CPairwiseAln::fMixedDir) == CPairwiseAln::fMixedDir) out << "fMixedDir" << endl;
    if (flags & CPairwiseAln::fOverlap) out << "fOverlap" << endl;
    if (flags & CPairwiseAln::fAbutting) out << "fAbutting" << endl;

    return out;
}


ostream& operator<<(ostream& out, TAlnSeqIdIRef& aln_seq_id_iref)
{
    out << aln_seq_id_iref->AsString()
        << " (base_width=" << aln_seq_id_iref->GetBaseWidth() 
        << ")";
    return out;
}        


ostream& operator<<(ostream& out, CPairwiseAln pairwise_aln)
{
    out << "CPairwiseAln" << endl;

    out << pairwise_aln.GetFirstId() << ", "
        << pairwise_aln.GetSecondId();
    
    cout << pairwise_aln.GetFlags();

    ITERATE (CPairwiseAln, aln_rng_it, pairwise_aln) {
        out << *aln_rng_it;
    }
    return out << endl;
}


ostream& operator<<(ostream& out, CAnchoredAln anchored_aln)
{
    out << "CAnchorAln has score of " << anchored_aln.GetScore() << " and contains " 
        << anchored_aln.GetDim() << " pair(s) of rows:" << endl;
    ITERATE(CAnchoredAln::TPairwiseAlnVector, pairwise_aln_i, anchored_aln.GetPairwiseAlns()) {
        out << *pairwise_aln_i;
    }
    return out << endl;
}


END_NCBI_SCOPE



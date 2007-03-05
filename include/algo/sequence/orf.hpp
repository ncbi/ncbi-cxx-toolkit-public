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
 * Authors:  Josh Cherry
 *
 * File Description:  code for finding and representing ORFs
 *
 */


#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <vector>


BEGIN_NCBI_SCOPE


//
// class COrf implements a simple open reading frame search.
//
// This class reports, as a series of seq-locs, all the possible open
// reading frames in a given sequence.  The sequence can be represented
// as either 
/// This class provides functions for finding all the ORFs
/// of a specified minimum length in a DNA sequence.

class NCBI_XALGOSEQ_EXPORT COrf
{
public:
    typedef CRange<TSeqPos> TRange;
    typedef vector<TRange>  TRangeVec;
    typedef vector< CRef<objects::CSeq_loc> > TLocVec;


    /// Find all ORFs in both orientations that
    /// are at least min_length_bp long.
    /// Report results as Seq-locs.
    /// seq must be in iupac.
    /// If allowable_starts is empty (the default), any sense codon can begin
    /// an ORF.  Otherwise, only codons in allowable_starts can do so.
    static void FindOrfs(const string& seq,
                         TLocVec& results,
                         unsigned int min_length_bp = 3,
                         int genetic_code = 1,
                         const vector<string>& allowable_starts = vector<string>());

    static void FindOrfs(const vector<char>& seq,
                         TLocVec& results,
                         unsigned int min_length_bp = 3,
                         int genetic_code = 1,
                         const vector<string>& allowable_starts = vector<string>());

    static void FindOrfs(const objects::CSeqVector& seq,
                         TLocVec& results,
                         unsigned int min_length_bp = 3,
                         int genetic_code = 1,
                         const vector<string>& allowable_starts = vector<string>());

    /**
    /// This version returns an annot full of CDS features.
    /// Optionally takes a CSeq_id (by CRef) for use in
    /// the feature table; otherwise ids are left unset.
    template<class Seq>
    static CRef<objects::CSeq_annot>
    FindOrfs(const Seq& seq,
             unsigned int min_length_bp = 3,
             int genetic_code = 1,
             CRef<objects::CSeq_id> id = CRef<objects::CSeq_id>())
    {
        // place to store orfs
        TLocVec orfs;
        FindOrfs(seq, orfs, min_length_bp, genetic_code);
        return MakeCDSAnnot(orfs, genetic_code, id);
    }
    **/

    /// Build an annot full of CDS features from CSeq_loc's.
    /// Optionally takes a CSeq_id (by CRef) for use in
    /// the feature table; otherwise ids are left unset.
    static CRef<objects::CSeq_annot>
    MakeCDSAnnot(const TLocVec& orfs, int genetic_code = 1,
                 objects::CSeq_id* id = NULL);
};




END_NCBI_SCOPE

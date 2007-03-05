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
 * File Description:  Predict antigenic determinants as described by
 *                    Kolaskar and Tongaonkar, 1990
 *
 */


#ifndef ALGO_SEQUENCE___ANTIGENIC__HPP
#define ALGO_SEQUENCE___ANTIGENIC__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/seq_vector.hpp>
#include <vector>

BEGIN_NCBI_SCOPE

class NCBI_XALGOSEQ_EXPORT CAntigenic
{
public:
    typedef vector< CRef<objects::CSeq_loc> > TLocVec;

    /// Predict antigenic determinants from protein sequence
    /// according to Kolaskar and Tongaonkar, 1990 (PMID 1702393).
    /// 
    /// For std char containers, seq must be in ncbistdaa.
    static void PredictSites(const string& seq,
                             TLocVec& results,
                             unsigned int min_len = 6);

    static void PredictSites(const vector<char>& seq,
                             TLocVec& results,
                             unsigned int min_len = 6);
    
    static void PredictSites(const objects::CSeqVector& seq,
                             TLocVec& results,
                             unsigned int min_len = 6);

private:
    static const double sm_Pa_table[26];

    template<class Seq>
    friend void x_PredictAGSites(const Seq& seq, TLocVec& results,
                                 int min_len);
};


END_NCBI_SCOPE

#endif  // ALGO_SEQUENCE___ANTIGENIC__HPP

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
 * File Description:  Prediction of signal sequences from protein sequence
 *                    according to von Heijne, 1986 and 1987
 *
 */


#ifndef ALGO_SEQUENCE___SIGNAL_SEQ__HPP
#define ALGO_SEQUENCE___SIGNAL_SEQ__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/seq_vector.hpp>
#include <vector>

BEGIN_NCBI_SCOPE

/// Protein signal sequence prediction according to
/// von Heijne, 1986 (PMID 3714490, as modified in his book).

class NCBI_XALGOSEQ_EXPORT CSignalSeq
{
public:
    enum EDomain {
        eEukaryotic,
        eBacterial
    };
    /// Find the most likely predicted signal sequence cleavage site (pos)
    /// and the associated score (>3.5 suggests a real signal sequence).
    /// Constrain pos to be no larger than max_pos.  Positions
    /// refer to residue just before cleaved bond.
    /// Length of seq must be at least 15, and max_pos must be
    /// at least 12.
    /// For std char containers, seq must be in ncbistdaa.
    static void Predict(const string& seq, EDomain domain,
                        TSeqPos max_pos, TSeqPos& pos, double& score);

    static void Predict(const vector<char>& seq, EDomain domain,
                        TSeqPos max_pos, TSeqPos& pos, double& score);

    static void Predict(const objects::CSeqVector& seq, EDomain domain,
                        TSeqPos max_pos, TSeqPos& pos, double& score);

private:
    template<class Seq> friend
    void x_PredictSignalSeq(const Seq& seq, CSignalSeq::EDomain domain,
                            TSeqPos max_pos, TSeqPos& pos, double& score);
};


END_NCBI_SCOPE

#endif  // ALGO_SEQUENCE___SIGNAL_SEQ__HPP

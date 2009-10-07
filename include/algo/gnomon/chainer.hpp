#ifndef ALGO_GNOMON___CHAINER__HPP
#define ALGO_GNOMON___CHAINER__HPP

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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <algo/gnomon/gnomon_model.hpp>

BEGIN_SCOPE(ncbi);
BEGIN_SCOPE(gnomon);


struct SMinScor {
    double m_min;
    double m_i5p_penalty;
    double m_i3p_penalty;
    double m_cds_bonus;
    double m_length_penalty;
    double m_minprotfrac;
    double m_endprotfrac;
    int m_prot_cds_len;
};


class NCBI_XALGOGNOMON_EXPORT CChainer {
public:
    CChainer();
    ~CChainer();

    TGeneModelList MakeChains(TAlignModelList& alignments,
                              CGnomonEngine& gnomon, const SMinScor& minscor,
                              int intersect_limit, int trim, size_t alignlimit,
                              const map<string,TSignedSeqRange>& mrnaCDS, 
                              map<string, pair<bool,bool> >& prot_complet,
                              double mininframefrac);
private:
    // Prohibit copy constructor and assignment operator
    CChainer(const CChainer& value);
    CChainer& operator= (const CChainer& value);

    class CChainerImpl;
    auto_ptr<CChainerImpl> m_data;
};

END_SCOPE(gnomon)
END_SCOPE(ncbi)


#endif  // ALGO_GNOMON___CHAINER__HPP

/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: dist.hpp

Author: Jason Papadopoulos

Contents: Interface for CDistances class

******************************************************************************/

/// @file dist.hpp
/// Interface for CDistances class

#ifndef _ALGO_COBALT_DIST_HPP_
#define _ALGO_COBALT_DIST_HPP_

#include <algo/cobalt/base.hpp>
#include <algo/cobalt/seq.hpp>
#include <algo/cobalt/hitlist.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

class CDistances {

public:
    CDistances() {}

    CDistances(vector<CSequence>& query_data,
               CHitList& hitlist, 
               SNCBIFullScoreMatrix& score_matrix,
               Blast_KarlinBlk *kbp)
    {
        ComputeMatrix(query_data, hitlist, score_matrix, kbp);
    }

    ~CDistances() {}

    void ComputeMatrix(vector<CSequence>& query_data,
                       CHitList& hitlist, 
                       SNCBIFullScoreMatrix& score_matrix,
                       Blast_KarlinBlk *kbp);

    const CDistMethods::TMatrix& GetMatrix() const { return m_Matrix; }

private:
    CDistMethods::TMatrix m_Matrix;

    void x_GetSelfScores(vector<CSequence>& query_data,
                         CHitList& hitlist,
                         SNCBIFullScoreMatrix& score_matrix,
                         Blast_KarlinBlk *kbp,
                         vector<double>& self_score);
};

END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif // _ALGO_COBALT_DIST_HPP_

/*--------------------------------------------------------------------
  $Log$
  Revision 1.1  2005/11/07 18:15:52  papadopo
  Initial revision

  ------------------------------------------------------------------*/

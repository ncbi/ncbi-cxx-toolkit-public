#ifndef ALGO_COBALT___DIST__HPP
#define ALGO_COBALT___DIST__HPP

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

#include <util/math/matrix.hpp>
#include <algo/phy_tree/dist_methods.hpp>
#include <algo/blast/core/blast_stat.h>

#include <algo/cobalt/base.hpp>
#include <algo/cobalt/seq.hpp>
#include <algo/cobalt/hitlist.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

/// Representation of pairwise distances, intended for use
/// in multiple sequence alignment applications
class NCBI_COBALT_EXPORT CDistances
{

public:
    /// Create empty distance matrix
    ///
    CDistances() {}

    /// Create a pairwise distance matrix
    /// @param query_data List of sequences for which distances
    ///                   will be computed [in]
    /// @param hitlist Collection of local alignments between
    ///                paris of the sequences in query_data [in]
    /// @param score_matrix log-odds score matrix to use in the
    ///                     distance calculations [in]
    /// @param karlin_blk Karlin-Altschul parameters to use in the 
    ///            distance calculations [in]
    ///
    CDistances(vector<CSequence>& query_data,
               CHitList& hitlist, 
               SNCBIFullScoreMatrix& score_matrix,
               Blast_KarlinBlk& karlin_blk)
    {
        ComputeMatrix(query_data, hitlist, 
                      score_matrix, karlin_blk);
    }

    /// Destructor
    ///
    ~CDistances() {}

    /// Recompute the distance matrix using new parameters
    /// @param query_data List of sequences for which distances
    ///                   will be computed [in]
    /// @param hitlist Collection of local alignments between
    ///                paris of the sequences in query_data [in]
    /// @param score_matrix log-odds score matrix to use in the
    ///                     distance calculations [in]
    /// @param karlin_blk Karlin-Altschul parameters to use in the 
    ///            distance calculations [in]
    ///
    void ComputeMatrix(vector<CSequence>& query_data,
                       CHitList& hitlist, 
                       SNCBIFullScoreMatrix& score_matrix,
                       Blast_KarlinBlk& karlin_blk);

    /// Access the current distance matrix
    /// @return The distance matrix
    const CDistMethods::TMatrix& GetMatrix() const { return m_Matrix; }

private:
    CDistMethods::TMatrix m_Matrix;     ///< Current distance matrix

    /// Compute the self-scores of the input sequences
    /// @param query_data List of sequences for which distances
    ///                   will be computed [in]
    /// @param hitlist Collection of local alignments between
    ///                paris of the sequences in query_data [in]
    /// @param score_matrix log-odds score matrix to use in the
    ///                     distance calculations [in]
    /// @param self_score The computed self scores [out]
    /// @param karlin_blk Karlin-Altschul parameters to use in the 
    ///            distance calculations [in]
    ///
    void x_GetSelfScores(vector<CSequence>& query_data,
                         CHitList& hitlist,
                         SNCBIFullScoreMatrix& score_matrix,
                         vector<double>& self_score,
                         Blast_KarlinBlk& karlin_blk);
};

END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif // ALGO_COBALT___DIST__HPP

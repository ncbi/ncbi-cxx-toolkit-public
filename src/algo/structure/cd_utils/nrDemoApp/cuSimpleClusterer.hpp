#ifndef CU_SIMPLE_CLUSTERER__HPP
#define CU_SIMPLE_CLUSTERER__HPP

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
* Author:  Chris Lanczycki
*
* File Description:     $Source$
*
*
*/

#include <corelib/ncbistd.hpp>
//#include <objects/seqloc/PDB_seq_id.hpp>
//#include <objects/seqloc/PDB_mol_id.hpp>

#include <iostream>
#include <fstream>

#include <algo/structure/cd_utils/cuBaseClusterer.hpp>
#include "cuSimpleSlc.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)


//////////////////////////////
//   CSimpleClusterer class
//////////////////////////////


class CSimpleClusterer : public CDistBasedClusterer {

public:

    //  assumes will pass array in MakeClusters call
    CSimpleClusterer(TDist clusteringThreshold = 0);

    //  does not copy to internal storage or assume ownership
    CSimpleClusterer(TSlc_DM distances, unsigned int dim, TDist clusteringThreshold = 0);

    //  copies to internal storage; assumes ownership of copy
    CSimpleClusterer(const TDist** distances, unsigned int dim, TDist clusteringThreshold = 0);

    virtual ~CSimpleClusterer();

    //  assumes distance passed in c-tor
    virtual unsigned int MakeClusters();

    //  ignores distances, if any, that were passed in c-tor
    virtual unsigned int MakeClusters(const TDist** distances, unsigned int dim);

    virtual bool IsValid() const;

//    void SetRootednessForTree(TreeAlgorithm::Rootedness rootedness);
//    TreeAlgorithm::Rootedness GetRootednessForTree() const;


private:

    void InitializeTree();
    void InitializeClusterer(unsigned int dim);
    void CopyDistances(const TDist** distances);

    //  Distance matrix as linearized array from upper-triangular symmetric matrix, including
    //  the diagonal.  Stored as (0,0), (0, 1), ..., (0, dim-1), (1, 1), ..., (dim-1, dim-1)
    TSlc_DM  m_distances;
    unsigned int m_dim;
    bool   m_ownDistances;

    CUTree*     m_cuTree;
//    TreeAlgorithm::Rootedness m_rootedness;

};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif  /* CU_SIMPLE_CLUSTERER__HPP */

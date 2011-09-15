/* $Id$
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
 * Author:  Charlie Liu
 *
 * File Description:
 *   Define the interface to lay out a phylogenetic tree view.
 */

#ifndef CU_SEQTREE_LAYOUT_HPP
#define CU_SEQTREE_LAYOUT_HPP

#include <algo/structure/cd_utils/cuSeqtree.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class NCBI_CDUTILS_EXPORT SeqTreeLayout
{
public:
    virtual ~SeqTreeLayout() {};
	virtual void calculateNodePositions(SeqTree& treeData, int maxX, int maxY)=0;
	virtual SeqTree::iterator findEdgeEnd(SeqTree& treeData, int x, int y, int edgeWidth)=0;
	virtual void getSize(int& x, int& y)=0;
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE
#endif

#ifndef ALGO_PHY_TREE___PHY_TREE_SERIAL__HPP
#define ALGO_PHY_TREE___PHY_TREE_SERIAL__HPP

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
 * File Description:  A manually defined serial object for
 *                    representing a phylogenetic tree
 *
 */


#include <algo/phy_tree/phy_node.hpp>
#include <serial/serialimpl.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE

class NCBI_XALGOPHYTREE_EXPORT CPhyTreeSerial : public CSerialObject
{
public:
    /// Seq-ids of terminal nodes
    typedef vector< CConstRef<objects::CSeq_id> > TIds;
    
    CPhyTreeSerial(void) {}

    CPhyTreeSerial(const TPhyTreeNode& val)
        : m_Tree(val) {}

    const TPhyTreeNode& GetTree(void) const {return m_Tree;}

    TPhyTreeNode& SetTree(void) {return m_Tree;}

    void SetTree(const TPhyTreeNode& val) {m_Tree = val;}

    /// Getters and setters for optional Seq-ids 
    /// associated with terminal nodes.
    /// For this to make sense, the integer ids of the terminal
    /// nodes must be indexes into this.
    const TIds& GetIds(void) const {return m_Ids;}
    TIds& SetIds(void) {return m_Ids;}
    void SetIds(const TIds& val) {m_Ids = val;}

    DECLARE_INTERNAL_TYPE_INFO();

private:
    TPhyTreeNode m_Tree;
    TIds m_Ids;
};

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/04/16 16:38:30  dicuccio
 * Changed TIds typedef - store CConstRef<>, not CRef<>
 *
 * Revision 1.3  2004/03/17 16:52:47  jcherry
 * Added optional Seq-ids
 *
 * Revision 1.2  2004/02/10 17:02:29  dicuccio
 * Formatting changes.  Added export specifiers
 *
 * Revision 1.1  2004/02/10 15:15:57  jcherry
 * Initial version
 *
 * ===========================================================================
 */


#endif  // ALGO_PHY_TREE___PHY_TREE_SERIAL__HPP

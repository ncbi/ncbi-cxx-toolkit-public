#ifndef PHYTREE_SIMPLIFY__HPP
#define PHYTREE_SIMPLIFY__HPP
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
 * Author:  Greg Boratyn
 *
 */

/// This file provides tree visitor classes and functions for node groupping 
///  and  simplification of phylogenetic trees

#include <stack>
#include <corelib/ncbi_tree.hpp>
#include <algo/phy_tree/bio_tree.hpp>

BEGIN_NCBI_SCOPE


/// Tree visitor, finds subtrees that form groups based on tree features
class CPhyTreeNodeGroupper
{
public:
    class CLabeledNode
    {
    public:
        CLabeledNode(CBioTreeDynamic::CBioNode* node,
                     const pair<string, string>& label_color)
            : m_Node(node), m_LabelColorPair(label_color) {}

        CBioTreeDynamic::CBioNode* GetNode(void) const {return m_Node;}
        const string& GetLabel(void) const {return m_LabelColorPair.first;}
        const string& GetColor(void) const {return m_LabelColorPair.second;}

    private:
        CBioTreeDynamic::CBioNode* m_Node;
        pair<string, string> m_LabelColorPair;
    };

public:
    typedef vector<CLabeledNode> CLabeledNodes;
    typedef CLabeledNodes::iterator CLabeledNodes_I;


public:
    CPhyTreeNodeGroupper(const string& feature_name,
                         const string& feature_color,
                         CBioTreeDynamic& tree,
                         CNcbiOfstream* ostr = NULL);

    virtual ~CPhyTreeNodeGroupper() {}
    void Init(const string& feature_name, const string& feature_color,
              CBioTreeDynamic& tree);
    const string& GetError(void) const {return m_Error;}
    virtual CLabeledNodes& GetLabeledNodes(void) {return m_LabeledNodes;}
    const string& GetFeatureName(void) const {return m_LabelFeatureName;}
    bool IsEmpty(void) const {return m_LabeledNodes.empty();}
    CLabeledNodes_I Begin(void) {return m_LabeledNodes.begin();}
    CLabeledNodes_I End() {return m_LabeledNodes.end();}
    int GetLabeledNodesNum(void) {return m_LabeledNodes.size();}
    ETreeTraverseCode operator()(CBioTreeDynamic::CBioNode& node, int delta);

protected:
    ETreeTraverseCode x_OnStepDown(CBioTreeDynamic::CBioNode& x_node);
    ETreeTraverseCode x_OnStepLeft(CBioTreeDynamic::CBioNode& x_node);
    ETreeTraverseCode x_OnStepRight(CBioTreeDynamic::CBioNode& x_node);
    bool x_IsRoot(CBioTreeDynamic::CBioNode* node) const
    {return node == m_Root;}


protected:
    string m_LabelFeatureName;
    string m_ColorFeatureName;
    string m_Error;
    CLabeledNodes m_LabeledNodes;
    stack< pair<string, string> > m_LabelStack;
    stack<TBioTreeNodeId> m_ParentIdStack;
    CBioTreeDynamic::CBioNode* m_Root;

    CNcbiOfstream* m_Ostr;  //diagnostics
};


///Tree visitor, finds all labels and node colors for leafes
class CPhyTreeLabelTracker
{
public:
    typedef map<string, string> TLabelColorMap;
    typedef TLabelColorMap::iterator TLabelColorMap_I;

public:
    CPhyTreeLabelTracker(const string& label, const string& color,
                         CBioTreeDynamic& tree);

    ETreeTraverseCode operator() (CBioTreeDynamic::CBioNode& node, int delta);
    const string& GetError(void) const {return m_Error;}
    TLabelColorMap_I Begin(void) {return m_LabelsColors.begin();}
    TLabelColorMap_I End(void) {return m_LabelsColors.end();}
    unsigned int GetNumLabels(void) const {return m_LabelsColors.size();}
    bool FoundQueryNode(void) const {return m_FoundQueryNode;}
    bool FoundSeqFromType(void) const { return m_FoundSeqFromType;}

protected:
    bool x_IsQuery(const CBioTreeDynamic::CBioNode& node) const;
    bool x_IsSeqFromType(const CBioTreeDynamic::CBioNode& node) const;

protected:
    string m_LabelFeatureTag;
    string m_ColorFeatureTag;
    TLabelColorMap m_LabelsColors;
    string m_Error;
    bool m_FoundQueryNode;
    bool m_FoundSeqFromType;
};


END_NCBI_SCOPE

#endif // PHYTREE_SIMPLIFY__HPP

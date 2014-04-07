/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               lavNational Center for Biotechnology Information
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
 * File Description: Tree visitor classes for phylogenetic node groupping
 *  and tree simplification
 */

#include <ncbi_pch.hpp>
#include <algo/phy_tree/phytree_format/phytree_format.hpp>
#include <algo/phy_tree/phytree_format/phytree_simplify.hpp>

BEGIN_NCBI_SCOPE

static const string s_kDifferentGroups = "$DIFFERENT_GROUPS";


CPhyTreeNodeGroupper::CPhyTreeNodeGroupper(const string& feature_name,
                                           const string& feature_color,
                                           CBioTreeDynamic& tree,
                                           CNcbiOfstream* ostr)
    : m_Root(NULL), m_Ostr(ostr)
{
    Init(feature_name, feature_color, tree);
}

void CPhyTreeNodeGroupper::Init(const string& feature_name,
                                const string& feature_color,
                                CBioTreeDynamic& tree)
{
    m_LabelFeatureName = feature_name;
    m_ColorFeatureName = feature_color;

    const CBioTreeFeatureDictionary& fdict = tree.GetFeatureDict();
    if (!fdict.HasFeature(m_LabelFeatureName)
        || !fdict.HasFeature(m_ColorFeatureName)) {

        m_Error = "Feature not in feature dictionary";
    }
    m_LabeledNodes.clear();

    while (!m_LabelStack.empty()) {
        m_LabelStack.pop();
    }

    while (!m_ParentIdStack.empty()) {
        m_ParentIdStack.pop();
    }
}

ETreeTraverseCode CPhyTreeNodeGroupper::operator()(
                                            CBioTreeDynamic::CBioNode& x_node,
                                            int delta)
{
    if (m_Ostr != NULL) {
      *m_Ostr << "stack top: ";
      if (m_LabelStack.empty())
        *m_Ostr << "empty";
      else
        *m_Ostr << m_LabelStack.top().first;
      *m_Ostr << ", num elements on label stack: " << m_LabelStack.size();
      *m_Ostr << ", num elements on parent stack: " << m_ParentIdStack.size();
      *m_Ostr << NcbiEndl;
    }
    
    if (!m_Error.empty()) {
        return eTreeTraverseStop;
    }

    if (m_Root == NULL)
        m_Root = &x_node;

    switch (delta) {
    case 0:  return x_OnStepDown(x_node);
    case 1:  return x_OnStepRight(x_node);
    case -1: return x_OnStepLeft(x_node);
    }

    return eTreeTraverse;
}

ETreeTraverseCode CPhyTreeNodeGroupper::x_OnStepRight(
                                           CBioTreeDynamic::CBioNode& x_node)
{
    //If leaf, then group name is put on stack
    if (m_Ostr != NULL)
        *m_Ostr << "x_OnStepRight, Id: " + NStr::IntToString(x_node.GetValue().GetId()) << NcbiEndl;

    //If this is a leaf, then the group name is saved for comparisons with
    // other nodes
    if (x_node.IsLeaf()) {
        pair<string, string> label_color
            = make_pair(x_node.GetFeature(m_LabelFeatureName),
                        x_node.GetFeature(m_ColorFeatureName));
        
        if (label_color.first.empty() || label_color.second.empty()) {
          m_Error = "Leafe node has unset feature, Id: " 
               + NStr::IntToString(x_node.GetValue().GetId());
          return eTreeTraverseStop;
        }

        m_LabelStack.push(label_color);

        if (m_Ostr != NULL)
            *m_Ostr << "Leaf, m_CurrentGroupName put on stack: " 
                    << m_LabelStack.top().first << NcbiEndl;

    }
    else {
        m_LabelStack.push(make_pair(NcbiEmptyString, NcbiEmptyString));
    }

    return eTreeTraverse;
}

ETreeTraverseCode CPhyTreeNodeGroupper::x_OnStepDown(
                                           CBioTreeDynamic::CBioNode& x_node)
{
    if (m_Ostr != NULL)
        *m_Ostr << "x_OnStepDown, Id: " 
          + NStr::IntToString(x_node.GetValue().GetId()) << NcbiEndl;


    if (x_node.IsLeaf()) {
        //Checking if different groups were alredy found 
        if (m_LabelStack.top().first == s_kDifferentGroups) {

              if (m_Ostr != NULL)
                *m_Ostr << "Leaf, m_CurrentGroupName == DIFFERENT_GROUPS" 
                        << NcbiEndl;

              return eTreeTraverse;
        }

        const string& name = x_node.GetFeature(m_LabelFeatureName);

        if (name == NcbiEmptyString) {
            m_Error = "Leafe node has unset feature, Id: " 
               + NStr::IntToString(x_node.GetValue().GetId());
            return eTreeTraverseStop;
        }

        if (m_Ostr != NULL)
            *m_Ostr << "Leaf with group name: " << name << NcbiEndl;

        //If the node's group name is different 
        // this is denoted by changing stack top
        if (name != m_LabelStack.top().first) {
            m_LabelStack.top().first = s_kDifferentGroups;

            if (m_Ostr != NULL)
                *m_Ostr << "  group name different, changing stack top to DIFFERENT_GROUPS"
                        << NcbiEndl;



        }
        else {
              if (m_Ostr != NULL)
              *m_Ostr << "  group name the same, stack top not changed"
                        << NcbiEndl;
        }

    }

    return eTreeTraverse;
}

ETreeTraverseCode CPhyTreeNodeGroupper::x_OnStepLeft(
                                            CBioTreeDynamic::CBioNode& x_node)
{
    if (m_Ostr != NULL)
       *m_Ostr << "x_OnStepLeft, Id: " + NStr::IntToString(x_node.GetValue().GetId()) << NcbiEndl;

    //If stack top holds information about subtree rooted in x_node
    // empty string means that this is the first subtree examined on this level
    const pair<string, string> subtree_label_color = m_LabelStack.top();

    //If subtree name is different from DIFFERENT_GROUPS
    // then subtree has common group
    if (subtree_label_color.first != s_kDifferentGroups) {

        //Checking whether subtree nodes were already marked and unmarking them
        // so that only the common parent node is marked for each group
        while (!m_ParentIdStack.empty() 
               && m_ParentIdStack.top() == (*x_node).GetId()) {
            m_LabeledNodes.pop_back();
            m_ParentIdStack.pop();
        }            

        //Marking common group subtree
        m_LabeledNodes.push_back(CLabeledNode(&x_node, m_LabelStack.top()));

        if (m_Ostr != NULL)
          *m_Ostr << "Subtree with common group name: " << m_LabelStack.top().first
                  << NcbiEndl;

        //Saving parent id so that this node can be unmarked if parent belongs
        // to the same group
        if (!x_IsRoot(&x_node)) {
            m_ParentIdStack.push((**x_node.GetParent()).GetId());
        }

    }
    else {
        //Cleaning parent id stack if this subtree does not have cmomon label
        while (!m_ParentIdStack.empty() && m_ParentIdStack.top() == (*x_node).GetId()) {
                m_ParentIdStack.pop();
        }
    }

    m_LabelStack.pop();
    if (m_Ostr != NULL)
      *m_Ostr << "  poping from stack" << NcbiEndl;

    //Comparing subtree group name to sibling nodes group name
    // and correcting stack top.
    if (!x_IsRoot(&x_node)) {
        if (m_LabelStack.top().first == NcbiEmptyString)
            m_LabelStack.top() = subtree_label_color;
        else
            if (m_LabelStack.top().first != subtree_label_color.first)
                m_LabelStack.top().first = s_kDifferentGroups;
    }

    return eTreeTraverse;
}


CPhyTreeLabelTracker::CPhyTreeLabelTracker(const string& label_feature, 
                                           const string& color_feature,
                                           CBioTreeDynamic& tree)
    : m_LabelFeatureTag(label_feature),
      m_ColorFeatureTag(color_feature),
      m_FoundQueryNode(false),
      m_FoundSeqFromType(false)
{
    const CBioTreeFeatureDictionary& fdict = tree.GetFeatureDict();
    if (!fdict.HasFeature(label_feature) || !fdict.HasFeature(color_feature)) {
        m_Error = "Feature not in feature dictionary";
    }

    m_LabelsColors.clear();
}

ETreeTraverseCode CPhyTreeLabelTracker::operator() (
                                             CBioTreeDynamic::CBioNode& node, 
                                             int delta)
{
    if (!m_Error.empty()) {
        return eTreeTraverseStop;
    }

    if (delta == 0 || delta == 1) {

        // if query node
        if (!m_FoundQueryNode && x_IsQuery(node)) {
            m_FoundQueryNode = true;
        }

        if (!m_FoundSeqFromType && x_IsSeqFromType(node)) {
            m_FoundSeqFromType = true;
        }

        if (node.IsLeaf()) {
            const string& label = node.GetFeature(m_LabelFeatureTag);
            const string& color = node.GetFeature(m_ColorFeatureTag);
            
            m_LabelsColors[label] = color;
        }
    }

    return eTreeTraverse;
}


bool CPhyTreeLabelTracker::x_IsQuery(const CBioTreeDynamic::CBioNode& node) const
{
    return node.GetFeature(CPhyTreeFormatter::GetFeatureTag(
                                            CPhyTreeFormatter::eNodeInfoId))
        == CPhyTreeFormatter::kNodeInfoQuery;
}

bool CPhyTreeLabelTracker::x_IsSeqFromType(
                                  const CBioTreeDynamic::CBioNode& node) const
{
    return node.GetFeature(CPhyTreeFormatter::GetFeatureTag(
                                            CPhyTreeFormatter::eNodeInfoId))
        == CPhyTreeFormatter::kNodeInfoSeqFromType;
}


END_NCBI_SCOPE

#ifndef BLAST_GUIDETREE___GUIDETREESIMPLIFY__HPP
#define BLAST_GUIDETREE___GUIDETREESIMPLIFY__HPP
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

/// @guide_tree_simplify.hpp
/// Visual simplification of phylogenetic tree.
///
/// This file provides tree visitor classes and functions for node groupping 
///  and  simplification of phylogenetic trees

#include <stack>
#include <corelib/ncbi_tree.hpp>
#include <gui/widgets/phylo_tree/phylo_tree_algorithm.hpp>


BEGIN_NCBI_SCOPE


///Tree visitor, finds subtrees that form groups based on tree features
class CPhyloTreeNodeGroupper : public IPhyloTreeVisitor
{
public:
    class CLabeledNode
    {
    public:
        CLabeledNode(TTreeType* node, const pair<string, string>& label_color)
                   : m_Node(node), m_LabelColorPair(label_color) {}
        TTreeType* GetNode(void) const {return m_Node;}
        const string& GetLabel(void) const {return m_LabelColorPair.first;}
        const string& GetColor(void) const {return m_LabelColorPair.second;}

    private:
        TTreeType* m_Node;
        pair<string, string> m_LabelColorPair;
    };

public:
    typedef vector<CLabeledNode> CLabeledNodes;
    typedef CLabeledNodes::iterator CLabeledNodes_I;


public:
    CPhyloTreeNodeGroupper(const string& feature_name, const string& feature_color, CNcbiOfstream* ostr = NULL);
    virtual ~CPhyloTreeNodeGroupper() {}
    void Init(const string& feature_name, const string& feature_color);
    const string& GetError(void) const {return m_Error;}
    virtual CLabeledNodes& GetLabeledNodes(void) {return m_LabeledNodes;}
    const string& GetFeatureName(void) const {return m_LabelFeatureName;}
    bool IsEmpty(void) const {return m_LabeledNodes.empty();}
    CLabeledNodes_I Begin(void) {return m_LabeledNodes.begin();}
    CLabeledNodes_I End() {return m_LabeledNodes.end();}
    int GetLabeledNodesNum(void) {return m_LabeledNodes.size();}

protected:
    virtual ETreeTraverseCode x_OnStep(TTreeType& x_node, int delta);
    virtual ETreeTraverseCode x_OnStepDown(TTreeType& x_node);
    virtual ETreeTraverseCode x_OnStepLeft(TTreeType& x_node);
    virtual ETreeTraverseCode x_OnStepRight(TTreeType& x_node);
    bool x_IsRoot(TTreeType* node) const {return node == m_Root;}


protected:
    string m_LabelFeatureName;
    string m_ColorFeatureName;
    TBioTreeFeatureId m_LabelFeatureId;
    TBioTreeFeatureId m_ColorFeatureId;
    string m_Error;
    CLabeledNodes m_LabeledNodes;
    stack< pair<string, string> > m_LabelStack;
    stack<IPhyNode::TID> m_ParentIdStack;
    TTreeType* m_Root;

    CNcbiOfstream* m_Ostr;  //diagnostics
};


///Tree visitor, finds all labels and node colors for leafes
class CPhyloTreeLabelTracker
{
public:
    typedef map<string, string> TLabelColorMap;
    typedef TLabelColorMap::iterator TLabelColorMap_I;

public:
    CPhyloTreeLabelTracker(const string& label, const string& color);
    ETreeTraverseCode operator() (CPhyloTreeNode& node, int delta);
    const string& GetError(void) const {return m_Error;}
    TLabelColorMap_I Begin(void) {return m_LabelsColors.begin();}
    TLabelColorMap_I End(void) {return m_LabelsColors.end();}
    unsigned int GetNumLabels(void) const {return m_LabelsColors.size();}
    CPhyloTreeNode* GetQueryNode(void) {return m_QueryNode;}
    const string& GetQueryNodeColor(void) const {return m_QueryNodeColor;}

protected:
    TBioTreeFeatureId m_LabelFeatureId;
    TBioTreeFeatureId m_ColorFeatureId;
    TLabelColorMap m_LabelsColors;
    string m_Error;
    CPhyloTreeNode* m_QueryNode;
    string m_QueryNodeColor;
};

/* Moved to guide_tree.hpp

///Tree visitor class, expands all nodes and corrects node colors
class CPhyloTreeExpander
{
public:
  ETreeTraverseCode operator()(CPhyloTreeNode& node, int delta) 
  {
      if (delta == 0 || delta == 1) {
          if (!node.Expanded() && !node.IsLeaf()) {
              node.ExpandCollapse(IPhyGraphicsNode::eShowChilds);
              (*node).SetFeature(CGuideTree::kNodeColorTag, "");
          }
      }
      return eTreeTraverse;
  }
};
*/

///Tree visitor, finds node in any subtree by id
//This can be optimized assuming node numbering scheme
class CPhyloTreeNodeFinder
{
public:
  CPhyloTreeNodeFinder(IPhyNode::TID node_id) : m_NodeId(node_id), m_Node(NULL) {}
  CPhyloTreeNode* GetNode(void) const {return m_Node;}
  ETreeTraverseCode operator()(CPhyloTreeNode& node, int delta)
  {
      if (delta == 0 || delta == 1) {
          if ((*node).GetId() == m_NodeId) {
              m_Node = &node;
              return eTreeTraverseStop;
          } 
      }
      return eTreeTraverse;
  }

protected:
  IPhyNode::TID m_NodeId;
  CPhyloTreeNode* m_Node;
};

/* Moved to guide_tree.hpp

// Tree visitor for examining whether a phylogenetic tree contains sequences
// with only one Blast Name
class CPhyloTreeSingleBlastNameExaminer
{
public:
    CPhyloTreeSingleBlastNameExaminer(void) : m_IsSingleBlastName(true) 
    {
        const CBioTreeFeatureDictionary& fdict = CPhyTreeNode::GetDictionary();
        if (!fdict.HasFeature(CGuideTree::kBlastNameTag)) {
            NCBI_THROW(CException, eInvalid, 
                       "No Blast Name feature CBioTreeFeatureDictionary");
        }
        else {
            m_BlastNameFeatureId = fdict.GetId(CGuideTree::kBlastNameTag);
        }
    }

    bool IsSingleBlastName(void) const {return m_IsSingleBlastName;}
    ETreeTraverseCode operator()(CPhyloTreeNode& node, int delta) 
    {
        if (delta == 0 || delta == 1) {
            if (node.IsLeaf()) {

                const CBioTreeFeatureList& flist 
                    = node.GetValue().GetBioTreeFeatureList();

                if (m_CurrentBlastName.empty()) {
                    m_CurrentBlastName 
                        = flist.GetFeatureValue(m_BlastNameFeatureId);
                }
                else {
                    if (m_CurrentBlastName 
                        != flist.GetFeatureValue(m_BlastNameFeatureId)) {
                        m_IsSingleBlastName = false;
                        return eTreeTraverseStop;
                    }
                }
            }
        }
        return eTreeTraverse;
    }
    
protected:
    bool m_IsSingleBlastName;
    TBioTreeFeatureId m_BlastNameFeatureId;
    string m_CurrentBlastName;
};
*/

END_NCBI_SCOPE

#endif // BLAST_GUIDETREE___GUIDETREESIMPLIFY__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:  Things for representing and manipulating bio trees
 *
 */

/// @file bio_tree.cpp
/// Things for representing and manipulating bio trees

#include <ncbi_pch.hpp>
#include <algo/phy_tree/bio_tree.hpp>

BEGIN_NCBI_SCOPE

/*
IBioTreeNode::~IBioTreeNode()
{}
*/


CBioTreeFeatureList::CBioTreeFeatureList()
{
}

CBioTreeFeatureList::CBioTreeFeatureList(const CBioTreeFeatureList& flist)
 : m_FeatureList(flist.m_FeatureList)
{}

CBioTreeFeatureList& 
CBioTreeFeatureList::operator=(const CBioTreeFeatureList& flist)
{
    m_FeatureList.assign(flist.m_FeatureList.begin(), 
                         flist.m_FeatureList.end());
    return *this;
}

void CBioTreeFeatureList::SetFeature(TBioTreeFeatureId id, 
                                     const string&     value)
{
    NON_CONST_ITERATE(TFeatureList, it, m_FeatureList) {
        if (it->id == id) {
            it->value = value;
            return;
        }
    }
    m_FeatureList.push_back(CBioTreeFeaturePair(id, value));
}

const string& 
CBioTreeFeatureList::GetFeatureValue(TBioTreeFeatureId id) const
{
    ITERATE(TFeatureList, it, m_FeatureList) {
        if (it->id == id) {
            return it->value;
        }
    }
    return kEmptyStr;
}

void CBioTreeFeatureList::RemoveFeature(TBioTreeFeatureId id)
{
    NON_CONST_ITERATE(TFeatureList, it, m_FeatureList) {
        if (it->id == id) {
            m_FeatureList.erase(it);
            return;
        }
    }
}






CBioTreeFeatureDictionary::CBioTreeFeatureDictionary()
 : m_IdCounter(0)
{
}


CBioTreeFeatureDictionary::CBioTreeFeatureDictionary(
                                      const CBioTreeFeatureDictionary& btr)
 : m_Dict(btr.m_Dict),
   m_Name2Id(btr.m_Name2Id),
   m_IdCounter(btr.m_IdCounter)
{
}

CBioTreeFeatureDictionary& 
CBioTreeFeatureDictionary::operator=(const CBioTreeFeatureDictionary& btr)
{
    Clear();

    ITERATE(TFeatureDict, it, btr.m_Dict) {
        m_Dict.insert(*it);
    }

    ITERATE(TFeatureNameIdx, it, btr.m_Name2Id) {
        m_Name2Id.insert(*it);
    }

    m_IdCounter = btr.m_IdCounter;

    return *this;
}

void CBioTreeFeatureDictionary::Clear()
{
    m_Dict.clear();
    m_Name2Id.clear();
    m_IdCounter = 0;
}

bool 
CBioTreeFeatureDictionary::HasFeature(const string& feature_name) const
{
    TFeatureNameIdx::const_iterator it = m_Name2Id.find(feature_name);
    return (it != m_Name2Id.end());
}

bool 
CBioTreeFeatureDictionary::HasFeature(TBioTreeFeatureId id) const
{
    TFeatureDict::const_iterator it = m_Dict.find(id);
    return (it != m_Dict.end());
}

TBioTreeFeatureId 
CBioTreeFeatureDictionary::Register(const string& feature_name)
{
    TFeatureNameIdx::const_iterator it = m_Name2Id.find(feature_name);
    if (it != m_Name2Id.end()) {
        /// Feature already exists
        return it->second;
    }

    unsigned id = m_IdCounter;
    Register(m_IdCounter, feature_name);
    return id;
}

void CBioTreeFeatureDictionary::Register(TBioTreeFeatureId id, 
										 const string& feature_name)
{
    if (m_Dict.count(id)) {
        if (m_Dict[id] == feature_name) {
            /// Duplicate registration of the same feature
            return;
        }
        NCBI_THROW(CException, eUnknown,
                   "Duplicate feature id: " + NStr::NumericToString(id));
    }
    if (m_Name2Id.count(feature_name)) {
        NCBI_THROW(CException, eUnknown,
                   "Duplicate feature name: " + feature_name);
    }
    m_IdCounter = max(m_IdCounter, id+1);
    m_Dict.insert(
           pair<TBioTreeFeatureId, string>(id, feature_name));
    m_Name2Id.insert(
           pair<string, TBioTreeFeatureId>(feature_name, id));

}

void
CBioTreeFeatureDictionary::Unregister(TBioTreeFeatureId id)
{
    if (!m_Dict.count(id)) {
        return;
    }

    m_Name2Id.erase(m_Dict[id]);
    m_Dict.erase(id);
}


TBioTreeFeatureId 
CBioTreeFeatureDictionary::GetId(const string& feature_name) const
{
    TFeatureNameIdx::const_iterator it = m_Name2Id.find(feature_name);
    if (it == m_Name2Id.end()) {
        return (TBioTreeFeatureId)-1;
    }
    return it->second;
}

string CBioTreeFeatureDictionary::GetName(TBioTreeFeatureId id) const
{
    TFeatureDict::const_iterator it = m_Dict.find(id);
    if (it != m_Dict.end())
        return it->second;
    return "";
}


static string s_EncodeLabel(const string& label);

// recursive function
void PrintNode(CNcbiOstream& os, const CBioTreeDynamic& tree,
               const CBioTreeDynamic::TBioTreeNode& node)
{
    if (!node.IsLeaf()) {
        os << '(';
        CBioTreeDynamic::TBioTreeNode::TNodeList_CI it = node.SubNodeBegin();
        for (;  it != node.SubNodeEnd();  ++it) {
            if (it != node.SubNodeBegin()) {
                os << ", ";
            }

            const CBioTreeDynamic::TBioTreeNode::TParent* p = *it;
            const CBioTreeDynamic::TBioTreeNode* pp = 
                (const CBioTreeDynamic::TBioTreeNode*)p;
            PrintNode(os, tree, *pp);
        }
        os << ')';
    }

    string label;
    if (tree.GetFeatureDict().HasFeature("label")) {
        label = node.GetValue().features
            .GetFeatureValue(tree.GetFeatureDict().GetId("label"));
    }
    if (node.IsLeaf() || !label.empty()) {
        os << s_EncodeLabel(label);
    }

    string dist_string;
    if (tree.GetFeatureDict().HasFeature("dist")) {
        dist_string = node.GetValue().features
            .GetFeatureValue(tree.GetFeatureDict().GetId("dist"));
    }    
    if (!dist_string.empty()) {
        os << ':' << dist_string;
    }
}


CNcbiOstream& operator<<(CNcbiOstream& os, const CBioTreeDynamic& tree)
{
    PrintNode(os, tree, *tree.GetTreeNode());
    os << ';' << endl;
    return os;
};


void WriteNexusTree(CNcbiOstream& os, const CBioTreeDynamic& tree,
                    const string& tree_name)
{
    os << "#nexus\n\nbegin trees;\ntree " << tree_name << " = "
       << tree << "\nend;" << endl;
};


// Encode a label for Newick format:
// If necessary, enclose it in single quotes,
// but first escape any single quotes by doubling them.
// e.g., "This 'label'" -> "'This ''label'''"
static string s_EncodeLabel(const string& label) {
    if (label.find_first_of("()[]':;,_") == string::npos) {
        // No need to quote, but any spaces must be changed to underscores
        string unquoted = label;
        for (size_t i = 0; i < label.size(); ++i) {
            if (unquoted[i] == ' ') {
                unquoted[i] = '_';
            }
        }
        return unquoted;
    }
    if (label.find_first_of("'") == string::npos) {
        return '\'' + label + '\'';
    }
    string rv;
    rv.reserve(label.size() + 2);
    rv.append(1, '\'');
    for (unsigned int i = 0;  i < label.size();  ++i) {
        rv.append(1, label[i]);
        if (label[i] == '\'') {
            // "'" -> "''"
            rv.append(1, label[i]);
        }
    }
    rv.append(1, '\'');

    return rv;
}


END_NCBI_SCOPE

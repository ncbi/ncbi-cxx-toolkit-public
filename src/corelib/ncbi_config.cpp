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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Parameters tree implementations
 *
 * ===========================================================================
 */
 
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbidll.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/error_codes.hpp>

#include <algorithm>
#include <memory>
#include <set>


#define NCBI_USE_ERRCODE_X   Corelib_Config


BEGIN_NCBI_SCOPE


static const char* kSubNode           = ".SubNode";
static const char* kSubSection        = ".SubSection";
static const char* kNodeName          = ".NodeName";
static const char* kIncludeSections   = ".Include";


static
void s_List2Set(const list<string>& src, set<string>* dst)
{
    ITERATE(list<string>, it, src) {
        dst->insert(*it);
    }
}


bool s_IsSubNode(const string& str)
{
    if (NStr::CompareNocase(kSubNode, str) == 0) {
        return true;
    }
    if (NStr::CompareNocase(kSubSection, str) == 0) {
        return true;
    }
    return false;
}


typedef CConfig::TParamTree TParamTree;
typedef CConfig::TParamValue TParamValue;
typedef map<TParamTree*, set<string> > TSectionMap;


void s_AddOrReplaceSubNode(TParamTree*   node_ptr,
                           const string& element_name,
                           const string& element_value)
{
    TParamTree* existing_node = const_cast<TParamTree*>
        (node_ptr->FindNode(element_name,
                            TParamTree::eImmediateSubNodes));
    if ( existing_node ) {
        existing_node->GetValue().value = element_value;
    }
    else {
        node_ptr->AddNode(TParamValue(element_name, element_value));
    }
}


TParamTree* s_FindSubNode(const string& path,
                          TParamTree*   tree_root)
{
    list<string> name_list;
    list<TParamTree*> node_list;

    NStr::Split(path, "/", name_list,
        NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    tree_root->FindNodes(name_list, &node_list);
    return node_list.empty() ? 0 : *node_list.rbegin();
}


void s_ParseSubNodes(const string& sub_nodes,
                     TParamTree*   parent_node,
                     TSectionMap&  inc_sections,
                     set<string>&  rm_sections)
{
    list<string> sub_list;
    NStr::Split(sub_nodes, ",; \t\n\r", sub_list,
        NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    set<string> sub_set;
    s_List2Set(sub_list, &sub_set);
    ITERATE(set<string>, sub_it, sub_set) {
        auto_ptr<TParamTree> sub_node(new TParamTree);
        size_t pos = sub_it->rfind('/');
        if (pos == string::npos) {
            sub_node->GetKey() = *sub_it;
        } else {
            // extract the last element in the path
            sub_node->GetKey() = sub_it->substr(pos + 1, sub_it->length());
        }
        inc_sections[sub_node.get()].insert(*sub_it);
        rm_sections.insert(*sub_it);
        parent_node->AddNode(sub_node.release());
    }
}


bool s_IsParentNode(TParamTree* parent, TParamTree* child)
{
    TParamTree* node = child;
    while ( node ) {
        if (node == parent) {
            return true;
        }
        node = (TParamTree*)node->GetParent();
    }
    return false;
}


void s_IncludeNode(TParamTree*       parent_node,
                   const TParamTree* inc_node)
{
    TParamTree::TNodeList_CI sub_it = inc_node->SubNodeBegin();
    TParamTree::TNodeList_CI sub_end = inc_node->SubNodeEnd();
    for ( ; sub_it != sub_end; ++sub_it) {
        TParamTree* sub_node =
            parent_node->FindSubNode((*sub_it)->GetKey());
        if ( sub_node ) {
            // Update the existing subtree to include all missing nodes
            s_IncludeNode(sub_node, *sub_it);
        }
        else {
            // Copy the whole subtree
            parent_node->AddNode(new TParamTree(**sub_it));
        }
    }
}


void s_ExpandSubNodes(TSectionMap& inc_sections,
                      TParamTree*  tree_root,
                      TParamTree*  node)
{
    TSectionMap::iterator current;
    if ( node ) {
        current = inc_sections.find(node);
    }
    else {
        current = inc_sections.begin();
        node = current->first;
    }
    if (current != inc_sections.end()) {
        // Node has included sections, expand them first.
        ITERATE(set<string>, inc_it, current->second) {
            TParamTree* inc_node = s_FindSubNode(*inc_it, tree_root);
            if ( !inc_node ) {
                continue;
            }
            if ( s_IsParentNode(inc_node, node) ) {
                _TRACE(Error << "Circular section reference: "
                            << node->GetKey() << "->" << *inc_it);
                continue; // skip the offending subnode
            }
            s_ExpandSubNodes(inc_sections, tree_root, inc_node);
            s_IncludeNode(node, inc_node);
        }
        inc_sections.erase(current);
    }
    // In case there are includes on deeper levels expand them too
    TParamTree::TNodeList_I sub_it = node->SubNodeBegin();
    TParamTree::TNodeList_I sub_end = node->SubNodeEnd();
    for ( ; sub_it != sub_end; ++sub_it) {
        s_ExpandSubNodes(inc_sections, tree_root, *sub_it);
    }
}


struct SNodeNameUpdater
{
    typedef set<TParamTree*> TNodeSet;
    TNodeSet& rm_node_name;
    SNodeNameUpdater(TNodeSet& node_set) : rm_node_name(node_set) {}

    ETreeTraverseCode operator()(TParamTree& node,
                                 int /* delta_level */);
};


ETreeTraverseCode SNodeNameUpdater::operator()(TParamTree& node,
                                               int /* delta_level */)
{
    if (NStr::CompareNocase(node.GetKey(), kNodeName) == 0) {
        TParamTree* parent = node.GetParent();
        if ( parent  &&  !node.GetValue().value.empty() ) {
            parent->GetKey() = node.GetValue().value;
            rm_node_name.insert(&node);
        }
    }
    return eTreeTraverse;
}


CConfig::TParamTree* CConfig::ConvertRegToTree(const IRegistry& reg)
{
    auto_ptr<TParamTree> tree_root(new TParamTree);

    list<string> sections;
    reg.EnumerateSections(&sections);

    // find the non-redundant set of top level sections

    set<string> all_section_names;
    s_List2Set(sections, &all_section_names);

    // Collect included and sub-noded names for each section.
    TSectionMap inc_sections;
    // Nodes used in .SubNode must be removed from the tree root.
    set<string> rm_sections;

    ITERATE(set<string>, name_it, all_section_names) {
        const string& section_name = *name_it;
        TParamTree* node_ptr;
        if (section_name.find('/') == string::npos) {
            auto_ptr<TParamTree> node(new TParamTree);
            node->GetKey() = section_name;
            tree_root->AddNode(node_ptr = node.release());
        } else {
            list<string> sub_node_list;
            NStr::Split(section_name, "/", sub_node_list,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
            node_ptr = tree_root->FindOrCreateNode(sub_node_list);
        }

        bool have_explicit_name = false;

        // Create section entries
        list<string> entries;
        reg.EnumerateEntries(section_name, &entries);

        ITERATE(list<string>, eit, entries) {
            const string& element_name = *eit;
            const string& element_value = reg.Get(section_name, element_name);

            if (NStr::CompareNocase(element_name, kNodeName) == 0) {
                have_explicit_name = true;
            }
            if (NStr::CompareNocase(element_name, kIncludeSections) == 0) {
                list<string> inc_list;
                NStr::Split(element_value, ",; \t\n\r", inc_list,
                    NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
                s_List2Set(inc_list, &inc_sections[node_ptr]);
                continue;
            }
            if (s_IsSubNode(element_name)) {
                s_ParseSubNodes(element_value,
                                node_ptr,
                                inc_sections,
                                rm_sections);
                continue;
            }

            s_AddOrReplaceSubNode(node_ptr, element_name, element_value);
        }
        // Force node name to prevent overriding it by includes
        if ( !have_explicit_name ) {
            node_ptr->AddNode(TParamValue(kNodeName, node_ptr->GetKey()));
        }
    }
    s_ExpandSubNodes(inc_sections, tree_root.get(), tree_root.get());

    // Remove nodes used in .SubNode
    ITERATE(set<string>, rm_it, rm_sections) {
        TParamTree* rm_node = s_FindSubNode(*rm_it, tree_root.get());
        if ( rm_node ) {
            rm_node->GetParent()->RemoveNode(rm_node);
        }
    }

    // Rename nodes as requested and remove .NodeName entries
    set<TParamTree*> rm_node_names;
    SNodeNameUpdater name_updater(rm_node_names);
    TreeDepthFirstTraverse(*tree_root, name_updater);
    ITERATE(set<TParamTree*>, rm_it, rm_node_names) {
        (*rm_it)->GetParent()->RemoveNode(*rm_it);
    }

    /*
    set<string> all_sections;
    set<string> sub_sections;
    set<string> top_sections;
    set<string> inc_sections;

    s_List2Set(sections, &all_sections);

    {{
        ITERATE(list<string>, it, sections) {
            const string& section_name = *it;
            s_GetSubNodes(reg, section_name, &sub_sections);            
            s_GetIncludes(reg, section_name, &inc_sections);            
        }
        set<string> non_top;
        non_top.insert(sub_sections.begin(), sub_sections.end());
        //non_top.insert(inc_sections.begin(), inc_sections.end());
        insert_iterator<set<string> > ins(top_sections, top_sections.begin());
        set_difference(all_sections.begin(), all_sections.end(),
                       non_top.begin(), non_top.end(),
                       ins);
    }}

    ITERATE(set<string>, sit, top_sections) {
        const string& section_name = *sit;

        TParamTree* node_ptr;
        if (section_name.find('/') == string::npos) {
            auto_ptr<TParamTree> node(new TParamTree);
            node->GetKey() = section_name;
            tree_root->AddNode(node_ptr = node.release());
        } else {
            list<string> sub_node_list;
            NStr::Split(section_name, "/", sub_node_list);
            node_ptr = tree_root->FindOrCreateNode( sub_node_list);
        }

        // Get section components

        list<string> entries;
        reg.EnumerateEntries(section_name, &entries);

        // Include other sections before processing any values
        s_ParamTree_IncludeSections(reg, section_name, node_ptr);

        ITERATE(list<string>, eit, entries) {
            const string& element_name = *eit;
            const string& element_value = reg.Get(section_name, element_name);

            if (NStr::CompareNocase(element_name, kIncludeSections) == 0) {
                continue;
            }
            if (NStr::CompareNocase(element_name, kNodeName) == 0) {
                node_ptr->GetKey() = element_value;
                continue;
            }
            if (s_IsSubNode(element_name)) {
                s_ParamTree_SplitConvertSubNodes(reg, element_value, node_ptr);
                continue;
            }

            s_AddOrReplaceSubNode(node_ptr, element_name, element_value);
        } // ITERATE eit

    } // ITERATE sit
    */

    return tree_root.release();
}


CConfig::CConfig(TParamTree* param_tree, EOwnership own)
    : m_ParamTree(param_tree, own)
{
    if ( !param_tree ) {
        m_ParamTree.reset(new TParamTree, eTakeOwnership);
    }
}


CConfig::CConfig(const IRegistry& reg)
{
    m_ParamTree.reset(ConvertRegToTree(reg), eTakeOwnership);
    _ASSERT(m_ParamTree.get());
}


CConfig::CConfig(const TParamTree* param_tree)
{
    if ( !param_tree ) {
        m_ParamTree.reset(new TParamTree, eTakeOwnership);
    }
    else {
        m_ParamTree.reset(const_cast<TParamTree*>(param_tree), eNoOwnership);
    }
}


CConfig::~CConfig()
{
}


string CConfig::GetString(const string&  driver_name,
                          const string&  param_name, 
                          EErrAction     on_error,
                          const string&  default_value,
                          const list<string>* synonyms)
{
    return x_GetString(driver_name, param_name,
                       on_error, default_value, synonyms);
}


const string& CConfig::GetString(const string&  driver_name,
                                 const string&  param_name, 
                                 EErrAction     on_error,
                                 const list<string>* synonyms)
{
    return x_GetString(driver_name, param_name,
                       on_error, kEmptyStr, synonyms);
}


const string& CConfig::x_GetString(const string&  driver_name,
                                   const string&  param_name, 
                                   EErrAction     on_error,
                                   const string&  default_value,
                                   const list<string>* synonyms)
{
    list<const TParamTree*> tns;
    const TParamTree* tn = m_ParamTree->FindSubNode(param_name);

    if (tn && !tn->GetValue().value.empty()) 
        tns.push_back(tn);
    if (synonyms) {
        ITERATE(list<string>, it, *synonyms) {
            tn = m_ParamTree->FindSubNode(*it);
            if (tn && !tn->GetValue().value.empty()) 
                tns.push_back(tn);
        }
    }
    if (tns.empty()) {
        if (on_error == eErr_NoThrow) {
            return default_value;
        }
        string msg = "Cannot init plugin " + driver_name +
                     ", missing parameter:" + param_name;
        if (synonyms) {
            ITERATE(list<string>, it, *synonyms) {
                if ( it == synonyms->begin() ) msg += " or ";
                else msg += ", ";
                msg += *it;
            }
        }
            
        NCBI_THROW(CConfigException, eParameterMissing, msg);
    }
    if (tns.size() > 1 ) {
        string msg = "There are more then 1 synonyms parameters ("; 
        ITERATE(list<const TParamTree*>, it, tns) {
            if (it != tns.begin()) msg += ", ";
            msg += (*it)->GetKey();
        }
        msg += ") defined";
        if (on_error == eErr_NoThrow) {
            msg += " for driver " + driver_name + ". Default value is used.";
            ERR_POST_X_ONCE(1, msg); 
            return default_value;
        }
        msg = "Cannot init plugin " + driver_name + ". " + msg;
        NCBI_THROW(CConfigException, eSynonymDuplicate, msg);
    }
    return (*tns.begin())->GetValue().value;
}


int CConfig::GetInt(const string&  driver_name,
                    const string&  param_name, 
                    EErrAction     on_error,
                    int            default_value,
                    const list<string>* synonyms)
{
    const string& param = GetString(driver_name, param_name, on_error, synonyms);

    if (param.empty()) {
        if (on_error == eErr_Throw) {
            string msg = "Cannot init " + driver_name +
                         ", empty parameter:" + param_name;
            NCBI_THROW(CConfigException, eParameterMissing, msg);
        } else {
            return default_value;
        }
    }

    try {
        return NStr::StringToInt(param);
    }
    catch (CStringException& ex)
    {
        if (on_error == eErr_Throw) {
            string msg = "Cannot init " + driver_name +
                          ", incorrect parameter format:" +
                          param_name  + " : " + param +
                          " " + ex.what();
            NCBI_THROW(CConfigException, eParameterMissing, msg);
        } else {
            string msg = "Configuration error " + driver_name +
                          ", incorrect parameter format:" +
                          param_name  + " : " + param +
                          " " + ex.what() + ". Default value is used";
            ERR_POST_X_ONCE(2, msg);
        }
    }
    return default_value;
}

Uint8 CConfig::GetDataSize(const string&  driver_name,
                           const string&  param_name, 
                           EErrAction     on_error,
                           unsigned int   default_value,
                           const list<string>* synonyms)
{
    const string& param = GetString(driver_name, param_name, on_error, synonyms);

    if (param.empty()) {
        if (on_error == eErr_Throw) {
            string msg = "Cannot init " + driver_name +
                         ", empty parameter:" + param_name;
            NCBI_THROW(CConfigException, eParameterMissing, msg);
        } else {
            return default_value;
        }
    }

    try {
        return NStr::StringToUInt8_DataSize(param);
    }
    catch (CStringException& ex)
    {
        if (on_error == eErr_Throw) {
            string msg = "Cannot init " + driver_name +
                         ", incorrect parameter format:" +
                         param_name  + " : " + param +
                         " " + ex.what();
            NCBI_THROW(CConfigException, eParameterMissing, msg);
        } else {
            string msg = "Configuration error " + driver_name +
                          ", incorrect parameter format:" +
                          param_name  + " : " + param +
                          " " + ex.what() + ". Default value is used";
            ERR_POST_X_ONCE(3, msg);
        }
    }
    return default_value;
}


bool CConfig::GetBool(const string&  driver_name,
                      const string&  param_name, 
                      EErrAction     on_error,
                      bool           default_value,
                      const list<string>* synonyms)
{
    const string& param = GetString(driver_name, param_name, on_error, synonyms);

    if (param.empty()) {
        if (on_error == eErr_Throw) {
            string msg = "Cannot init " + driver_name +
                         ", empty parameter:" + param_name;
            NCBI_THROW(CConfigException, eParameterMissing, msg);
        } else {
            return default_value;
        }
    }

    try {
        return NStr::StringToBool(param);
    }
    catch (CStringException& ex)
    {
        if (on_error == eErr_Throw) {
            string msg = "Cannot init " + driver_name +
                         ", incorrect parameter format:" +
                         param_name  + " : " + param +
                         ". " + ex.what();
            NCBI_THROW(CConfigException, eParameterMissing, msg);
        } else {
            string msg = "Configuration error " + driver_name +
                          ", incorrect parameter format:" +
                          param_name  + " : " + param +
                          " " + ex.what() + ". Default value is used";
            ERR_POST_X_ONCE(4, msg);
        }
    }
    return default_value;
}

double CConfig::GetDouble(const string&  driver_name,
                          const string&  param_name, 
                          EErrAction     on_error,
                          double         default_value,
                          const list<string>* synonyms)
{
    const string& param = GetString(driver_name, param_name, on_error, synonyms);

    if (param.empty()) {
        if (on_error == eErr_Throw) {
            string msg = "Cannot init " + driver_name +
                         ", empty parameter:" + param_name;
            NCBI_THROW(CConfigException, eParameterMissing, msg);
        } else {
            return default_value;
        }
    }

    try {
        return NStr::StringToDouble(param, NStr::fDecimalPosixOrLocal);
    }
    catch (CStringException& ex)
    {
        if (on_error == eErr_Throw) {
            string msg = "Cannot init " + driver_name +
                          ", incorrect parameter format:" +
                          param_name  + " : " + param +
                          " " + ex.what();
            NCBI_THROW(CConfigException, eParameterMissing, msg);
        } else {
            string msg = "Configuration error " + driver_name +
                          ", incorrect parameter format:" +
                          param_name  + " : " + param +
                          " " + ex.what() + ". Default value is used";
            ERR_POST_X_ONCE(5, msg);
        }
    }
    return default_value;
}

const char* CConfigException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eParameterMissing: return "eParameterMissing";
    case eSynonymDuplicate: return "eSynonymDuplicate";
    case eInvalidParameter: return "eInvalidParameter";
    default:                return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE

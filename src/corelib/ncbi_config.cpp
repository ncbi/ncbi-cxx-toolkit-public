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

#include <algorithm>
#include <memory>
#include <set>

BEGIN_NCBI_SCOPE


/// @internal
static
void s_ParamTree_ConvertSubNodes(const IRegistry&      reg,
                                 const list<string>&   sub_nodes,
                                 CConfig::TParamTree*  node);
/// @internal
static
void s_ParamTree_SplitConvertSubNodes(const IRegistry&      reg,
                                      const string&         sub_nodes,
                                      CConfig::TParamTree*  node);

static const string kSubNode           = ".SubNode";
static const string kSubSection        = ".SubSection";
static const string kNodeName          = ".NodeName";
static const string kIncludeSections   = ".Include";


/// @internal
static
void s_List2Set(const list<string>& src, set<string>* dst)
{
    ITERATE(list<string>, it, src) {
        dst->insert(*it);
    }
}

/// @internal
static 
bool s_IsSubNode(const string& str)
{
    const string* aliases[] = { &kSubNode, &kSubSection };

    for (unsigned int i = 0; i < (sizeof(aliases)/sizeof(aliases[0])); ++i) {
        const string& element_name = *aliases[i];
        if (NStr::CompareNocase(element_name, str) == 0) {
            return true;
        }
    }
    return false;
}

/// @internal
static
void s_GetSubNodes(const IRegistry&      reg, 
                   const string&         section, 
                   set<string>*          dst)
{
    const string* aliases[] = { &kSubNode, &kSubSection };

    for (unsigned int i = 0; i < (sizeof(aliases)/sizeof(aliases[0])); ++i) {
        const string& element_name = *aliases[i];
        const string& element = reg.Get(section, element_name);
        if (!element.empty()) {
            list<string> sub_node_list;
            NStr::Split(element, ",; \t\n\r", sub_node_list);
            s_List2Set(sub_node_list, dst);
        }
    }
}

/// @internal
static
void s_AddOrReplaceSubNode(CConfig::TParamTree* node_ptr,
                           const string& element_name,
                           const string& element_value)
{
    CConfig::TParamTree* existing_node = const_cast<CConfig::TParamTree*>
        (node_ptr->FindNode(element_name,
                            CConfig::TParamTree::eImmediateSubNodes));
    if ( existing_node ) {
        existing_node->GetValue().value = element_value;
    }
    else {
        node_ptr->AddNode(CConfig::TParamValue(element_name, element_value));
    }
}

static
void s_ParamTree_SplitConvertSubNodes(const IRegistry&      reg,
                                      const string&         sub_nodes,
                                      CConfig::TParamTree*  node);

/// @internal
static
void s_ParamTree_IncludeSections(const IRegistry& reg,
                                 const string& section_name,
                                 CConfig::TParamTree* node_ptr)
{
    if ( !reg.HasEntry(section_name, kIncludeSections) ) {
        return;
    }

    const string& element = reg.Get(section_name, kIncludeSections);
    list<string> inc_list;
    if ( element.empty() ) {
        return;
    }
    NStr::Split(element, ",; \t\n\r", inc_list);

    ITERATE(list<string>, inc_it, inc_list) {
        const string& inc_section_name = *inc_it;
        const string& inc_alias_name = reg.Get(inc_section_name, kNodeName);
        string inc_node_name(inc_alias_name.empty() ?
            inc_section_name : inc_alias_name);

        // Check if this node is an ancestor (circular reference)
        CConfig::TParamTree* parent_node =  node_ptr;
        while (parent_node) {
            const string& id = parent_node->GetKey();
            if (NStr::CompareNocase(inc_node_name, id) == 0) {
                _TRACE(Error << "PluginManger: circular section reference "
                             << node_ptr->GetKey() << "->" << inc_node_name);
                continue; // skip the offending subnode
            }
            parent_node = (CConfig::TParamTree*)parent_node->GetParent();
        } // while

        list<string> entries;
        reg.EnumerateEntries(inc_section_name, &entries);
        if (entries.empty()) {
            continue;
        }

        // Include other sections before processing any values
        s_ParamTree_IncludeSections(reg, inc_section_name, node_ptr);

        // include elements
        ITERATE(list<string>, eit, entries) {
            const string& element_name = *eit;

            if (NStr::CompareNocase(element_name, kNodeName) == 0) {
                continue;
            }
            if (NStr::CompareNocase(element_name, kIncludeSections) == 0) {
                continue;
            }
            const string& element_value =
                reg.Get(inc_section_name, element_name);

            if (s_IsSubNode(element_name)) {
                s_ParamTree_SplitConvertSubNodes(reg, element_value, node_ptr);
                continue;
            }

            s_AddOrReplaceSubNode(node_ptr, element_name, element_value);
        } // ITERATE eit
    } // ITERATE inc_it
}

/// @internal
static
void s_ParamTree_ConvertSubNode(const IRegistry&      reg,
                                const string&         sub_node_name,
                                CConfig::TParamTree*  node)
{
    const string& section_name = sub_node_name;
    const string& alias_name = reg.Get(section_name, kNodeName);

    string node_name(alias_name.empty() ? section_name : alias_name);

    // Check if this node is an ancestor (circular reference)

    CConfig::TParamTree* parent_node =  node;
    while (parent_node) {
        const string& id = parent_node->GetKey();
        if (NStr::CompareNocase(node_name, id) == 0) {
            _TRACE(Error << "PluginManger: circular section reference " 
                            << node->GetKey() << "->" << node_name);
            return; // skip the offending subnode
        }
        parent_node = (CConfig::TParamTree*)parent_node->GetParent();
    } // while

    list<string> entries;
    reg.EnumerateEntries(section_name, &entries);
    if (entries.empty()) {
        return;
    }

    CConfig::TParamTree* sub_node_ptr = const_cast<CConfig::TParamTree*>
        (node->FindNode(node_name,
                        CConfig::TParamTree::eImmediateSubNodes));
    if ( !sub_node_ptr ) {
        auto_ptr<CConfig::TParamTree> sub_node(new CConfig::TParamTree);
        sub_node->GetKey() = node_name;
        sub_node_ptr = sub_node.release();
        node->AddNode(sub_node_ptr);
    }

    // Include other sections before processing any values
    s_ParamTree_IncludeSections(reg, section_name, sub_node_ptr);

    // convert elements
    ITERATE(list<string>, eit, entries) {
        const string& element_name = *eit;

        if (NStr::CompareNocase(element_name, kIncludeSections) == 0) {
            continue;
        }
        if (NStr::CompareNocase(element_name, kNodeName) == 0) {
            continue;
        }
        const string& element_value = reg.Get(section_name, element_name);

        if (s_IsSubNode(element_name)) {
            s_ParamTree_SplitConvertSubNodes(reg, element_value,sub_node_ptr);
            continue;
        }
        s_AddOrReplaceSubNode(sub_node_ptr, element_name, element_value);
    } // ITERATE eit
}


/// @internal
static
void s_ParamTree_SplitConvertSubNodes(const IRegistry&      reg,
                                      const string&         sub_nodes,
                                      CConfig::TParamTree*  node)
{
    list<string> sub_node_list;
    NStr::Split(sub_nodes, ",; \t\n\r", sub_node_list);
    s_ParamTree_ConvertSubNodes(reg, sub_node_list, node);
}


/// @internal
static
void s_ParamTree_ConvertSubNodes(const IRegistry&      reg,
                                 const list<string>&   sub_nodes,
                                 CConfig::TParamTree*  node)
{
    _ASSERT(node);
    ITERATE(list<string>, it, sub_nodes) {
        const string& sub_node = *it;
        s_ParamTree_ConvertSubNode(reg, sub_node, node);
    }
}

/// @internal
static
void s_GetIncludes(const IRegistry&      reg, 
                   const string&         section, 
                   set<string>*          dst)
{
    const string& element = reg.Get(section, kIncludeSections);
    if ( !element.empty() ) {
        list<string> inc_list;
        NStr::Split(element, ",; \t\n\r", inc_list);
        s_List2Set(inc_list, dst);
    }
}

CConfig::TParamTree* CConfig::ConvertRegToTree(const IRegistry& reg)
{
    auto_ptr<TParamTree> tree_root(new TParamTree);

    list<string> sections;
    reg.EnumerateSections(&sections);

    // find the non-redundant set of top level sections

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
        non_top.insert(inc_sections.begin(), inc_sections.end());
        insert_iterator<set<string> > ins(top_sections, top_sections.begin());
        set_difference(all_sections.begin(), all_sections.end(),
                       non_top.begin(), non_top.end(),
                       ins);
    }}

    ITERATE(set<string>, sit, top_sections) {
        const string& section_name = *sit;

        TParamTree* node_ptr;
        {{
            auto_ptr<TParamTree> node(new TParamTree);
            node->GetKey() = section_name;
            tree_root->AddNode(node_ptr = node.release());
        }}

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

    return tree_root.release();
}


CConfig::CConfig(TParamTree* param_tree, EOwnership own)
    : m_ParamTree(param_tree), m_OwnTree(own)
{
    if ( !param_tree ) {
        m_ParamTree = new TParamTree;
        m_OwnTree = eTakeOwnership;
    }
}


CConfig::CConfig(const IRegistry& reg)
{
    m_ParamTree = ConvertRegToTree(reg);
    _ASSERT(m_ParamTree);
    m_OwnTree = eTakeOwnership;
}


CConfig::CConfig(const TParamTree* param_tree)
{
    if ( !param_tree ) {
        m_ParamTree = new TParamTree;
        m_OwnTree = eTakeOwnership;
    }
    else {
        m_ParamTree = const_cast<TParamTree*>(param_tree);
        m_OwnTree = eNoOwnership;
    }
}


CConfig::~CConfig()
{
    if (m_OwnTree == eTakeOwnership) {
        delete m_ParamTree;
    }
}


string CConfig::GetString(const string&  driver_name,
                          const string&  param_name, 
                          EErrAction     on_error,
                          const string&  default_value)
{
    try {
        return GetString(driver_name, param_name, eErr_Throw);
    } catch (CConfigException) {
        if (on_error == eErr_NoThrow) {
            return default_value;
        } else {
            throw;
        }
    }
}

const string& CConfig::GetString(const string&  driver_name,
                                 const string&  param_name, 
                                 EErrAction     on_error)
{
    const TParamTree* tn = m_ParamTree->FindSubNode(param_name);

    if (tn == 0 || tn->GetValue().value.empty()) {
        if (on_error == eErr_NoThrow) {
            return kEmptyStr;
        }
        string msg = "Cannot init plugin " + driver_name +
                     ", missing parameter:" + param_name;
        NCBI_THROW(CConfigException, eParameterMissing, msg);
    }
    return tn->GetValue().value;
}


int CConfig::GetInt(const string&  driver_name,
                    const string&  param_name, 
                    EErrAction     on_error,
                    int            default_value)
{
    const string& param = GetString(driver_name, param_name, on_error);

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
        if (on_error == eErr_NoThrow) {
            string msg = "Cannot init " + driver_name +
                          ", incorrect parameter format:" +
                          param_name  + " : " + param +
                          " " + ex.what();
            NCBI_THROW(CConfigException, eParameterMissing, msg);
        }
    }
    return default_value;
}

Uint8 CConfig::GetDataSize(const string&  driver_name,
                           const string&  param_name, 
                           EErrAction     on_error,
                           unsigned int   default_value)
{
    const string& param = GetString(driver_name, param_name, on_error);

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
        }
    }
    return default_value;
}


bool CConfig::GetBool(const string&  driver_name,
                      const string&  param_name, 
                      EErrAction     on_error,
                      bool           default_value)
{
    const string& param = GetString(driver_name, param_name, on_error);

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
        }
    }
    return default_value;
}

const char* CConfigException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eParameterMissing: return "eParameterMissing";
    default:                return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE

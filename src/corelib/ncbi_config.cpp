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
void ParamTree_ConvertSubNodes(const CNcbiRegistry&     reg,
                                   const list<string>&      sub_nodes,
                                   TParamTree* node);
/// @internal
static
void ParamTree_SplitConvertSubNodes(const CNcbiRegistry&     reg,
                                        const string&            sub_nodes,
                                        TParamTree* node);



static const string kSubNode    = ".SubNode";
static const string kSubSection = ".SubSection";
static const string kNodeName   = ".NodeName";



/// @internal
static
void s_List2Set(const list<string>& src, set<string>* dst)
{
    ITERATE(list<string>, it, src) {
        dst->insert(*it);
    }
}


static 
bool s_IsSubNode(const string& str)
{
    const string* aliases[] = { &kSubNode, &kSubSection };

    for (unsigned i = 0; i < (sizeof(aliases)/sizeof(aliases[0])); ++i) {
        const string& element_name = *aliases[i];
        if (NStr::CompareNocase(element_name, str) == 0) {
            return true;
        }
    }
    return false;
}

static
void s_GetSubNodes(const CNcbiRegistry&   reg, 
                   const string&          section, 
                   set<string>*           dst)
{
    const string* aliases[] = { &kSubNode, &kSubSection };

    for (unsigned i = 0; i < (sizeof(aliases)/sizeof(aliases[0])); ++i) {
        const string& element_name = *aliases[i];
        const string& element = reg.Get(section, element_name);
        if (!element.empty()) {
            list<string> sub_node_list;
            NStr::Split(element, ",; ", sub_node_list);
            s_List2Set(sub_node_list, dst);
        }
    }
}




/// @internal
static
void ParamTree_ConvertSubNode(const CNcbiRegistry&      reg,
                              const string&             sub_node_name,
                              TParamTree*               node)
{
    const string& section_name = sub_node_name;
    const string& alias_name = reg.Get(section_name, kNodeName);

    string node_name(alias_name.empty() ? section_name : alias_name);

    // Check if this node is an ancestor (circular reference)

    TParamTree* parent_node =  node;
    while (parent_node) {
        const string& id = parent_node->GetId();
        if (NStr::CompareNocase(node_name, id) == 0) {
            _TRACE(Error << "PluginManger: circular section reference " 
                            << node->GetId() << "->" << node_name);
            return; // skip the offending subnode
        }
        parent_node = (TParamTree*)parent_node->GetParent();

    } // while

    list<string> entries;
    reg.EnumerateEntries(section_name, &entries);
    if (entries.empty())
        return;


    TParamTree* sub_node_ptr;
    {{
    auto_ptr<TParamTree> sub_node(new TParamTree);
    sub_node->SetId(node_name);
    sub_node_ptr = sub_node.release();
    node->AddNode(sub_node_ptr);
    }}

    // convert elements

    ITERATE(list<string>, eit, entries) {
        const string& element_name = *eit;

        if (NStr::CompareNocase(element_name, kNodeName) == 0) {
            continue;
        }
        const string& element_value = reg.Get(section_name, element_name);

        if (s_IsSubNode(element_name)) {
            ParamTree_SplitConvertSubNodes(reg, element_value, sub_node_ptr);
            continue;
        }

        sub_node_ptr->AddNode(element_name, element_value);

    } // ITERATE eit

}


/// @internal
static
void ParamTree_SplitConvertSubNodes(const CNcbiRegistry&  reg,
                                    const string&         sub_nodes,
                                    TParamTree*           node)
{
    list<string> sub_node_list;
    NStr::Split(sub_nodes, ",; ", sub_node_list);

    ParamTree_ConvertSubNodes(reg, sub_node_list, node);
}

/// @internal
static
void ParamTree_ConvertSubNodes(const CNcbiRegistry&     reg,
                                   const list<string>&      sub_nodes,
                                   TParamTree* node)
{
    _ASSERT(node);

    ITERATE(list<string>, it, sub_nodes) {
        const string& sub_node = *it;
        ParamTree_ConvertSubNode(reg, sub_node, node);
    }
}




TParamTree* ParamTree_ConvertRegToTree(const CNcbiRegistry& reg)
{
    auto_ptr<TParamTree> tree_root(new TParamTree);

    list<string> sections;
    reg.EnumerateSections(&sections);


    // find the non-redundant set of top level sections

    set<string> all_sections;
    set<string> sub_sections;
    set<string> top_sections;

    s_List2Set(sections, &all_sections);

    {{
        ITERATE(list<string>, it, sections) {
            const string& section_name = *it;
            s_GetSubNodes(reg, section_name, &sub_sections);            
        }
        insert_iterator<set<string> > ins(top_sections, top_sections.begin());
        set_difference(all_sections.begin(), all_sections.end(),
                       sub_sections.begin(), sub_sections.end(),
                       ins);
    }}


    ITERATE(set<string>, sit, top_sections) {
        const string& section_name = *sit;

        TParamTree* node_ptr;
        {{
        auto_ptr<TParamTree> node(new TParamTree);
        node->SetId(section_name);

        tree_root->AddNode(node_ptr = node.release());
        }}

        // Get section components

        list<string> entries;
        reg.EnumerateEntries(section_name, &entries);

        ITERATE(list<string>, eit, entries) {
            const string& element_name = *eit;
            const string& element_value = reg.Get(section_name, element_name);
            
            if (NStr::CompareNocase(element_name, kNodeName) == 0) {
                node_ptr->SetId(element_value);
                continue;
            }

            if (s_IsSubNode(element_name)) {
                ParamTree_SplitConvertSubNodes(reg,
                                               element_value,
                                               node_ptr);
                continue;
            }

            node_ptr->AddNode(element_name, element_value);

        } // ITERATE eit


    } // ITERATE sit

    return tree_root.release();
}



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/09/22 16:52:58  ucko
 * +<memory> for auto_ptr<>
 *
 * Revision 1.2  2004/09/22 15:34:18  kuznets
 * MAGIC: rename ncbi_paramtree->ncbi_config
 *
 * Revision 1.1  2004/09/22 13:54:25  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */



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
 *   Plugin manager implementations
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/plugin_manager.hpp>
#include <corelib/ncbidll.hpp>
#include <corelib/ncbireg.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//  CPluginManager_DllResolver::
//

CPluginManager_DllResolver::CPluginManager_DllResolver(void)
 : m_DllNamePrefix("ncbi_plugin"),
   m_EntryPointPrefix("NCBI_EntryPoint"),
   m_Version(CVersionInfo::kAny),
   m_DllResolver(0)
{}

CPluginManager_DllResolver::CPluginManager_DllResolver(
                    const string& interface_name,
                    const string& driver_name,
                    const CVersionInfo& version)
 : m_DllNamePrefix("ncbi_plugin"),
   m_EntryPointPrefix("NCBI_EntryPoint"),
   m_InterfaceName(interface_name),
   m_DriverName(driver_name),
   m_Version(version),
   m_DllResolver(0)
{
}


CPluginManager_DllResolver::~CPluginManager_DllResolver(void)
{
    delete m_DllResolver;
}

CDllResolver& 
CPluginManager_DllResolver::Resolve(const vector<string>& paths,
                                    const string&         driver_name,
                                    const CVersionInfo&   version)
{
    CDllResolver* resolver = GetCreateDllResolver();
    _ASSERT(resolver);

    const string& drv = driver_name.empty() ? m_DriverName : driver_name;
    const CVersionInfo& ver = version.IsAny() ? m_Version : version;

    // Generate DLL masks

    // Ignore version to find dlls having no version in their names
    string mask = GetDllNameMask(m_InterfaceName, drv, CVersionInfo::kAny);
    vector<string> masks;
    masks.push_back(mask);

    resolver->FindCandidates(paths, masks, 
                             CDllResolver::fDefaultDllPath, drv);
    return *resolver;
}

CDllResolver& CPluginManager_DllResolver::Resolve(const string& path)
{
    _ASSERT(!path.empty());
    vector<string> paths;
    paths.push_back(path);
    return Resolve(paths);
}


string 
CPluginManager_DllResolver::GetDllName(const string&       interface_name,
                                       const string&       driver_name,
                                       const CVersionInfo& version) const
{
    string prefix = GetDllNamePrefix();
    string name = prefix;
    if (!interface_name.empty()) {
        name.append("_");
        name.append(interface_name);
    }

    if (!driver_name.empty()) {
        name.append("_");
        name.append(driver_name);
    }

    if (version.IsAny()) {
        return name;
    } else {
        
#if defined(NCBI_OS_MSWIN)
        string delimiter = "_";

#elif defined(NCBI_OS_UNIX)
        string delimiter = ".";
        name.append(NCBI_PLUGIN_SUFFIX);
#endif

        name.append(delimiter);
        name.append(NStr::IntToString(version.GetMajor()));
        name.append(delimiter);
        name.append(NStr::IntToString(version.GetMinor()));
        name.append(delimiter);
        name.append(NStr::IntToString(version.GetPatchLevel()));
    }

    return name;
}

string 
CPluginManager_DllResolver::GetDllNameMask(
        const string&       interface_name,
        const string&       driver_name,
        const CVersionInfo& version) const
{
    string prefix = GetDllNamePrefix();
    string name = prefix;

    name.append("_");
    if (interface_name.empty()) {
        name.append("*");
    } else {
        name.append(interface_name);
    }

    name.append("_");

    if (driver_name.empty()) {
        name.append("*");
    } else {
        name.append(driver_name);
    } 

    if (version.IsAny()) {
#if defined(NCBI_OS_MSWIN)
        name.append("*" NCBI_PLUGIN_SUFFIX);
#elif defined(NCBI_OS_UNIX)
        name.append(NCBI_PLUGIN_SUFFIX "*");
#endif
    } else {
        
#if defined(NCBI_OS_MSWIN)
        string delimiter = "_";

#elif defined(NCBI_OS_UNIX)
        string delimiter = ".";
        name.append(NCBI_PLUGIN_SUFFIX);
#endif

        name.append(delimiter);
        if (version.GetMajor() <= 0) {
            name.append("*");
        } else {
            name.append(NStr::IntToString(version.GetMajor()));
        }

        name.append(delimiter);

        if (version.GetMinor() <= 0) {
            name.append("*");
        } else {
            name.append(NStr::IntToString(version.GetMinor()));
        }

        name.append(delimiter);
        name.append("*");  // always get the best patch level
        
    }

    return name;
}

string 
CPluginManager_DllResolver::GetEntryPointName(
                      const string&       interface_name,
                      const string&       driver_name) const
{
    string prefix = GetEntryPointPrefix();
    string name = prefix;
    if (!interface_name.empty()) {
        name.append("_");
        name.append(interface_name);
    }

    if (!driver_name.empty()) {
        name.append("_");
        name.append(driver_name);
    }

    return name;
}


string CPluginManager_DllResolver::GetEntryPointPrefix() const 
{ 
    return m_EntryPointPrefix; 
}

string CPluginManager_DllResolver::GetDllNamePrefix() const 
{ 
    return m_DllNamePrefix; 
}

void CPluginManager_DllResolver::SetDllNamePrefix(const string& prefix)
{ 
    m_DllNamePrefix = prefix; 
}

CDllResolver* CPluginManager_DllResolver::CreateDllResolver() const
{
    vector<string> entry_point_names;
    string entry_name;
    

    // Generate all variants of entry point names
    // some of them can duplicate, and that's legal. Resolver stops trying
    // after the first success.

    entry_name = GetEntryPointName(m_InterfaceName, "${driver}");
    entry_point_names.push_back(entry_name);
    
    entry_name = GetEntryPointName(kEmptyStr, kEmptyStr);
    entry_point_names.push_back(entry_name);
    
    entry_name = GetEntryPointName(m_InterfaceName, kEmptyStr);
    entry_point_names.push_back(entry_name);
    
    entry_name = GetEntryPointName(kEmptyStr, "${driver}");
    entry_point_names.push_back(entry_name);

    // Make the library dependent entry point templates
    string base_name_templ = "${basename}";
    string prefix = GetEntryPointPrefix();
    
    // Make "NCBI_EntryPoint_libname" EP name
    entry_name = prefix;
    entry_name.append("_");
    entry_name.append(base_name_templ);
    entry_point_names.push_back(entry_name);
        
    // Make "NCBI_EntryPoint_interface_libname" EP name
    if (!m_InterfaceName.empty()) {
        entry_name = prefix;
        entry_name.append("_");
        entry_name.append(m_InterfaceName);
        entry_name.append("_");        
        entry_name.append(base_name_templ);
        entry_point_names.push_back(entry_name);
    }
    
    // Make "NCBI_EntryPoint_driver_libname" EP name
    if (!m_DriverName.empty()) {
        entry_name = prefix;
        entry_name.append("_");
        entry_name.append(m_DriverName);
        entry_name.append("_");        
        entry_name.append(base_name_templ);
        entry_point_names.push_back(entry_name);
    }

    CDllResolver* resolver = new CDllResolver(entry_point_names);

    return resolver;
 }

CDllResolver* CPluginManager_DllResolver::GetCreateDllResolver()
{
    if (m_DllResolver == 0) {
        m_DllResolver = CreateDllResolver();
    }
    return m_DllResolver;
}


/////////////////////////////////////////////////////////////////////////////
//  PluginManager_ConvertRegToTree
//


static const string kSubNode    = ".SubNode";
static const string kSubSection = ".SubSection";
static const string kNodeName   = ".NodeName";




/// @internal
static
void PluginManager_ConvertSubNodes(const CNcbiRegistry&     reg,
                                   const list<string>&      sub_nodes,
                                   TPluginManagerParamTree* node);
/// @internal
static
void PluginManager_SplitConvertSubNodes(const CNcbiRegistry&     reg,
                                        const string&            sub_nodes,
                                        TPluginManagerParamTree* node);

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
void PluginManager_ConvertSubNode(const CNcbiRegistry&      reg,
                                  const string&             sub_node_name,
                                  TPluginManagerParamTree*  node)
{
    const string& section_name = sub_node_name;
    const string& alias_name = reg.Get(section_name, kNodeName);

    string node_name(alias_name.empty() ? section_name : alias_name);

    // Check if this node is an ancestor (circular reference)

    TPluginManagerParamTree* parent_node =  node;
    while (parent_node) {
        const string& id = parent_node->GetId();
        if (NStr::CompareNocase(node_name, id) == 0) {
            _TRACE(Error << "PluginManger: circular section reference " 
                            << node->GetId() << "->" << node_name);
            return; // skip the offending subnode
        }
        parent_node = (TPluginManagerParamTree*)parent_node->GetParent();

    } // while

    list<string> entries;
    reg.EnumerateEntries(section_name, &entries);
    if (entries.empty())
        return;


    TPluginManagerParamTree* sub_node_ptr;
    {{
    auto_ptr<TPluginManagerParamTree> sub_node(new TPluginManagerParamTree);
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
            PluginManager_SplitConvertSubNodes(reg, element_value, sub_node_ptr);
            continue;
        }

        sub_node_ptr->AddNode(element_name, element_value);

    } // ITERATE eit

}

/// @internal
static
void PluginManager_SplitConvertSubNodes(const CNcbiRegistry&     reg,
                                        const string&            sub_nodes,
                                        TPluginManagerParamTree* node)
{
    list<string> sub_node_list;
    NStr::Split(sub_nodes, ",; ", sub_node_list);

    PluginManager_ConvertSubNodes(reg, sub_node_list, node);
}

/// @internal
static
void PluginManager_ConvertSubNodes(const CNcbiRegistry&     reg,
                                   const list<string>&      sub_nodes,
                                   TPluginManagerParamTree* node)
{
    _ASSERT(node);

    ITERATE(list<string>, it, sub_nodes) {
        const string& sub_node = *it;
        PluginManager_ConvertSubNode(reg, sub_node, node);
    }
}




TPluginManagerParamTree* 
PluginManager_ConvertRegToTree(const CNcbiRegistry& reg)
{
    auto_ptr<TPluginManagerParamTree> tree_root(new TPluginManagerParamTree);

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

        TPluginManagerParamTree* node_ptr;
        {{
        auto_ptr<TPluginManagerParamTree> node(new TPluginManagerParamTree);
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
                PluginManager_SplitConvertSubNodes(reg,
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
 * Revision 1.12  2004/08/09 16:43:43  grichenk
 * Ignore version when resolving DLL name
 *
 * Revision 1.11  2004/08/09 15:39:26  kuznets
 * Improved support of driver name
 *
 * Revision 1.10  2004/07/29 20:20:15  ucko
 * #include <algorithm> for set_difference().
 *
 * Revision 1.9  2004/07/29 13:23:27  kuznets
 * GCC fixes. Code cleanup.
 *
 * Revision 1.8  2004/07/29 13:14:57  kuznets
 * + PluginManager_ConvertRegToTree
 *
 * Revision 1.7  2004/06/23 17:13:56  ucko
 * Centralize plugin naming in ncbidll.hpp.
 *
 * Revision 1.6  2004/05/14 13:59:27  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.5  2003/12/09 13:25:59  kuznets
 * Added DLL name based entry point names
 *
 * Revision 1.4  2003/11/18 15:26:48  kuznets
 * Minor cosmetic changes
 *
 * Revision 1.3  2003/11/17 17:04:22  kuznets
 * Cosmetic fixes
 *
 * Revision 1.2  2003/11/12 18:57:21  kuznets
 * Implemented dll resolution.
 *
 *
 * ===========================================================================
 */


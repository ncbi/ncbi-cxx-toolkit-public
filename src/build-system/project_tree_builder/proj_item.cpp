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
 * Author:  Viatcheslav Gorelenkov
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include "proj_item.hpp"
#include "msvc_prj_utils.hpp"


BEGIN_NCBI_SCOPE


//-----------------------------------------------------------------------------

CProjKey::CProjKey(void)
:m_Type(eNoProj)
{
}


CProjKey::CProjKey(TProjType type, const string& project_id)
:m_Type(type),
 m_Id  (project_id)
{
}
CProjKey::CProjKey(TProjType type, const string& project_id, const string& id_suffix)
:m_Type(type),
 m_Id  (project_id),
 m_Suffix(id_suffix)
{
}

CProjKey::CProjKey(const CProjKey& key)
:m_Type(key.m_Type),
 m_Id  (key.m_Id),
 m_Suffix(key.m_Suffix)
{
}


CProjKey& CProjKey::operator= (const CProjKey& key)
{
    if (this != &key) {
        m_Type = key.m_Type;
        m_Id   = key.m_Id;
        m_Suffix = key.m_Suffix;
    }
    return *this;
}


CProjKey::~CProjKey(void)
{
}


bool CProjKey::operator< (const CProjKey& key) const
{
    if (m_Type < key.m_Type)
        return true;
    else if (m_Type > key.m_Type)
        return false;
    else
        return m_Id < key.m_Id;
}


bool CProjKey::operator== (const CProjKey& key) const
{
    return m_Type == key.m_Type && m_Id == key.m_Id;
}


bool CProjKey::operator!= (const CProjKey& key) const
{
    return !(*this == key);
}


CProjKey::TProjType CProjKey::Type(void) const
{
    return m_Type;
}


const string& CProjKey::Id(void) const
{
    return m_Id;
}
string CProjKey::FullId(void) const
{
    return m_Id + m_Suffix;
}


//-----------------------------------------------------------------------------
CProjItem::CProjItem(void)
{
    Clear();
}


CProjItem::CProjItem(const CProjItem& item)
{
    SetFrom(item);
}


CProjItem& CProjItem::operator= (const CProjItem& item)
{
    if (this != &item) {
        Clear();
        SetFrom(item);
    }
    return *this;
}


CProjItem::CProjItem(TProjType type,
                     const string& name,
                     const string& id,
                     const string& sources_base,
                     const list<string>&   sources, 
                     const list<CProjKey>& depends,
                     const list<string>&   requires,
                     const list<string>&   libs_3_party,
                     const list<string>&   include_dirs,
                     const list<string>&   defines,
                     EMakeFileType maketype,
                     const string& guid)
   :m_Name    (name), 
    m_ID      (id),
    m_ProjType(type),
    m_SourcesBaseDir (sources_base),
    m_Sources (sources), 
    m_Depends (depends),
    m_Requires(requires),
    m_Libs3Party (libs_3_party),
    m_IncludeDirs(include_dirs),
    m_Defines (defines),
    m_MakeType(maketype),
    m_GUID(guid),
    m_IsBundle(false),
    m_External(false),
    m_StyleObjcpp(false)
{
}


CProjItem::~CProjItem(void)
{
    Clear();
}


void CProjItem::Clear(void)
{
    m_ProjType = CProjKey::eNoProj;
    m_MakeType = eMakeType_Undefined;
    m_IsBundle = false;
    m_External = false;
    m_StyleObjcpp = false;
    m_MkName.clear();
    m_DataSource.Clear();
}


void CProjItem::SetFrom(const CProjItem& item)
{
    m_Name           = item.m_Name;
    m_ID		     = item.m_ID;
    m_ProjType       = item.m_ProjType;
    m_SourcesBaseDir = item.m_SourcesBaseDir;
    m_Pch            = item.m_Pch;
    m_Sources        = item.m_Sources;
    m_Depends        = item.m_Depends;
    m_UnconditionalDepends = item.m_UnconditionalDepends;
    m_Requires       = item.m_Requires;
    m_Libs3Party     = item.m_Libs3Party;
    m_IncludeDirs    = item.m_IncludeDirs;
    m_DatatoolSources= item.m_DatatoolSources;
    m_Defines        = item.m_Defines;
    m_NcbiCLibs      = item.m_NcbiCLibs;
    m_MakeType       = item.m_MakeType;
    m_GUID           = item.m_GUID;
    m_DllHost        = item.m_DllHost;
    m_HostedLibs     = item.m_HostedLibs;

    m_ExportHeadersDest = item.m_ExportHeadersDest;
    m_ExportHeaders     = item.m_ExportHeaders;
    m_Watchers          = item.m_Watchers;
    m_CheckInfo         = item.m_CheckInfo;
    m_CheckConfigs      = item.m_CheckConfigs;

    m_Includes = item.m_Includes;
    m_Inlines  = item.m_Inlines;
    m_ExtraFiles = item.m_ExtraFiles;
    m_ProjTags = item.m_ProjTags;
    m_CustomBuild = item.m_CustomBuild;
    
    m_IsBundle = item.m_IsBundle;
    m_External = item.m_External;
    m_StyleObjcpp = item.m_StyleObjcpp;
    m_MkName = item.m_MkName;
    m_DataSource = item.m_DataSource;
}

string CProjItem::GetPath(void) const
{
    string path = CDirEntry::ConcatPath(m_SourcesBaseDir, "Makefile.");
    path += m_Name;
    switch (m_ProjType) {
    case CProjKey::eApp:
        path += ".app";
        break;
    case CProjKey::eLib:
        path += ".lib";
        break;
    case CProjKey::eDll:
        path += ".dll";
        break;
    case CProjKey::eMsvc:
        if (CMsvc7RegSettings::GetMsvcPlatform() != CMsvc7RegSettings::eUnix) {
            path += ".msvc";
        }
        break;
    case CProjKey::eDataSpec:
        path += ".dataspec";
        break;
    default:
        break;
    }
    return path;
}

bool CProjItem::HasDataspecDependency(void) const
{
    if ( !m_DatatoolSources.empty() ) {
        return true;
    }
    ITERATE( list<CProjKey>, d, m_Depends) {
        if (d->Type() == CProjKey::eDataSpec) {
            return true;
        }
    }
    return false;
}


END_NCBI_SCOPE

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
#include <app/project_tree_builder/proj_item.hpp>


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


CProjKey::CProjKey(const CProjKey& key)
:m_Type(key.m_Type),
 m_Id  (key.m_Id)
{
}


CProjKey& CProjKey::operator= (const CProjKey& key)
{
    if (this != &key) {
        m_Type = key.m_Type;
        m_Id   = key.m_Id;
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
                     EMakeFileType maketype)
   :m_Name    (name), 
    m_ID      (id),
    m_ProjType(type),
    m_SourcesBaseDir (sources_base),
    m_MsvcProjectMakefileDir(sources_base),
    m_Sources (sources), 
    m_Depends (depends),
    m_Requires(requires),
    m_Libs3Party (libs_3_party),
    m_IncludeDirs(include_dirs),
    m_Defines (defines),
    m_MakeType(maketype)
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
}


void CProjItem::SetFrom(const CProjItem& item)
{
    m_Name           = item.m_Name;
    m_ID		     = item.m_ID;
    m_ProjType       = item.m_ProjType;
    m_SourcesBaseDir = item.m_SourcesBaseDir;
    m_MsvcProjectMakefileDir = item.m_MsvcProjectMakefileDir;
    m_Sources        = item.m_Sources;
    m_Depends        = item.m_Depends;
    m_Requires       = item.m_Requires;
    m_Libs3Party     = item.m_Libs3Party;
    m_IncludeDirs    = item.m_IncludeDirs;
    m_DatatoolSources= item.m_DatatoolSources;
    m_Defines        = item.m_Defines;
    m_NcbiCLibs      = item.m_NcbiCLibs;
    m_MakeType     = item.m_MakeType;
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.27  2005/01/31 16:37:38  gouriano
 * Keep track of subproject types and propagate it down the project tree
 *
 * Revision 1.26  2005/01/10 15:40:09  gouriano
 * Make PTB pick up MSVC tune-up for DLLs
 *
 * Revision 1.25  2004/10/12 16:17:33  ivanov
 * Cosmetics
 *
 * Revision 1.24  2004/08/04 13:27:24  gouriano
 * Added processing of EXPENDABLE projects
 *
 * Revision 1.23  2004/05/21 21:41:41  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.22  2004/03/02 16:32:06  gorelenk
 * Implementation of classes CProjectItemsTree CCyclicDepends
 * SProjectTreeFolder CProjectTreeFolders moved to proj_tree.cpp
 * Implementation of classes SMakeProjectT SAppProjectT SLibProjectT
 * SAsnProjectT SAsnProjectSingleT SAsnProjectMultipleT and
 * CProjectTreeBuilder moved to proj_tree_builder.cpp.
 *
 * Revision 1.21  2004/03/01 17:59:35  gorelenk
 * Added implementation of class CCyclicDepends member-functions.
 *
 * Revision 1.20  2004/02/27 18:08:25  gorelenk
 * Changed implementation of CProjectTreeBuilder::ProcessDir.
 *
 * Revision 1.19  2004/02/26 21:29:04  gorelenk
 * Changed implementations of member-functions of class CProjectTreeBuilder:
 * BuildProjectTree, BuildOneProjectTree and ProcessDir because of use
 * IProjectFilter* filter instead of const string& subtree_to_build.
 *
 * Revision 1.18  2004/02/24 17:20:11  gorelenk
 * Added implementation of member-function CreateNcbiCToolkitLibs
 * of struct SAppProjectT.
 * Changed implementation of member-function DoCreate
 * of struct SAppProjectT.
 *
 * Revision 1.17  2004/02/23 20:58:41  gorelenk
 * Fixed double properties apperience in generated MSVC projects.
 *
 * Revision 1.16  2004/02/20 22:53:59  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.15  2004/02/18 23:35:40  gorelenk
 * Added special processing for NCBI_C_LIBS tag.
 *
 * Revision 1.14  2004/02/13 16:02:24  gorelenk
 * Modified procedure of depends resolve.
 *
 * Revision 1.13  2004/02/11 16:46:29  gorelenk
 * Cnanged LOG_POST message.
 *
 * Revision 1.12  2004/02/10 21:20:44  gorelenk
 * Added support of DATATOOL_SRC tag.
 *
 * Revision 1.11  2004/02/10 18:17:05  gorelenk
 * Added support of depends fine-tuning.
 *
 * Revision 1.10  2004/02/06 23:15:00  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.9  2004/02/04 23:58:29  gorelenk
 * Added support of include dirs and 3-rd party libs.
 *
 * Revision 1.8  2004/02/03 17:12:55  gorelenk
 * Changed implementation of classes CProjItem and CProjectItemsTree.
 *
 * Revision 1.7  2004/01/30 20:44:22  gorelenk
 * Initial revision.
 *
 * Revision 1.6  2004/01/28 17:55:50  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.5  2004/01/26 19:27:30  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.4  2004/01/22 17:57:55  gorelenk
 * first version
 *
 * ===========================================================================
 */

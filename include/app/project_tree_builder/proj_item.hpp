#ifndef PROJECT_TREE_BUILDER__PROJ_ITEM__HPP
#define PROJECT_TREE_BUILDER__PROJ_ITEM__HPP

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


#include <app/project_tree_builder/proj_datatool_generated_src.hpp>
#include <app/project_tree_builder/file_contents.hpp>

#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CProjKey --
///
/// Project key  abstraction.
///
/// Project key (type + project_id).

class CProjKey
{
public:
    typedef enum {
        eNoProj,
        eLib,
        eApp,
        eDll,
        eMsvc,
        eLast 
    } TProjType;

    CProjKey(void);
    CProjKey(TProjType type, const string& project_id);
    CProjKey(const CProjKey& key);
    CProjKey& operator= (const CProjKey& key);
    ~CProjKey(void);

    bool operator<  (const CProjKey& key) const;
    bool operator== (const CProjKey& key) const;
    bool operator!= (const CProjKey& key) const;

    TProjType     Type(void) const;
    const string& Id  (void) const;

private:
    TProjType m_Type;
    string    m_Id;

};

/////////////////////////////////////////////////////////////////////////////
///
/// CProjItem --
///
/// Project abstraction.
///
/// Representation of one project from the build tree.

class CProjItem
{
public:
    typedef CProjKey::TProjType TProjType;


    CProjItem(void);
    CProjItem(const CProjItem& item);
    CProjItem& operator= (const CProjItem& item);

    CProjItem(TProjType type,
              const string&         name,
              const string&         id,
              const string&         sources_base,
              const list<string>&   sources, 
              const list<CProjKey>& depends,
              const list<string>&   requires,
              const list<string>&   libs_3_party,
              const list<string>&   include_dirs,
              const list<string>&   defines,
              EMakeFileType maketype);
    
    ~CProjItem(void);

    /// Name of atomic project.
    string       m_Name;

    /// ID of atomic project.
    string       m_ID;

    /// Type of the project.
    TProjType    m_ProjType;

    /// Base directory of source files (....c++/src/a/ )
    string       m_SourcesBaseDir;

    /// Directory of the MsvcProjectMakefile
    string       m_MsvcProjectMakefileDir;

    /// List of source files without extension ( *.cpp or *.c ) -
    /// with relative pathes from m_SourcesBaseDir.
    list<string> m_Sources;
    
    /// What projects this project is depend upon (IDs).
    list<CProjKey> m_Depends;

    /// What this project requires to have (in user site).
    list<string> m_Requires;

    /// Resolved contents of LIBS flag (Third-party libs)
    list<string> m_Libs3Party;

    /// Resolved contents of CPPFLAG ( -I$(include)<m_IncludeDir> -I$(srcdir)/..)
    /// Absolute pathes
    list<string>  m_IncludeDirs;

    /// Source files *.asn , *.dtd to be processed by datatool app
    list<CDataToolGeneratedSrc> m_DatatoolSources;

    /// Defines like USE_MS_DBLIB
    list<string>  m_Defines;

    /// Libraries from NCBI C Toolkit to link with
    list<string>  m_NcbiCLibs;
    
    /// Type of the project
    EMakeFileType m_MakeType;


private:
    void Clear(void);
    void SetFrom(const CProjItem& item);
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.21  2005/01/31 16:38:00  gouriano
 * Keep track of subproject types and propagate it down the project tree
 *
 * Revision 1.20  2005/01/10 15:40:49  gouriano
 * Make PTB pick up MSVC tune-up for DLLs
 *
 * Revision 1.19  2004/08/04 13:24:58  gouriano
 * Added processing of EXPENDABLE projects
 *
 * Revision 1.18  2004/06/10 15:12:55  gorelenk
 * Added newline at the file end to avoid GCC warning.
 *
 * Revision 1.17  2004/05/10 19:46:22  gorelenk
 * + eMsvc in CProjKey.
 *
 * Revision 1.16  2004/03/18 17:41:03  gorelenk
 * Aligned classes member-functions parameters inside declarations.
 *
 * Revision 1.15  2004/03/10 16:45:39  gorelenk
 * Added eDll to enum TProjType.
 *
 * Revision 1.14  2004/03/02 16:33:12  gorelenk
 * Declarations of classes CProjectItemsTree CCyclicDepends
 * SProjectTreeFolder CProjectTreeFolders moved to proj_tree.hpp
 * Declarations of classes SMakeProjectT SAppProjectT SLibProjectT
 * SAsnProjectT SAsnProjectSingleT SAsnProjectMultipleT and
 * CProjectTreeBuilder moved to proj_tree_builder.hpp.
 *
 * Revision 1.13  2004/03/01 17:58:32  gorelenk
 * Added declaration of CCyclicDepends class.
 *
 * Revision 1.12  2004/02/26 21:24:03  gorelenk
 * Declaration member-functions of class CProjectTreeBuilder:
 * BuildProjectTree, BuildOneProjectTree and ProcessDir changed to use
 * IProjectFilter* filter instead of const string& subtree_to_build.
 *
 * Revision 1.11  2004/02/24 17:15:25  gorelenk
 * Added member m_NcbiCLibs to class CProjItem.
 * Added declaration of member function CreateNcbiCToolkitLibs
 * to struct SAppProjectT.
 *
 * Revision 1.10  2004/02/20 22:55:12  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.9  2004/02/06 23:15:40  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.8  2004/02/04 23:15:27  gorelenk
 * To class CProjItem added members m_Libs3Party and m_IncludeDirs.
 * Redesigned constructors of class CProjItem.
 *
 * Revision 1.7  2004/02/03 17:05:08  gorelenk
 * Added members to class CProjItem
 *
 * Revision 1.6  2004/01/30 20:42:21  gorelenk
 * Added support of ASN projects
 *
 * Revision 1.5  2004/01/26 19:25:42  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.4  2004/01/22 17:57:09  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif //PROJECT_TREE_BUILDER__PROJ_ITEM__HPP


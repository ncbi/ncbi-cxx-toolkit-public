#ifndef PROJECT_TREE_BUILDER__PROJ_TREE_BUILDER__HPP
#define PROJECT_TREE_BUILDER__PROJ_TREE_BUILDER__HPP

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

#include "proj_item.hpp"
#include "proj_tree.hpp"

#include <set>
#include "resolver.hpp"

#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE


// Traits classes - creation helpers for CProjectTreeBuilder:

/////////////////////////////////////////////////////////////////////////////
///
/// SMakeProjectT --
///
/// Base traits and policies.
///
/// Common traits and policies for all project types.

struct SMakeProjectT
{
    typedef CProjectItemsTree::TFiles TFiles;

    struct SMakeInInfo
    {
        typedef enum {
            eUnknown,
            eApp,
            eLib,
            eDll,
            eASN,
            eDTD,
            eXSD,
            eWSDL,
            eMsvc
        } TMakeinType;

        SMakeInInfo(TMakeinType          type,
                    const list<string>&  names,
                    EMakeFileType        maketype)
            :m_Type     (type),
             m_ProjNames(names),
             m_MakeType(maketype)
        {
        }
        
        TMakeinType   m_Type;
        list<string>  m_ProjNames;
        EMakeFileType m_MakeType;
    };

    static CProjItem::TProjType GetProjType(const string& base_dir,
                                            const string& projname,
                                            SMakeInInfo::TMakeinType type=SMakeInInfo::eUnknown);

    typedef list<SMakeInInfo> TMakeInInfoList;
    static void    AnalyzeMakeIn(const CSimpleMakeFileContents& makein_contents,
                                 TMakeInInfoList*               info);

    
    static string CreateMakeAppLibFileName(const string& base_dir,
                                           const string& projname,
                                           SMakeInInfo::TMakeinType type=SMakeInInfo::eUnknown);

    static void   CreateFullPathes        (const string&      dir, 
                                           const list<string> files,
                                           list<string>*      full_pathes);

    static string GetOneIncludeDir        (const string& flag, 
                                           const string& token);
    
    
    static void   CreateIncludeDirs       (const list<string>& cpp_flags,
                                           const string&       source_base_dir,
                                           list<string>*       include_dirs);

    static void   CreateDefines           (const list<string>& cpp_flags,
                                           list<string>*       defines);

    
    static void   Create3PartyLibs        (const list<string>& libs_flags, 
                                           list<string>*       libs_list,
                                           const string* mkname = NULL);

    static void   DoResolveDefs           (CSymResolver&              resolver,
                                           CProjectItemsTree::TFiles& files,
                                           const set<string>&         keys);

    static bool   IsMakeInFile            (const string& name);

    static bool   IsMakeLibFile           (const string& name);

    static bool   IsMakeDllFile           (const string& name);

    static bool   IsMakeAppFile           (const string& name);

    static bool   IsUserProjFile          (const string& name);

    static void   VerifyLibDepends       ( list<CProjKey>&  depends_ids,
                                           const string& mkname,
                                           list<string>& liborder);

    static void   ConvertLibDepends       (const list<string>& depends_libs, 
                                           list<CProjKey>*     depends_ids,
                                           const string* mkname = NULL);

    static void   ConvertLibDependsMacro  (const list<string>& depends, 
                                           list<string>& depends_libs);

    static bool   IsConfigurableDefine    (const string& define);
    static string StripConfigurableDefine (const string& define);

    static bool   HasConfigurableDefine    (const string& define);
    static string ExtractConfigurableDefine (const string& define);
};



/////////////////////////////////////////////////////////////////////////////
///
/// SAppProjectT --
///
/// APP_PROJ traits and policies.
///
/// Traits and policies specific for APP_PROJ.

struct SAppProjectT : public SMakeProjectT
{
    static void CreateNcbiCToolkitLibs(const CSimpleMakeFileContents& makefile,
                                       list<string>*                  libs_list);

    static CProjKey DoCreate(const string&      source_base_dir,
                             const string&      proj_name,
                             const string&      applib_mfilepath,
                             const TFiles&      makeapp, 
                             CProjectItemsTree* tree,
                             EMakeFileType maketype);
};


/////////////////////////////////////////////////////////////////////////////
///
/// SLibProjectT --
///
/// LIB_PROJ traits and policies.
///
/// Traits and policies specific for LIB_PROJ.

struct SLibProjectT : public SMakeProjectT
{
    static CProjKey DoCreate(const string&      source_base_dir,
                             const string&      proj_name,
                             const string&      applib_mfilepath,
                             const TFiles&      makeapp, 
                             CProjectItemsTree* tree,
                             EMakeFileType maketype);

    static CProjKey DoCreateDataSpec(
                            const string& source_base_dir,
                            const string& proj_name,
                            const string& proj_id,
                            CProjectItemsTree* tree,
                            EMakeFileType maketype);
};

struct SDllProjectT : public SMakeProjectT
{
    static CProjKey DoCreate(const string&      source_base_dir,
                             const string&      proj_name,
                             const string&      applib_mfilepath,
                             const TFiles&      makeapp, 
                             CProjectItemsTree* tree,
                             EMakeFileType maketype);
};

CProjItem CreateUtilityProjectItem( const string& prj_dir, const string& name);

/////////////////////////////////////////////////////////////////////////////
///
/// SAsnProjectT --
///
/// Base traits and policies for project with datatool-generated source files.
///
/// Common traits and policies for projects with datatool-generated sources.

struct SAsnProjectT : public SMakeProjectT
{
    typedef CProjectItemsTree::TProjects TProjects;

    static CProjKey DoCreate(const string&      source_base_dir,
                             const string&      proj_name,
                             const string&      applib_mfilepath,
                             const TFiles&      makeapp, 
                             const TFiles&      makelib, 
                             CProjectItemsTree* tree,
                             const SMakeProjectT::SMakeInInfo& makeinfo);
    
    enum TAsnType {
        eNoAsn,
        eSingle,
        eMultiple
    };

    static TAsnType GetAsnProjectType(const string& applib_mfilepath,
                                      const TFiles& makeapp,
                                      const TFiles& makelib);
};


/////////////////////////////////////////////////////////////////////////////
///
/// SAsnProjectSingleT --
///
/// Traits and policies for project one ASN/DTD file.
///
/// Traits and policies specific for project with one ASN/DTD file.

struct SAsnProjectSingleT : public SAsnProjectT
{
    static CProjKey DoCreate(const string&      source_base_dir,
                             const string&      proj_name,
                             const string&      applib_mfilepath,
                             const TFiles&      makeapp, 
                             const TFiles&      makelib, 
                             CProjectItemsTree* tree,
                             const SMakeProjectT::SMakeInInfo& makeinfo);
};


/////////////////////////////////////////////////////////////////////////////
///
/// SAsnProjectMultipleT --
///
/// Traits and policies for project multiple ASN/DTD files.
///
/// Traits and policies specific for project with several ASN/DTD files.

struct SAsnProjectMultipleT : public SAsnProjectT
{
    static CProjKey DoCreate(const string&      source_base_dir,
                             const string&      proj_name,
                             const string&      applib_mfilepath,
                             const TFiles&      makeapp, 
                             const TFiles&      makelib, 
                             CProjectItemsTree* tree,
                             const SMakeProjectT::SMakeInInfo& makeinfo);
};


/////////////////////////////////////////////////////////////////////////////
///
/// SUserProjectT --
///
/// Traits and policies for user project makefiles
///
/// Traits and policies specific for user-generated projects

struct SMsvcProjectT : public SMakeProjectT
{
    static CProjKey DoCreate(const string&      source_base_dir,
                             const string&      proj_name,
                             const string&      applib_mfilepath,
                             const TFiles&      makemsvc, 
                             CProjectItemsTree* tree,
                             EMakeFileType maketype);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CProjectTreeBuilder --
///
/// Builder class for project tree.
///
/// Builds tree, resolvs macrodefines and adds dependents projects.

class CProjectTreeBuilder
{
public:
    typedef map<string, CSimpleMakeFileContents> TFiles;

    //              IN      LIB     APP
//    typedef STriple<TFiles, TFiles, TFiles> TMakeFiles;
    
    struct SMakeFiles
    {
        TFiles m_In;
        TFiles m_Lib;
        TFiles m_Dll;
        TFiles m_App;
        TFiles m_User;
    };


    /// Build project tree and include all projects this tree depends upon
    static void BuildProjectTree(const IProjectFilter* filter,
                                 const string&         root_src_path,
                                 CProjectItemsTree*    tree  );
    static bool VerifyBuildOrder(const CProjItem& item,
                                 list<string> dependencies,
                                 const CProjectItemsTree&  tree);
private:
    /// Build one project tree and do not resolve (include) depends
    static void BuildOneProjectTree(const IProjectFilter* filter,
                                    const string&         root_src_path,
                                    CProjectItemsTree*    tree  );
    
    static void ProcessDir (const string&         dir_name, 
                            bool                  is_root,
                            const IProjectFilter* filter,
                            SMakeFiles*           makefiles,
                            EMakeFileType         maketype,
                            const CSimpleMakeFileContents* parent);

    static void ProcessMakeInFile  (const string& file_name, 
                                    SMakeFiles*   makefiles,
                                    EMakeFileType type,
                                    const CSimpleMakeFileContents* parent);

    static void ProcessMakeLibFile (const string& file_name, 
                                    SMakeFiles*   makefiles,
                                    EMakeFileType type,
                                    const CSimpleMakeFileContents* parent);

    static void ProcessMakeDllFile (const string& file_name, 
                                    SMakeFiles*   makefiles,
                                    EMakeFileType type,
                                    const CSimpleMakeFileContents* parent);

    static void ProcessMakeAppFile (const string& file_name, 
                                    SMakeFiles*   makefiles,
                                    EMakeFileType type,
                                    const CSimpleMakeFileContents* parent);

    static void ProcessUserProjFile(const string& file_name, 
                                    SMakeFiles*   makefiles,
                                    EMakeFileType type,
                                    const CSimpleMakeFileContents* parent);

    static void UpdateGraphDep( TFiles files);
    static void ResolveDefs(CSymResolver& resolver, SMakeFiles& makefiles);

    
    static void AddDatatoolSourcesDepends(CProjectItemsTree* tree);

};


END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER__PROJ_TREE_BUILDER__HPP

#ifndef PROJECT_TREE_BULDER__PROJ_BUILDER_APP__HPP
#define PROJECT_TREE_BULDER__PROJ_BUILDER_APP__HPP

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

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <app/project_tree_builder/proj_utils.hpp>
#include <app/project_tree_builder/file_contents.hpp>
#include <app/project_tree_builder/resolver.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/msvc_site.hpp>
#include <app/project_tree_builder/msvc_makefile.hpp>
#include <app/project_tree_builder/msvc_dlls_info.hpp>


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// CProjBulderApp --
///
/// Abstraction of proj_tree_builder application.
///
/// Singleton implementation.

class CProjBulderApp : public CNcbiApplication
{
public:
    

    /// ShortCut for enums
    int EnumOpt(const string& enum_name, const string& enum_val) const;

    /// Singleton
    friend CProjBulderApp& GetApp(void);


private:
    CProjBulderApp(void);

    virtual void Init(void);
    virtual int  Run (void);
    virtual void Exit(void);

    /// Parse program arguments.
    void ParseArguments(void);
    void VerifyArguments(void);

    //TFiles m_FilesMakeIn;
    //TFiles m_FilesMakeLib;
    //TFiles m_FilesMakeApp;

    typedef map<string, CSimpleMakeFileContents> TFiles;
    void DumpFiles(const TFiles& files, const string& filename) const;
    
    auto_ptr<CMsvc7RegSettings> m_MsvcRegSettings;
    auto_ptr<CMsvcSite>         m_MsvcSite;
    auto_ptr<CMsvcMetaMakefile> m_MsvcMetaMakefile;

    auto_ptr<SProjectTreeInfo>  m_ProjectTreeInfo;

    auto_ptr<CBuildType>        m_BuildType;

//    auto_ptr<CMsvcDllsInfo>     m_DllsInfo;

    auto_ptr<CProjectItemsTree> m_WholeTree;

    auto_ptr<CDllSrcFilesDistr> m_DllSrcFilesDistr;

    const CProjectItemsTree*    m_CurrentBuildTree;

    set<string>  m_ProjectTags;
    set<string>  m_AllowedTags;
    set<string>  m_DisallowedTags;
    bool m_ScanningWholeTree;

public:

    string m_Root;
    string m_Subtree;
    string m_Solution;
    string m_StatusDir;
    bool   m_Dll;
    bool m_BuildPtb;
    bool m_AddMissingLibs;
    bool m_ScanWholeTree;
    bool m_TweakVTuneR;
    bool m_TweakVTuneD;
    string m_BuildRoot;
    string m_ProjTags;
    bool m_ConfirmCfg;

public:

    void GetMetaDataFiles(list<string>* files) const;


    const CMsvc7RegSettings& GetRegSettings    (void);
    
    const CMsvcSite&         GetSite           (void);

    const CMsvcMetaMakefile& GetMetaMakefile   (void);

    const SProjectTreeInfo&  GetProjectTreeInfo(void);

    const CBuildType&        GetBuildType      (void);

    const string&            GetBuildRoot      (void) const
    {
        return m_BuildRoot;
    }
    const CProjectItemsTree& GetWholeTree      (void);
    const CProjectItemsTree* GetCurrentBuildTree(void) const
    {
        return m_CurrentBuildTree;
    }

    CDllSrcFilesDistr&       GetDllFilesDistr  (void);

    string GetDatatoolId          (void) const;
    string GetDatatoolPathForApp  (void) const;
    string GetDatatoolPathForLib  (void) const;
    string GetDatatoolCommandLine (void) const;

    string GetProjectTreeRoot(void) const;
    bool   IsAllowedProjectTag(const CSimpleMakeFileContents& mk,
                               string& unmet) const;
    void   LoadProjectTags(const string& filename);
    string ProcessLocationMacros(string data);
    bool IsScanningWholeTree(void) const {return m_ScanningWholeTree;}
    
private:
    void    GetBuildConfigs     (list<SConfigInfo>* configs);
    void    GenerateMsvcProjects(CProjectItemsTree& projects_tree);
    void    GenerateUnixProjects(CProjectItemsTree& projects_tree);
    void    CreateFeaturesAndPackagesFiles(const list<SConfigInfo>* configs);
    bool    ConfirmConfiguration(void);
};


/// access to App singleton

CProjBulderApp& GetApp(void);


/////////////////////////////////////////////////////////////////////////////
///
/// CProjBulderAppException --
///
/// Exception class.
///
/// proj_tree_builder specific exceptions.

class CProjBulderAppException : public CException
{
public:
    enum EErrCode {
        eFileCreation,
        eEnumValue,
        eFileOpen,
        eProjectType,
        eBuildConfiguration,
        eMetaMakefile,
        eConfigureDefinesPath,
        eUnknownProjectTag
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eFileCreation:
            return "Can not create file";
        case eEnumValue:
            return "Bad or missing enum value";
        case eFileOpen:
            return "Can not open file";
        case eProjectType:
            return "Unknown project type";
        case eBuildConfiguration:
            return "Unknown build configuration";
        case eMetaMakefile:
            return "Bad or missed metamakefile";
        case eConfigureDefinesPath:
            return "Path to configure defines file is empty";
        case eUnknownProjectTag:
            return "Unregistered project tag";
        default:
            return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CProjBulderAppException, CException);
};


END_NCBI_SCOPE

#endif //PROJECT_TREE_BULDER__PROJ_BUILDER_APP__HPP

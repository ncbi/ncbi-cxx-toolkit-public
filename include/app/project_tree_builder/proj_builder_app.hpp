#ifndef PROJ_BUILDER_APP_HEADER
#define PROJ_BUILDER_APP_HEADER

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
#include <app/project_tree_builder/file_contents.hpp>
#include <app/project_tree_builder/resolver.hpp>
#include <app/project_tree_builder/proj_utils.hpp>


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
    friend CProjBulderApp& GetApp();


private:
    CProjBulderApp(void);

    virtual void Init(void);
    virtual int  Run (void);
    virtual void Exit(void);

    /// Parse program arguments.
    void ParseArguments(void);



    /// Root dir of building tree.
    string m_Root;

    /// src child dir of Root.
    string m_RootSrc;

    /// Subtree to buil (default is m_RootSrc).
    string m_SubTree;

    /// Solution to build.
    string m_Solution;


    //TFiles m_FilesMakeIn;
    //TFiles m_FilesMakeLib;
    //TFiles m_FilesMakeApp;

    typedef map<string, CSimpleMakeFileContents> TFiles;
    void DumpFiles(const TFiles& files, const string& filename) const;

public:

    void GetMetaDataFiles (list<string>* files)   const;
    void GetBuildConfigs  (list<string>* configs) const;
};


/// access to App singleton

CProjBulderApp& GetApp();


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
        eBuildConfiguration
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
        default:
            return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CProjBulderAppException, CException);
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/01/22 17:57:09  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif

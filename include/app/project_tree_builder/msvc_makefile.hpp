#ifndef MSVC_MAKEFILE_HEADER
#define MSVC_MAKEFILE_HEADER
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
#include <corelib/ncbireg.hpp>
#include <app/project_tree_builder/proj_item.hpp>

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CMsvcMetaMakefile --
///
/// Abstraction of global settings for building of C++ projects.
///
/// Provides information about default compiler, linker and librarian 
/// settinngs.

class CMsvcMetaMakefile
{
public:
    CMsvcMetaMakefile(const string& file_path);
    
    bool Empty(void) const;

    string GetCompilerOpt (const string& opt, bool debug) const;
    string GetLinkerOpt   (const string& opt, bool debug) const;
    string GetLibrarianOpt(const string& opt, bool debug) const;

protected:
    CNcbiRegistry m_MakeFile;

private:
     //Prohibited to
    CMsvcMetaMakefile(void);
    CMsvcMetaMakefile(const CMsvcMetaMakefile&);
    CMsvcMetaMakefile& operator= (const CMsvcMetaMakefile&);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CMsvcProjectMakefile --
///
/// Abstraction of project MSVC specific settings
///
/// Provides information about project include/exclude to build, 
/// additional/excluded files and project specific compiler, linker 
/// and librarian settinngs.

class CMsvcProjectMakefile : public CMsvcMetaMakefile
{
public:
    CMsvcProjectMakefile(const string& file_path);

    bool IsExcludeProject(bool default_val) const;

    void GetAdditionalSourceFiles(bool debug, list<string>* files) const;
    void GetExcludedSourceFiles  (bool debug, list<string>* files) const;

private:
    
    
    //Prohibited to
    CMsvcProjectMakefile(void);
    CMsvcProjectMakefile(const CMsvcProjectMakefile&);
    CMsvcProjectMakefile& operator= (const CMsvcProjectMakefile&);
};

/// Create project makefile name
string CreateMsvcProjectMakefileName(const CProjItem& project);


/// Get option with taking into account 2 makefiles : matafile and project_file

/// Compiler
string GetCompilerOpt(const CMsvcMetaMakefile&    meta_file, 
                      const CMsvcProjectMakefile& project_file,
                      const string&               opt,
                      bool                        debug);

/// Linker
string GetLinkerOpt  (const CMsvcMetaMakefile&    meta_file, 
                      const CMsvcProjectMakefile& project_file,
                      const string&               opt,
                      bool                        debug);

/// Librarian
string GetLibrarianOpt(const CMsvcMetaMakefile&    meta_file, 
                       const CMsvcProjectMakefile& project_file,
                       const string&               opt,
                       bool                        debug);


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/01/26 19:25:40  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * ===========================================================================
 */

#endif // MSVC_MAKEFILE_HEADER
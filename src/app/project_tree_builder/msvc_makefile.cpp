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

#include <app/project_tree_builder/stl_msvc_usage.hpp>
#include <app/project_tree_builder/msvc_makefile.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>

#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE


CMsvcMetaMakefile::CMsvcMetaMakefile(const string& file_path)
{
    CNcbiIfstream ifs(file_path.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if (ifs)
        m_MakeFile.Read(ifs);
}


bool CMsvcMetaMakefile::Empty(void) const
{
    return m_MakeFile.Empty();
}


static string s_GetOpt(const CNcbiRegistry& makefile, 
                       const string& section, 
                       const string& opt, 
                       bool debug)
{
    string section_spec = section + (debug? ".Debug" : ".Release");
    string val_spec = makefile.GetString(section_spec, opt, "");
    if ( !val_spec.empty() )
        return val_spec;

    return makefile.GetString(section, opt, "");
}


string CreateMsvcProjectMakefileName(const CProjItem& project)
{
    string name("Makefile.");
    
    name += project.m_Name + '.';
    
    switch (project.m_ProjType) {
    case CProjItem::eApp:
        name += "app.";
        break;
    case CProjItem::eLib:
        name += "lib.";
        break;
    default:
        NCBI_THROW(CProjBulderAppException, 
                   eProjectType, 
                   NStr::IntToString(project.m_ProjType));
        break;
    }

    name += "msvc";
    return name;
}


string CMsvcMetaMakefile::GetCompilerOpt(const string& opt, bool debug) const
{
    return s_GetOpt(m_MakeFile, "Compiler", opt, debug);
}


string CMsvcMetaMakefile::GetLinkerOpt(const string& opt, bool debug) const
{
    return s_GetOpt(m_MakeFile, "Linker", opt, debug);
}


string CMsvcMetaMakefile::GetLibrarianOpt(const string& opt, bool debug) const
{
    return s_GetOpt(m_MakeFile, "Librarian", opt, debug);
}


CMsvcProjectMakefile::CMsvcProjectMakefile(const string& file_path)
    :CMsvcMetaMakefile(file_path)
{
}


bool CMsvcProjectMakefile::IsExcludeProject(bool default_val) const
{
    string val = m_MakeFile.GetString("Common", "ExcludeProject", "");

    if ( val.empty() )
        return default_val;

    return val == "FALSE";
}


void CMsvcProjectMakefile::GetAdditionalSourceFiles(bool debug, 
                                                    list<string>* files) const
{
    string files_string = 
        s_GetOpt(m_MakeFile, "AddToProject", "SourceFiles", debug);
    
    NStr::Split(files_string, " \t,", *files);
}


void CMsvcProjectMakefile::GetExcludedSourceFiles(bool debug, 
                                                  list<string>* files) const
{
    string files_string = 
        s_GetOpt(m_MakeFile, "ExcludedFromProject", "SourceFiles", debug);
    
    NStr::Split(files_string, " \t,", *files);
}


string GetCompilerOpt(const CMsvcMetaMakefile&    meta_file, 
                      const CMsvcProjectMakefile& project_file,
                      const string&               opt,
                      bool                        debug)
{
   string val = project_file.GetCompilerOpt(opt, debug);
   if ( !val.empty() )
       return val;
   
   return meta_file.GetCompilerOpt(opt, debug);
}


string GetLinkerOpt(const CMsvcMetaMakefile&    meta_file, 
                    const CMsvcProjectMakefile& project_file,
                    const string&               opt,
                    bool                        debug)
{
   string val = project_file.GetLinkerOpt(opt, debug);
   if ( !val.empty() )
       return val;
   
   return meta_file.GetLinkerOpt(opt, debug);
}


string GetLibrarianOpt(const CMsvcMetaMakefile&    meta_file, 
                       const CMsvcProjectMakefile& project_file,
                       const string&               opt,
                       bool                        debug)
{
   string val = project_file.GetLibrarianOpt(opt, debug);
   if ( !val.empty() )
       return val;
   
   return meta_file.GetLibrarianOpt(opt, debug);
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/01/26 19:27:28  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * ===========================================================================
 */

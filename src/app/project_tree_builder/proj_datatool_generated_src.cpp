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


BEGIN_NCBI_SCOPE


CDataToolGeneratedSrc::CDataToolGeneratedSrc(void)
{
}


CDataToolGeneratedSrc::CDataToolGeneratedSrc(const string& source_file_path)
{
    LoadFrom(source_file_path, this);
}


CDataToolGeneratedSrc::CDataToolGeneratedSrc(const CDataToolGeneratedSrc& src)
{
    SetFrom(src);
}


CDataToolGeneratedSrc& 
CDataToolGeneratedSrc::operator= (const CDataToolGeneratedSrc& src)
{
    if(this != &src)
    {
        SetFrom(src);
    }
    return *this;
}


CDataToolGeneratedSrc::~CDataToolGeneratedSrc(void)
{
}


void CDataToolGeneratedSrc::LoadFrom(const string&          source_file_path,
                                     CDataToolGeneratedSrc* src)
{
    src->Clear();

    string dir;
    string base;
    string ext;
    CDirEntry::SplitPath(source_file_path, &dir, &base, &ext);

    src->m_SourceBaseDir = dir;
    src->m_SourceFile    = base + ext;

    CSimpleMakeFileContents fc(CDirEntry::ConcatPath(dir, base + ".module"));
    
    CSimpleMakeFileContents::TContents::const_iterator p = 
        fc.m_Contents.find("MODULE_IMPORT");
    if (p != fc.m_Contents.end()) {
        const list<string>& modules = p->second;
        ITERATE(list<string>, p, modules) {
            // add ext from source file to all modules to import
            const string& module = *p;
            src->m_ImportModules.push_back(module + ext);
        }
    }
}


bool CDataToolGeneratedSrc::IsEmpty(void) const
{
    return m_SourceFile.empty();
}


void CDataToolGeneratedSrc::Clear(void)
{
    m_SourceFile.erase();
    m_SourceBaseDir.erase();
    m_ImportModules.clear();
}


void CDataToolGeneratedSrc::SetFrom(const CDataToolGeneratedSrc& src)
{
    m_SourceFile     = src.m_SourceFile;
    m_SourceBaseDir  = src.m_SourceBaseDir;
    m_ImportModules        = src.m_ImportModules;
}




END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/01/30 20:44:22  gorelenk
 * Initial revision.
 *
 * ===========================================================================
 */

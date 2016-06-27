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
 * Authors:  Andrei Gourianov
 *
 * File Description:
 *   Sample library
 *
 */

#include <ncbi_pch.hpp>
#include <sample/lib/basic/sample_lib_basic.hpp>


BEGIN_NCBI_SCOPE


CSampleLibraryObject::CSampleLibraryObject()
{
#if defined(NCBI_OS_UNIX)
    m_EnvPath = "PATH";
    m_EnvSeparator = ":";
#else
    m_EnvPath = "Path";
    m_EnvSeparator = ";";
#endif
}


bool CSampleLibraryObject::FindInPath(list<string>& found, const string& mask)
{
    // Get PATH
    const string path =
        CNcbiApplication::Instance()->GetEnvironment().Get(m_EnvPath);

    // Get PATH folders
    list<string> folders;
    NStr::Split(path, m_EnvSeparator, folders, NStr::fSplit_Tokenize);

    // Find file(s)
    for (auto f : folders) {
        CDir::TEntries entries = CDir(f).GetEntries(mask);
        for (auto e : entries) {
            found.push_back( e->GetPath() );
        }
    }

    return !found.empty();
}


CSampleLibraryObject::~CSampleLibraryObject()
{
}



END_NCBI_SCOPE


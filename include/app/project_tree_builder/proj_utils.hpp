#ifndef PROJ_UTILS_01152004_HEADER
#define PROJ_UTILS_01152004_HEADER

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


/// Utilits for Project Tree Builder:


#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE

/// Key Value struct
struct SKeyValue
{
    string m_Key;
    string m_Value;
};


/// Abstraction of project tree general information
struct SProjectTreeInfo
{
    /// Root of the project tree
    string m_Root;

    /// Subtree to buil (default is m_Src).
    string m_SubTree;

    /// Branch of tree to be implicit exclude from build
    list<string> m_ImplicitExcludedAbsDirs;

    /// <include> branch of tree
    string m_Include;

    /// <src> branch of tree
    string m_Src;

    /// <compilers> branch of tree
    string m_Compilers;
};

// Get parent directory
string ParentDir (const string& dir_abs);

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2004/02/11 15:39:53  gorelenk
 * Added support for multiple implicit excludes from source tree.
 *
 * Revision 1.7  2004/02/06 23:15:40  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.6  2004/02/04 23:13:24  gorelenk
 * struct SProjectTreeInfo redesigned.
 *
 * Revision 1.5  2004/02/03 17:03:39  gorelenk
 * Added members to struct SProjectTreeInfo
 *
 * Revision 1.4  2004/01/28 17:55:07  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.3  2004/01/22 17:57:09  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif //PROJ_UTILS_01152004_HEADER
#ifndef PROJECT_TREE_BUILDER__MSVC_PRJ_DEFINES__HPP
#define PROJECT_TREE_BUILDER__MSVC_PRJ_DEFINES__HPP

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



/// MSVC 7.10 project defines
#define MSVC_PROJECT_FILE_EXT   ".vcproj"

#define MSVC_PROJECT_PROJECT_TYPE   "Visual C++"
#define MSVC_PROJECT_KEYWORD_WIN32  "Win32Proj"

/// _MasterProject defines
#define MSVC_MASTERPROJECT_ROOT_NAMESPACE "MasterProject"
#define MSVC_MASTERPROJECT_KEYWORD        "ManagedCProj"


/// MSVC solution defines
#define MSVC_SOLUTION_ROOT_GUID     "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"

#define MSVC_DEFAULT_LIBS_TAG       "DefaultLibs"

/// Separator for list values in registry
#define LIST_SEPARATOR      " \t\n\r,;"
#define LIST_SEPARATOR_LIBS " \t\n\r"


#define MSVC_REG_SECTION            "msvc"
#define XCODE_REG_SECTION           "xcode"
#define UNIX_REG_SECTION            "unix"
#define MSVC_SOLUTION_HEADER_LINE   \
                "Microsoft Visual Studio Solution File, Format Version "

#define MSVC_CONFIGNAME    "$(ConfigurationName)"
#define XCODE_CONFIGNAME   "$(CONFIGURATION)"

#endif //PROJECT_TREE_BUILDER__MSVC_PRJ_DEFINES__HPP

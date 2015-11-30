#ifndef PROJECT_TREE_BUILDER__MSVC_DLLS_INDO_UTILS__HPP
#define PROJECT_TREE_BUILDER__MSVC_DLLS_INDO_UTILS__HPP

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
#include "msvc_prj_defines.hpp"
#include <corelib/ncbistr.hpp>

#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE


inline void GetDllsList   (const CPtbRegistry& registry, 
                           list<string>*        dlls_ids)
{
    dlls_ids->clear();

    string dlls_ids_str = 
        registry.GetString("DllBuild", "DLLs");
    
    NStr::Split(dlls_ids_str, LIST_SEPARATOR, *dlls_ids, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
}



inline void GetHostedLibs (const CPtbRegistry& registry,
                           const string&        dll_id,
                           list<string>*        lib_ids)
{
    string hosting_str = registry.GetString(dll_id, "Hosting");
    NStr::Split(hosting_str, LIST_SEPARATOR, *lib_ids, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

}



END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER__MSVC_DLLS_INDO_UTILS__HPP

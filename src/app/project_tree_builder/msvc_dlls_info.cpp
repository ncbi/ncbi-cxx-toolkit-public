
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

#include <app/project_tree_builder/msvc_dlls_info.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>
#include <corelib/ncbistre.hpp>

BEGIN_NCBI_SCOPE


CMsvcDllsInfo::CMsvcDllsInfo(const string& file_path)
{
    CNcbiIfstream ifs(file_path.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if ( !ifs )
        NCBI_THROW(CProjBulderAppException, eFileOpen, file_path);

    m_Registry.Read(ifs);
}


void CMsvcDllsInfo::GetDllsList(list<string>* dlls_ids) const
{
    dlls_ids->clear();

    string dlls_ids_str = 
        m_Registry.GetString("DllBuild", "DLLs", "");
    
    NStr::Split(dlls_ids_str, LIST_SEPARATOR, *dlls_ids);
}


bool CMsvcDllsInfo::SDllInfo::IsEmpty(void) const
{
    return  m_Hosting.empty()        &&
            m_ExportDefines.empty()  &&
            m_Depends.empty();
}
 
       
void CMsvcDllsInfo::SDllInfo::Clear(void)
{
    m_Hosting.clear();
    m_ExportDefines.clear();
    m_Depends.clear();
}


void CMsvcDllsInfo::GelDllInfo(const string& dll_id, SDllInfo* dll_info) const
{
    dll_info->Clear();

    string hosting_str = m_Registry.GetString(dll_id, "Hosting", "");
    NStr::Split(hosting_str, LIST_SEPARATOR, dll_info->m_Hosting);

    string export_defines_str = 
        m_Registry.GetString(dll_id, "ExportDefines", "");
    NStr::Split(export_defines_str, LIST_SEPARATOR, dll_info->m_ExportDefines);

    string depends_str = m_Registry.GetString(dll_id, "Dependencies", "");
    NStr::Split(depends_str, LIST_SEPARATOR, dll_info->m_Depends);
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/03/03 22:16:45  gorelenk
 * Added implementation of class CMsvcDllsInfo.
 *
 * Revision 1.1  2004/03/03 17:07:14  gorelenk
 * Initial revision.
 *
 * ===========================================================================
 */

#ifndef WRITER_INTERFACE__HPP_INCLUDED
#define WRITER_INTERFACE__HPP_INCLUDED
/* */

/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko
*
*  File Description: Base data writer interface
*
*/

#include <corelib/plugin_manager.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CWriter;
class CGB_Writer_PluginManager_DllResolver : public CPluginManager_DllResolver
{
public:
    CGB_Writer_PluginManager_DllResolver
    (const string&       interface_name,
     const string&       driver_name = kEmptyStr,
     const CVersionInfo& version     = CVersionInfo::kAny,
     CDll::EAutoUnload unload_dll = CDll::eNoAutoUnload)
        : CPluginManager_DllResolver(interface_name,
                                     driver_name,
                                     version,
                                     unload_dll)
        {
        }

    virtual
    string GetDllName(const string&       /*interface_name*/,
                      const string&       driver_name  = kEmptyStr,
                      const CVersionInfo& version      = CVersionInfo::kAny)
        const
        {
            return CPluginManager_DllResolver::GetDllName("xreader",
                                                          driver_name,
                                                          version);
        }
    virtual
    string GetDllNameMask(const string&       /*interface_name*/,
                          const string&       driver_name = kEmptyStr,
                          const CVersionInfo& version     = CVersionInfo::kAny,
                          EVersionLocation    ver_lct     = eBeforeSuffix)
        const
        {
            return CPluginManager_DllResolver::GetDllNameMask("xreader",
                                                              driver_name,
                                                              version,
                                                              ver_lct);
        }
};

END_SCOPE(objects)

NCBI_DECLARE_INTERFACE_VERSION(objects::CWriter,  "xwriter", 4, 9, 0);

template<>
class CDllResolver_Getter<objects::CWriter>
{
public:
    CPluginManager_DllResolver* operator()(void)
    {
        CPluginManager_DllResolver* resolver =
            new objects::CGB_Writer_PluginManager_DllResolver
            ("xwriter",
             kEmptyStr,
             CVersionInfo::kAny,
             CDll::eAutoUnload);
        resolver->SetDllNamePrefix("ncbi");
        return resolver;
    }
};

END_NCBI_SCOPE

#endif//WRITER_INTERFACE__HPP_INCLUDED

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
* Author: Eugene Vasilchenko
*
* File Description:
*   Base class for module sets
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2004/05/17 21:03:14  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.5  2000/08/25 15:59:22  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.4  2000/04/07 19:26:28  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.3  2000/02/01 21:48:02  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.2  1999/12/28 18:55:58  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.1  1999/12/21 17:18:35  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/datatool/mcontainer.hpp>
#include <serial/datatool/namespace.hpp>

BEGIN_NCBI_SCOPE

CModuleContainer::CModuleContainer(void)
    : m_Parent(0)
{
}

CModuleContainer::~CModuleContainer(void)
{
}

void CModuleContainer::SetModuleContainer(const CModuleContainer* parent)
{
    _ASSERT(m_Parent == 0 && parent != 0);
    m_Parent = parent;
}

const CModuleContainer& CModuleContainer::GetModuleContainer(void) const
{
    _ASSERT(m_Parent != 0);
    return *m_Parent;
}

const CNcbiRegistry& CModuleContainer::GetConfig(void) const
{
    return GetModuleContainer().GetConfig();
}

const string& CModuleContainer::GetSourceFileName(void) const
{
    return GetModuleContainer().GetSourceFileName();
}

string CModuleContainer::GetFileNamePrefix(void) const
{
    return GetModuleContainer().GetFileNamePrefix();
}

EFileNamePrefixSource CModuleContainer::GetFileNamePrefixSource(void) const
{
    return GetModuleContainer().GetFileNamePrefixSource();
}

CDataType* CModuleContainer::InternalResolve(const string& module,
                                             const string& type) const
{
    return GetModuleContainer().InternalResolve(module, type);
}

const CNamespace& CModuleContainer::GetNamespace(void) const
{
    return GetModuleContainer().GetNamespace();
}

string CModuleContainer::GetNamespaceRef(const CNamespace& ns) const
{
    return m_Parent?
        GetModuleContainer().GetNamespaceRef(ns):
        GetNamespace().GetNamespaceRef(ns);
}

END_NCBI_SCOPE

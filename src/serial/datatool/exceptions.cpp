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
*   datatool exceptions
*
*/

#include <ncbi_pch.hpp>
#include "exceptions.hpp"
#include "type.hpp"
#include <corelib/ncbiutil.hpp>

BEGIN_NCBI_SCOPE

CResolvedTypeSet::CResolvedTypeSet(const string& name)
    : m_Name(name)
{
}

CResolvedTypeSet::CResolvedTypeSet(const string& module, const string& name)
    : m_Module(module), m_Name(name)
{
}

CResolvedTypeSet::~CResolvedTypeSet(void)
{
}

void CResolvedTypeSet::Add(CDataType* type)
{
    m_Types.push_back(type);
}

void CResolvedTypeSet::Add(const CAmbiguiousTypes& types)
{
    ITERATE ( list<CDataType*>, i, types.GetTypes() ) {
        m_Types.push_back(*i);
    }
}

CDataType* CResolvedTypeSet::GetType(void) const THROWS((CDatatoolException))
{
    if ( m_Types.empty() ) {
        string msg = "type not found: ";
        if ( !m_Module.empty() ) {
            msg += m_Module;
            msg += '.';
        }
        NCBI_THROW(CNotFoundException,eType,msg+m_Name);
    }

    {
        list<CDataType*>::const_iterator i = m_Types.begin();
        CDataType* type = *i;
        ++i;
        if ( i == m_Types.end() )
            return type;
    }
    string msg = "ambiguous types: ";
    if ( !m_Module.empty() ) {
        msg += m_Module;
        msg += '.';
    }
    msg += m_Name;
    msg += " defined in:";
    ITERATE ( list<CDataType*>, i, m_Types ) {
        msg += ' ';
        msg += (*i)->GetSourceFileName();
        msg += ':';
        msg += NStr::IntToString((*i)->GetSourceLine());
    }
    NCBI_THROW2(CAmbiguiousTypes,eAmbiguious,msg, m_Types);
}

END_NCBI_SCOPE

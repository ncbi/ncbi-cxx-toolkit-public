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
*   Unified interface to application environment
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1999/05/04 16:14:46  vasilche
* Fixed problems with program environment.
* Added class CNcbiEnvironment for cached access to C environment.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE

CNcbiEnvironment::CNcbiEnvironment(void)
{
}

CNcbiEnvironment::CNcbiEnvironment(const char* const* env)
{
    // load ready environment values
    for ( ; *env; ++env ) {
        const char* s = *env;
        const char* eq = strchr(s, '=');
        if ( !eq ) {
            ERR_POST("CNcbiEnvironment: bad string '" << s << "'");
            continue;
        }
        m_Cache[string(s, eq)] = eq + 1;
    }
}

CNcbiEnvironment::~CNcbiEnvironment(void)
{
}

const string& CNcbiEnvironment::Get(const string& name) const
{
    map<string, string>::const_iterator i = m_Cache.find(name);
    if ( i != m_Cache.end() )
        return i->second;
    return m_Cache[name] = Load(name);
}

string CNcbiEnvironment::Load(const string& name) const
{
    const char* s = getenv(name.c_str());
    if ( !s )
        return NcbiEmptyString;
    else
        return s;
}

END_NCBI_SCOPE

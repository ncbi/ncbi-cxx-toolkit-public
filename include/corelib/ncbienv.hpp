#ifndef NCBIENV__HPP
#define NCBIENV__HPP

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
* Revision 1.1  1999/05/04 16:14:06  vasilche
* Fixed problems with program environment.
* Added class CNcbiEnvironment for cached access to C environment.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <map>

BEGIN_NCBI_SCOPE

class CNcbiEnvironment
{
public:
    CNcbiEnvironment(void);
    CNcbiEnvironment(const char* const* env);
    virtual ~CNcbiEnvironment(void);

    const string& Get(const string& name) const;

protected:
    virtual string Load(const string& name) const;

private:
    mutable map<string, string> m_Cache;
};

#include <corelib/ncbienv.inl>

END_NCBI_SCOPE

#endif  /* NCBIENV__HPP */

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
* Author: 
*   Eugene Vasilchenko
*
* File Description:
*   Standard CObject and CRef classes for reference counter based GC
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/03/07 14:03:15  vasilche
* Added CObject class as base for reference counted objects.
* Added CRef templace for reference to CObject descendant.
*
* 
* ===========================================================================
*/

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

CNullPointerError::CNullPointerError(void)
    THROWS_NONE
{
}

CNullPointerError::~CNullPointerError(void)
    THROWS_NONE
{
}

const char* CNullPointerError::what() const
    THROWS_NONE
{
    return "null pointer error";
}

CObject::~CObject(void)
    THROWS((runtime_error))
{
    if ( Referenced() )
        throw runtime_error("delete referenced CObject object");
}

void CObject::RemoveLastReference(void)
    THROWS((runtime_error))
{
    switch ( m_Counter ) {
    case 0:
        // last reference removed
        delete this;
        break;
    case unsigned(-1):
    case unsigned(eDoNotDelete - 1):
        m_Counter++;
        throw runtime_error("RemoveReference() without AddReference()");
    case eDoNotDelete:
        // last reference to static object removed -> do nothing
        break;
    }
}

void CObject::ReleaseReference(void)
    THROWS((runtime_error))
{
    if ( m_Counter == 1 ) {
        m_Counter = 0;
    }
    else {
        throw runtime_error("Illegal CObject::ReleaseReference() call");
    }
}

END_NCBI_SCOPE

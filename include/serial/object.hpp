#ifndef OBJECT__HPP
#define OBJECT__HPP

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  1999/08/13 20:22:57  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.1  1999/08/13 15:53:43  vasilche
* C++ analog of asntool: datatool
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeinfo.hpp>

BEGIN_NCBI_SCOPE

class CObject {
public:
    CObject(void)
        : m_Object(0), m_TypeInfo(0)
        {
        }
    CObject(TTypeInfo typeInfo)
        : m_Object(typeInfo->Create()), m_TypeInfo(typeInfo)
        {
        }
    CObject(TObjectPtr object, TTypeInfo typeInfo)
        : m_Object(object), m_TypeInfo(typeInfo)
        {
        }
    ~CObject(void)
        {
        }


    void Set(TObjectPtr object, TTypeInfo typeInfo)
        {
            m_Object = object;
            m_TypeInfo = typeInfo;
        }

    operator bool(void) const
        {
            return m_Object != 0;
        }

    TObjectPtr GetObject(void)
        {
            return m_Object;
        }
    TConstObjectPtr GetObject(void) const
        {
            return m_Object;
        }
    TTypeInfo GetTypeInfo(void) const
        {
            return m_TypeInfo;
        }

private:
    TObjectPtr m_Object;
    TTypeInfo m_TypeInfo;
};

//#include <serial/object.inl>

END_NCBI_SCOPE

#endif  /* OBJECT__HPP */

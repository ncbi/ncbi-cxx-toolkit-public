#ifndef TYPEREF__HPP
#define TYPEREF__HPP

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
* Revision 1.1  1999/06/04 20:51:41  vasilche
* First compilable version of serialization.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeinfo.hpp>

BEGIN_NCBI_SCOPE

class CTypeInfo;

class CTypeRef
{
public:
    typedef CTypeInfo::TTypeInfo TTypeInfo;

    CTypeRef(void)
        : m_Id(0), m_Creator(0), m_TypeInfo(0)
        {
        }
    CTypeRef(const type_info& id, void (*creator)(void))
        : m_Id(&id), m_Creator(creator), m_TypeInfo(0)
        {
        }
    CTypeRef(TTypeInfo typeInfo)
        : m_Id(0), m_Creator(0), m_TypeInfo(typeInfo)
        {
        }

    TTypeInfo Get(void) const
        {
            TTypeInfo typeInfo = m_TypeInfo;
            if ( !typeInfo ) {
                typeInfo = m_TypeInfo =
                    CTypeInfo::GetTypeInfoBy(*m_Id, m_Creator);
            }
            return typeInfo;
        }

private:

    const type_info* m_Id;
    void (*m_Creator)(void);

    mutable TTypeInfo m_TypeInfo;
};

//#include <serial/typeref.inl>

END_NCBI_SCOPE

#endif  /* TYPEREF__HPP */

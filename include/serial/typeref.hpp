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
* Revision 1.6  1999/09/14 18:54:07  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.5  1999/08/13 15:53:46  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.4  1999/07/13 20:18:12  vasilche
* Changed types naming.
*
* Revision 1.3  1999/06/24 14:44:47  vasilche
* Added binary ASN.1 output.
*
* Revision 1.2  1999/06/09 18:39:01  vasilche
* Modified templates to work on Sun.
*
* Revision 1.1  1999/06/04 20:51:41  vasilche
* First compilable version of serialization.
*
* ===========================================================================
*/

#include <serial/serialdef.hpp>

BEGIN_NCBI_SCOPE

class CTypeRef;

class CTypeInfoSource
{
public:
    CTypeInfoSource(void);
    virtual ~CTypeInfoSource(void);
    
    virtual TTypeInfo GetTypeInfo(void) = 0;

protected:
    int m_RefCount;
    friend class CTypeRef;
};

class CTypeRef
{
public:
    CTypeRef(void)
        : m_Getter(sx_Abort), m_Source(0), m_TypeInfo(0)
        {
        }
    CTypeRef(TTypeInfo typeInfo)
        : m_Getter(sx_Return), m_Source(0), m_TypeInfo(typeInfo)
        {
        }
    CTypeRef(TTypeInfo (*getter)(void));
    CTypeRef(TTypeInfo (*getter)(TTypeInfo typeInfo), const CTypeRef& arg);
    CTypeRef(CTypeInfoSource* source)
        : m_Getter(sx_Resolve), m_Source(source), m_TypeInfo(0)
        {
        }
    CTypeRef(const CTypeRef& typeRef)
        : m_Getter(typeRef.m_Getter), m_Source(Ref(typeRef.m_Source)),
          m_TypeInfo(typeRef.m_TypeInfo)
        {
        }
    CTypeRef& operator=(const CTypeRef& typeRef)
        {
            CTypeInfoSource* source = Ref(typeRef.m_Source);
            m_Getter = sx_Abort;
            Unref();
            m_Getter = typeRef.m_Getter;
            m_Source = source;
            m_TypeInfo = typeRef.m_TypeInfo;
            return *this;
        }
    ~CTypeRef(void)
        {
            Unref();
        }

    TTypeInfo Get(void) const
        {
            return m_Getter(*this);
        }

private:

    static CTypeInfoSource* Ref(CTypeInfoSource* source)
        {
            if ( source )
                ++source->m_RefCount;
            return source;
        }
    void Unref(void) const
        {
            CTypeInfoSource* source = m_Source;
            m_Source = 0;
            if ( source && --source->m_RefCount <= 0 )
                delete source;
        }
    
    static TTypeInfo sx_Abort(const CTypeRef& typeRef);
    static TTypeInfo sx_Return(const CTypeRef& typeRef);
    static TTypeInfo sx_Resolve(const CTypeRef& typeRef);

    mutable TTypeInfo (*m_Getter)(const CTypeRef& );
    mutable CTypeInfoSource* m_Source;
    mutable TTypeInfo m_TypeInfo;
};

class CGetTypeInfoSource : public CTypeInfoSource
{
public:
    CGetTypeInfoSource(TTypeInfo (*getter)(void));

    virtual TTypeInfo GetTypeInfo(void);

private:
    TTypeInfo (*m_Getter)(void);
};

class CGet1TypeInfoSource : public CTypeInfoSource
{
public:
    CGet1TypeInfoSource(TTypeInfo (*getter)(TTypeInfo ),
                        const CTypeRef& arg);

    virtual TTypeInfo GetTypeInfo(void);

private:
    TTypeInfo (*m_Getter)(TTypeInfo );
    CTypeRef m_Argument;
};

#include <serial/typeref.inl>

END_NCBI_SCOPE

#endif  /* TYPEREF__HPP */

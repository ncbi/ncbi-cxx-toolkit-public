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
* Revision 1.9  2000/09/26 17:38:08  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.8  2000/05/24 20:08:16  vasilche
* Implemented XML dump.
*
* Revision 1.7  1999/12/17 19:04:55  vasilche
* Simplified generation of GetTypeInfo methods.
*
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

private:
    CTypeInfoSource(const CTypeInfoSource& );
    CTypeInfoSource& operator=(const CTypeInfoSource& );
};

class CTypeRef
{
public:
    CTypeRef(void)
        : m_Getter(sx_Abort)
        {
        }
    CTypeRef(TTypeInfo typeInfo)
        : m_Getter(sx_Return)
        {
            m_Return = typeInfo;
        }
    typedef TTypeInfo (*TGetProc)(void);
    CTypeRef(TGetProc getProc)
        : m_Getter(sx_GetProc)
        {
            m_GetProc = getProc;
        }
    typedef TTypeInfo (*TGet1Proc)(TTypeInfo arg);
    CTypeRef(TGet1Proc getter, const CTypeRef& arg);
    typedef TTypeInfo (*TGet2Proc)(TTypeInfo arg1, TTypeInfo arg2);
    CTypeRef(TGet2Proc getter, const CTypeRef& arg1, const CTypeRef& arg2);
    CTypeRef(CTypeInfoSource* source);
    CTypeRef(const CTypeRef& typeRef);
    CTypeRef& operator=(const CTypeRef& typeRef);
    ~CTypeRef(void);

    TTypeInfo Get(void) const
        {
            return m_Getter(*this);
        }

    operator bool(void) const
        {
            return m_Getter != sx_Abort;
        }
    bool operator!(void) const
        {
            return m_Getter == sx_Abort;
        }

private:

    void Unref(void);
    void Assign(const CTypeRef& typeRef);
    
    static TTypeInfo sx_Abort(const CTypeRef& typeRef);
    static TTypeInfo sx_Return(const CTypeRef& typeRef);
    static TTypeInfo sx_GetProc(const CTypeRef& typeRef);
    static TTypeInfo sx_Resolve(const CTypeRef& typeRef);

    TTypeInfo (*m_Getter)(const CTypeRef& );
    union {
        TTypeInfo m_Return;
        TGetProc m_GetProc;
        CTypeInfoSource* m_Resolve;
    };
};

class CGet1TypeInfoSource : public CTypeInfoSource
{
public:
    CGet1TypeInfoSource(CTypeRef::TGet1Proc getter, const CTypeRef& arg);
    ~CGet1TypeInfoSource(void);

    virtual TTypeInfo GetTypeInfo(void);

private:
    CTypeRef::TGet1Proc m_Getter;
    CTypeRef m_Argument;
};

class CGet2TypeInfoSource : public CTypeInfoSource
{
public:
    CGet2TypeInfoSource(CTypeRef::TGet2Proc getter,
                        const CTypeRef& arg1, const CTypeRef& arg2);
    ~CGet2TypeInfoSource(void);

    virtual TTypeInfo GetTypeInfo(void);

private:
    CTypeRef::TGet2Proc m_Getter;
    CTypeRef m_Argument1;
    CTypeRef m_Argument2;
};

//#include <serial/typeref.inl>

END_NCBI_SCOPE

#endif  /* TYPEREF__HPP */

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
*/

#include <serial/serialdef.hpp>
#include <corelib/ncbicntr.hpp>


/** @addtogroup GenClassSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CTypeRef;

class NCBI_XSERIAL_EXPORT CTypeInfoSource
{
public:
    CTypeInfoSource(void);
    virtual ~CTypeInfoSource(void);
    
    virtual TTypeInfo GetTypeInfo(void) = 0;

protected:
    CAtomicCounter m_RefCount;
    friend class CTypeRef;

private:
    CTypeInfoSource(const CTypeInfoSource& );
    CTypeInfoSource& operator=(const CTypeInfoSource& );
};

class NCBI_XSERIAL_EXPORT CTypeRef
{
public:
    CTypeRef(void);
    CTypeRef(TTypeInfo typeInfo);

    typedef TTypeInfo (*TGetProc)(void);
    CTypeRef(TGetProc getProc);

    typedef TTypeInfo (*TGet1Proc)(TTypeInfo arg);
    CTypeRef(TGet1Proc getter, const CTypeRef& arg);

    typedef TTypeInfo (*TGet2Proc)(TTypeInfo arg1, TTypeInfo arg2);
    CTypeRef(TGet2Proc getter, const CTypeRef& arg1, const CTypeRef& arg2);

    CTypeRef(TGet2Proc getter,
             const CTypeRef& arg1,
             TGet1Proc getter2, const CTypeRef& arg2);
    CTypeRef(TGet2Proc getter,
             TGet1Proc getter1, const CTypeRef& arg1,
             const CTypeRef& arg2);
    CTypeRef(TGet2Proc getter,
             TGet1Proc getter1, const CTypeRef& arg1,
             TGet1Proc getter2, const CTypeRef& arg2);

    CTypeRef(CTypeInfoSource* source);
    CTypeRef(const CTypeRef& typeRef);
    CTypeRef& operator=(const CTypeRef& typeRef);
    ~CTypeRef(void);

    TTypeInfo Get(void) const;
    DECLARE_OPERATOR_BOOL(m_Getter != sx_GetAbort);

    bool operator==(const CTypeRef& typeRef) const
    {
        return Get() == typeRef.Get();
    }
    bool operator!=(const CTypeRef& typeRef) const
    {
        return Get() != typeRef.Get();
    }

private:

    void Unref(void);
    void Assign(const CTypeRef& typeRef);
    
    static TTypeInfo sx_GetAbort(const CTypeRef& typeRef);
    static TTypeInfo sx_GetReturn(const CTypeRef& typeRef);
    static TTypeInfo sx_GetProc(const CTypeRef& typeRef);
    static TTypeInfo sx_GetResolve(const CTypeRef& typeRef);

    TTypeInfo (*m_Getter)(const CTypeRef& );
    TTypeInfo m_ReturnData;
    union {
        TGetProc m_GetProcData;
        CTypeInfoSource* m_ResolveData;
    };
};

class NCBI_XSERIAL_EXPORT CGet1TypeInfoSource : public CTypeInfoSource
{
public:
    CGet1TypeInfoSource(CTypeRef::TGet1Proc getter, const CTypeRef& arg);
    ~CGet1TypeInfoSource(void);

    virtual TTypeInfo GetTypeInfo(void);

private:
    CTypeRef::TGet1Proc m_Getter;
    CTypeRef m_Argument;
};

class NCBI_XSERIAL_EXPORT CGet2TypeInfoSource : public CTypeInfoSource
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


/* @} */


#include <serial/impl/typeref.inl>

END_NCBI_SCOPE

#endif  /* TYPEREF__HPP */

#ifndef STDTYPES__HPP
#define STDTYPES__HPP

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

#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>
#include <serial/typeinfo.hpp>
#include <vector>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class NCBI_XSERIAL_EXPORT CPrimitiveTypeInfo : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:
    typedef bool (*TIsDefaultFunction)(TConstObjectPtr objectPtr);
    typedef void (*TSetDefaultFunction)(TObjectPtr objectPtr);
    typedef bool (*TEqualsFunction)(TConstObjectPtr o1, TConstObjectPtr o2,
                                    ESerialRecursionMode how);
    typedef void (*TAssignFunction)(TObjectPtr dst, TConstObjectPtr src,
                                    ESerialRecursionMode how);

    CPrimitiveTypeInfo(size_t size,
                       EPrimitiveValueType valueType, bool isSigned = true);
    CPrimitiveTypeInfo(size_t size, const char* name,
                       EPrimitiveValueType valueType, bool isSigned = true);
    CPrimitiveTypeInfo(size_t size, const string& name,
                       EPrimitiveValueType valueType, bool isSigned = true);

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr , TConstObjectPtr,
                        ESerialRecursionMode how = eRecursive) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src,
                        ESerialRecursionMode how = eRecursive) const;

    EPrimitiveValueType GetPrimitiveValueType(void) const;

    bool IsSigned(void) const;

    virtual bool GetValueBool(TConstObjectPtr objectPtr) const;
    virtual void SetValueBool(TObjectPtr objectPtr, bool value) const;

    virtual char GetValueChar(TConstObjectPtr objectPtr) const;
    virtual void SetValueChar(TObjectPtr objectPtr, char value) const;

    virtual Int4 GetValueInt4(TConstObjectPtr objectPtr) const;
    virtual void SetValueInt4(TObjectPtr objectPtr, Int4 value) const;
    virtual Uint4 GetValueUint4(TConstObjectPtr objectPtr) const;
    virtual void SetValueUint4(TObjectPtr objectPtr, Uint4 value) const;

    virtual Int8 GetValueInt8(TConstObjectPtr objectPtr) const;
    virtual void SetValueInt8(TObjectPtr objectPtr, Int8 value) const;
    virtual Uint8 GetValueUint8(TConstObjectPtr objectPtr) const;
    virtual void SetValueUint8(TObjectPtr objectPtr, Uint8 value) const;

    int GetValueInt(TConstObjectPtr objectPtr) const;
    void SetValueInt(TObjectPtr objectPtr, int value) const;
    unsigned GetValueUInt(TConstObjectPtr objectPtr) const;
    void SetValueUInt(TObjectPtr objectPtr, unsigned value) const;

    long GetValueLong(TConstObjectPtr objectPtr) const;
    void SetValueLong(TObjectPtr objectPtr, long value) const;
    unsigned long GetValueULong(TConstObjectPtr objectPtr) const;
    void SetValueULong(TObjectPtr objectPtr, unsigned long value) const;

    virtual double GetValueDouble(TConstObjectPtr objectPtr) const;
    virtual void SetValueDouble(TObjectPtr objectPtr, double value) const;
#if SIZEOF_LONG_DOUBLE != 0
    virtual long double GetValueLDouble(TConstObjectPtr objectPtr) const;
    virtual void SetValueLDouble(TObjectPtr objectPtr,
                                 long double value) const;
#endif

    virtual void GetValueString(TConstObjectPtr objectPtr,
                                string& value) const;
    virtual void SetValueString(TObjectPtr objectPtr,
                                const string& value) const;

    virtual void GetValueOctetString(TConstObjectPtr objectPtr,
                                     vector<char>& value) const;
    virtual void SetValueOctetString(TObjectPtr objectPtr,
                                     const vector<char>& value) const;

    virtual void GetValueBitString(TConstObjectPtr objectPtr,
                                   CBitString& value) const;
    virtual void SetValueBitString(TObjectPtr objectPtr,
                                   const CBitString& value) const;

    virtual void GetValueAnyContent(TConstObjectPtr objectPtr,
                                    CAnyContentObject& value) const;
    virtual void SetValueAnyContent(TObjectPtr objectPtr,
                                    const CAnyContentObject& value) const;

    static const CPrimitiveTypeInfo* GetIntegerTypeInfo(size_t size,
                                                        bool sign = true);

    void SetMemFunctions(TTypeCreate,
                         TIsDefaultFunction, TSetDefaultFunction,
                         TEqualsFunction, TAssignFunction);
    void SetIOFunctions(TTypeReadFunction, TTypeWriteFunction,
                        TTypeCopyFunction, TTypeSkipFunction);
protected:
    friend class CObjectInfo;
    friend class CConstObjectInfo;

    EPrimitiveValueType m_ValueType;
    bool m_Signed;

    TIsDefaultFunction m_IsDefault;
    TSetDefaultFunction m_SetDefault;
    TEqualsFunction m_Equals;
    TAssignFunction m_Assign;
};

class NCBI_XSERIAL_EXPORT CVoidTypeInfo : public CPrimitiveTypeInfo
{
    typedef CPrimitiveTypeInfo CParent;
public:
    CVoidTypeInfo(void);
};

class NCBI_XSERIAL_EXPORT CNullTypeInfo : public CVoidTypeInfo
{
    typedef CVoidTypeInfo CParent;
public:
    CNullTypeInfo(void);

    static TTypeInfo GetTypeInfo(void);
};

// template for getting type info of standard types
template<typename T>
class CStdTypeInfo
{
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<bool>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
    static TTypeInfo GetTypeInfoNullBool(void);
    static CTypeInfo* CreateTypeInfoNullBool(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<char>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<signed char>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<unsigned char>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<short>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<unsigned short>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<int>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<unsigned>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

#ifndef NCBI_INT8_IS_LONG
EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<long>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<unsigned long>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};
#endif

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<Int8>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<Uint8>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<float>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<double>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

#if SIZEOF_LONG_DOUBLE != 0
EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<long double>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};
#endif

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<string>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
    static TTypeInfo GetTypeInfoStringStore(void);
    static CTypeInfo* CreateTypeInfoStringStore(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<ncbi::CStringUTF8>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
    static TTypeInfo GetTypeInfoStringStore(void);
    static CTypeInfo* CreateTypeInfoStringStore(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<char*>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<const char*>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo< vector<char> >
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo< vector<signed char> >
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo< vector<unsigned char> >
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<ncbi::CAnyContentObject>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};

EMPTY_TEMPLATE
class NCBI_XSERIAL_EXPORT CStdTypeInfo<CBitString>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};


/* @} */


#include <serial/impl/stdtypes.inl>

END_NCBI_SCOPE

#endif  /* STDTYPES__HPP */

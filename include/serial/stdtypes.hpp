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

#if SIZEOF_LONG == 4
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



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.34  2006/10/05 19:23:04  gouriano
* Some headers moved into impl
*
* Revision 1.33  2006/07/26 19:06:30  ucko
* Add reflective accessors for BitString and AnyContent values.
*
* Revision 1.32  2005/11/29 17:42:49  gouriano
* Added CBitString class
*
* Revision 1.31  2004/03/25 15:57:55  gouriano
* Added possibility to copy and compare serial object non-recursively
*
* Revision 1.30  2003/08/13 15:47:02  gouriano
* implemented serialization of AnyContent objects
*
* Revision 1.29  2003/05/22 20:08:42  gouriano
* added UTF8 strings
*
* Revision 1.28  2003/04/15 16:18:57  siyan
* Added doxygen support
*
* Revision 1.27  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.26  2001/08/31 20:05:43  ucko
* Fix ICC build.
*
* Revision 1.25  2001/01/30 21:40:58  vasilche
* Fixed dealing with unsigned enums.
*
* Revision 1.24  2000/12/15 15:38:01  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.23  2000/11/07 17:25:13  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.22  2000/10/13 16:28:32  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.21  2000/09/18 20:00:10  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.20  2000/09/01 13:16:03  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.19  2000/07/03 18:42:37  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.18  2000/05/24 20:08:15  vasilche
* Implemented XML dump.
*
* Revision 1.17  2000/03/07 14:05:32  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.16  2000/01/10 19:46:34  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.15  2000/01/05 19:43:47  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.14  1999/12/28 18:55:40  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.13  1999/12/17 19:04:54  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.12  1999/09/22 20:11:51  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.11  1999/09/14 18:54:06  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.10  1999/08/13 15:53:45  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.9  1999/07/13 20:18:08  vasilche
* Changed types naming.
*
* Revision 1.8  1999/07/07 21:56:33  vasilche
* Fixed problem with static variables in templates on SunPro
*
* Revision 1.7  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.6  1999/07/07 19:58:46  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.5  1999/06/30 16:04:37  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.4  1999/06/24 14:44:45  vasilche
* Added binary ASN.1 output.
*
* Revision 1.3  1999/06/09 18:39:00  vasilche
* Modified templates to work on Sun.
*
* Revision 1.2  1999/06/04 20:51:38  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:29  vasilche
* Commit just in case.
*
* ===========================================================================
*/

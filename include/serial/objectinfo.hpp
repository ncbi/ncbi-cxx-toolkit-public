#ifndef OBJECTINFO__HPP
#define OBJECTINFO__HPP

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
#include <corelib/ncbiobj.hpp>
#include <serial/serialdef.hpp>
#include <serial/typeinfo.hpp>
#include <serial/continfo.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/stdtypes.hpp>
#include <serial/classinfob.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <vector>
#include <memory>


/** @addtogroup UserCodeSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CObjectTypeInfo;
class CConstObjectInfo;
class CObjectInfo;

class CPrimitiveTypeInfo;
class CClassTypeInfoBase;
class CClassTypeInfo;
class CChoiceTypeInfo;
class CContainerTypeInfo;
class CPointerTypeInfo;

class CMemberId;
class CItemInfo;
class CMemberInfo;
class CVariantInfo;

class CReadContainerElementHook;

class CObjectTypeInfoMI;
class CObjectTypeInfoVI;
class CObjectTypeInfoCV;
class CConstObjectInfoMI;
class CConstObjectInfoCV;
class CConstObjectInfoEI;
class CObjectInfoMI;
class CObjectInfoCV;
class CObjectInfoEI;

class NCBI_XSERIAL_EXPORT CObjectTypeInfo
{
public:
    typedef CObjectTypeInfoMI CMemberIterator;
    typedef CObjectTypeInfoVI CVariantIterator;
    typedef CObjectTypeInfoCV CChoiceVariant;

    CObjectTypeInfo(TTypeInfo typeinfo = 0);

    ETypeFamily GetTypeFamily(void) const;

    bool Valid(void) const;
    operator bool(void) const;
    bool operator!(void) const;
    
    bool operator==(const CObjectTypeInfo& type) const;
    bool operator!=(const CObjectTypeInfo& type) const;

    // primitive type interface
    // only when GetTypeFamily() == CTypeInfo::eTypePrimitive
    EPrimitiveValueType GetPrimitiveValueType(void) const;
    bool IsPrimitiveValueSigned(void) const;

    // container interface
    // only when GetTypeFamily() == CTypeInfo::eTypeContainer
    CObjectTypeInfo GetElementType(void) const;

    // class interface
    // only when GetTypeFamily() == CTypeInfo::eTypeClass
    CMemberIterator BeginMembers(void) const;
    CMemberIterator FindMember(const string& memberName) const;
    CMemberIterator FindMemberByTag(int memberTag) const;

    // choice interface
    // only when GetTypeFamily() == CTypeInfo::eTypeChoice
    CVariantIterator BeginVariants(void) const;
    CVariantIterator FindVariant(const string& memberName) const;
    CVariantIterator FindVariantByTag(int memberTag) const;

    // pointer interface
    // only when GetTypeFamily() == CTypeInfo::eTypePointer
    CObjectTypeInfo GetPointedType(void) const;

    void SetLocalReadHook(CObjectIStream& stream,
                          CReadObjectHook* hook) const;
    void SetGlobalReadHook(CReadObjectHook* hook) const;
    void ResetLocalReadHook(CObjectIStream& stream) const;
    void ResetGlobalReadHook(void) const;
    void SetPathReadHook(CObjectIStream* stream, const string& path,
                         CReadObjectHook* hook) const;
    void SetLocalWriteHook(CObjectOStream& stream,
                          CWriteObjectHook* hook) const;
    void SetGlobalWriteHook(CWriteObjectHook* hook) const;
    void ResetLocalWriteHook(CObjectOStream& stream) const;
    void ResetGlobalWriteHook(void) const;
    void SetPathWriteHook(CObjectOStream* stream, const string& path,
                          CWriteObjectHook* hook) const;
    void SetLocalSkipHook(CObjectIStream& stream,
                          CSkipObjectHook* hook) const;
    void SetGlobalSkipHook(CSkipObjectHook* hook) const;
    void ResetLocalSkipHook(CObjectIStream& stream) const;
    void ResetGlobalSkipHook(void) const;
    void SetPathSkipHook(CObjectIStream* stream, const string& path,
                         CSkipObjectHook* hook) const;
    void SetLocalCopyHook(CObjectStreamCopier& stream,
                          CCopyObjectHook* hook) const;
    void SetGlobalCopyHook(CCopyObjectHook* hook) const;
    void ResetLocalCopyHook(CObjectStreamCopier& stream) const;
    void ResetGlobalCopyHook(void) const;
    void SetPathCopyHook(CObjectStreamCopier* stream, const string& path,
                         CCopyObjectHook* hook) const;

public: // mostly for internal use
    TTypeInfo GetTypeInfo(void) const;
    const CPrimitiveTypeInfo* GetPrimitiveTypeInfo(void) const;
    const CClassTypeInfo* GetClassTypeInfo(void) const;
    const CChoiceTypeInfo* GetChoiceTypeInfo(void) const;
    const CContainerTypeInfo* GetContainerTypeInfo(void) const;
    const CPointerTypeInfo* GetPointerTypeInfo(void) const;

    TMemberIndex FindMemberIndex(const string& name) const;
    TMemberIndex FindMemberIndex(int tag) const;
    TMemberIndex FindVariantIndex(const string& name) const;
    TMemberIndex FindVariantIndex(int tag) const;

    CMemberIterator GetMemberIterator(TMemberIndex index) const;
    CVariantIterator GetVariantIterator(TMemberIndex index) const;

protected:
    void ResetTypeInfo(void);
    void SetTypeInfo(TTypeInfo typeinfo);

    void CheckTypeFamily(ETypeFamily family) const;
    void WrongTypeFamily(ETypeFamily needFamily) const;

private:
    TTypeInfo m_TypeInfo;

private:
    CTypeInfo* GetNCTypeInfo(void) const;
};

class NCBI_XSERIAL_EXPORT CConstObjectInfo : public CObjectTypeInfo
{
public:
    typedef TConstObjectPtr TObjectPtrType;
    typedef CConstObjectInfoEI CElementIterator;
    typedef CConstObjectInfoMI CMemberIterator;
    typedef CConstObjectInfoCV CChoiceVariant;
    
    enum ENonCObject {
        eNonCObject
    };

    // create empty CObjectInfo
    CConstObjectInfo(void);
    // initialize CObjectInfo
    CConstObjectInfo(TConstObjectPtr objectPtr, TTypeInfo typeInfo);
    CConstObjectInfo(pair<TConstObjectPtr, TTypeInfo> object);
    CConstObjectInfo(pair<TObjectPtr, TTypeInfo> object);
    // initialize CObjectInfo when we are sure that object 
    // is not inherited from CObject (for efficiency)
    CConstObjectInfo(TConstObjectPtr objectPtr, TTypeInfo typeInfo,
                     ENonCObject nonCObject);

    // reset CObjectInfo to empty state
    void Reset(void);
    // set CObjectInfo
    CConstObjectInfo& operator=(pair<TConstObjectPtr, TTypeInfo> object);
    CConstObjectInfo& operator=(pair<TObjectPtr, TTypeInfo> object);

    // check if CObjectInfo initialized with valid object
    bool Valid(void) const;
    operator bool(void) const;
    bool operator!(void) const;

    void ResetObjectPtr(void);

    // get pointer to object
    TConstObjectPtr GetObjectPtr(void) const;
    pair<TConstObjectPtr, TTypeInfo> GetPair(void) const;

    // primitive type interface
    bool GetPrimitiveValueBool(void) const;
    char GetPrimitiveValueChar(void) const;

    Int4 GetPrimitiveValueInt4(void) const;
    Uint4 GetPrimitiveValueUint4(void) const;
    Int8 GetPrimitiveValueInt8(void) const;
    Uint8 GetPrimitiveValueUint8(void) const;
    int GetPrimitiveValueInt(void) const;
    unsigned GetPrimitiveValueUInt(void) const;
    long GetPrimitiveValueLong(void) const;
    unsigned long GetPrimitiveValueULong(void) const;

    double GetPrimitiveValueDouble(void) const;

    void GetPrimitiveValueString(string& value) const;
    string GetPrimitiveValueString(void) const;

    void GetPrimitiveValueOctetString(vector<char>& value) const;

    // class interface
    CMemberIterator GetMember(CObjectTypeInfo::CMemberIterator m) const;
    CMemberIterator BeginMembers(void) const;
    CMemberIterator GetClassMemberIterator(TMemberIndex index) const;
    CMemberIterator FindClassMember(const string& memberName) const;
    CMemberIterator FindClassMemberByTag(int memberTag) const;

    // choice interface
    TMemberIndex GetCurrentChoiceVariantIndex(void) const;
    CChoiceVariant GetCurrentChoiceVariant(void) const;

    // pointer interface
    CConstObjectInfo GetPointedObject(void) const;
    
    // container interface
    CElementIterator BeginElements(void) const;

protected:
    void Set(TConstObjectPtr objectPtr, TTypeInfo typeInfo);
    
private:
    TConstObjectPtr m_ObjectPtr; // object pointer
    CConstRef<CObject> m_Ref; // hold reference to CObject for correct removal
};

class NCBI_XSERIAL_EXPORT CObjectInfo : public CConstObjectInfo
{
    typedef CConstObjectInfo CParent;
public:
    typedef TObjectPtr TObjectPtrType;
    typedef CObjectInfoEI CElementIterator;
    typedef CObjectInfoMI CMemberIterator;
    typedef CObjectInfoCV CChoiceVariant;

    // create empty CObjectInfo
    CObjectInfo(void);
    // initialize CObjectInfo
    CObjectInfo(TObjectPtr objectPtr, TTypeInfo typeInfo);
    CObjectInfo(pair<TObjectPtr, TTypeInfo> object);
    // initialize CObjectInfo when we are sure that object 
    // is not inherited from CObject (for efficiency)
    CObjectInfo(TObjectPtr objectPtr, TTypeInfo typeInfo,
                ENonCObject nonCObject);
    // create CObjectInfo with new object
    explicit CObjectInfo(TTypeInfo typeInfo);

    // set CObjectInfo to point to another object
    CObjectInfo& operator=(pair<TObjectPtr, TTypeInfo> object);
    
    // get pointer to object
    TObjectPtr GetObjectPtr(void) const;
    pair<TObjectPtr, TTypeInfo> GetPair(void) const;

    // primitive type interface
    void SetPrimitiveValueBool(bool value);
    void SetPrimitiveValueChar(char value);

    void SetPrimitiveValueLong(long value);
    void SetPrimitiveValueULong(unsigned long value);

    void SetPrimitiveValueDouble(double value);

    void SetPrimitiveValueString(const string& value);

    void SetPrimitiveValueOctetString(const vector<char>& value);

    // class interface
    CMemberIterator GetMember(CObjectTypeInfo::CMemberIterator m) const;
    CMemberIterator BeginMembers(void) const;
    CMemberIterator GetClassMemberIterator(TMemberIndex index) const;
    CMemberIterator FindClassMember(const string& memberName) const;
    CMemberIterator FindClassMemberByTag(int memberTag) const;

    // choice interface
    CChoiceVariant GetCurrentChoiceVariant(void) const;

    // pointer interface
    CObjectInfo GetPointedObject(void) const;

    // container interface
    CElementIterator BeginElements(void) const;
    void ReadContainer(CObjectIStream& in, CReadContainerElementHook& hook);
};


/* @} */


#include <serial/objectinfo.inl>

END_NCBI_SCOPE

#endif  /* OBJECTINFO__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2004/04/30 13:29:09  gouriano
* Remove obsolete function declarations
*
* Revision 1.8  2004/01/05 14:24:08  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.7  2003/07/29 18:47:46  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.6  2003/04/15 16:18:11  siyan
* Added doxygen support
*
* Revision 1.5  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.4  2001/05/17 14:57:17  lavr
* Typos corrected
*
* Revision 1.3  2001/01/22 23:20:30  vakatov
* + CObjectInfo::GetMember(), CConstObjectInfo::GetMember()
*
* Revision 1.2  2000/12/15 15:37:59  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.1  2000/10/20 15:51:24  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* ===========================================================================
*/

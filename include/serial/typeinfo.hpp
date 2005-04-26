#ifndef TYPEINFO__HPP
#define TYPEINFO__HPP

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
#include <serial/serialdef.hpp>
#include <serial/hookdata.hpp>
#include <serial/hookfunc.hpp>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CObject;

class CObjectIStream;
class CObjectOStream;
class CObjectStreamCopier;

class CClassTypeInfo;

class CObjectTypeInfo;
class CConstObjectInfo;
class CObjectInfo;

class CReadObjectHook;
class CWriteObjectHook;
class CSkipObjectHook;
class CCopyObjectHook;

class CTypeInfoFunctions;

class CNamespaceInfoItem;
class CObjectMemoryPool;

// CTypeInfo class contains all information about C++ types (both basic and
// classes): members and layout in memory.
class NCBI_XSERIAL_EXPORT CTypeInfo
{
protected:
    CTypeInfo(ETypeFamily typeFamily, size_t size);
    CTypeInfo(ETypeFamily typeFamily, size_t size, const char* name);
    CTypeInfo(ETypeFamily typeFamily, size_t size, const string& name);
public:
    // various function pointers
    typedef TObjectPtr (*TTypeCreate)(TTypeInfo objectType,
                                      CObjectMemoryPool* memoryPool);

    virtual ~CTypeInfo(void);

    ETypeFamily GetTypeFamily(void) const;

    // name of this type
    const string& GetName(void) const;

    // namespace name
    bool HasNamespaceName(void) const;
    const string& GetNamespaceName(void) const;
    void SetNamespaceName(const string& ns_name) const;

    // namespace prefix
    bool HasNamespacePrefix(void) const;
    const string& GetNamespacePrefix(void) const;
    void SetNamespacePrefix(const string& ns_prefix) const;

    // name of module
    virtual const string& GetModuleName(void) const;
    void SetModuleName(const string& name);
    void SetModuleName(const char* name);

    // size of data object in memory (like sizeof in C)
    size_t GetSize(void) const;

    // creates object of this type in heap (can be deleted by operator delete)
    TObjectPtr Create(CObjectMemoryPool* memoryPool = 0) const;

    // deletes object
    virtual void Delete(TObjectPtr object) const;
    // clear object contents so Delete will not leave unused memory allocated
    // note: object contents is not guaranteed to be in initial state
    //       (as after Create), to do so you should call SetDefault after
    virtual void DeleteExternalObjects(TObjectPtr object) const;

    // check, whether object contains default value
    virtual bool IsDefault(TConstObjectPtr object) const = 0;
    // check if both objects contain the same values
    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                        ESerialRecursionMode how = eRecursive) const = 0;
    // set object to default value
    virtual void SetDefault(TObjectPtr dst) const = 0;
    // set object to copy of another one
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src,
                        ESerialRecursionMode how = eRecursive) const = 0;

    // return true if type is inherited from CObject
    bool IsCObject(void) const;
    virtual const CObject* GetCObjectPtr(TConstObjectPtr objectPtr) const;
    // return true CTypeInfo of object (redefined in polymorphic classes)
    virtual TTypeInfo GetRealTypeInfo(TConstObjectPtr object) const;

    // I/O interface:
    void ReadData(CObjectIStream& in, TObjectPtr object) const;
    void WriteData(CObjectOStream& out, TConstObjectPtr object) const;
    void CopyData(CObjectStreamCopier& copier) const;
    void SkipData(CObjectIStream& in) const;

    virtual bool IsParentClassOf(const CClassTypeInfo* classInfo) const;
    virtual bool IsType(TTypeInfo type) const;
    virtual bool MayContainType(TTypeInfo type) const;
    bool IsOrMayContainType(TTypeInfo type) const;

    // hooks
    void SetGlobalReadHook(CReadObjectHook* hook);
    void SetLocalReadHook(CObjectIStream& in, CReadObjectHook* hook);
    void ResetGlobalReadHook(void);
    void ResetLocalReadHook(CObjectIStream& in);
    void SetPathReadHook(CObjectIStream* in, const string& path,
                         CReadObjectHook* hook);

    void SetGlobalWriteHook(CWriteObjectHook* hook);
    void SetLocalWriteHook(CObjectOStream& out, CWriteObjectHook* hook);
    void ResetGlobalWriteHook(void);
    void ResetLocalWriteHook(CObjectOStream& out);
    void SetPathWriteHook(CObjectOStream* out, const string& path,
                          CWriteObjectHook* hook);

    void SetGlobalSkipHook(CSkipObjectHook* hook);
    void SetLocalSkipHook(CObjectIStream& in, CSkipObjectHook* hook);
    void ResetGlobalSkipHook(void);
    void ResetLocalSkipHook(CObjectIStream& in);
    void SetPathSkipHook(CObjectIStream* in, const string& path,
                         CSkipObjectHook* hook);

    void SetGlobalCopyHook(CCopyObjectHook* hook);
    void SetLocalCopyHook(CObjectStreamCopier& copier, CCopyObjectHook* hook);
    void ResetGlobalCopyHook(void);
    void ResetLocalCopyHook(CObjectStreamCopier& copier);
    void SetPathCopyHook(CObjectStreamCopier* copier, const string& path,
                         CCopyObjectHook* hook);

    // default methods without checking hook
    void DefaultReadData(CObjectIStream& in, TObjectPtr object) const;
    void DefaultWriteData(CObjectOStream& out, TConstObjectPtr object) const;
    void DefaultCopyData(CObjectStreamCopier& copier) const;
    void DefaultSkipData(CObjectIStream& in) const;

private:
    // private constructors to avoid copying
    CTypeInfo(const CTypeInfo&);
    CTypeInfo& operator=(const CTypeInfo&);

    // type information
    ETypeFamily m_TypeFamily;
    size_t m_Size;
    string m_Name;
    string m_ModuleName;
    mutable CNamespaceInfoItem* m_InfoItem;

protected:
    void SetCreateFunction(TTypeCreate func);
    void SetReadFunction(TTypeReadFunction func);
    void SetWriteFunction(TTypeWriteFunction func);
    void SetCopyFunction(TTypeCopyFunction func);
    void SetSkipFunction(TTypeSkipFunction func);

    bool m_IsCObject;

private:
    // type specific function pointers
    TTypeCreate m_CreateFunction;

    CHookData<CReadObjectHook, TTypeReadFunction> m_ReadHookData;
    CHookData<CWriteObjectHook, TTypeWriteFunction> m_WriteHookData;
    CHookData<CSkipObjectHook, TTypeSkipFunction> m_SkipHookData;
    CHookData<CCopyObjectHook, TTypeCopyFunction> m_CopyHookData;

    friend class CTypeInfoFunctions;

    void x_CreateInfoItemIfNeeded(void) const;
};


/* @} */


#include <serial/typeinfo.inl>

END_NCBI_SCOPE

#endif  /* TYPEINFO__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.47  2005/04/26 14:18:50  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.46  2004/03/25 15:57:55  gouriano
* Added possibility to copy and compare serial object non-recursively
*
* Revision 1.45  2004/01/05 14:24:09  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.44  2003/11/24 14:10:04  grichenk
* Changed base class for CAliasTypeInfo to CPointerTypeInfo
*
* Revision 1.43  2003/11/18 18:11:47  grichenk
* Resolve aliased type info before using it in CObjectTypeInfo
*
* Revision 1.42  2003/09/16 14:49:15  gouriano
* Enhanced AnyContent objects to support XML namespaces and attribute info items.
*
* Revision 1.41  2003/08/25 15:58:32  gouriano
* added possibility to use namespaces in XML i/o streams
*
* Revision 1.40  2003/07/29 18:47:46  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.39  2003/04/15 16:19:06  siyan
* Added doxygen support
*
* Revision 1.38  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.37  2002/09/09 18:14:00  grichenk
* Added CObjectHookGuard class.
* Added methods to be used by hooks for data
* reading and skipping.
*
* Revision 1.36  2001/10/22 15:16:20  grichenk
* Optimized CTypeInfo::IsCObject()
*
* Revision 1.35  2001/05/17 14:59:47  lavr
* Typos corrected
*
* Revision 1.34  2000/11/07 17:25:13  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.33  2000/10/03 17:22:36  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.32  2000/09/29 16:18:15  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.31  2000/09/18 20:00:11  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.30  2000/09/01 13:16:04  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.29  2000/08/15 19:44:43  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.28  2000/07/03 18:42:38  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.27  2000/06/16 16:31:08  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.26  2000/05/24 20:08:16  vasilche
* Implemented XML dump.
*
* Revision 1.25  2000/05/09 16:38:34  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.24  2000/03/29 15:55:22  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.23  2000/03/14 14:43:30  vasilche
* Fixed error reporting.
*
* Revision 1.22  2000/03/07 14:05:33  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.21  2000/02/17 20:02:30  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.20  1999/12/28 18:55:40  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.19  1999/12/17 19:04:55  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.18  1999/09/22 20:11:51  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.17  1999/09/14 18:54:07  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.16  1999/08/31 17:50:05  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.15  1999/07/20 18:22:57  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.14  1999/07/19 15:50:22  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.13  1999/07/13 20:18:10  vasilche
* Changed types naming.
*
* Revision 1.12  1999/07/07 19:58:47  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.11  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
* Revision 1.10  1999/07/01 17:55:23  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.9  1999/06/30 18:54:55  vasilche
* Fixed some errors under MSVS
*
* Revision 1.8  1999/06/30 16:04:39  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.7  1999/06/24 14:44:47  vasilche
* Added binary ASN.1 output.
*
* Revision 1.6  1999/06/15 16:20:09  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.5  1999/06/10 21:06:42  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.4  1999/06/07 19:59:38  vasilche
* offset_t -> size_t
*
* Revision 1.3  1999/06/07 19:30:21  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:40  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:32  vasilche
* Commit just in case.
*
* ===========================================================================
*/

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
*
* ---------------------------------------------------------------------------
* $Log$
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

#include <serial/serialdef.hpp>
#include <corelib/ncbiobj.hpp>
#include <typeinfo>
#include <memory>

BEGIN_NCBI_SCOPE

class CObjectIStream;
class CObjectOStream;
class CClassTypeInfo;
class COObjectList;
class CTypeRef;
class CMemberId;
class CMemberInfo;
class CConstObjectInfo;
class CObjectInfo;

// this structure is used for sorting C++ standard type_info class by pointer
struct CLessTypeInfo
{
    // to avoid warning under MSVS, where type_info::before() erroneously
    // returns int, we'll define overloaded functions:
    static bool ToBool(bool b)
        {
            return b;
        }
    static bool ToBool(int i)
        {
            return i != 0;
        }

    bool operator()(const type_info* i1, const type_info* i2) const
		{
            return ToBool(i1->before(*i2));
        }
};

// CTypeInfo class contains all information about C++ types (both basic and
// classes): members and layout in memory.
class CTypeInfo
{
protected:
    CTypeInfo(void);
    CTypeInfo(const string& name);
    CTypeInfo(const char* name);
public:
    virtual ~CTypeInfo(void);

    // type family
    enum ETypeFamily {
        eTypePrimitive, // see CPrimitiveTypeInfo
        eTypeClass,     // see CClassTypeInfo
        eTypeChoice,    // see CChoiceTypeInfo
        eTypeContainer, // see CContainerTypeInfo
        eTypePointer    // see CPointerTypeInfo
    };
    virtual ETypeFamily GetTypeFamily(void) const = 0;

    // name of this type
    const string& GetName(void) const
        { return m_Name; }

    // size of data object in memory (like sizeof in C)
    virtual size_t GetSize(void) const = 0;

    // helpers: return end of object
    TObjectPtr EndOf(TObjectPtr object) const
        { return Add(object, GetSize()); }
    TConstObjectPtr EndOf(TConstObjectPtr object) const
        { return Add(object, GetSize()); }

    // creates object of this type in heap (can be deleted by operator delete)
    virtual TObjectPtr Create(void) const;
    // deletes object
    virtual void Delete(TObjectPtr object) const;
    // clear object contents so Delete will not leave unused memory allocated
    // note: object contents is not guaranteed to be in initial state
    //       (as after Create), to do so you should call SetDefault after
    virtual void DeleteExternalObjects(TObjectPtr object) const;

    // check, whether object contains default value
    virtual bool IsDefault(TConstObjectPtr object) const = 0;
    // check if both objects contain the same valuse
    virtual bool Equals(TConstObjectPtr object1,
                        TConstObjectPtr object2) const = 0;
    // set object to default value
    virtual void SetDefault(TObjectPtr dst) const = 0;
    // set object to copy of another one
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const = 0;

    // return true if type is inherited from CObject
    virtual bool IsCObject(void) const;
    virtual const CObject* GetCObjectPtr(TConstObjectPtr objectPtr) const;
    // return true CTypeInfo of object (redefined in polimorfic classes)
    virtual TTypeInfo GetRealTypeInfo(TConstObjectPtr object) const;

    // I/O interface:
    // read object
    virtual void ReadData(CObjectIStream& in,
                          TObjectPtr object) const = 0;
    virtual void SkipData(CObjectIStream& in) const = 0;
    // write object
    virtual void WriteData(CObjectOStream& out,
                           TConstObjectPtr object) const = 0;

    virtual bool IsType(TTypeInfo type) const;
    virtual bool MayContainType(TTypeInfo type) const;
    virtual bool IsOrMayContainType(TTypeInfo type) const;
    virtual bool IsParentClassOf(const CClassTypeInfo* classInfo) const;

private:
    friend class CObjectOStream;
    friend class CObjectIStream;

    CTypeInfo(const CTypeInfo&);
    CTypeInfo& operator=(const CTypeInfo&);
    string m_Name;
};

// helper template for various types:
template<typename T>
struct CType
{
    CType(void);
    CType(const CType&);
    CType& operator=(const CType&);
    ~CType(void);
public:
    typedef T TObjectType; // type of object

    // object getters:
    static TObjectType& Get(TObjectPtr object)
        {
            return *static_cast<TObjectType*>(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectType*>(object);
        }
    static TObjectType& Get(const CObjectInfo& object)
        {
            return Get(object.GetObjectPtr());
        }
    static const TObjectType& Get(const CConstObjectInfo& object)
        {
            return Get(object.GetObjectPtr());
        }

    static size_t GetSize(void)
        {
            return sizeof(TObjectType);
        }

#ifdef _DEBUG
private:
    static void AssertPointer(const CObject* /*selector*/, const void* ptr)
        {
            // assert that ptr is really pointer to T
            _ASSERT(dynamic_cast<const T*>(static_cast<const CObject*>(ptr)));
        }
    static void AssertPointer(const void* /*selector*/, const void* /*ptr*/)
        {
            // cannot check if T is not subclass of CObject
        }
public:
    static void AssertPointer(const void* ptr)
        {
            _ASSERT(ptr);
            const T* selector = static_cast<const T*>(0);
            AssertPointer(selector, ptr);
        }
#else
    static void AssertPointer(const void* /*ptr*/)
        {
            // do nothing in non _DEBUG mode
        }
#endif
};

#include <serial/typeinfo.inl>

END_NCBI_SCOPE

#endif  /* TYPEINFO__HPP */

#ifndef STLTYPES__HPP
#define STLTYPES__HPP

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
* Revision 1.48  2000/09/01 13:16:03  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.47  2000/08/15 19:44:42  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.46  2000/07/10 19:32:05  vasilche
* Avoid one more internal compiler error of WorkShop C++.
*
* Revision 1.45  2000/07/10 19:01:00  vasilche
* Avoid internal WorkShop C++ compiler error.
*
* Revision 1.44  2000/07/03 18:42:38  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.43  2000/06/16 20:01:21  vasilche
* Avoid use of unexpected_exception() which is unimplemented on Mac.
*
* Revision 1.42  2000/06/16 16:31:08  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.41  2000/06/01 19:06:58  vasilche
* Added parsing of XML data.
*
* Revision 1.40  2000/05/25 13:27:13  vasilche
* Fixed error with mixing list<> and set<>.
*
* Revision 1.39  2000/05/24 20:50:51  vasilche
* Fixed compilation error.
*
* Revision 1.38  2000/05/24 20:08:15  vasilche
* Implemented XML dump.
*
* Revision 1.37  2000/05/09 16:38:34  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.36  2000/05/04 16:22:22  vasilche
* Cleaned and optimized blocks and members.
*
* Revision 1.35  2000/04/10 21:01:39  vasilche
* Fixed Erase for map/set.
* Added iteratorbase.hpp header for basic internal classes.
*
* Revision 1.34  2000/04/10 18:01:52  vasilche
* Added Erase() for STL types in type iterators.
*
* Revision 1.33  2000/03/29 15:55:22  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.32  2000/03/10 15:01:42  vasilche
* Fixed OPTIONAL members reading.
*
* Revision 1.31  2000/03/07 14:05:32  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.30  2000/02/17 20:02:29  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.29  2000/01/10 19:46:34  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.28  2000/01/05 19:43:47  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.27  1999/12/28 21:04:22  vasilche
* Removed three more implicit virtual destructors.
*
* Revision 1.26  1999/12/28 18:55:40  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.25  1999/12/17 19:04:54  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.24  1999/12/01 17:36:21  vasilche
* Fixed CHOICE processing.
*
* Revision 1.23  1999/10/26 15:25:21  vasilche
* Added multiset implementation.
*
* Revision 1.22  1999/10/08 21:00:39  vasilche
* Implemented automatic generation of unnamed ASN.1 types.
*
* Revision 1.21  1999/09/27 14:17:59  vasilche
* Fixed bug with overloaded construtors of Block.
*
* Revision 1.20  1999/09/22 20:11:51  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.19  1999/09/14 18:54:06  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.18  1999/09/01 17:38:03  vasilche
* Fixed vector<char> implementation.
* Added explicit naming of class info.
* Moved IMPLICIT attribute from member info to class info.
*
* Revision 1.17  1999/08/31 17:50:04  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.16  1999/07/20 18:22:57  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.15  1999/07/19 15:50:20  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.14  1999/07/15 19:35:06  vasilche
* Implemented map<K, V>.
*
* Revision 1.13  1999/07/15 16:59:55  vasilche
* Fixed template use in typedef.
*
* Revision 1.12  1999/07/15 16:54:44  vasilche
* Implemented vector<X> & vector<char> as special case.
*
* Revision 1.11  1999/07/13 20:18:08  vasilche
* Changed types naming.
*
* Revision 1.10  1999/07/01 17:55:22  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.9  1999/06/30 16:04:38  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.8  1999/06/24 14:44:46  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/16 20:35:25  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.6  1999/06/15 16:20:08  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.5  1999/06/11 19:15:50  vasilche
* Working binary serialization and deserialization of first test object.
*
* Revision 1.4  1999/06/10 21:06:42  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.3  1999/06/09 18:39:00  vasilche
* Modified templates to work on Sun.
*
* Revision 1.2  1999/06/04 20:51:39  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:31  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <serial/typeinfo.hpp>
#include <serial/continfo.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/typeref.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objhook.hpp>
#include <serial/memberid.hpp>
#include <serial/ptrinfo.hpp>
#include <set>
#include <map>
#include <list>
#include <vector>
#include <memory>

BEGIN_NCBI_SCOPE

class CClassTypeInfo;

template<typename Data>
class CStlClassInfo_auto_ptr : public CPointerTypeInfo
{
    typedef CPointerTypeInfo CParent;
public:
    typedef Data TDataType;
    typedef auto_ptr<TDataType> TObjectType;

    CStlClassInfo_auto_ptr(TTypeInfo typeInfo)
        : CParent(typeInfo)
        { }
    CStlClassInfo_auto_ptr(const CTypeRef& typeRef)
        : CParent(typeRef)
        { }
    
    TConstObjectPtr x_GetObjectPointer(TConstObjectPtr object) const
        {
            return static_cast<const TObjectType*>(object)->get();
        }
    void SetObjectPointer(TObjectPtr object, TObjectPtr data) const
        {
            static_cast<TObjectType*>(object)->
                reset(static_cast<TDataType*>(data));
        }

    virtual size_t GetSize(void) const
        {
            return sizeof(TObjectType);
        }

    static TTypeInfo GetTypeInfo(TTypeInfo info)
        {
            return new CStlClassInfo_auto_ptr<Data>(info);
        }
};

template<typename Data>
class CRefTypeInfo : public CPointerTypeInfo
{
    typedef CPointerTypeInfo CParent;
public:
    typedef Data TDataType;
    typedef CRef<TDataType> TObjectType;

    CRefTypeInfo(TTypeInfo typeInfo)
        : CParent(typeInfo)
        { }
    CRefTypeInfo(const CTypeRef& typeRef)
        : CParent(typeRef)
        { }
    
    TConstObjectPtr x_GetObjectPointer(TConstObjectPtr object) const
        {
            return static_cast<const TObjectType*>(object)->GetPointerOrNull();
        }
    void SetObjectPointer(TObjectPtr object, TObjectPtr data) const
        {
            static_cast<TObjectType*>(object)->
                Reset(static_cast<TDataType*>(data));
        }

    virtual size_t GetSize(void) const
        {
            return sizeof(TObjectType);
        }

    static TTypeInfo GetTypeInfo(TTypeInfo info)
        {
            return new CRefTypeInfo<Data>(info);
        }
};

template<class Container>
class CStlElementConstIterator : public CContainerTypeInfo::CConstIterator
{
    typedef CContainerTypeInfo::CConstIterator CParent;
public:
    typedef const Container TContainer;
    typedef const typename Container::value_type TValue;
    typedef typename Container::const_iterator TIterator;

    CParent* Clone(void) const
        {
            return new CStlElementConstIterator<Container>(*this);
        }

    bool Init(TConstObjectPtr containerPtr)
        {
            TContainer* c = m_Container =
                static_cast<TContainer*>(containerPtr);
            return (m_Iterator = c->begin()) != c->end();
        }
    bool Next(void)
        {
            return ++m_Iterator != m_Container->end();
        }
    TConstObjectPtr GetElementPtr(void) const
        {
            TValue& element = *m_Iterator;
            return &element;
        }

private:
    TContainer* m_Container;
    TIterator m_Iterator;
};

template<class Container>
class CStlElementIterator : public CContainerTypeInfo::CIterator
{
    typedef CContainerTypeInfo::CIterator CParent;
public:
    typedef Container TContainer;
    typedef typename Container::value_type TValue;
    typedef typename Container::iterator TIterator;

    CParent* Clone(void) const
        {
            return new CStlElementIterator<Container>(*this);
        }

    bool Init(TObjectPtr containerPtr)
        {
            TContainer* c = m_Container =
                static_cast<TContainer*>(containerPtr);
            return (m_Iterator = c->begin()) != c->end();
        }
    bool Next(void)
        {
            return ++m_Iterator != m_Container->end();
        }
    TObjectPtr GetElementPtr(void) const
        {
            TValue& element = *m_Iterator;
            return &element;
        }
    bool Erase(void)
        {
            TContainer* c = m_Container;
            return (m_Iterator = c->erase(m_Iterator)) != c->end();
        }

private:
    TContainer* m_Container;
    TIterator m_Iterator;
};

template<class Container>
class CStlElementIterator_set : public CContainerTypeInfo::CIterator
{
    typedef CContainerTypeInfo::CIterator CParent;
public:
    typedef Container TContainer;
    typedef typename Container::value_type TValue;
    typedef typename Container::iterator TIterator;

    CParent* Clone(void) const
        {
            return new CStlElementIterator_set<Container>(*this);
        }

    bool Init(TObjectPtr containerPtr)
        {
            TContainer* c = m_Container =
                static_cast<TContainer*>(containerPtr);
            return (m_Iterator = c->begin()) != c->end();
        }
    bool Next(void)
        {
            return ++m_Iterator != m_Container->end();
        }
    TObjectPtr GetElementPtr(void) const
        {
            THROW1_TRACE(runtime_error,
                         "cannot get pointer to element of set");
        }
    bool Erase(void)
        {
            TContainer* c = m_Container;
            TIterator erase = m_Iterator++;
            c->erase(erase);
            return m_Iterator != c->end();
        }

private:
    TContainer* m_Container;
    TIterator m_Iterator;
};

class CStlOneArgTemplate : public CContainerTypeInfo
{
    typedef CContainerTypeInfo CParent;
public:

    CStlOneArgTemplate(TTypeInfo dataType, bool randomOrder);
    CStlOneArgTemplate(const CTypeRef& dataType, bool randomOrder);

    const CMemberId& GetDataId(void) const
        {
            return m_DataId;
        }
    void SetDataId(const CMemberId& id);

    TTypeInfo GetDataTypeInfo(void) const
        {
            return m_DataType.Get();
        }

    virtual TTypeInfo GetElementType(void) const;

private:
    CMemberId m_DataId;
    CTypeRef m_DataType;
};

class CStlTwoArgsTemplate : public CContainerTypeInfo
{
    typedef CContainerTypeInfo CParent;
public:

    CStlTwoArgsTemplate(TTypeInfo keyType, TTypeInfo valueType,
                        bool randomOrder);
    CStlTwoArgsTemplate(const CTypeRef& keyType, const CTypeRef& valueType,
                        bool randomOrder);

    const CMemberId& GetKeyId(void) const
        {
            return m_KeyId;
        }
    const CMemberId& GetValueId(void) const
        {
            return m_ValueId;
        }
    void SetKeyId(const CMemberId& id);
    void SetValueId(const CMemberId& id);

    TTypeInfo GetKeyTypeInfo(void) const
        {
            return m_KeyType.Get();
        }
    TTypeInfo GetValueTypeInfo(void) const
        {
            return m_ValueType.Get();
        }

private:
    CMemberId m_KeyId;
    CMemberId m_ValueId;
    CTypeRef m_KeyType;
    CTypeRef m_ValueType;
};

template<typename List>
class CStlListTemplateBase : public CStlOneArgTemplate
{
    typedef CStlOneArgTemplate CParent;
public:
    typedef List TObjectType;
    typedef typename TObjectType::value_type value_type;

    CStlListTemplateBase(TTypeInfo dataType, bool randomOrder = false)
        : CParent(dataType, randomOrder)
        {
        }
    CStlListTemplateBase(const CTypeRef& dataType, bool randomOrder = false)
        : CParent(dataType, randomOrder)
        {
        }

protected:
    static TObjectType& Get(TObjectPtr object)
        {
            return *static_cast<TObjectType*>(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectType*>(object);
        }
    static const value_type& GetElement(TConstObjectPtr elementPtr)
        {
            return *static_cast<const value_type*>(elementPtr);
        }

    virtual TObjectPtr Create(void) const
        {
            return new TObjectType;
        }

    virtual size_t GetSize(void) const
        {
            return sizeof(TObjectType);
        }
    virtual bool IsDefault(TConstObjectPtr object) const
        {
            return Get(object).empty();
        }

    virtual void SetDefault(TObjectPtr dst) const
        {
            Get(dst).clear();
        }

    CConstIterator* NewConstIterator(void) const
        {
            return new CStlElementConstIterator<List>;
        }
    CIterator* NewIterator(void) const
        {
            return new CStlElementIterator<List>;
        }

    void AddElement(TObjectPtr containerPtr, TConstObjectPtr elementPtr) const
        {
            Get(containerPtr).push_back(GetElement(elementPtr));
        }
    void AddElement(TObjectPtr containerPtr, CObjectIStream& in) const
        {
            List& container = Get(containerPtr);
            TTypeInfo elementType = GetDataTypeInfo();
            {
                // push empty element
                value_type value;
                container.push_back(value);
            }
            in.ReadObject(&container.back(), elementType);
        }
};

template<typename Data>
class CStlClassInfo_list : public CStlListTemplateBase< list<Data> >
{
    typedef CStlListTemplateBase< list<Data> > CParent;
public:
    CStlClassInfo_list(TTypeInfo typeInfo, bool randomOrder = false)
        : CParent(typeInfo, randomOrder)
        {
        }
    CStlClassInfo_list(const CTypeRef& typeRef, bool randomOrder = false)
        : CParent(typeRef, randomOrder)
        {
        }

    static TTypeInfo GetTypeInfo(TTypeInfo info)
        {
            return new CStlClassInfo_list<Data>(info);
        }

    static TTypeInfo GetSetTypeInfo(TTypeInfo info)
        {
            return new CStlClassInfo_list<Data>(info, true);
        }
};

template<typename Data>
class CStlClassInfo_vector : public CStlListTemplateBase< vector<Data> >
{
    typedef CStlListTemplateBase< vector<Data> > CParent;
public:
    CStlClassInfo_vector(TTypeInfo info)
        : CParent(info)
        { }
    CStlClassInfo_vector(const CTypeRef& typeRef)
        : CParent(typeRef)
        { }

    static TTypeInfo GetTypeInfo(TTypeInfo info)
        {
            return new CStlClassInfo_vector<Data>(info);
        }
};

template<class Set>
class CStlClassInfoSetBase : public CStlOneArgTemplate
{
    typedef CStlOneArgTemplate CParent;
public:
    typedef Set TObjectType;
    typedef typename Set::value_type value_type;

    CStlClassInfoSetBase(TTypeInfo dataType)
        : CParent(dataType, true)
        {
        }
    CStlClassInfoSetBase(const CTypeRef& dataType)
        : CParent(dataType, true)
        {
        }

protected:
    static TObjectType& Get(TObjectPtr object)
        {
            return *static_cast<TObjectType*>(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectType*>(object);
        }
    static const value_type& GetElement(TConstObjectPtr elementPtr)
        {
            return *static_cast<const value_type*>(elementPtr);
        }

    virtual TObjectPtr Create(void) const
        {
            return new TObjectType;
        }

    virtual size_t GetSize(void) const
        {
            return sizeof(TObjectType);
        }
    virtual bool IsDefault(TConstObjectPtr object) const
        {
            return Get(object).empty();
        }

    virtual void SetDefault(TObjectPtr dst) const
        {
            Get(dst).clear();
        }

    CConstIterator* NewConstIterator(void) const
        {
            return new CStlElementConstIterator<Set>;
        }
    CIterator* NewIterator(void) const
        {
            return new CStlElementIterator_set<Set>;
        }
};

template<typename Data>
class CStlClassInfo_set : public CStlClassInfoSetBase< set<Data> >
{
    typedef CStlClassInfoSetBase< set<Data> > CParent;
public:
    typedef typename CParent::TObjectType TObjectType;
    typedef typename CParent::value_type value_type;

    CStlClassInfo_set(TTypeInfo info)
        : CParent(info)
        {
        }
    CStlClassInfo_set(const CTypeRef& typeRef)
        : CParent(typeRef)
        {
        }

    static TTypeInfo GetTypeInfo(TTypeInfo info)
        {
            return new CStlClassInfo_set<Data>(info);
        }

protected:
    void InsertElement(TObjectPtr containerPtr,
                       TConstObjectPtr elementPtr) const
        {
            if ( !Get(containerPtr).insert(GetElement(elementPtr)).second )
                ThrowDuplicateElementError();
        }
    void AddElement(TObjectPtr containerPtr, TConstObjectPtr elementPtr) const
        {
            InsertElement(containerPtr, elementPtr);
        }
    void AddElement(TObjectPtr containerPtr, CObjectIStream& in) const
        {
            Data data;
            in.ReadObject(&data, GetDataTypeInfo());
            InsertElement(containerPtr, &data);
        }
};

template<typename Data>
class CStlClassInfo_multiset : public CStlClassInfoSetBase< multiset<Data> >
{
    typedef CStlClassInfoSetBase< multiset<Data> > CParent;
public:
    CStlClassInfo_multiset(TTypeInfo dataType)
        : CParent(dataType)
        {
        }
    CStlClassInfo_multiset(const CTypeRef& dataType)
        : CParent(dataType)
        {
        }

    static TTypeInfo GetTypeInfo(TTypeInfo info)
        {
            return new CStlClassInfo_multiset<Data>(info);
        }

protected:
    void InsertElement(TObjectPtr containerPtr,
                       TConstObjectPtr elementPtr) const
        {
            Get(containerPtr).insert(GetElement(elementPtr));
        }
    void AddElement(TObjectPtr containerPtr, TConstObjectPtr elementPtr) const
        {
            InsertElement(containerPtr, elementPtr);
        }
    void AddElement(TObjectPtr containerPtr, CObjectIStream& in) const
        {
            Data data;
            in.ReadObject(&data, GetDataTypeInfo());
            InsertElement(containerPtr, &data);
        }
};

class CStlClassInfoMapImpl : public CStlTwoArgsTemplate
{
    typedef CStlTwoArgsTemplate CParent;
public:

    CStlClassInfoMapImpl(TTypeInfo keyType,
                         TConstObjectPtr keyOffset,
                         TTypeInfo valueType,
                         TConstObjectPtr valueOffset);
    CStlClassInfoMapImpl(const CTypeRef& keyType,
                         TConstObjectPtr keyOffset,
                         const CTypeRef& valueType,
                         TConstObjectPtr valueOffset);

    virtual TTypeInfo GetElementType(void) const;

    const CClassTypeInfo* GetElementClassType(void) const;

private:
    mutable AutoPtr<const CClassTypeInfo> m_ElementType;
    TConstObjectPtr m_KeyOffset, m_ValueOffset;
};

template<class Map>
class CStlClassInfoMapBase : public CStlClassInfoMapImpl
{
    typedef CStlClassInfoMapImpl CParent;

public:
    typedef Map TObjectType;
    typedef typename TObjectType::value_type value_type;
    typedef typename TObjectType::key_type key_type;
    typedef typename TObjectType::mapped_type mapped_type;

    CStlClassInfoMapBase(TTypeInfo keyType, TTypeInfo dataType)
        : CParent(keyType, &static_cast<const value_type*>(0)->first,
                  dataType, &static_cast<const value_type*>(0)->second)
        {
        }
    CStlClassInfoMapBase(const CTypeRef& keyType, const CTypeRef& dataType)
        : CParent(keyType, &static_cast<const value_type*>(0)->first,
                  dataType, &static_cast<const value_type*>(0)->second)
        {
        }

protected:
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectType*>(object);
        }
    static TObjectType& Get(TObjectPtr object)
        {
            return *static_cast<TObjectType*>(object);
        }
    static const value_type& GetElement(TConstObjectPtr elementPtr)
        {
            return *static_cast<const value_type*>(elementPtr);
        }

    virtual size_t GetSize(void) const
        {
            return sizeof(TObjectType);
        }

    virtual TObjectPtr Create(void) const
        {
            return new TObjectType;
        }

    virtual bool IsDefault(TConstObjectPtr object) const
        {
            return Get(object).empty();
        }

    virtual void SetDefault(TObjectPtr dst) const
        {
            Get(dst).clear();
        }

    CConstIterator* NewConstIterator(void) const
        {
            return new CStlElementConstIterator<Map>;
        }
    CIterator* NewIterator(void) const
        {
            return new CStlElementIterator_set<Map>;
        }
};

template<typename Key, typename Value>
class CStlClassInfo_map : public CStlClassInfoMapBase< map<Key, Value> >
{
    typedef CStlClassInfoMapBase< map<Key, Value> > CParent;
public:
    typedef typename CParent::TObjectType TObjectType;
    typedef typename CParent::value_type value_type;

    CStlClassInfo_map(TTypeInfo keyType, TTypeInfo dataType)
        : CParent(keyType, dataType)
        {
        }
    CStlClassInfo_map(const CTypeRef& keyType, const CTypeRef& dataType)
        : CParent(keyType, dataType)
        {
        }

    static TTypeInfo GetTypeInfo(TTypeInfo keyInfo, TTypeInfo dataInfo)
        {
            return new CStlClassInfo_map<Key, Value>(keyInfo, dataInfo);
        }

protected:
    void InsertElement(TObjectPtr containerPtr,
                       TConstObjectPtr elementPtr) const
        {
            if ( !Get(containerPtr).insert(GetElement(elementPtr)).second )
                ThrowDuplicateElementError();
        }
    void AddElement(TObjectPtr containerPtr, TConstObjectPtr elementPtr) const
        {
            InsertElement(containerPtr, elementPtr);
        }
    void AddElement(TObjectPtr containerPtr, CObjectIStream& in) const
        {
            value_type data;
            in.ReadObject(&data, GetElementClassType());
            InsertElement(containerPtr, &data);
        }
};

template<typename Key, typename Value>
class CStlClassInfo_multimap : public CStlClassInfoMapBase< multimap<Key, Value> >
{
    typedef CStlClassInfoMapBase< multimap<Key, Value> > CParent;
public:
    typedef typename CParent::value_type value_type;

    CStlClassInfo_multimap(TTypeInfo keyInfo, TTypeInfo dataInfo)
        : CParent(keyType, dataType)
        {
        }
    CStlClassInfo_multimap(const CTypeRef& keyType, const CTypeRef& dataType)
        : CParent(keyType, dataType)
        {
        }

    static TTypeInfo GetTypeInfo(TTypeInfo keyInfo, TTypeInfo dataInfo)
        {
            return new CStlClassInfo_multimap<Key, Value>(keyInfo, dataInfo);
        }

protected:
    void InsertElement(TObjectPtr containerPtr,
                       TConstObjectPtr elementPtr) const
        {
            Get(containerPtr).insert(GetElement(elementPtr));
        }
    void AddElement(TObjectPtr containerPtr, TConstObjectPtr elementPtr) const
        {
            InsertElement(containerPtr, elementPtr);
        }
    void AddElement(TObjectPtr containerPtr, CObjectIStream& in) const
        {
            value_type data;
            in.ReadValue(&data, GetElementClassType());
            InsertElement(containerPtr, &data);
        }
};

END_NCBI_SCOPE

#endif  /* STLTYPES__HPP */

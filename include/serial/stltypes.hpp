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
* Revision 1.54  2000/10/13 20:22:47  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.53  2000/10/13 16:28:33  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.52  2000/10/03 17:22:35  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.51  2000/09/19 20:16:53  vasilche
* Fixed type in CStlClassInfo_auto_ptr.
* Added missing include serialutil.hpp.
*
* Revision 1.50  2000/09/18 20:00:11  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.49  2000/09/01 18:16:47  vasilche
* Added files to MSVC project.
* Fixed errors on MSVC.
*
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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbiutil.hpp>
#include <set>
#include <map>
#include <list>
#include <vector>
#include <memory>
#include <serial/serialutil.hpp>
#include <serial/stltypesimpl.hpp>
#include <serial/ptrinfo.hpp>

BEGIN_NCBI_SCOPE

template<typename Data>
class CStlClassInfo_auto_ptr
{
public:
    typedef Data TDataType;
    typedef auto_ptr<TDataType> TObjectType;

    static TTypeInfo GetTypeInfo(TTypeInfo dataType)
        {
            return CStlClassInfoUtil::Get_auto_ptr(dataType, &CreateTypeInfo);
        }
    static TTypeInfo CreateTypeInfo(TTypeInfo dataType)
        {
            CPointerTypeInfo* typeInfo =
                new CPointerTypeInfo(sizeof(TObjectType), dataType);
            typeInfo->SetFunctions(&GetData, &SetData);
            return typeInfo;
        }

protected:
    static TObjectPtr GetData(const CPointerTypeInfo* /*objectType*/,
                              TObjectPtr objectPtr)
        {
            return CTypeConverter<TObjectType>::Get(objectPtr).get();
        }
    static void SetData(const CPointerTypeInfo* /*objectType*/,
                        TObjectPtr objectPtr,
                        TObjectPtr dataPtr)
        {
            CTypeConverter<TObjectType>::Get(objectPtr).
                reset(&CTypeConverter<TDataType>::Get(dataPtr));
        }
};

template<typename Data>
class CRefTypeInfo
{
public:
    typedef Data TDataType;
    typedef CRef<TDataType> TObjectType;

    static TTypeInfo GetTypeInfo(TTypeInfo dataType)
        {
            return CStlClassInfoUtil::Get_CRef(dataType, &CreateTypeInfo);
        }
    static TTypeInfo CreateTypeInfo(TTypeInfo dataType)
        {
            CPointerTypeInfo* typeInfo =
                new CPointerTypeInfo(sizeof(TObjectType), dataType);
            typeInfo->SetFunctions(&GetData, &SetData);
            return typeInfo;
        }

protected:
    static TObjectPtr GetData(const CPointerTypeInfo* /*objectType*/,
                              TObjectPtr objectPtr)
        {
            return CTypeConverter<TObjectType>::Get(objectPtr).GetPointer();
        }
    static void SetData(const CPointerTypeInfo* /*objectType*/,
                        TObjectPtr objectPtr,
                        TObjectPtr dataPtr)
        {
            CTypeConverter<TObjectType>::Get(objectPtr).
                Reset(&CTypeConverter<TDataType>::Get(dataPtr));
        }
};

template<typename Data>
class CAutoPtrTypeInfo
{
public:
    typedef Data TDataType;
    typedef AutoPtr<TDataType> TObjectType;

    static TTypeInfo GetTypeInfo(TTypeInfo dataType)
        {
            return CStlClassInfoUtil::Get_AutoPtr(dataType, &CreateTypeInfo);
        }
    static TTypeInfo CreateTypeInfo(TTypeInfo dataType)
        {
            CPointerTypeInfo* typeInfo =
                new CPointerTypeInfo(sizeof(TObjectType), dataType);
            typeInfo->SetFunctions(&GetData, &SetData);
            return typeInfo;
        }

protected:
    static TObjectPtr GetData(const CPointerTypeInfo* /*objectType*/,
                              TObjectPtr objectPtr)
        {
            return CTypeConverter<TObjectType>::Get(objectPtr).get();
        }
    static void SetData(const CPointerTypeInfo* /*objectType*/,
                        TObjectPtr objectPtr,
                        TObjectPtr dataPtr)
        {
            CTypeConverter<TObjectType>::Get(objectPtr).
                reset(&CTypeConverter<TDataType>::Get(dataPtr));
        }
};

template<class Container>
class CStlClassInfoFunctions
{
public:
    typedef Container TObjectType;
    typedef typename TObjectType::value_type TElementType;

    static TObjectType& Get(TObjectPtr objectPtr)
        {
            return CTypeConverter<TObjectType>::Get(objectPtr);
        }
    static const TObjectType& Get(TConstObjectPtr objectPtr)
        {
            return CTypeConverter<TObjectType>::Get(objectPtr);
        }

    static TObjectPtr CreateContainer(TTypeInfo /*objectType*/)
        {
            return new TObjectType();
        }

    static bool IsDefault(TConstObjectPtr objectPtr)
        {
            return Get(objectPtr).empty();
        }
    static void SetDefault(TObjectPtr objectPtr)
        {
            Get(objectPtr).clear();
        }

    static void AddElement(const CContainerTypeInfo* /*containerType*/,
                           TObjectPtr containerPtr, TConstObjectPtr elementPtr)
        {
            Get(containerPtr).push_back(CTypeConverter<TElementType>::Get(elementPtr));
        }
    static void AddElementIn(const CContainerTypeInfo* containerType,
                             TObjectPtr containerPtr, CObjectIStream& in)
        {
            Get(containerPtr).push_back(TElementType());
            containerType->GetElementType()->ReadData(in, &Get(containerPtr).back());
        }

    static void SetMemFunctions(CStlOneArgTemplate* info)
        {
            info->SetMemFunctions(&CreateContainer, &IsDefault, &SetDefault);
        }
    static void SetAddElementFunctions(CStlOneArgTemplate* info)
        {
            info->SetAddElementFunctions(&AddElement, &AddElementIn);
        }
};

template<class Container>
class CStlClassInfoFunctions_set : public CStlClassInfoFunctions<Container>
{
    typedef CStlClassInfoFunctions<Container> CParent;
public:
    typedef typename CParent::TObjectType TObjectType;
    typedef typename TObjectType::value_type TElementType;

    static void InsertElement(TObjectPtr containerPtr,
                              const TElementType& element)
        {
            if ( !Get(containerPtr).insert(element).second )
                CStlClassInfoUtil::ThrowDuplicateElementError();
        }
    static void AddElement(const CContainerTypeInfo* /*containerType*/,
                           TObjectPtr containerPtr, TConstObjectPtr elementPtr)
        {
            InsertElement(containerPtr,
                          CTypeConverter<TElementType>::Get(elementPtr));
        }
    static void AddElementIn(const CContainerTypeInfo* containerType,
                             TObjectPtr containerPtr, CObjectIStream& in)
        {
            TElementType data;
            containerType->GetElementType()->ReadData(in, &data);
            InsertElement(containerPtr, data);
        }

    static void SetAddElementFunctions(CStlOneArgTemplate* info)
        {
            info->SetAddElementFunctions(&AddElement, &AddElementIn);
        }
};

template<class Container>
class CStlClassInfoFunctions_multiset :
    public CStlClassInfoFunctions<Container>
{
    typedef CStlClassInfoFunctions<Container> CParent;
public:
    typedef typename CParent::TObjectType TObjectType;
    typedef typename TObjectType::value_type TElementType;

    static void InsertElement(TObjectPtr containerPtr,
                              const TElementType& element)
        {
            Get(containerPtr).insert(element);
        }
    static void AddElement(const CContainerTypeInfo* /*containerType*/,
                           TObjectPtr containerPtr, TConstObjectPtr elementPtr)
        {
            InsertElement(containerPtr,
                          CTypeConverter<TElementType>::Get(elementPtr));
        }
    static void AddElementIn(const CContainerTypeInfo* containerType,
                             TObjectPtr containerPtr, CObjectIStream& in)
        {
            TElementType data;
            containerType->GetElementType()->ReadData(in, &data);
            InsertElement(containerPtr, data);
        }

    static void SetAddElementFunctions(CStlOneArgTemplate* info)
        {
            info->SetAddElementFunctions(&AddElement, &AddElementIn);
        }
};

template<class Container, class Iterator,
    typename ContainerPtr, typename ElementRef, typename ObjectPtr>
class CStlClassInfoFunctionsIBase :
    public CStlClassInfoFunctions<Container>
{
public:
    typedef CContainerTypeInfo::TIteratorDataPtr TIteratorDataPtr;
    typedef CContainerTypeInfo::TNewIteratorResult TNewIteratorResult;

    typedef pair<Iterator, ContainerPtr> TIterator;
    static TIterator* It(TIteratorDataPtr data)
        {
            return static_cast<TIterator*>(data);
        }

    static TNewIteratorResult
    InitIterator(const CContainerTypeInfo* /*containerType*/,
                 ObjectPtr containerPtr)
        {
            TIterator* data =
                new TIterator(Get(containerPtr).begin(),
                              &Get(containerPtr));
            return TNewIteratorResult(data,
                                      data->first != Get(containerPtr).end());
        }
    static void ReleaseIterator(TIteratorDataPtr data)
        {
            delete It(data);
        }
    static TIteratorDataPtr CopyIterator(TIteratorDataPtr data)
        {
            return new TIterator(*It(data));
        }
    static TNewIteratorResult NextElement(TIteratorDataPtr data)
        {
            TIterator* it = It(data);
            return TNewIteratorResult(it, ++it->first != it->second->end());
        }
    static ObjectPtr GetElementPtr(TIteratorDataPtr data)
        {
            ElementRef e= *It(data)->first;
            return &e;
        }
};

template<class Container>
class CStlClassInfoFunctionsCI :
    public CStlClassInfoFunctionsIBase<Container, typename Container::const_iterator, const Container*, const typename Container::value_type&, TConstObjectPtr>
{
public:
    static void SetIteratorFunctions(CStlOneArgTemplate* info)
        {
            info->SetConstIteratorFunctions(&InitIterator, &ReleaseIterator,
                                            &CopyIterator, &NextElement,
                                            &GetElementPtr);
        }
};

template<class Container>
class CStlClassInfoFunctionsI :
    public CStlClassInfoFunctionsIBase<Container, typename Container::iterator, Container*, typename Container::value_type&, TObjectPtr>
{
    typedef CStlClassInfoFunctionsIBase<Container, typename Container::iterator, Container*, typename Container::value_type&, TObjectPtr> CParent;
public:
    typedef typename CParent::TIterator TIterator;
    typedef CContainerTypeInfo::TIteratorDataPtr TIteratorDataPtr;
    typedef CContainerTypeInfo::TNewIteratorResult TNewIteratorResult;
    
    static TNewIteratorResult EraseElement(TIteratorDataPtr data)
        {
            TIterator* it = It(data);
            Container* c = it->second;
            return TNewIteratorResult(it,
                                      (it->first = c->erase(it->first)) != c->end());
        }

    static void SetIteratorFunctions(CStlOneArgTemplate* info)
        {
            info->SetIteratorFunctions(&InitIterator, &ReleaseIterator,
                                       &CopyIterator, &NextElement,
                                       &GetElementPtr, &EraseElement);
        }
};

template<class Container>
class CStlClassInfoFunctionsI_set :
    public CStlClassInfoFunctionsIBase<Container, typename Container::iterator, Container*, typename Container::value_type&, TObjectPtr>
{
    typedef CStlClassInfoFunctionsIBase<Container, typename Container::iterator, Container*, typename Container::value_type&, TObjectPtr> CParent;
public:
    typedef typename CParent::TIterator TIterator;
    typedef CContainerTypeInfo::TIteratorDataPtr TIteratorDataPtr;
    typedef CContainerTypeInfo::TNewIteratorResult TNewIteratorResult;
    
    static TObjectPtr GetElementPtr(TIteratorDataPtr /*data*/)
        {
            CStlClassInfoUtil::CannotGetElementOfSet();
            return 0;
        }
    static TNewIteratorResult EraseElement(TIteratorDataPtr data)
        {
            TIterator* it = It(data);
            Container* c = it->second;
            typename TObjectType::iterator erase = it->first++;
            c->erase(erase);
            return TNewIteratorResult(it, it->first != c->end());
        }

    static void SetIteratorFunctions(CStlOneArgTemplate* info)
        {
            info->SetIteratorFunctions(&InitIterator, &ReleaseIterator,
                                       &CopyIterator, &NextElement,
                                       &GetElementPtr, &EraseElement);
        }
};

template<typename Data>
class CStlClassInfo_list
{
public:
    typedef list<Data> TObjectType;

    static TTypeInfo GetTypeInfo(TTypeInfo elementType)
        {
            return CStlClassInfoUtil::Get_list(elementType, &CreateTypeInfoC);
        }
    static CTypeInfo* CreateTypeInfo(TTypeInfo elementType)
        {
            CStlOneArgTemplate* info =
                new CStlOneArgTemplate(sizeof(TObjectType), elementType,
                                       false);
            SetFunctions(info);
            return info;
        }
    static TTypeInfo CreateTypeInfoC(TTypeInfo elementType)
        {
            return CreateTypeInfo(elementType);
        }

    static TTypeInfo GetSetTypeInfo(TTypeInfo elementType)
        {
            return CStlClassInfoUtil::GetSet_list(elementType,
                                                  &CreateSetTypeInfoC);
        }
    static CTypeInfo* CreateSetTypeInfo(TTypeInfo elementType)
        {
            CStlOneArgTemplate* info =
                new CStlOneArgTemplate(sizeof(TObjectType), elementType,
                                       true);
            SetFunctions(info);
            return info;
        }
    static TTypeInfo CreateSetTypeInfoC(TTypeInfo elementType)
        {
            return CreateSetTypeInfo(elementType);
        }

    static void SetFunctions(CStlOneArgTemplate* info)
        {
            CStlClassInfoFunctions<TObjectType>::SetMemFunctions(info);
            CStlClassInfoFunctions<TObjectType>::SetAddElementFunctions(info);
            CStlClassInfoFunctionsCI<TObjectType>::SetIteratorFunctions(info);
            CStlClassInfoFunctionsI<TObjectType>::SetIteratorFunctions(info);
        }
};

template<typename Data>
class CStlClassInfo_vector
{
public:
    typedef vector<Data> TObjectType;

    static TTypeInfo GetTypeInfo(TTypeInfo elementType)
        {
            return CStlClassInfoUtil::Get_vector(elementType,
                                                 &CreateTypeInfoC);
        }
    static CTypeInfo* CreateTypeInfo(TTypeInfo elementType)
        {
            CStlOneArgTemplate* info =
                new CStlOneArgTemplate(sizeof(TObjectType), elementType,
                                       false);

            CStlClassInfoFunctions<TObjectType>::SetMemFunctions(info);
            CStlClassInfoFunctions<TObjectType>::SetAddElementFunctions(info);
            CStlClassInfoFunctionsCI<TObjectType>::SetIteratorFunctions(info);
            CStlClassInfoFunctionsI<TObjectType>::SetIteratorFunctions(info);

            return info;
        }
    static TTypeInfo CreateTypeInfoC(TTypeInfo elementType)
        {
            return CreateTypeInfo(elementType);
        }
};

template<typename Data>
class CStlClassInfo_set
{
public:
    typedef set<Data> TObjectType;

    static TTypeInfo GetTypeInfo(TTypeInfo elementType)
        {
            return CStlClassInfoUtil::Get_set(elementType, &CreateTypeInfoC);
        }
    static CTypeInfo* CreateTypeInfo(TTypeInfo elementType)
        {
            CStlOneArgTemplate* info =
                new CStlOneArgTemplate(sizeof(TObjectType), elementType,
                                       true);

            CStlClassInfoFunctions<TObjectType>::SetMemFunctions(info);
            CStlClassInfoFunctions_set<TObjectType>::SetAddElementFunctions(info);
            CStlClassInfoFunctionsCI<TObjectType>::SetIteratorFunctions(info);
            CStlClassInfoFunctionsI_set<TObjectType>::SetIteratorFunctions(info);

            return info;
        }
    static TTypeInfo CreateTypeInfoC(TTypeInfo elementType)
        {
            return CreateTypeInfo(elementType);
        }
};

template<typename Data>
class CStlClassInfo_multiset
{
public:
    typedef multiset<Data> TObjectType;

    static TTypeInfo GetTypeInfo(TTypeInfo elementType)
        {
            return CStlClassInfoUtil::Get_multiset(elementType,
                                                   &CreateTypeInfoC);
        }
    static CTypeInfo* CreateTypeInfo(TTypeInfo elementType)
        {
            CStlOneArgTemplate* info =
                new CStlOneArgTemplate(sizeof(TObjectType), elementType,
                                       true);

            CStlClassInfoFunctions<TObjectType>::SetMemFunctions(info);
            CStlClassInfoFunctions_multiset<TObjectType>::SetAddElementFunctions(info);
            CStlClassInfoFunctionsCI<TObjectType>::SetIteratorFunctions(info);
            CStlClassInfoFunctionsI_set<TObjectType>::SetIteratorFunctions(info);

            return info;
        }
    static TTypeInfo CreateTypeInfoC(TTypeInfo elementType)
        {
            return CreateTypeInfo(elementType);
        }
};

template<typename Key, typename Value>
class CStlClassInfo_map
{
public:
    typedef map<Key, Value> TObjectType;
    typedef typename TObjectType::value_type TElementType;

    static TTypeInfo GetTypeInfo(TTypeInfo keyType, TTypeInfo valueType)
        {
            return CStlClassInfoUtil::Get_map(keyType, valueType,
                                              &CreateTypeInfoC);
        }

    static CTypeInfo* CreateTypeInfo(TTypeInfo keyType, TTypeInfo valueType)
        {
            CStlTwoArgsTemplate* info =
                new CStlTwoArgsTemplate(sizeof(TObjectType),
                                        keyType,
                                        offsetof(TElementType, first),
                                        valueType,
                                        offsetof(TElementType, second),
                                        true);
            
            CStlClassInfoFunctions<TObjectType>::SetMemFunctions(info);
            CStlClassInfoFunctions_set<TObjectType>::SetAddElementFunctions(info);
            CStlClassInfoFunctionsCI<TObjectType>::SetIteratorFunctions(info);
            CStlClassInfoFunctionsI_set<TObjectType>::SetIteratorFunctions(info);
            
            return info;
        }
    static TTypeInfo CreateTypeInfoC(TTypeInfo keyType, TTypeInfo valueType)
        {
            return CreateTypeInfo(keyType, valueType);
        }
};

template<typename Key, typename Value>
class CStlClassInfo_multimap
{
public:
    typedef multimap<Key, Value> TObjectType;
    typedef typename TObjectType::value_type TElementType;

    static TTypeInfo GetTypeInfo(TTypeInfo keyType, TTypeInfo valueType)
        {
            return CStlClassInfoUtil::Get_multimap(keyType, valueType,
                                                   &CreateTypeInfoC);
        }

    static CTypeInfo* CreateTypeInfo(TTypeInfo keyType, TTypeInfo valueType)
        {
            CStlTwoArgsTemplate* info =
                new CStlTwoArgsTemplate(sizeof(TObjectType),
                                        keyType,
                                        offsetof(TElementType, first),
                                        valueType,
                                        offsetof(TElementType, second),
                                        true);
            
            CStlClassInfoFunctions<TObjectType>::SetMemFunctions(info);
            CStlClassInfoFunctions_multiset<TObjectType>::SetAddElementFunctions(info);
            CStlClassInfoFunctionsCI<TObjectType>::SetIteratorFunctions(info);
            CStlClassInfoFunctionsI_set<TObjectType>::SetIteratorFunctions(info);
            
            return info;
        }
    static TTypeInfo CreateTypeInfoC(TTypeInfo keyType, TTypeInfo valueType)
        {
            return CreateTypeInfo(keyType, valueType);

        }
};

END_NCBI_SCOPE

#endif  /* STLTYPES__HPP */

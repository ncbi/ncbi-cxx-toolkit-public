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
#include <serial/impl/stltypesimpl.hpp>
#include <serial/impl/ptrinfo.hpp>
#include <serial/objistr.hpp>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


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
    static CTypeInfo* CreateTypeInfo(TTypeInfo dataType)
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
    static CTypeInfo* CreateTypeInfo(TTypeInfo dataType)
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
                Reset(CTypeConverter<TDataType>::GetPointer(dataPtr));
        }
};

template<typename Data>
class CConstRefTypeInfo
{
public:
    typedef Data TDataType;
    typedef CConstRef<TDataType> TObjectType;

    static TTypeInfo GetTypeInfo(TTypeInfo dataType)
        {
            return CStlClassInfoUtil::Get_CConstRef(dataType, &CreateTypeInfo);
        }
    static CTypeInfo* CreateTypeInfo(TTypeInfo dataType)
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
            // Bleh.  Need to return a void* rather than a const Data*
            return const_cast<TDataType*>
                (CTypeConverter<TObjectType>::Get(objectPtr).GetPointer());
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
    static CTypeInfo* CreateTypeInfo(TTypeInfo dataType)
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

    static TObjectPtr CreateContainer(TTypeInfo /*objectType*/,
                                      CObjectMemoryPool* /*memoryPool*/)
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

    static TObjectPtr AddElement(const CContainerTypeInfo* containerType,
                                 TObjectPtr containerPtr,
                                 TConstObjectPtr elementPtr,
                                 ESerialRecursionMode how = eRecursive)
        {
            TObjectType& container = Get(containerPtr);
#if defined(_RWSTD_VER) && !defined(_RWSTD_STRICT_ANSI)
            container.allocation_size(container.size());
#endif
            if ( elementPtr ) {
                TElementType elm;
                containerType->GetElementType()->Assign
                    (&elm, &CTypeConverter<TElementType>::Get(elementPtr), how);
                container.push_back(elm);
            }
            else {
                container.push_back(TElementType());
            }
            return &container.back();
        }
    static TObjectPtr AddElementIn(const CContainerTypeInfo* containerType,
                                   TObjectPtr containerPtr,
                                   CObjectIStream& in)
        {
            TObjectType& container = Get(containerPtr);
#if defined(_RWSTD_VER) && !defined(_RWSTD_STRICT_ANSI)
            container.allocation_size(container.size());
#endif
            container.push_back(TElementType());
            containerType->GetElementType()->ReadData(in, &container.back());
            if (in.GetDiscardCurrObject()) {
                container.pop_back();
                in.SetDiscardCurrObject(false);
                return 0;
            }
            return &container.back();
        }

    static size_t GetElementCount(const CContainerTypeInfo*,
                                  TConstObjectPtr containerPtr)
        {
            const TObjectType& container = Get(containerPtr);
            return container.size();
        }

    static void SetMemFunctions(CStlOneArgTemplate* info)
        {
            info->SetMemFunctions(&CreateContainer, &IsDefault, &SetDefault);
        }
    static void SetAddElementFunctions(CStlOneArgTemplate* info)
        {
            info->SetAddElementFunctions(&AddElement, &AddElementIn);
        }
    static void SetCountFunctions(CStlOneArgTemplate* info)
        {
            info->SetCountFunctions(&GetElementCount);
        }
};

template<class Container>
class CStlClassInfoFunctions_vec : public CStlClassInfoFunctions<Container>
{
    typedef CStlClassInfoFunctions<Container> CParent;
public:
    typedef typename CParent::TObjectType TObjectType;
    typedef typename TObjectType::value_type TElementType;

    static void ReserveElements(const CContainerTypeInfo*,
                                TObjectPtr containerPtr, size_t count)
        {
            TObjectType& container = CParent::Get(containerPtr);
            container.reserve(count);
        }

    static void SetCountFunctions(CStlOneArgTemplate* info)
        {
            info->SetCountFunctions(&CParent::GetElementCount,
                                    &ReserveElements);
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
            TObjectType& container = CParent::Get(containerPtr);
#if defined(_RWSTD_VER) && !defined(_RWSTD_STRICT_ANSI)
            container.allocation_size(container.size());
#endif
            if ( !container.insert(element).second )
                CStlClassInfoUtil::ThrowDuplicateElementError();
        }
    static TObjectPtr AddElement(const CContainerTypeInfo* /*containerType*/,
                                 TObjectPtr containerPtr,
                                 TConstObjectPtr elementPtr,
                                 ESerialRecursionMode /* how = eRecursive */)
        {
            InsertElement(containerPtr,
                          CTypeConverter<TElementType>::Get(elementPtr));
            return 0;
        }
    // this structure is required to initialize pointers by null before reading
    struct SInitializer
    {
        SInitializer() : data() {}
        TElementType data;
    };
    static TObjectPtr AddElementIn(const CContainerTypeInfo* containerType,
                                   TObjectPtr containerPtr,
                                   CObjectIStream& in)
        {
            SInitializer data;
            containerType->GetElementType()->ReadData(in, &data.data);
            InsertElement(containerPtr, data.data);
            return 0;
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
            TObjectType& container = CParent::Get(containerPtr);
#if defined(_RWSTD_VER) && !defined(_RWSTD_STRICT_ANSI)
            container.allocation_size(container.size());
#endif
            container.insert(element);
        }
    static TObjectPtr AddElement(const CContainerTypeInfo* /*containerType*/,
                                 TObjectPtr containerPtr,
                                 TConstObjectPtr elementPtr,
                                 ESerialRecursionMode how = eRecursive)
        {
            InsertElement(containerPtr,
                          CTypeConverter<TElementType>::Get(elementPtr));
            return 0;
        }
    // this structure is required to initialize pointers by null before reading
    struct SInitializer
    {
        SInitializer() : data() {}
        TElementType data;
    };
    static TObjectPtr AddElementIn(const CContainerTypeInfo* containerType,
                                   TObjectPtr containerPtr,
                                   CObjectIStream& in)
        {
            SInitializer data;
            containerType->GetElementType()->ReadData(in, &data.data);
            InsertElement(containerPtr, data.data);
            return 0;
        }

    static void SetAddElementFunctions(CStlOneArgTemplate* info)
        {
            info->SetAddElementFunctions(&AddElement, &AddElementIn);
        }
};

template<class Container, class StlIterator,
    typename ContainerPtr, typename ElementRef,
    class TypeInfoIterator>
class CStlClassInfoFunctionsIBase :
    public CStlClassInfoFunctions<Container>
{
public:
    typedef StlIterator TStlIterator;
    typedef TypeInfoIterator TTypeInfoIterator;
    typedef typename TTypeInfoIterator::TObjectPtr TObjectPtr;
    typedef CStlClassInfoFunctions<Container> CParent;

    static TStlIterator& It(TTypeInfoIterator& iter)
        {
            if ( sizeof(TStlIterator) <= sizeof(iter.m_IteratorData) ) {
                void* data = &iter.m_IteratorData;
                return *static_cast<TStlIterator*>(data);
            }
            else {
                void* data = iter.m_IteratorData;
                return *static_cast<TStlIterator*>(data);
            }
        }
    static const TStlIterator& It(const TTypeInfoIterator& iter)
        {
            if ( sizeof(TStlIterator) <= sizeof(iter.m_IteratorData) ) {
                const void* data = &iter.m_IteratorData;
                return *static_cast<const TStlIterator*>(data);
            }
            else {
                const void* data = iter.m_IteratorData;
                return *static_cast<const TStlIterator*>(data);
            }
        }
    static bool InitIterator(TTypeInfoIterator& iter)
        {
            TStlIterator stl_iter
                = CParent::Get(iter.GetContainerPtr()).begin();
            if ( sizeof(TStlIterator) <= sizeof(iter.m_IteratorData) ) {
                void* data = &iter.m_IteratorData;
                new (data) TStlIterator(stl_iter);
            }
            else {
                iter.m_IteratorData = new TStlIterator(stl_iter);
            }
            return stl_iter != CParent::Get(iter.GetContainerPtr()).end();
        }
    static void ReleaseIterator(TTypeInfoIterator& iter)
        {
            if ( sizeof(TStlIterator) <= sizeof(iter.m_IteratorData) ) {
                void* data = &iter.m_IteratorData;
                static_cast<TStlIterator*>(data)->~StlIterator();
            }
            else {
                void* data = iter.m_IteratorData;
                delete static_cast<TStlIterator*>(data);
            }
        }
    static void CopyIterator(TTypeInfoIterator& dst,
                             const TTypeInfoIterator& src)
        {
            It(dst) = It(src);
        }

    static bool NextElement(TTypeInfoIterator& iter)
        {
            return ++It(iter) != CParent::Get(iter.GetContainerPtr()).end();
        }
    static TObjectPtr GetElementPtr(const TTypeInfoIterator& iter)
        {
            ElementRef e= *It(iter);
            return &e;
        }
};

template<class Container>
class CStlClassInfoFunctionsCI :
    public CStlClassInfoFunctionsIBase<Container, typename Container::const_iterator, const Container*, const typename Container::value_type&, CContainerTypeInfo::CConstIterator>
{
    typedef CStlClassInfoFunctionsIBase<Container, typename Container::const_iterator, const Container*, const typename Container::value_type&, CContainerTypeInfo::CConstIterator> CParent;
public:
    static void SetIteratorFunctions(CStlOneArgTemplate* info)
        {
            info->SetConstIteratorFunctions(&CParent::InitIterator,
                                            &CParent::ReleaseIterator,
                                            &CParent::CopyIterator,
                                            &CParent::NextElement,
                                            &CParent::GetElementPtr);
        }
};

template<class Container>
class CStlClassInfoFunctionsI :
    public CStlClassInfoFunctionsIBase<Container, typename Container::iterator, Container*, typename Container::value_type&, CContainerTypeInfo::CIterator>
{
    typedef CStlClassInfoFunctionsIBase<Container, typename Container::iterator, Container*, typename Container::value_type&, CContainerTypeInfo::CIterator> CParent;
public:
    typedef typename CParent::TStlIterator TStlIterator;
    typedef typename CParent::TTypeInfoIterator TTypeInfoIterator;
    typedef typename CParent::TObjectPtr TObjectPtr;
    
    static bool EraseElement(TTypeInfoIterator& iter)
        {
            TStlIterator& it = CParent::It(iter);
            Container* c = static_cast<Container*>(iter.GetContainerPtr());
            it = c->erase(it);
            return it != c->end();
        }
    static void EraseAllElements(TTypeInfoIterator& iter)
        {
            Container* c = static_cast<Container*>(iter.GetContainerPtr());
            c->erase(CParent::It(iter), c->end());
        }

    static void SetIteratorFunctions(CStlOneArgTemplate* info)
        {
            info->SetIteratorFunctions(&CParent::InitIterator,
                                       &CParent::ReleaseIterator,
                                       &CParent::CopyIterator,
                                       &CParent::NextElement,
                                       &CParent::GetElementPtr,
                                       &EraseElement, &EraseAllElements);
        }
};

template<class Container>
class CStlClassInfoFunctionsI_set :
    public CStlClassInfoFunctionsIBase<Container, typename Container::iterator, Container*, typename Container::value_type&, CContainerTypeInfo::CIterator>
{
    typedef CStlClassInfoFunctionsIBase<Container, typename Container::iterator, Container*, typename Container::value_type&, CContainerTypeInfo::CIterator> CParent;
public:
    typedef typename CParent::TStlIterator TStlIterator;
    typedef typename CParent::TTypeInfoIterator TTypeInfoIterator;
    typedef typename CParent::TObjectPtr TObjectPtr;
    
    static TObjectPtr GetElementPtr(const TTypeInfoIterator& /*data*/)
        {
            CStlClassInfoUtil::CannotGetElementOfSet();
            return 0;
        }
    static bool EraseElement(TTypeInfoIterator& iter)
        {
            TStlIterator& it = CParent::It(iter);
            Container* c = static_cast<Container*>(iter.GetContainerPtr());
            TStlIterator erase = it++;
            c->erase(erase);
            return it != c->end();
        }
    static void EraseAllElements(TTypeInfoIterator& iter)
        {
            Container* c = static_cast<Container*>(iter.GetContainerPtr());
            c->erase(CParent::It(iter), c->end());
        }

    static void SetIteratorFunctions(CStlOneArgTemplate* info)
        {
            info->SetIteratorFunctions(&CParent::InitIterator,
                                       &CParent::ReleaseIterator,
                                       &CParent::CopyIterator,
                                       &CParent::NextElement,
                                       &GetElementPtr,
                                       &EraseElement, &EraseAllElements);
        }
};

template<typename Data>
class CStlClassInfo_list
{
public:
    typedef list<Data> TObjectType;

    static TTypeInfo GetTypeInfo(TTypeInfo elementType)
        {
            return CStlClassInfoUtil::Get_list(elementType, &CreateTypeInfo);
        }
    static CTypeInfo* CreateTypeInfo(TTypeInfo elementType)
        {
            CStlOneArgTemplate* info =
                new CStlOneArgTemplate(sizeof(TObjectType), elementType,
                                       false);
            SetFunctions(info);
            return info;
        }
    static CTypeInfo* CreateTypeInfo(TTypeInfo elementType, const string& name)
        {
            CStlOneArgTemplate* info =
                new CStlOneArgTemplate(sizeof(TObjectType), elementType,
                                       false, name);
            SetFunctions(info);
            return info;
        }

    static TTypeInfo GetSetTypeInfo(TTypeInfo elementType)
        {
            return CStlClassInfoUtil::GetSet_list(elementType,
                                                  &CreateSetTypeInfo);
        }
    static CTypeInfo* CreateSetTypeInfo(TTypeInfo elementType)
        {
            CStlOneArgTemplate* info =
                new CStlOneArgTemplate(sizeof(TObjectType), elementType,
                                       true);
            SetFunctions(info);
            return info;
        }
    static CTypeInfo* CreateSetTypeInfo(TTypeInfo elementType, const string& name)
        {
            CStlOneArgTemplate* info =
                new CStlOneArgTemplate(sizeof(TObjectType), elementType,
                                       true, name);
            SetFunctions(info);
            return info;
        }

    static void SetFunctions(CStlOneArgTemplate* info)
        {
            CStlClassInfoFunctions<TObjectType>::SetMemFunctions(info);
            CStlClassInfoFunctions<TObjectType>::SetAddElementFunctions(info);
            CStlClassInfoFunctions<TObjectType>::SetCountFunctions(info);
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
                                                 &CreateTypeInfo);
        }
    static CTypeInfo* CreateTypeInfo(TTypeInfo elementType)
        {
            CStlOneArgTemplate* info =
                new CStlOneArgTemplate(sizeof(TObjectType), elementType,
                                       false);

            SetFunctions(info);
            return info;
        }

    static TTypeInfo GetSetTypeInfo(TTypeInfo elementType)
        {
            return CStlClassInfoUtil::GetSet_vector(elementType,
                                                    &CreateSetTypeInfo);
        }
    static CTypeInfo* CreateSetTypeInfo(TTypeInfo elementType)
        {
            CStlOneArgTemplate* info =
                new CStlOneArgTemplate(sizeof(TObjectType), elementType,
                                       true);
            SetFunctions(info);
            return info;
        }

    static void SetFunctions(CStlOneArgTemplate* info)
        {
            CStlClassInfoFunctions<TObjectType>::SetMemFunctions(info);
            CStlClassInfoFunctions<TObjectType>::SetAddElementFunctions(info);
            CStlClassInfoFunctions_vec<TObjectType>::SetCountFunctions(info);
            CStlClassInfoFunctionsCI<TObjectType>::SetIteratorFunctions(info);
            CStlClassInfoFunctionsI<TObjectType>::SetIteratorFunctions(info);
        }
};

template<typename Data>
class CStlClassInfo_set
{
public:
    typedef set<Data> TObjectType;

    static TTypeInfo GetTypeInfo(TTypeInfo elementType)
        {
            return CStlClassInfoUtil::Get_set(elementType, &CreateTypeInfo);
        }
    static CTypeInfo* CreateTypeInfo(TTypeInfo elementType)
        {
            CStlOneArgTemplate* info =
                new CStlOneArgTemplate(sizeof(TObjectType), elementType,
                                       true);

            CStlClassInfoFunctions<TObjectType>::SetMemFunctions(info);
            CStlClassInfoFunctions_set<TObjectType>::SetAddElementFunctions(info);
            CStlClassInfoFunctions<TObjectType>::SetCountFunctions(info);
            CStlClassInfoFunctionsCI<TObjectType>::SetIteratorFunctions(info);
            CStlClassInfoFunctionsI_set<TObjectType>::SetIteratorFunctions(info);

            return info;
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
                                                   &CreateTypeInfo);
        }
    static CTypeInfo* CreateTypeInfo(TTypeInfo elementType)
        {
            CStlOneArgTemplate* info =
                new CStlOneArgTemplate(sizeof(TObjectType), elementType,
                                       true);

            CStlClassInfoFunctions<TObjectType>::SetMemFunctions(info);
            CStlClassInfoFunctions_multiset<TObjectType>::SetAddElementFunctions(info);
            CStlClassInfoFunctions<TObjectType>::SetCountFunctions(info);
            CStlClassInfoFunctionsCI<TObjectType>::SetIteratorFunctions(info);
            CStlClassInfoFunctionsI_set<TObjectType>::SetIteratorFunctions(info);

            return info;
        }
};

template<typename Data, typename Comparator>
class CStlClassInfo_set2
{
public:
    typedef set<Data, Comparator> TObjectType;

    static TTypeInfo GetTypeInfo(TTypeInfo elementType)
        {
            static TTypeInfo info = 0;
            return CStlClassInfoUtil::GetInfo(info,
                                              elementType,
                                              &CreateTypeInfo);
        }
    static CTypeInfo* CreateTypeInfo(TTypeInfo elementType)
        {
            CStlOneArgTemplate* info =
                new CStlOneArgTemplate(sizeof(TObjectType), elementType,
                                       true);

            CStlClassInfoFunctions<TObjectType>::SetMemFunctions(info);
            CStlClassInfoFunctions_set<TObjectType>::SetAddElementFunctions(info);
            CStlClassInfoFunctions<TObjectType>::SetCountFunctions(info);
            CStlClassInfoFunctionsCI<TObjectType>::SetIteratorFunctions(info);
            CStlClassInfoFunctionsI_set<TObjectType>::SetIteratorFunctions(info);

            return info;
        }
};

template<typename Data, typename Comparator>
class CStlClassInfo_multiset2
{
public:
    typedef multiset<Data, Comparator> TObjectType;

    static TTypeInfo GetTypeInfo(TTypeInfo elementType)
        {
            static TTypeInfo info = 0;
            return CStlClassInfoUtil::GetInfo(info,
                                              elementType,
                                              &CreateTypeInfo);
        }
    static CTypeInfo* CreateTypeInfo(TTypeInfo elementType)
        {
            CStlOneArgTemplate* info =
                new CStlOneArgTemplate(sizeof(TObjectType), elementType,
                                       true);

            CStlClassInfoFunctions<TObjectType>::SetMemFunctions(info);
            CStlClassInfoFunctions_multiset<TObjectType>::SetAddElementFunctions(info);
            CStlClassInfoFunctions<TObjectType>::SetCountFunctions(info);
            CStlClassInfoFunctionsCI<TObjectType>::SetIteratorFunctions(info);
            CStlClassInfoFunctionsI_set<TObjectType>::SetIteratorFunctions(info);

            return info;
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
                                              &CreateTypeInfo);
        }

    static CTypeInfo* CreateTypeInfo(TTypeInfo keyType, TTypeInfo valueType)
        {
            TElementType* dummy = 0;
            CStlTwoArgsTemplate* info =
                new CStlTwoArgsTemplate
                (sizeof(TObjectType),
                 keyType,
                 reinterpret_cast<TPointerOffsetType>(&dummy->first),
                 valueType,
                 reinterpret_cast<TPointerOffsetType>(&dummy->second),
                 true);
            
            CStlClassInfoFunctions<TObjectType>::SetMemFunctions(info);
            CStlClassInfoFunctions_set<TObjectType>::SetAddElementFunctions(info);
            CStlClassInfoFunctions<TObjectType>::SetCountFunctions(info);
            CStlClassInfoFunctionsCI<TObjectType>::SetIteratorFunctions(info);
            CStlClassInfoFunctionsI_set<TObjectType>::SetIteratorFunctions(info);
            
            return info;
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
                                                   &CreateTypeInfo);
        }

    static CTypeInfo* CreateTypeInfo(TTypeInfo keyType, TTypeInfo valueType)
        {
            TElementType* dummy = 0;
            CStlTwoArgsTemplate* info =
                new CStlTwoArgsTemplate
                (sizeof(TObjectType),
                 keyType,
                 reinterpret_cast<TPointerOffsetType>(&dummy->first),
                 valueType,
                 reinterpret_cast<TPointerOffsetType>(&dummy->second),
                 true);
            
            CStlClassInfoFunctions<TObjectType>::SetMemFunctions(info);
            CStlClassInfoFunctions_multiset<TObjectType>::SetAddElementFunctions(info);
            CStlClassInfoFunctions<TObjectType>::SetCountFunctions(info);
            CStlClassInfoFunctionsCI<TObjectType>::SetIteratorFunctions(info);
            CStlClassInfoFunctionsI_set<TObjectType>::SetIteratorFunctions(info);
            
            return info;
        }
};

template<typename Key, typename Value, typename Comparator>
class CStlClassInfo_map3
{
public:
    typedef map<Key, Value, Comparator> TObjectType;
    typedef typename TObjectType::value_type TElementType;

    static TTypeInfo GetTypeInfo(TTypeInfo keyType, TTypeInfo valueType)
        {
            static TTypeInfo info = 0;
            return CStlClassInfoUtil::GetInfo(info,
                                              keyType, valueType,
                                              &CreateTypeInfo);
        }

    static CTypeInfo* CreateTypeInfo(TTypeInfo keyType, TTypeInfo valueType)
        {
            TElementType* dummy = 0;
            CStlTwoArgsTemplate* info =
                new CStlTwoArgsTemplate
                (sizeof(TObjectType),
                 keyType,
                 reinterpret_cast<TPointerOffsetType>(&dummy->first),
                 valueType,
                 reinterpret_cast<TPointerOffsetType>(&dummy->second),
                 true);
            
            CStlClassInfoFunctions<TObjectType>::SetMemFunctions(info);
            CStlClassInfoFunctions_set<TObjectType>::SetAddElementFunctions(info);
            CStlClassInfoFunctions<TObjectType>::SetCountFunctions(info);
            CStlClassInfoFunctionsCI<TObjectType>::SetIteratorFunctions(info);
            CStlClassInfoFunctionsI_set<TObjectType>::SetIteratorFunctions(info);
            
            return info;
        }
};

template<typename Key, typename Value, typename Comparator>
class CStlClassInfo_multimap3
{
public:
    typedef multimap<Key, Value, Comparator> TObjectType;
    typedef typename TObjectType::value_type TElementType;

    static TTypeInfo GetTypeInfo(TTypeInfo keyType, TTypeInfo valueType)
        {
            static TTypeInfo info = 0;
            return CStlClassInfoUtil::GetInfo(info,
                                              keyType, valueType,
                                              &CreateTypeInfo);
        }

    static CTypeInfo* CreateTypeInfo(TTypeInfo keyType, TTypeInfo valueType)
        {
            TElementType* dummy = 0;
            CStlTwoArgsTemplate* info =
                new CStlTwoArgsTemplate
                (sizeof(TObjectType),
                 keyType,
                 reinterpret_cast<TPointerOffsetType>(&dummy->first),
                 valueType,
                 reinterpret_cast<TPointerOffsetType>(&dummy->second),
                 true);
            
            CStlClassInfoFunctions<TObjectType>::SetMemFunctions(info);
            CStlClassInfoFunctions_multiset<TObjectType>::SetAddElementFunctions(info);
            CStlClassInfoFunctions<TObjectType>::SetCountFunctions(info);
            CStlClassInfoFunctionsCI<TObjectType>::SetIteratorFunctions(info);
            CStlClassInfoFunctionsI_set<TObjectType>::SetIteratorFunctions(info);
            
            return info;
        }
};

END_NCBI_SCOPE

/* @} */

#endif  /* STLTYPES__HPP */

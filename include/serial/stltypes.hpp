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
#include <corelib/ncbiobj.hpp>
#include <serial/typeref.hpp>
#include <serial/objstack.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/memberid.hpp>
#include <serial/choiceptr.hpp>
#include <serial/iteratorbase.hpp>
#include <set>
#include <map>
#include <list>
#include <vector>
#include <memory>

BEGIN_NCBI_SCOPE

class CClassInfoTmpl;

class CStlOneArgTemplate : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:

    CStlOneArgTemplate(TTypeInfo dataType);
    CStlOneArgTemplate(const CTypeRef& dataType);
    ~CStlOneArgTemplate(void);

    const CMemberId& GetDataId(void) const
        {
            return m_DataId;
        }
    void SetDataId(const CMemberId& id);

    TTypeInfo GetDataTypeInfo(void) const
        {
            return m_DataType.Get();
        }

    virtual bool MayContainType(TTypeInfo type) const;
    virtual bool HaveChildren(TConstObjectPtr object) const;
    virtual void BeginTypes(CChildrenTypesIterator& cc) const;
    virtual bool ValidTypes(const CChildrenTypesIterator& cc) const;
    virtual TTypeInfo GetChildType(const CChildrenTypesIterator& cc) const;
    virtual void NextType(CChildrenTypesIterator& cc) const;

private:
    CMemberId m_DataId;
    CTypeRef m_DataType;
};

class CStlTwoArgsTemplate : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:

    CStlTwoArgsTemplate(TTypeInfo keyType, TTypeInfo valueType);
    CStlTwoArgsTemplate(const CTypeRef& keyType, const CTypeRef& valueType);
    ~CStlTwoArgsTemplate(void);

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

    virtual bool MayContainType(TTypeInfo type) const;
    virtual bool HaveChildren(TConstObjectPtr object) const;
    virtual void BeginTypes(CChildrenTypesIterator& cc) const;
    virtual bool ValidTypes(const CChildrenTypesIterator& cc) const;
    virtual TTypeInfo GetChildType(const CChildrenTypesIterator& cc) const;
    virtual void NextType(CChildrenTypesIterator& cc) const;

private:
    CMemberId m_KeyId;
    CMemberId m_ValueId;
    CTypeRef m_KeyType;
    CTypeRef m_ValueType;
};

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

template<typename Data>
class CChoiceStlClassInfo_auto_ptr : public CChoicePointerTypeInfo
{
    typedef CChoicePointerTypeInfo CParent;
public:
    typedef Data TDataType;
    typedef auto_ptr<TDataType> TObjectType;

    CChoiceStlClassInfo_auto_ptr(TTypeInfo info)
        : CParent(info)
        { }
    CChoiceStlClassInfo_auto_ptr(const CTypeRef& type)
        : CParent(type)
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

    static TTypeInfo GetTypeInfo(TTypeInfo type)
        {
            return new CChoiceStlClassInfo_auto_ptr<Data>(type);
        }
};

template<typename List>
class CListArrayReader : public CObjectArrayReader
{
public:
    CListArrayReader(TTypeInfo dataTypeInfo, List& object)
        : m_DataTypeInfo(dataTypeInfo), m_Object(object)
        {
        }

    virtual void ReadFrom(CObjectIStream& in)
        {
			typename List::value_type value;
            m_Object.push_back(value);
            m_DataTypeInfo->ReadData(in, &m_Object.back());
        }

private:
    TTypeInfo m_DataTypeInfo;
    List& m_Object;
};

template<typename List>
class CListArrayWriter : public CObjectArrayWriter
{
public:
    CListArrayWriter(TTypeInfo dataTypeInfo, const List& object)
        : CObjectArrayWriter(object.empty()), m_DataTypeInfo(dataTypeInfo),
          m_Iterator(object.begin()), m_End(object.end())
        {
        }

    virtual void WriteTo(CObjectOStream& out)
        {
            m_DataTypeInfo->WriteData(out, &*m_Iterator);
            m_NoMoreElements = ++m_Iterator == m_End;
        }

private:
    TTypeInfo m_DataTypeInfo;
    typename List::const_iterator m_Iterator;
    typename List::const_iterator m_End;
};

template<typename Set>
class CSetArrayReader : public CObjectArrayReader
{
public:
    CSetArrayReader(TTypeInfo dataTypeInfo, Set& object)
        : m_DataTypeInfo(dataTypeInfo), m_Object(object)
        {
        }

    virtual void ReadFrom(CObjectIStream& in)
        {
            typename Set::value_type data;
            m_DataTypeInfo->ReadData(in, &data);
            m_Object.insert(data);
        }

private:
    TTypeInfo m_DataTypeInfo;
    Set& m_Object;
};

template<typename List>
class CStlListTemplateImpl : public CStlOneArgTemplate
{
    typedef CStlOneArgTemplate CParent;
public:
    typedef List TObjectType;
    typedef typename TObjectType::const_iterator TConstIterator;

    CStlListTemplateImpl(TTypeInfo dataType,
                         bool randomOrder = false)
        : CParent(dataType), m_RandomOrder(randomOrder)
        { }
    CStlListTemplateImpl(const CTypeRef& dataType,
                         bool randomOrder = false)
        : CParent(dataType), m_RandomOrder(randomOrder)
        { }

    bool RandomOrder(void) const
        {
            return m_RandomOrder;
        }

    static TObjectType& Get(TObjectPtr object)
        {
            return *static_cast<TObjectType*>(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectType*>(object);
        }
    static TObjectType& Get(const CChildrenIterator& object)
        {
            return Get(object.GetParentPtr());
        }
    static const TObjectType& Get(const CConstChildrenIterator& object)
        {
            return Get(object.GetParentPtr());
        }

    typedef typename TObjectType::const_iterator CConstIterator;
    typedef typename TObjectType::iterator CIterator;
    static const CConstIterator& Iterator(const CConstChildrenIterator& cc)
        {
            return static_cast<const CObjectFor<CConstIterator>*>(cc.GetIndex().m_AnyIterator.GetPointer())->GetData();
        }
    static CConstIterator& Iterator(CConstChildrenIterator& cc)
        {
            return static_cast<CObjectFor<CConstIterator>*>(cc.GetIndex().m_AnyIterator.GetPointer())->GetData();
        }
    static const CIterator& Iterator(const CChildrenIterator& cc)
        {
            return static_cast<const CObjectFor<CIterator>*>(cc.GetIndex().m_AnyIterator.GetPointer())->GetData();
        }
    static CIterator& Iterator(CChildrenIterator& cc)
        {
            return static_cast<CObjectFor<CIterator>*>(cc.GetIndex().m_AnyIterator.GetPointer())->GetData();
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
    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
        {
            const TObjectType& l1 = Get(object1);
            const TObjectType& l2 = Get(object2);
            if ( l1.size() != l2.size() )
                return false;
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            for ( TConstIterator i1 = l1.begin(), i2 = l2.begin();
                  i1 != l1.end(); ++i1, ++i2 ) {
                if ( !dataTypeInfo->Equals(&*i1, &*i2) )
                    return false;
            }
            return true;
        }

    virtual void SetDefault(TObjectPtr dst) const
        {
            Get(dst).clear();
        }

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const
        {
            const TObjectType& from = Get(src);
            Reserve(dst, from.size());
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            size_t index = 0;
            for ( TConstIterator i = from.begin(); i != from.end(); ++i ) {
                dataTypeInfo->Assign(AddEmpty(dst, index++), &*i);
            }
        }

    void Begin(CConstChildrenIterator& cc) const
        {
            cc.GetIndex().m_AnyIterator = CObjectFor<CConstIterator>::New();
            Iterator(cc) = Get(cc).begin();
        }
    void Begin(CChildrenIterator& cc) const
        {
            cc.GetIndex().m_AnyIterator = CObjectFor<CIterator>::New();
            Iterator(cc) = Get(cc).begin();
            cc.GetIndex().m_Index = 0; // valid
        }
    bool Valid(const CConstChildrenIterator& cc) const
        {
            return Iterator(cc) != Get(cc).end();
        }
    bool Valid(const CChildrenIterator& cc) const
        {
            return cc.GetIndex().m_Index == 0 && Iterator(cc) != Get(cc).end();
        }
    void GetChild(const CConstChildrenIterator& cc,
                  CConstObjectInfo& child) const
        {
            child.Set(&*Iterator(cc), GetDataTypeInfo());
        }
    void GetChild(const CChildrenIterator& cc,
                  CObjectInfo& child) const
        {
            child.Set(const_cast<TObjectPtr>(static_cast<TConstObjectPtr>(&*Iterator(cc))), GetDataTypeInfo());
        }
    void Next(CConstChildrenIterator& cc) const
        {
            ++Iterator(cc);
        }
    void Next(CChildrenIterator& cc) const
        {
            if ( cc.GetIndex().m_Index == 1 ) {
                // already set after Erase
                cc.GetIndex().m_Index = 0;
            }
            else {
                ++Iterator(cc);
            }
        }

protected:
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            CListArrayWriter<List> writer(dataTypeInfo, Get(object));
            out.WriteArray(writer, this, RandomOrder(), GetDataTypeInfo());
        }

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            CListArrayReader<List> reader(dataTypeInfo, Get(object));
            in.ReadArray(reader, this, RandomOrder(), dataTypeInfo);
        }

    virtual void SkipData(CObjectIStream& in) const
        {
            in.SkipArray(this, RandomOrder(), GetDataTypeInfo());
        }

    virtual void Reserve(TObjectPtr object, size_t length) const = 0;
    virtual TObjectPtr AddEmpty(TObjectPtr object, size_t index) const = 0;

private:
    bool m_RandomOrder;
};

template<typename Data>
class CStlClassInfo_list : public CStlListTemplateImpl< list<Data> >
{
    typedef CStlListTemplateImpl< list<Data> > CParent;
public:
    typedef Data TDataType;

    CStlClassInfo_list(TTypeInfo typeInfo, bool randomOrder = false)
        : CParent(typeInfo, randomOrder)
        { }
    CStlClassInfo_list(const CTypeRef& typeRef, bool randomOrder = false)
        : CParent(typeRef, randomOrder)
        { }

    static TTypeInfo GetTypeInfo(TTypeInfo info)
        {
            return new CStlClassInfo_list<Data>(info);
        }

    static TTypeInfo GetSetTypeInfo(TTypeInfo info)
        {
            return new CStlClassInfo_list<Data>(info, true);
        }

    void Erase(CChildrenIterator& cc) const
        {
            Iterator(cc) = Get(cc).erase(Iterator(cc));
            cc.GetIndex().m_Index = 1;
        }

protected:
    virtual void Reserve(TObjectPtr object, size_t ) const
        {
            Get(object).clear();
        }

    virtual TObjectPtr AddEmpty(TObjectPtr object, size_t /*index*/) const
        {
            TObjectType& l = Get(object);
            l.push_back(TDataType());
            return &l.back();
        }
};

template<typename Data>
class CStlClassInfo_vector : public CStlListTemplateImpl< vector<Data> >
{
    typedef CStlListTemplateImpl< vector<Data> > CParent;
public:
    typedef Data TDataType;

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

    void Erase(CChildrenIterator& cc) const
        {
            Iterator(cc) = Get(cc).erase(Iterator(cc));
            cc.GetIndex().m_Index = 1;
        }

protected:
    virtual void Reserve(TObjectPtr object, size_t length) const
        {
            Get(object).resize(length);
        }

    virtual TObjectPtr AddEmpty(TObjectPtr object, size_t index) const
        {
            TObjectType& l = Get(object);
            if ( index >= l.size() )
                l.push_back(TDataType());
            return &l[index];
        }
};

template<class Set, typename Data>
class CStlClassInfoSetBase : public CStlListTemplateImpl<Set>
{
    typedef CStlListTemplateImpl<Set> CParent;
public:
    typedef Data TDataType;

    CStlClassInfoSetBase(TTypeInfo type)
        : CParent(type, true)
        {
        }
    CStlClassInfoSetBase(const CTypeRef& typeRef)
        : CParent(typeRef, true)
        {
        }

    void Erase(CChildrenIterator& cc) const
        {
            CIterator erase = Iterator(cc);
            ++Iterator(cc);
            cc.GetIndex().m_Index = 1;
            Get(cc).erase(erase);
        }

protected:
    virtual void Reserve(TObjectPtr object, size_t ) const
        {
            Get(object).clear();
        }

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            CSetArrayReader<Set> reader(dataTypeInfo, Get(object));
            in.ReadArray(reader, this, RandomOrder(), dataTypeInfo);
        }

    virtual TObjectPtr AddEmpty(TObjectPtr , size_t) const
        {
            return 0;
        }
};

template<typename Data>
class CStlClassInfo_set :
    public CStlClassInfoSetBase<set<Data>, Data>
{
    typedef CStlClassInfoSetBase<set<Data>, Data> CParent;
public:
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
};

template<typename Data>
class CStlClassInfo_multiset :
    public CStlClassInfoSetBase<multiset<Data>, Data>
{
    typedef CStlClassInfoSetBase<multiset<Data>, Data> CParent;
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
};

class CStlClassInfoMapImpl : public CStlTwoArgsTemplate
{
    typedef CStlTwoArgsTemplate CParent;
public:

    CStlClassInfoMapImpl(TTypeInfo keyType, TTypeInfo valueType);
    CStlClassInfoMapImpl(const CTypeRef& keyType, const CTypeRef& valueType);
    ~CStlClassInfoMapImpl(void);

    const CClassInfoTmpl* GetElementType(void) const;

    void WriteKeyAndValue(CObjectOStream& out, CObjectStackClass& cls,
                          TConstObjectPtr key, TConstObjectPtr value) const;
    void ReadKey(CObjectIStream& in, CObjectStackClass& cls,
                 TObjectPtr keyPtr) const;
    void ReadValue(CObjectIStream& in, CObjectStackClass& cls,
                   TObjectPtr valuePtr) const;
    bool EqualsKeyValuePair(TConstObjectPtr key1, TConstObjectPtr key2,
                            TConstObjectPtr value1, TConstObjectPtr value2) const;

private:
    mutable AutoPtr<const CClassInfoTmpl> m_ElementType;
};

class CMapArrayWriterBase : public CObjectArrayWriter
{
public:
    CMapArrayWriterBase(const CStlClassInfoMapImpl* mapTypeInfo, bool empty)
        : CObjectArrayWriter(empty), m_MapTypeInfo(mapTypeInfo)
        {
        }

    virtual void WriteTo(CObjectOStream& out);

protected:
    virtual void WriteElement(CObjectOStream& out, CObjectStackClass& cls) = 0;

    const CStlClassInfoMapImpl* m_MapTypeInfo;
};

template<typename Map>
class CMapArrayWriter : public CMapArrayWriterBase
{
public:
    CMapArrayWriter(const CStlClassInfoMapImpl* mapTypeInfo, const Map& object)
        : CMapArrayWriterBase(mapTypeInfo, object.empty()),
          m_Iterator(object.begin()), m_End(object.end())
        {
        }

protected:
    virtual void WriteElement(CObjectOStream& out, CObjectStackClass& cls)
        {
            m_MapTypeInfo->WriteKeyAndValue(out, cls,
                                            &m_Iterator->first,
                                            &m_Iterator->second);
            m_NoMoreElements = ++m_Iterator == m_End;
        }

private:
    typename Map::const_iterator m_Iterator;
    typename Map::const_iterator m_End;
};

class CMapArrayReaderBase : public CObjectArrayReader
{
public:
    CMapArrayReaderBase(const CStlClassInfoMapImpl* mapTypeInfo)
        : m_MapTypeInfo(mapTypeInfo)
        {
        }

    virtual void ReadFrom(CObjectIStream& in);

protected:
    virtual void ReadElement(CObjectIStream& in, CObjectStackClass& cls) = 0;

    const CStlClassInfoMapImpl* m_MapTypeInfo;
};

template<typename Key, typename Value>
class CArrayReader_map : public CMapArrayReaderBase
{
public:
    CArrayReader_map(const CStlClassInfoMapImpl* mapTypeInfo,
                     map<Key, Value>& object)
        : CMapArrayReaderBase(mapTypeInfo), m_Object(object)
        {
        }

protected:
    virtual void ReadElement(CObjectIStream& in, CObjectStackClass& cls)
        {
            Key key;
            m_MapTypeInfo->ReadKey(in, cls, &key);
            m_MapTypeInfo->ReadValue(in, cls, &m_Object[key]);
        }

private:
    map<Key, Value>& m_Object;
};

template<typename Key, typename Value>
class CArrayReader_multimap : public CMapArrayReaderBase
{
public:
    CArrayReader_multimap(const CStlClassInfoMapImpl* mapTypeInfo,
                          multimap<Key, Value>& object)
        : CMapArrayReaderBase(mapTypeInfo), m_Object(object)
        {
        }
    
protected:
    virtual void ReadElement(CObjectIStream& in, CObjectStackClass& cls)
        {
            Key key;
            m_MapTypeInfo->ReadKey(in, cls, &key);
            Value value;
            m_MapTypeInfo->ReadValue(in, cls, &value);
            m_Object.insert(typename multimap<Key, Value>::value_type(key,
                                                                      value));
        }

private:
    multimap<Key, Value>& m_Object;
};

template<class Map, typename Key, typename Value>
class CStlClassInfoMapBase : public CStlClassInfoMapImpl
{
    typedef CStlClassInfoMapImpl CParent;

public:
    typedef Key TKeyType;
    typedef Value TValueType;
    typedef Map TObjectType;
    typedef typename TObjectType::const_iterator TConstIterator;

    CStlClassInfoMapBase(TTypeInfo keyType, TTypeInfo dataType)
        : CParent(keyType, dataType)
        {
        }
    CStlClassInfoMapBase(const CTypeRef& keyType, const CTypeRef& dataType)
        : CParent(keyType, dataType)
        {
        }

    static const TObjectType& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectType*>(object);
        }
    static TObjectType& Get(TObjectPtr object)
        {
            return *static_cast<TObjectType*>(object);
        }
    static TObjectType& Get(const CChildrenIterator& object)
        {
            return Get(object.GetParentPtr());
        }
    static const TObjectType& Get(const CConstChildrenIterator& object)
        {
            return Get(object.GetParentPtr());
        }

    typedef typename TObjectType::const_iterator CConstIterator;
    typedef typename TObjectType::iterator CIterator;
    static const CConstIterator& Iterator(const CConstChildrenIterator& cc)
        {
            return static_cast<const CObjectFor<CConstIterator>*>(cc.GetIndex().m_AnyIterator.GetPointer())->GetData();
        }
    static CConstIterator& Iterator(CConstChildrenIterator& cc)
        {
            return static_cast<CObjectFor<CConstIterator>*>(cc.GetIndex().m_AnyIterator.GetPointer())->GetData();
        }
    static const CIterator& Iterator(const CChildrenIterator& cc)
        {
            return static_cast<const CObjectFor<CIterator>*>(cc.GetIndex().m_AnyIterator.GetPointer())->GetData();
        }
    static CIterator& Iterator(CChildrenIterator& cc)
        {
            return static_cast<CObjectFor<CIterator>*>(cc.GetIndex().m_AnyIterator.GetPointer())->GetData();
        }
    static size_t Index(const CConstChildrenIterator& cc)
        {
            return cc.GetIndex().m_Index;
        }
    static size_t& Index(CConstChildrenIterator& cc)
        {
            return cc.GetIndex().m_Index;
        }
    static size_t Index(const CChildrenIterator& cc)
        {
            return cc.GetIndex().m_Index;
        }
    static size_t& Index(CChildrenIterator& cc)
        {
            return cc.GetIndex().m_Index;
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
    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
        {
            const TObjectType& o1 = Get(object1);
            const TObjectType& o2 = Get(object2);
            if ( o1.size() != o2.size() )
                return false;
            for ( TConstIterator i1 = o1.begin(), i2 = o2.begin();
                  i1 != o1.end(); ++i1, ++i2 ) {
                if ( !EqualsKeyValuePair(&i1->first, &i2->first,
                                         &i1->second, &i2->second) )
                    return false;
            }
            return true;
        }

    virtual void SetDefault(TObjectPtr dst) const
        {
            Get(dst).clear();
        }

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const
        {
            TObjectType& to = Get(dst);
            const TObjectType& from = Get(src);
            for ( TConstIterator i = from.begin(); i != from.end(); ++i ) {
                to.insert(*i);
            }
        }

    void Begin(CConstChildrenIterator& cc) const
        {
            cc.GetIndex().m_AnyIterator = CObjectFor<CConstIterator>::New();
            Iterator(cc) = Get(cc).begin();
            Index(cc) = 0;
        }
    void Begin(CChildrenIterator& cc) const
        {
            cc.GetIndex().m_AnyIterator = CObjectFor<CIterator>::New();
            Iterator(cc) = Get(cc).begin();
            Index(cc) = 0;
        }
    bool Valid(const CConstChildrenIterator& cc) const
        {
            return Iterator(cc) != Get(cc).end();
        }
    bool Valid(const CChildrenIterator& cc) const
        {
            return Iterator(cc) != Get(cc).end();
        }
    void GetChild(const CConstChildrenIterator& cc,
                  CConstObjectInfo& child) const
        {
            if ( Index(cc) == 0 )
                child.Set(&Iterator(cc)->first, GetKeyTypeInfo());
            else
                child.Set(&Iterator(cc)->second, GetValueTypeInfo());
        }
    void GetChild(const CChildrenIterator& cc,
                  CObjectInfo& child) const
        {
            if ( Index(cc) == 0 )
                child.Set(const_cast<Key*>(&Iterator(cc)->first),
                          GetKeyTypeInfo());
            else
                child.Set(&Iterator(cc)->second, GetValueTypeInfo());
        }
    void Next(CConstChildrenIterator& cc) const
        {
            if ( ++Index(cc) > 1 ) {
                ++Iterator(cc);
                Index(cc) = 0;
            }
        }
    void Next(CChildrenIterator& cc) const
        {
            if ( ++Index(cc) > 1 ) {
                ++Iterator(cc);
                Index(cc) = 0;
            }
        }
    void Erase(CChildrenIterator& cc) const
        {
            if ( Index(cc) == 0 ) {
                // erase whole entry
                CIterator erase = Iterator(cc);
                ++Iterator(cc);
                Index(cc) = -1;
                Get(cc).erase(erase);
            }
            else {
                // cannot erase value
                CParent::Erase(cc);
            }
        }

protected:
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            CMapArrayWriter<Map> writer(this, Get(object));
            out.WriteArray(writer, this, true, GetElementType());
        }

    virtual void SkipData(CObjectIStream& in) const
        {
            in.SkipArray(this, true, GetElementType());
        }
};

template<typename Key, typename Value>
class CStlClassInfo_map :
    public CStlClassInfoMapBase<map<Key, Value>, Key, Value>
{
    typedef CStlClassInfoMapBase<map<Key, Value>, Key, Value> CParent;

public:
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
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            CArrayReader_map<Key, Value> reader(this, Get(object));
            in.ReadArray(reader, this, true, GetElementType());
        }
};

template<typename Key, typename Value>
class CStlClassInfo_multimap :
    public CStlClassInfoMapBase<multimap<Key, Value>, Key, Value>
{
    typedef CStlClassInfoMapBase<multimap<Key, Value>, Key, Value> CParent;

public:
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
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            CArrayReader_multimap<Key, Value> reader(this, Get(object));
            in.ReadArray(reader, this, true, GetElementType());
        }
};

template<typename Char>
class CStlClassInfoChar_vector : public CTypeInfo {
    typedef CTypeInfo CParent;
    typedef vector<Char> TObjectType;
    typedef CType<TObjectType> TType;
public:
    typedef Char TChar;

    CStlClassInfoChar_vector<Char>(void)
        {
        }

    static TObjectType& Get(TObjectPtr object)
        {
            return TType::Get(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return TType::Get(object);
        }

    virtual size_t GetSize(void) const
        {
            return TType::GetSize();
        }

    virtual TObjectPtr Create(void) const
        {
            return new TObjectType;
        }

    virtual bool IsDefault(TConstObjectPtr object) const
        {
            return Get(object).empty();
        }

    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
        {
            return Get(object1) == Get(object2);
        }

    virtual void SetDefault(TObjectPtr dst) const
        {
            Get(dst).clear();
        }

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const
        {
            Get(dst) = Get(src);
        }

    static TTypeInfo GetTypeInfo(void);

private:
    static char* ToChar(Char* p)
        { return reinterpret_cast<char*>(p); }
    static const char* ToChar(const Char* p)
        { return reinterpret_cast<const char*>(p); }

protected:
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            const TObjectType& o = Get(object);
            size_t length = o.size();
            CObjectOStream::ByteBlock block(out, length);
            if ( length > 0 )
                block.Write(ToChar(&o.front()), length);
        }
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            TObjectType& o = Get(object);
            CObjectIStream::ByteBlock block(in);
            if ( block.KnownLength() ) {
                size_t length = block.GetExpectedLength();
                o.resize(length);
                block.Read(ToChar(&o.front()), length, true);
            }
            else {
                // length is known -> copy via buffer
                Char buffer[4096];
                size_t count;
                o.clear();
                while ( (count = block.Read(ToChar(buffer),
                                            sizeof(buffer))) != 0 ) {
                    o.insert(o.end(), buffer, buffer + count);
                }
            }
        }
    virtual void SkipData(CObjectIStream& in) const
        {
            in.SkipByteBlock();
        }
};

END_NCBI_SCOPE

#endif  /* STLTYPES__HPP */

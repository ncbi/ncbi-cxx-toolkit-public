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
#include <serial/typeref.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/memberid.hpp>
#include <serial/choiceptr.hpp>
#include <set>
#include <map>
#include <list>
#include <vector>
#include <memory>

BEGIN_NCBI_SCOPE

class CStlOneArgTemplate : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:

    CStlOneArgTemplate(const char* templ, TTypeInfo dataType);
    CStlOneArgTemplate(const char* templ, const CTypeRef& dataType);
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

private:
    CMemberId m_DataId;
    CTypeRef m_DataType;
};

class CStlTwoArgsTemplate : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:

    CStlTwoArgsTemplate(const char* templ,
                        TTypeInfo keyType, TTypeInfo valueType);
    CStlTwoArgsTemplate(const char* templ,
                        const CTypeRef& keyType, const CTypeRef& valueType);
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
        : CParent("auto_ptr", typeInfo, typeInfo)
        { }
    CStlClassInfo_auto_ptr(const CTypeRef& typeRef)
        : CParent("auto_ptr", "?", typeRef)
        { }
    
    TConstObjectPtr GetObjectPointer(TConstObjectPtr object) const
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
class CChoiceStlClassInfo_auto_ptr : public CChoicePointerTypeInfo
{
    typedef CChoicePointerTypeInfo CParent;
public:
    typedef Data TDataType;
    typedef auto_ptr<TDataType> TObjectType;

    CChoiceStlClassInfo_auto_ptr(TTypeInfo info)
        : CParent("auto_ptr", info, info)
        { }
    CChoiceStlClassInfo_auto_ptr(const CTypeRef& type)
        : CParent("auto_ptr", "?", type)
        { }
    
    TConstObjectPtr GetObjectPointer(TConstObjectPtr object) const
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
class CStlListTemplateImpl : public CStlOneArgTemplate
{
    typedef CStlOneArgTemplate CParent;
public:
    typedef List TObjectType;
    typedef typename TObjectType::const_iterator TConstIterator;

    CStlListTemplateImpl(const char* templ, TTypeInfo dataType)
        : CParent(templ, dataType), m_RandomOrder(false)
        { }
    CStlListTemplateImpl(const char* templ, const CTypeRef& dataType)
        : CParent(templ, dataType), m_RandomOrder(false)
        { }

    bool RandomOrder(void) const
        {
            return m_RandomOrder;
        }
    void SetRandomOrder(bool random = true)
        {
            m_RandomOrder = random;
        }

    static TObjectType& Get(TObjectPtr object)
        {
            return *static_cast<TObjectType*>(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectType*>(object);
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
            for ( TConstIterator i = from.begin(); i != from.end();
                  ++i, ++index ) {
                dataTypeInfo->Assign(AddEmpty(dst, index), &*i);
            }
        }

protected:
    virtual void CollectExternalObjects(COObjectList& objectList,
                                        TConstObjectPtr object) const
        {
            const TObjectType& l = Get(object);
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            for ( TConstIterator i = l.begin(); i != l.end(); ++i ) {
                dataTypeInfo->CollectObjects(objectList, &*i);
            }
        }

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            const TObjectType& l = Get(object);
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            CObjectOStream::Block block(l.size(), out, RandomOrder());
            for ( TConstIterator i = l.begin(); i != l.end(); ++i ) {
                block.Next();
                out.WriteExternalObject(&*i, dataTypeInfo);
            }
        }

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            CObjectIStream::Block block(CObjectIStream::eFixed,
                                        in, RandomOrder());
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            if ( block.Fixed() )
                Reserve(object, block.GetSize());
            while ( block.Next() ) {
                in.ReadExternalObject(AddEmpty(object, block.GetIndex()),
                                      dataTypeInfo);
            }
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

    CStlClassInfo_list(TTypeInfo typeInfo)
        : CParent("std::list", typeInfo)
        { }
    CStlClassInfo_list(const CTypeRef& typeRef)
        : CParent("std::list", typeRef)
        { }

    static TTypeInfo GetTypeInfo(TTypeInfo info)
        {
            return new CStlClassInfo_list<Data>(info);
        }

protected:
    virtual void Reserve(TObjectPtr object, size_t ) const
        {
            Get(object).clear();
        }

    virtual TObjectPtr AddEmpty(TObjectPtr object, size_t ) const
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
        : CParent("std::vector", info)
        { }
    CStlClassInfo_vector(const CTypeRef& typeRef)
        : CParent("std::vector", typeRef)
        { }

    static TTypeInfo GetTypeInfo(TTypeInfo info)
        {
            return new CStlClassInfo_vector<Data>(info);
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
            return &(l[index]);
        }
};

template<class Set, typename Data>
class CStlClassInfoSetBase : public CStlListTemplateImpl<Set>
{
    typedef CStlListTemplateImpl<Set> CParent;
public:
    typedef Data TDataType;

    CStlClassInfoSetBase(const char* templ, TTypeInfo type)
        : CParent(templ, type)
        {
        }
    CStlClassInfoSetBase(const char* templ, const CTypeRef& typeRef)
        : CParent(templ, typeRef)
        {
        }

protected:
    virtual void Reserve(TObjectPtr object, size_t ) const
        {
            Get(object).clear();
        }

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            TObjectType& o = Get(object);
            CObjectIStream::Block block(CObjectIStream::eFixed, in);
            while ( block.Next() ) {
                Data data;
                GetDataTypeInfo()->ReadData(in, &data);
                o.insert(data);
            }
        }

    virtual TObjectPtr AddEmpty(TObjectPtr , size_t ) const
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
        : CParent("std::set", info)
        {
        }
    CStlClassInfo_set(const CTypeRef& typeRef)
        : CParent("std::set", typeRef)
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
        : CParent("std::multiset", dataType)
        {
        }
    CStlClassInfo_multiset(const CTypeRef& dataType)
        : CParent("std::multiset", dataType)
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

    CStlClassInfoMapImpl(const char* templ,
                         TTypeInfo keyType, TTypeInfo valueType)
        : CParent(templ, keyType, valueType)
        { }
    CStlClassInfoMapImpl(const char* templ,
                         const CTypeRef& keyType, const CTypeRef& valueType)
        : CParent(templ, keyType, valueType)
        { }

protected:
    void CollectKeyValuePair(COObjectList& objectList,
                             TConstObjectPtr key, TConstObjectPtr value) const;
    void WriteKeyValuePair(CObjectOStream& out,
                           TConstObjectPtr key, TConstObjectPtr value) const;
    void ReadKeyValuePair(CObjectIStream& in,
                          TObjectPtr key, TObjectPtr value) const;
    bool EqualsKeyValuePair(TConstObjectPtr key1, TConstObjectPtr key2,
                            TConstObjectPtr value1, TConstObjectPtr value2) const;
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

    CStlClassInfoMapBase(const char* templ,
                         TTypeInfo keyType, TTypeInfo dataType)
        : CParent(templ, keyType, dataType)
        {
        }
    CStlClassInfoMapBase(const char* templ,
                         const CTypeRef& keyType, const CTypeRef& dataType)
        : CParent(templ, keyType, dataType)
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

protected:
    virtual void CollectExternalObjects(COObjectList& objectList,
                                        TConstObjectPtr object) const
        {
            const TObjectType& o = Get(object);
            for ( TConstIterator i = o.begin(); i != o.end(); ++i ) {
                CollectKeyValuePair(objectList, &i->first, &i->second);
            }
        }

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            const TObjectType& o = Get(object);
            CObjectOStream::Block block(out);
            for ( TConstIterator i = o.begin(); i != o.end(); ++i ) {
                block.Next();
                WriteKeyValuePair(out, &i->first, &i->second);
            }
        }

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            TObjectType& o = Get(object);
            CObjectIStream::Block block(in);
            while ( block.Next() ) {
                TKeyType key;
                TValueType value;
                ReadKeyValuePair(in, &key, &value);
                typename TObjectType::value_type insert(key, value);
                o.insert(insert);
            }
        }
};

template<typename Key, typename Value>
class CStlClassInfo_map :
    public CStlClassInfoMapBase<map<Key, Value>, Key, Value>
{
    typedef CStlClassInfoMapBase<map<Key, Value>, Key, Value> CParent;

public:
    CStlClassInfo_map(TTypeInfo keyType, TTypeInfo dataType)
        : CParent("std::map", keyType, dataType)
        {
        }
    CStlClassInfo_map(const CTypeRef& keyType, const CTypeRef& dataType)
        : CParent("std::map", keyType, dataType)
        {
        }

    static TTypeInfo GetTypeInfo(TTypeInfo keyInfo, TTypeInfo dataInfo)
        {
            return new CStlClassInfo_map<Key, Value>(keyInfo, dataInfo);
        }
};

template<typename Key, typename Value>
class CStlClassInfo_multimap :
    public CStlClassInfoMapBase<multimap<Key, Value>, Key, Value>
{
    typedef CStlClassInfoMapBase<multimap<Key, Value>, Key, Value> CParent;

public:
    CStlClassInfo_multimap(TTypeInfo keyInfo, TTypeInfo dataInfo)
        : CParent("std::multimap", keyType, dataType)
        {
        }
    CStlClassInfo_multimap(const CTypeRef& keyType, const CTypeRef& dataType)
        : CParent("std::multimap", keyType, dataType)
        {
        }

    static TTypeInfo GetTypeInfo(TTypeInfo keyInfo, TTypeInfo dataInfo)
        {
            return new CStlClassInfo_multimap<Key, Value>(keyInfo, dataInfo);
        }
};

template<typename Char>
class CStlClassInfoChar_vector : public CTypeInfoTmpl< vector<Char> > {
    typedef CTypeInfoTmpl< vector<Char> > CParent;
public:
    typedef Char TChar;

    CStlClassInfoChar_vector<Char>(void)
        : CParent("vector< ? char >")
        {
        }

    static TTypeInfo GetTypeInfo(void);

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
                Char buffer[1024];
                size_t count;
                o.clear();
                while ( (count = block.Read(ToChar(buffer),
                                            sizeof(buffer))) != 0 ) {
                    o.insert(o.end(), buffer, buffer + count);
                }
            }
        }
};

END_NCBI_SCOPE

#endif  /* STLTYPES__HPP */

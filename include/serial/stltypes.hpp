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
#include <serial/serialdef.hpp>
#include <serial/typeref.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <map>
#include <list>
#include <vector>

BEGIN_NCBI_SCOPE

class CStlOneArgTemplate : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:

    CStlOneArgTemplate(const CTypeRef& dataType)
        : m_DataType(dataType)
        { }

    TTypeInfo GetDataTypeInfo(void) const
        {
            return m_DataType.Get();
        }

private:
    CTypeRef m_DataType;
};

template<typename List>
class CStlOneArgTemplateImpl : public CStlOneArgTemplate
{
    typedef CStlOneArgTemplate CParent;
public:
    typedef List TObjectType;
    typedef typename TObjectType::const_iterator TConstIterator;

    CStlOneArgTemplateImpl(const CTypeRef& dataType)
        : CParent(dataType)
        { }

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
            for ( TObjectType::const_iterator i = l.begin();
                  i != l.end(); ++i ) {
                dataTypeInfo->CollectObjects(objectList, &*i);
            }
        }

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            const TObjectType& l = Get(object);
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            CObjectOStream::Block block(out, l.size());
            for ( TObjectType::const_iterator i = l.begin();
                  i != l.end(); ++i ) {
                block.Next();
                out.WriteExternalObject(&*i, dataTypeInfo);
            }
        }

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            CObjectIStream::Block block(in, CObjectIStream::eFixed);
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            Reserve(object, block.GetSize());
            while ( block.Next() ) {
                in.ReadExternalObject(AddEmpty(object, block.GetIndex()),
                                      dataTypeInfo);
            }
        }

    virtual void Reserve(TObjectPtr object, size_t length) const = 0;
    virtual TObjectPtr AddEmpty(TObjectPtr object, size_t index) const = 0;
};

template<typename Data>
class CStlClassInfoList : public CStlOneArgTemplateImpl< list<Data> >
{
    typedef CStlOneArgTemplateImpl<TObjectType> CParent;
public:
    typedef Data TDataType;

    CStlClassInfoList(void)
        : CParent(GetTypeRef(static_cast<const TDataType*>(0)))
        { }
    CStlClassInfoList(const CTypeRef& typeRef)
        : CParent(typeRef)
        { }

    static TTypeInfo GetTypeInfo(void)
        {
            static TTypeInfo typeInfo = new CStlClassInfoList;
            return typeInfo;
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
class CStlClassInfoVector : public CStlOneArgTemplateImpl< vector<Data> >
{
    typedef CStlOneArgTemplateImpl<TObjectType> CParent;
public:
    typedef Data TDataType;

    CStlClassInfoVector(void)
        : CParent(GetTypeRef(static_cast<const TDataType*>(0)))
        { }
    CStlClassInfoVector(const CTypeRef& typeRef)
        : CParent(typeRef)
        { }

    static TTypeInfo GetTypeInfo(void)
        {
            static TTypeInfo typeInfo = new CStlClassInfoVector;
            return typeInfo;
        }

protected:
    virtual void Reserve(TObjectPtr object, size_t length) const
        {
            Get(object).assign(length);
        }

    virtual TObjectPtr AddEmpty(TObjectPtr object, size_t index) const
        {
            return &Get(object).[index];
        }
};

class CStlClassInfoMapImpl : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:

    CStlClassInfoMapImpl(const CTypeRef& keyType, const CTypeRef& valueType)
        : m_KeyType(keyType), m_ValueType(valueType)
        { }

    TTypeInfo GetKeyTypeInfo(void) const
        {
            return m_KeyType.Get();
        }

    TTypeInfo GetValueTypeInfo(void) const
        {
            return m_ValueType.Get();
        }

protected:
    void CollectKeyValuePair(COObjectList& objectList,
                             TConstObjectPtr key, TConstObjectPtr value) const;
    void WriteKeyValuePair(CObjectOStream& out,
                           TConstObjectPtr key, TConstObjectPtr value) const;
    void ReadKeyValuePair(CObjectIStream& in,
                          TObjectPtr key, TObjectPtr value) const;

private:
    CTypeRef m_KeyType;
    CTypeRef m_ValueType;
};

template<typename Key, typename Value>
class CStlClassInfoMap : CStlClassInfoMapImpl
{
    typedef CStlClassInfoMapImpl CParent;

public:
    typedef Key TKeyType;
    typedef Value TValueType;
    typedef map<TKeyType, TValueType> TObjectType;

    CStlClassInfoMap(void);

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

    static TTypeInfo GetTypeInfo(void)
        {
            static TTypeInfo typeInfo = new CStlClassInfoMap;
            return typeInfo;
        }

    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
        {
            const TObjectType& o1 = Get(object1);
            const TObjectType& o2 = Get(object2);
            if ( o1.size() != o2.size() )
                return false;
            TTypeInfo keyTypeInfo = GetKeyTypeInfo();
            TTypeInfo valueTypeInfo = GetValueTypeInfo();
            for ( TObjectType::const_iterator i1 = o1.begin(), i2 = o2.begin();
                  i1 != o1.end(); ++i1, ++i2 ) {
                if ( !keyTypeInfo->Equals(&i1->first, &i2->first) ||
                     !valueTypeInfo->Equals(&i1->second, &i2->second) )
                    return false;
            }
            return true;
        }

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const
        {
            TObjectType& to = Get(dst);
            const TObjectType& from = Get(src);
            to.clear();
            for ( TObjectType::const_iterator i = from.begin();
                  i != from.end(); ++i ) {
                to.insert(*i);
            }
        }

protected:
    virtual void CollectExternalObjects(COObjectList& objectList,
                                        TConstObjectPtr object) const
        {
            const TObjectType& o = Get(object);
            for ( TObjectType::const_iterator i = o.begin();
                  i != o.end(); ++i ) {
                CollectKeyValuePair(objectList, &i->first, &i->second);
            }
        }

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            const TObjectType& o = Get(object);
            CObjectOStream::Block block(out, o.size());
            for ( TObjectType::const_iterator i = o.begin();
                  i != o.end(); ++i ) {
                WriteKeyValuePair(out, &i->first, &i->second);
            }
        }

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            TObjectType& o = Get(object);
            CObjectIStream::Block block(in, CObjectIStream::eFixed);
            while ( block.Next() ) {
                TObjectType::value_type value;
                ReadKeyValuePair(in, &value->first, &value->second);
                o.insert(value);
            }
        }
};

class CStlClassInfoCharVector : public CTypeInfo {
public:
    typedef vector<char> TObjectType;
    CStlClassInfoCharVector(void);

    static const TObjectType& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectType*>(object);
        }
    static TObjectType& Get(TObjectPtr object)
        {
            return *static_cast<TObjectType*>(object);
        }

    static TTypeInfo GetTypeInfo(void);

    virtual size_t GetSize(void) const;
    virtual TObjectPtr Create(void) const;

    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

protected:
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const;
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;
};

END_NCBI_SCOPE

#endif  /* STLTYPES__HPP */

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

class CTemplateResolver1;
class CTemplateResolver2;

class CStlClassInfoListImpl : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:

    CStlClassInfoListImpl(const type_info& id, const CTypeRef& dataType);

    TTypeInfo GetDataTypeInfo(void) const
        {
            return m_DataTypeRef.Get();
        }

    virtual void AnnotateTemplate(CObjectOStream& out) const;

private:
    static CTemplateResolver1 sm_Resolver;

    CTypeRef m_DataTypeRef;
};

template<typename Data>
class CStlClassInfoList : CStlClassInfoListImpl
{
    typedef CStlClassInfoListImpl CParent;

public:
    typedef Data TDataType;
    typedef list<TDataType> TObjectType;

    CStlClassInfoList(void);

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
            static TTypeInfo typeInfo = new CStlClassInfoList;
            return typeInfo;
        }

    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
        {
            const TObjectType& l1 = Get(object1);
            const TObjectType& l2 = Get(object2);
            if ( l1.size() != l2.size() )
                return false;
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            for ( TObjectType::const_iterator i1 = l1.begin(), i2 = l2.begin();
                  i1 != l1.end(); ++i1, ++i2 ) {
                if ( !dataTypeInfo->Equals(&*i1, &*i2) )
                    return false;
            }
            return true;
        }

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const
        {
            TObjectType& to = Get(dst);
            const TObjectType& from = Get(src);
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            to.clear();
            for ( TObjectType::const_iterator i = from.begin();
                  i != from.end(); ++i ) {
                to.push_back(TDataType());
                dataTypeInfo->Assign(&to.back(), &*i);
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
            CObjectOStream::Block block(out, CObjectOStream::eSequence,
                                        l.size());
            for ( TObjectType::const_iterator i = l.begin();
                  i != l.end(); ++i ) {
                block.Next();
                out.WriteExternalObject(&*i, dataTypeInfo);
            }
        }

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            TObjectType& l = Get(object);
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            CObjectIStream::Block block(in, CObjectIStream::eSequence,
                                        CObjectIStream::eFixed);
            while ( block.Next() ) {
                l.push_back(TDataType());
                in.ReadExternalObject(&l.back(), dataTypeInfo);
            }
        }
};

END_NCBI_SCOPE

#endif  /* STLTYPES__HPP */

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
#include <serial/classinfo.hpp>
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
    typedef list<Data> TObjectType;

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

    virtual bool IsDefault(TConstObjectPtr object) const
        {
            return Get(object).empty();
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
            TObjectType& l = Get(object);
            TTypeInfo dataTypeInfo = GetDataTypeInfo();
            CObjectIStream::Block block(in, CObjectIStream::eFixed);
            while ( block.Next() ) {
                l.push_back(Data());
                in.ReadExternalObject(&l.back(), dataTypeInfo);
            }
        }
};

END_NCBI_SCOPE

#endif  /* STLTYPES__HPP */

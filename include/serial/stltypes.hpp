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

BEGIN_NCBI_SCOPE

class CTemplateResolver
{
    typedef map<string, const CTemplateResolver*> TResolvers;
public:
    typedef CTypeInfo::TTypeInfo TTypeInfo;

    CTemplateResolver(const string& name)
        : m_Name(name)
        {
            if ( !sm_Resolvers.insert(TResolvers::value_type(name,
                                                             this)).second ) {
                throw runtime_error("duplicate " + name + "<...> template");
            }
        }
    
    const string& GetName(void) const
        {
            return m_Name;
        }

    static TTypeInfo ResolveTemplate(CObjectIStream& in)
        {
            string name = in.ReadString();
            TResolvers::iterator i = sm_Resolvers.find(name);
            if ( i == sm_Resolvers.end() ) {
                throw runtime_error("undefined " + name + "<...> template");
            }
            return i->second->Resolve(in);
        }

protected:

    virtual TTypeInfo Resolve(CObjectIStream& in) const = 0;

private:
    static TResolvers sm_Resolvers;

    const string m_Name;
};

class CTemplateResolver1 : public CTemplateResolver
{
    typedef CTemplateResolver CParent;
    typedef map<TTypeInfo, TTypeInfo> TTMap;
public:
    CTemplateResolver1(const string& name)
        : CParent(name)
        {
        }

    void Register(TTypeInfo templ, TTypeInfo arg)
        {
            if ( !m_Templates.insert(TTMap::value_type(arg,
                                                       templ)).second ) {
                throw runtime_error("duplicate " + GetName() +
                                    "<" + arg->GetName() + "> template");
            }
        }

    void Write(CObjectOStream& out, TTypeInfo arg) const
        {
            out.WriteTemplateInfo(GetName(), arg);
        }

protected:

    virtual TTypeInfo Resolve(CObjectIStream& in) const
        {
            CObjectIStream::Block block(in, true);
            if ( !block.Next() )
                throw runtime_error("no argument");
            const CTypeInfo* arg = in.ReadTypeInfo();
            TTMap::const_iterator i = m_Templates.find(arg);
            if ( i == m_Templates.end() ) {
                throw runtime_error("undefined " +  GetName() +
                                    "<" + arg->GetName() + "> template");
            }
            return i->second;
        }

private:
    TTMap m_Templates;
};

class CTemplateResolver2 : public CTemplateResolver
{
    typedef CTemplateResolver CParent;
    typedef map<TTypeInfo, TTypeInfo> TTMap2;
    typedef map<TTypeInfo, TTMap2> TTMap;
public:
    CTemplateResolver2(const string& name)
        : CParent(name)
        {
        }

    void Register(TTypeInfo templ, TTypeInfo arg1, TTypeInfo arg2)
        {
            if ( !m_Templates[arg1].insert(TTMap2::value_type(arg2,
                                                              templ)).second ) {
                throw runtime_error("duplicate " + GetName() +
                                    "<" + arg1->GetName() + ", " +
                                    arg2->GetName() +
                                    "> template");
            }
        }

    void Write(CObjectOStream& out, TTypeInfo arg1, TTypeInfo arg2) const
        {
            out.WriteTemplateInfo(GetName(), arg1, arg2);
        }

protected:

    virtual TTypeInfo Resolve(CObjectIStream& in) const
        {
            CObjectIStream::Block block(in, true);
            if ( !block.Next() )
                throw runtime_error("no template argument");
            TTypeInfo arg1 = in.ReadTypeInfo();
            TTMap::const_iterator i = m_Templates.find(arg1);
            if ( i == m_Templates.end() ) {
                throw runtime_error("undefined " + GetName() +
                                    "<" + arg1->GetName() + ", ...> template");
            }
            if ( !block.Next() )
                throw runtime_error("no template argument");
            TTypeInfo arg2 = in.ReadTypeInfo();
            TTMap2::const_iterator i2 = i->second.find(arg2);
            if ( i2 == i->second.end() ) {
                throw runtime_error("undefined " + GetName() +
                                    "<" + arg1->GetName() + ", " +
                                    arg2->GetName() +
                                    "> template");
            }
            return i2->second;
        }

private:
    TTMap m_Templates;
};

class CStlClassInfoListImpl : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:

    CStlClassInfoListImpl(const string& name, TTypeInfo typeInfo)
        : CParent(name), m_DataTypeInfo(typeInfo)
        {
            sm_Resolver.Register(this, typeInfo);
        }

    TTypeInfo GetDataTypeInfo(void) const
        {
            return m_DataTypeInfo;
        }

    virtual void AnnotateTemplate(CObjectOStream& out) const
        {
            sm_Resolver.Write(out, GetDataTypeInfo());
        }

private:
    static CTemplateResolver1 sm_Resolver;

    TTypeInfo const m_DataTypeInfo;
};

template<class Data>
class CStlClassInfoList : CStlClassInfoListImpl
{
    typedef CStlClassInfoListImpl CParent;

public:
    CStlClassInfoList(void)
        : CParent(typeid(list<Data>).name(), GetTypeInfo(typeid(Data).name()))
        {
        }

    virtual TObjectPtr Create(void) const
        {
            return new list<Data>;
        }

protected:
    virtual void CollectMembers(CObjectList& list, TConstObjectPtr object) const
        {
            const list<Data>& l = static_cast<const list<Data>*>(object);
            for ( list<Data>::const_iterator i = l.begin(); i != l.end(); ++i ) {
                AddObject(list, &*i, GetDataTypeInfo());
            }
        }

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            const list<Data>& l = static_cast<const list<Data>*>(object);
            CObjectOStream::Block block(out, l.size());
            for ( list<Data>::const_iterator i = l.begin(); i != l.end(); ++i ) {
                block.Next();
                GetDataTypeInfo().Write(in, &*i);
            }
        }

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            list<Data>& l = *static_cast<list<Data>*>(object);
            CObjectIStream::Block block(in);
            while ( block.Next() ) {
                l.push_back(Data());
                GetDataTypeInfo().Read(out, &l.back());
            }
        }
};

END_NCBI_SCOPE

#endif  /* STLTYPES__HPP */

#ifndef TMPLINFO__HPP
#define TMPLINFO__HPP

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
* Revision 1.2  1999/06/24 14:44:46  vasilche
* Added binary ASN.1 output.
*
* Revision 1.1  1999/06/04 20:51:40  vasilche
* First compilable version of serialization.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <map>
#include <vector>

BEGIN_NCBI_SCOPE

class CTypeInfo;
class CObjectOStream;
class CObjectIStream;

class CTemplateResolver
{
    typedef map<string, const CTemplateResolver*> TResolvers;
public:

    CTemplateResolver(const string& name);

    const string& GetName(void) const
        {
            return m_Name;
        }

    static TTypeInfo ResolveTemplate(const string& name,
                                     TTypeInfo arg)
        {
            return GetResolver(name)->GetTemplate(arg);
        }

    static TTypeInfo ResolveTemplate(const string& name,
                                     TTypeInfo arg1, TTypeInfo arg2)
        {
            return GetResolver(name)->GetTemplate(arg1, arg2);
        }

    static TTypeInfo ResolveTemplate(const string& name,
                                     const vector<TTypeInfo>& args)
        {
            return GetResolver(name)->GetTemplate(args);
        }

protected:
    static const CTemplateResolver* GetResolver(const string& name);

    virtual TTypeInfo GetTemplate(TTypeInfo arg) const;
    virtual TTypeInfo GetTemplate(TTypeInfo arg1, TTypeInfo arg2) const;
    virtual TTypeInfo GetTemplate(const vector<TTypeInfo>& args) const;

private:
    static TResolvers sm_Resolvers;

    const string m_Name;
};

class CTemplateResolver1 : public CTemplateResolver
{
    typedef CTemplateResolver CParent;
    typedef map<TTypeInfo, TTypeInfo> TTMap;
public:
    CTemplateResolver1(const string& name);

    void Register(TTypeInfo tmpl, TTypeInfo arg);

    void Write(CObjectOStream& out, TTypeInfo tmpl, TTypeInfo arg) const;

protected:

    using CParent::GetTemplate;
    virtual TTypeInfo GetTemplate(TTypeInfo arg) const;

private:
    TTMap m_Templates;
};

class CTemplateResolver2 : public CTemplateResolver
{
    typedef CTemplateResolver CParent;
    typedef map<TTypeInfo, TTypeInfo> TTMap2;
    typedef map<TTypeInfo, TTMap2> TTMap;
public:
    CTemplateResolver2(const string& name);

    void Register(TTypeInfo tmpl, TTypeInfo arg1, TTypeInfo arg2);

    void Write(CObjectOStream& out, TTypeInfo tmpl,
               TTypeInfo arg1, TTypeInfo arg2) const;

protected:

    using CParent::GetTemplate;
    virtual TTypeInfo GetTemplate(TTypeInfo arg1, TTypeInfo arg2) const;

private:
    TTMap m_Templates;
};

//#include <serial/tmplinfo.inl>

END_NCBI_SCOPE

#endif  /* TMPLINFO__HPP */

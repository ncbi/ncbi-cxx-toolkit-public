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
* Revision 1.5  1999/06/24 14:45:02  vasilche
* Added binary ASN.1 output.
*
* Revision 1.4  1999/06/16 20:35:36  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.3  1999/06/15 16:19:53  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.2  1999/06/07 19:30:28  vasilche
* More bug fixes
*
* Revision 1.1  1999/06/04 20:51:50  vasilche
* First compilable version of serialization.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/tmplinfo.hpp>
#include <serial/typeinfo.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

CTemplateResolver::TResolvers CTemplateResolver::sm_Resolvers;

CTemplateResolver::CTemplateResolver(const string& name)
    : m_Name(name)
{
/*
    if ( !sm_Resolvers.insert(TResolvers::value_type(name,
                                                     this)).second ) {
        THROW1_TRACE(runtime_error, "duplicate " + name + "<...> template");
    }
*/
}
    
const CTemplateResolver* CTemplateResolver::GetResolver(const string& name)
{
    TResolvers::iterator i = sm_Resolvers.find(name);
    if ( i == sm_Resolvers.end() ) {
        THROW1_TRACE(runtime_error, "undefined " + name + "<...> template");
    }
    return i->second;
}

TTypeInfo CTemplateResolver::GetTemplate(TTypeInfo arg) const
{
    THROW1_TRACE(runtime_error,
                 "undefined " + GetName() + "<" + arg->GetName() + "> template");
}

TTypeInfo CTemplateResolver::GetTemplate(TTypeInfo arg1,
                                                    TTypeInfo arg2) const
{
    THROW1_TRACE(runtime_error,
                 "undefined " + GetName() + "<" + arg1->GetName() + ", " +
                        arg2->GetName() + "> template");
}

TTypeInfo CTemplateResolver::GetTemplate(const vector<TTypeInfo>& args) const
{
    switch ( args.size() ) {
    case 1:
        return GetTemplate(args[0]);
    case 2:
        return GetTemplate(args[0], args[1]);
    default:
        THROW1_TRACE(runtime_error, "undefined " + GetName() + "<...> template");
    }
}


CTemplateResolver1::CTemplateResolver1(const string& name)
    : CParent(name)
{
}

void CTemplateResolver1::Register(TTypeInfo templ, TTypeInfo arg)
{
    if ( !m_Templates.insert(TTMap::value_type(arg,
                                               templ)).second ) {
        throw runtime_error("duplicate " + GetName() +
                            "<" + arg->GetName() + "> template");
    }
}

void CTemplateResolver1::Write(CObjectOStream& ,
                               TTypeInfo , TTypeInfo ) const
{
    //    out.WriteTemplateInfo(GetName(), tmpl, arg);
}

TTypeInfo CTemplateResolver1::GetTemplate(TTypeInfo arg) const
{
    TTMap::const_iterator i = m_Templates.find(arg);
    if ( i == m_Templates.end() ) {
        return CParent::GetTemplate(arg);
    }
    return i->second;
}



CTemplateResolver2::CTemplateResolver2(const string& name)
    : CParent(name)
{
}

void CTemplateResolver2::Register(TTypeInfo templ,
                                  TTypeInfo arg1, TTypeInfo arg2)
{
    if ( !m_Templates[arg1].insert(TTMap2::value_type(arg2,
                                                      templ)).second ) {
        throw runtime_error("duplicate " + GetName() +
                            "<" + arg1->GetName() + ", " +
                            arg2->GetName() +
                            "> template");
    }
}

void CTemplateResolver2::Write(CObjectOStream& , TTypeInfo ,
                               TTypeInfo , TTypeInfo ) const
{
    //    out.WriteTemplateInfo(GetName(), tmpl, arg1, arg2);
}

TTypeInfo CTemplateResolver2::GetTemplate(TTypeInfo arg1,
                                                     TTypeInfo arg2) const
{
    TTMap::const_iterator i = m_Templates.find(arg1);
    if ( i == m_Templates.end() ) {
        throw runtime_error("undefined " + GetName() +
                            "<" + arg1->GetName() + ", ...> template");
    }
    TTMap2::const_iterator i2 = i->second.find(arg2);
    if ( i2 == i->second.end() ) {
        return CParent::GetTemplate(arg1, arg2);
    }
    return i2->second;
}

END_NCBI_SCOPE

#if defined(NODEMAP__HPP)  &&  !defined(NODEMAP__INL)
#define NODEMAP__INL

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
*    Various tag mappers classes
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  1999/04/27 14:49:58  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* Revision 1.4  1998/12/24 16:15:38  vasilche
* Added CHTMLComment class.
* Added TagMappers from static functions.
*
* Revision 1.3  1998/12/23 21:20:59  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.2  1998/12/22 16:39:12  vasilche
* Added ReadyTagMapper to map tags to precreated nodes.
*
* Revision 1.1  1998/12/21 22:24:58  vasilche
* A lot of cleaning.
*
* ===========================================================================
*/

template<class C>
inline StaticTagMapperByNode<C>::StaticTagMapperByNode(CNCBINode* (*function)(C* node))
    : m_Function(function)
{
}

template<class C>
inline CNCBINode* StaticTagMapperByNode<C>::MapTag(CNCBINode* _this, const string&) const
{
    return m_Function(dynamic_cast<C*>(_this));
}

template<class C>
inline StaticTagMapperByNodeAndName<C>::StaticTagMapperByNodeAndName(CNCBINode* (*function)(C* node, const string& name))
    : m_Function(function)
{
}

template<class C>
inline CNCBINode* StaticTagMapperByNodeAndName<C>::MapTag(CNCBINode* _this, const string&) const
{
    return m_Function(dynamic_cast<C*>(_this), name);
}

template<class C>
inline TagMapper<C>::TagMapper(CNCBINode* (C::*method)(void))
    : m_Method(method)
{
}

template<class C>
inline CNCBINode* TagMapper<C>::MapTag(CNCBINode* _this, const string&) const
{
    return (dynamic_cast<C*>(_this)->*m_Method)();
}

template<class C>
inline TagMapperByName<C>::TagMapperByName(CNCBINode* (C::*method)(const string& name))
    : m_Method(method)
{
}

template<class C>
inline CNCBINode* TagMapperByName<C>::MapTag(CNCBINode* _this, const string& name) const
{
    return (dynamic_cast<C*>(_this)->*m_Method)(name);
}

#endif /* def NODEMAP__HPP  &&  ndef NODEMAP__INL */

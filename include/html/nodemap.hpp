#ifndef NODEMAP__HPP
#define NODEMAP__HPP

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
* Revision 1.1  1998/12/21 22:24:58  vasilche
* A lot of cleaning.
*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <page.hpp>

//  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//  !!! PUT YOUR OTHER #include's HERE !!!
//  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE

class CNCBINode;
class CHTMLBasicPage;

struct BaseTagMapper {
    virtual ~BaseTagMapper(void);

    virtual CNCBINode* MapTag(CHTMLBasicPage* page, const string& name) const = 0;

    virtual BaseTagMapper* Clone(void) const = 0;
};

struct BaseTagMapperCaller
{
    virtual CNCBINode* Call(CHTMLBasicPage* page, const string& name) const = 0;

    virtual BaseTagMapperCaller* Clone() const = 0;
};

template<class C>
struct TagMapperCaller : BaseTagMapperCaller
{
    TagMapperCaller(CNCBINode* (C::*method)(void))
        : m_Method(method)
    {
    }

    virtual CNCBINode* Call(CHTMLBasicPage* page, const string& ) const
    {
        return (dynamic_cast<C*>(page)->*m_Method)();
    }

    virtual BaseTagMapperCaller* Clone() const
    {
        return new TagMapperCaller(m_Method);
    }

private:
    CNCBINode* (C::*m_Method)(void);
};

template<class C>
struct TagMapperCallerWithName : BaseTagMapperCaller
{
    TagMapperCallerWithName(CNCBINode* (C::*method)(const string&))
        : m_Method(method)
    {
    }

    virtual CNCBINode* Call(CHTMLBasicPage* page, const string& name) const
    {
        return (dynamic_cast<C*>(page)->*m_Method)(name);
    }

    virtual BaseTagMapperCaller* Clone() const
    {
        return new TagMapperCallerWithName(m_Method);
    }

private:
    CNCBINode* (C::*m_Method)(const string&);
};

template<class C>
struct TagMapper : BaseTagMapper
{
    TagMapper(CNCBINode* (C::*method)(void))
        : m_Caller(new TagMapperCaller<C>(method))
    {
    }

    TagMapper(CNCBINode* (C::*method)(const string& tagname))
        : m_Caller(new TagMapperCallerWithName<C>(method))
    {
    }

    ~TagMapper(void)
    {
        delete m_Caller;
    }
    
    virtual BaseTagMapper* Clone(void) const
    {
        return new TagMapper(*this);
    }

    virtual CNCBINode* MapTag(CHTMLBasicPage* page, const string& name) const
    {
        return m_Caller->Call(page, name);
    }

private:
    BaseTagMapperCaller* m_Caller;

    TagMapper(const TagMapper& src)
        : m_Caller(src.m_Caller->Clone())
    {
    }
};

///////////////////////////////////////////////////////
// All inline function implementations and internal data
// types, etc. are in this file
#include <nodemap.inl>

// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif  /* NODEMAP__HPP */

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
*   Various tag mappers classes
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  1999/07/08 18:05:13  vakatov
* Fixed compilation warnings
*
* Revision 1.7  1999/05/28 16:32:10  vasilche
* Fixed memory leak in page tag mappers.
*
* Revision 1.6  1999/04/27 14:49:58  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* Revision 1.5  1998/12/28 20:29:13  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.4  1998/12/24 16:15:37  vasilche
* Added CHTMLComment class.
* Added TagMappers from static functions.
*
* Revision 1.3  1998/12/23 21:20:59  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.2  1998/12/22 16:39:11  vasilche
* Added ReadyTagMapper to map tags to precreated nodes.
*
* Revision 1.1  1998/12/21 22:24:58  vasilche
* A lot of cleaning.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

class CNCBINode;

struct BaseTagMapper
{
    virtual ~BaseTagMapper(void);

    virtual BaseTagMapper* Clone(void) const = 0;

    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const = 0;
};

struct StaticTagMapper : public BaseTagMapper
{
    StaticTagMapper(CNCBINode* (*function)(void));

    virtual BaseTagMapper* Clone(void) const;
    
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const;

private:
    CNCBINode* (*m_Function)(void);
};

struct StaticTagMapperByName : public BaseTagMapper
{
    StaticTagMapperByName(CNCBINode* (*function)(const string& name));
    
    virtual BaseTagMapper* Clone(void) const;
    
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const;

private:
    CNCBINode* (*m_Function)(const string& name);
};

template<class C>
struct StaticTagMapperByNode : public BaseTagMapper
{
    StaticTagMapperByNode(CNCBINode* (*function)(C* node));
    
    virtual BaseTagMapper* Clone(void) const;
    
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const;

private:
    CNCBINode* (*m_Function)(C* node);
};

template<class C>
struct StaticTagMapperByNodeAndName : public BaseTagMapper
{
    StaticTagMapperByNodeAndName(CNCBINode* (*function)(C* node, const string& name));
    
    virtual BaseTagMapper* Clone(void) const;
    
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const;

private:
    CNCBINode* (*m_Function)(C* node, const string& name);
};

struct ReadyTagMapper : public BaseTagMapper
{
    ReadyTagMapper(CNCBINode* node);

    virtual BaseTagMapper* Clone(void) const;
    
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const;

private:
    auto_ptr<CNCBINode> m_Node;
};

template<class C>
struct TagMapper : public BaseTagMapper
{
    TagMapper(CNCBINode* (C::*method)(void));

    virtual BaseTagMapper* Clone(void) const;
    
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const;

private:
    CNCBINode* (C::*m_Method)(void);
};

template<class C>
struct TagMapperByName : public BaseTagMapper
{
    TagMapperByName(CNCBINode* (C::*method)(const string& name));

    virtual BaseTagMapper* Clone(void) const;
    
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const;

private:
    CNCBINode* (C::*m_Method)(const string& name);
};

#include <html/nodemap.inl>

inline
BaseTagMapper* CreateTagMapper(CNCBINode* node)
{
    return new ReadyTagMapper(node);
}

inline
BaseTagMapper* CreateTagMapper(CNCBINode* (*function)(void))
{
    return new StaticTagMapper(function);
}

inline
BaseTagMapper* CreateTagMapper(CNCBINode* (*function)(const string& name))
{
    return new StaticTagMapperByName(function);
}

template<class C>
inline
BaseTagMapper* CreateTagMapper(CNCBINode* (*function)(C* node))
{
    return new StaticTagMapperByNode<C>(function);
}

template<class C>
inline
BaseTagMapper* CreateTagMapper(CNCBINode* (*function)(C* node, const string& name))
{
    return new StaticTagMapperByNodeAndName<C>(function);
}

template<class C>
inline
BaseTagMapper* CreateTagMapper(const C*, CNCBINode* (C::*method)(void))
{
    return new TagMapper<C>(method);
}

template<class C>
inline
BaseTagMapper* CreateTagMapper(const C*,
                               CNCBINode* (C::*method)(const string& name))
{
    return new TagMapperByName<C>(method);
}

END_NCBI_SCOPE

#endif  /* NODEMAP__HPP */

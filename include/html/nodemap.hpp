#ifndef HTML___NODEMAP__HPP
#define HTML___NODEMAP__HPP

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
 * Author:  Eugene Vasilchenko
 *
 */

/// @file nodemap.hpp 
/// Various tag mappers classes.


#include <corelib/ncbistd.hpp>
#include <html/node.hpp>


/** @addtogroup TagMapper
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class CNCBINode;

struct NCBI_XHTML_EXPORT BaseTagMapper
{
    virtual ~BaseTagMapper(void) {};
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const = 0;
};


struct NCBI_XHTML_EXPORT StaticTagMapper : public BaseTagMapper
{
    StaticTagMapper(CNCBINode* (*function)(void))
        : m_Function(function)
        { return; }
    virtual
    CNCBINode* MapTag(CNCBINode* /*_this*/, const string& /*name*/) const
        { return (*m_Function)(); }
private:
    CNCBINode* (*m_Function)(void);
};


struct NCBI_XHTML_EXPORT StaticTagMapperByName : public BaseTagMapper
{
    StaticTagMapperByName(CNCBINode* (*function)(const string& name))
        : m_Function(function)
        { return; };
    virtual CNCBINode* MapTag(CNCBINode* /*_this*/, const string& name) const
        { return (*m_Function)(name); }
private:
    CNCBINode* (*m_Function)(const string& name);
};


struct NCBI_XHTML_EXPORT StaticTagMapperByData : public BaseTagMapper
{
    StaticTagMapperByData(CNCBINode* (*function)(void* data), void* data)
        : m_Function(function), m_Data(data)
        { return; }
    virtual
    CNCBINode* MapTag(CNCBINode* /*_this*/, const string& /*name*/) const
        { return (*m_Function)(m_Data); }
private:
    CNCBINode* (*m_Function)(void* data);
    void* m_Data;
};


struct NCBI_XHTML_EXPORT StaticTagMapperByDataAndName : public BaseTagMapper
{
    StaticTagMapperByDataAndName(
        CNCBINode* (*function)(void* data, const string& name), void* data)
        : m_Function(function), m_Data(data)
        { return; }
    virtual CNCBINode* MapTag(CNCBINode* /*_this*/, const string& name) const
        { return (*m_Function)(m_Data, name); }
private:
    CNCBINode* (*m_Function)(void* data, const string& name);
    void* m_Data;
};


template<class C>
struct StaticTagMapperByNode : public BaseTagMapper
{
    StaticTagMapperByNode(CNCBINode* (*function)(C* node))
        : m_Function(function)
        { return; }
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const
        { return m_Function(dynamic_cast<C*>(_this)); }

private:
    CNCBINode* (*m_Function)(C* node);
};


template<class C>
struct StaticTagMapperByNodeAndName : public BaseTagMapper
{
    StaticTagMapperByNodeAndName(
        CNCBINode* (*function)(C* node, const string& name))
        : m_Function(function)
        { return; }
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const
        { return m_Function(dynamic_cast<C*>(_this), name); }
private:
    CNCBINode* (*m_Function)(C* node, const string& name);
};


template<class C, typename T>
struct StaticTagMapperByNodeAndData : public BaseTagMapper
{
    StaticTagMapperByNodeAndData(
        CNCBINode* (*function)(C* node, T data), T data)
        : m_Function(function), m_Data(data)
        { return; }
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& /*name*/) const
        { return m_Function(dynamic_cast<C*>(_this), m_Data); }
private:
    CNCBINode* (*m_Function)(C* node, T data);
    T m_Data;
};


template<class C, typename T>
struct StaticTagMapperByNodeAndDataAndName : public BaseTagMapper
{
    StaticTagMapperByNodeAndDataAndName(
        CNCBINode* (*function)(C* node, T data, const string& name), T data)
        : m_Function(function), m_Data(data)
        { return; }
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const
    { return m_Function(dynamic_cast<C*>(_this), m_Data, name); }
private:
    CNCBINode* (*m_Function)(C* node, T data, const string& name);
    T m_Data;
};


struct NCBI_XHTML_EXPORT ReadyTagMapper : public BaseTagMapper
{
    ReadyTagMapper(CNCBINode* node)
        : m_Node(node)
        { return; }
    virtual CNCBINode* MapTag(CNCBINode* /*_this*/,
                              const string& /*name*/) const
        { return &*m_Node; }
private:
    mutable CNodeRef m_Node;
};


template<class C>
struct TagMapper : public BaseTagMapper
{
    TagMapper(CNCBINode* (C::*method)(void))
        : m_Method(method)
        { return; }
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const
        { return (dynamic_cast<C*>(_this)->*m_Method)(); }
private:
    CNCBINode* (C::*m_Method)(void);
};


template<class C>
struct TagMapperByName : public BaseTagMapper
{
    TagMapperByName(CNCBINode* (C::*method)(const string& name))
        : m_Method(method)
        { return; }
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const
        { return (dynamic_cast<C*>(_this)->*m_Method)(name); }
private:
    CNCBINode* (C::*m_Method)(const string& name);
};


template<class C, typename T>
struct TagMapperByData : public BaseTagMapper
{
    TagMapperByData(CNCBINode* (C::*method)(T data), T data)
        : m_Method(method), m_Data(data)
        { return; }
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& /*name*/) const
        { return (dynamic_cast<C*>(_this)->*m_Method)(m_Data); }
private:
    CNCBINode* (C::*m_Method)(T data);
    T m_Data;
};


template<class C, typename T>
struct TagMapperByDataAndName : public BaseTagMapper
{
    TagMapperByDataAndName(
        CNCBINode* (C::*method)(T data, const string& name), T data)
        : m_Method(method), m_Data(data)
        { return; }
    virtual CNCBINode* MapTag(CNCBINode* _this, const string& name) const
        { return (dynamic_cast<C*>(_this)->*m_Method)(m_Data, name); }
private:
    CNCBINode* (C::*m_Method)(T data, const string& name);
    T m_Data;
};


//=============================================================================
//  Inline methods
//=============================================================================

inline
BaseTagMapper* CreateTagMapper(CNCBINode* node)
{
    return new ReadyTagMapper(node);
}

inline
BaseTagMapper* CreateTagMapper(
    CNCBINode* (*function)(void))
{
    return new StaticTagMapper(function);
}

inline
BaseTagMapper* CreateTagMapper(
    CNCBINode* (*function)(const string& name))
{
    return new StaticTagMapperByName(function);
}

inline
BaseTagMapper* CreateTagMapper(
    CNCBINode* (*function)(void* data), void* data)
{
    return new StaticTagMapperByData(function, data);
}

inline
BaseTagMapper* CreateTagMapper(
    CNCBINode* (*function)(void* data, const string& name), void* data)
{
    return new StaticTagMapperByDataAndName(function, data);
}

template<class C>
BaseTagMapper* CreateTagMapper(CNCBINode* (*function)(C* node))
{
    return new StaticTagMapperByNode<C>(function);
}

template<class C>
BaseTagMapper* CreateTagMapper(
    CNCBINode* (*function)(C* node, const string& name))
{
    return new StaticTagMapperByNodeAndName<C>(function);
}

template<class C, typename T>
BaseTagMapper* CreateTagMapper(
    CNCBINode* (*function)(C* node, T data), T data)
{
    return new StaticTagMapperByNodeAndData<C,T>(function, data);
}

template<class C, typename T>
BaseTagMapper* CreateTagMapper(
    CNCBINode* (*function)(C* node, T data, const string& name), T data)
{
    return new StaticTagMapperByNodeAndDataAndName<C,T>(function, data);
}

template<class C>
BaseTagMapper* CreateTagMapper(const C*, CNCBINode* (C::*method)(void))
{
    return new TagMapper<C>(method);
}

template<class C>
BaseTagMapper* CreateTagMapper(
    const C*, CNCBINode* (C::*method)(const string& name))
{
    return new TagMapperByName<C>(method);
}

template<class C, typename T>
BaseTagMapper* CreateTagMapper(CNCBINode* (C::*method)(T data), T data)
{
    return new TagMapperByData<C,T>(method, data);
}

template<class C, typename T>
BaseTagMapper* CreateTagMapper(
    CNCBINode* (C::*method)(T data, const string& name), T data)
{
    return new TagMapperByDataAndName<C,T>(method, data);
}


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.19  2006/05/10 14:48:45  ivanov
 * Get rid of warnings about unused variables
 *
 * Revision 1.18  2006/04/28 13:37:50  ivanov
 * Get rid about unused variable warnings
 *
 * Revision 1.17  2005/08/22 13:29:40  ivanov
 * Added missed ';'
 *
 * Revision 1.16  2005/08/22 12:13:27  ivanov
 * Dropped nodemap.[cpp|inl]
 *
 * Revision 1.15  2004/02/02 14:14:15  ivanov
 * Added TagMapper to functons and class methods which used a data parameter
 *
 * Revision 1.14  2004/01/16 15:12:32  ivanov
 * Minor cosmetic changes
 *
 * Revision 1.13  2003/11/05 18:41:06  dicuccio
 * Added export specifiers
 *
 * Revision 1.12  2003/11/03 17:02:53  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.11  2003/04/25 13:45:35  siyan
 * Added doxygen groupings
 *
 * Revision 1.10  1999/10/28 13:40:31  vasilche
 * Added reference counters to CNCBINode.
 *
 * Revision 1.9  1999/09/03 21:27:28  vakatov
 * + #include <memory>
 *
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

#endif  /* HTML___NODEMAP__HPP */

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

#include <ncbi_pch.hpp>
#include <html/page.hpp>


BEGIN_NCBI_SCOPE


BaseTagMapper::~BaseTagMapper()
{
    return;
}


StaticTagMapper::StaticTagMapper(CNCBINode* (*function)(void))
    : m_Function(function)
{
    return;
}


CNCBINode* StaticTagMapper::MapTag(CNCBINode*, const string&) const
{
    return (*m_Function)();
}


StaticTagMapperByName::StaticTagMapperByName(
    CNCBINode* (*function)(const string& name))
    : m_Function(function)
{
    return;
}


CNCBINode* StaticTagMapperByName::MapTag(CNCBINode*, const string& name) const
{
    return (*m_Function)(name);
}


StaticTagMapperByData::StaticTagMapperByData(
    CNCBINode* (*function)(void* data), void* data)
    : m_Function(function), m_Data(data)
{
    return;
}


CNCBINode* StaticTagMapperByData::MapTag(CNCBINode*,
                                         const string& /*name*/) const
{
    return (*m_Function)(m_Data);
}


StaticTagMapperByDataAndName::StaticTagMapperByDataAndName(
    CNCBINode* (*function)(void* data, const string& name), void* data)
    : m_Function(function), m_Data(data)
{
    return;
}


CNCBINode* StaticTagMapperByDataAndName::MapTag(CNCBINode*,
                                                const string& name) const
{
    return (*m_Function)(m_Data, name);
}


ReadyTagMapper::ReadyTagMapper(CNCBINode* node)
    : m_Node(node)
{
    return;
}


CNCBINode* ReadyTagMapper::MapTag(CNCBINode*, const string&) const
{
    return &*m_Node;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2004/05/17 20:59:50  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.9  2004/02/02 14:14:45  ivanov
 * Added TagMapper to functons and class methods which used a data parameter
 *
 * Revision 1.8  2003/11/03 17:03:08  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.7  2000/03/07 15:26:13  vasilche
 * Removed second definition of CRef.
 *
 * Revision 1.6  1999/10/28 13:40:36  vasilche
 * Added reference counters to CNCBINode.
 *
 * Revision 1.5  1999/05/28 16:32:15  vasilche
 * Fixed memory leak in page tag mappers.
 *
 * Revision 1.4  1999/01/06 21:35:37  vasilche
 * Avoid use of Clone.
 * Fixed default CHTML_text width.
 *
 * Revision 1.3  1998/12/28 20:29:19  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.2  1998/12/24 16:15:42  vasilche
 * Added CHTMLComment class.
 * Added TagMappers from static functions.
 *
 * Revision 1.1  1998/12/22 16:39:15  vasilche
 * Added ReadyTagMapper to map tags to precreated nodes.
 *
 * ===========================================================================
 */

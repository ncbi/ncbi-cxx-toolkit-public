/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko
*
*  File Description: Serialization hooks to make strings with equal value
*                    to share representation object.
*
*/

#include <objmgr/impl/pack_string.hpp>
#include <serial/objistr.hpp>
#include <serial/objectiter.hpp>

BEGIN_NCBI_SCOPE


CPackStringHook::CPackStringHook(size_t length_limit, size_t count_limit)
    : m_LengthLimit(length_limit), m_CountLimit(count_limit),
      m_Skipped(0), m_CompressedIn(0), m_CompressedOut(0)
{
}


CPackStringHook::~CPackStringHook(void)
{
/*
    NcbiCout << "CPackStringHook: skipped: " << m_Skipped<<" compressed "<<m_CompressedIn<<" string into "<<m_CompressedOut<<" :";
    ITERATE ( set<string>, it, m_Strings ) {
        NcbiCout << " \"" << *it<<"\"";
    }
    NcbiCout << NcbiEndl;
*/
}


void CPackStringHook::ReadString(CObjectIStream& in, string& s)
{
    in.ReadString(s);
    if ( s.size() > m_LengthLimit ) {
        ++m_Skipped;
        return;
    }
    if ( m_CompressedOut < m_CountLimit ) {
        ++m_CompressedIn;
        pair<set<string>::iterator,bool> ins = m_Strings.insert(s);
        if ( ins.second ) {
            ++m_CompressedOut;
            ins.first->c_str();
        }
        s = *ins.first;
        if ( s.c_str() != ins.first->data() ) {
            THROW1_TRACE(runtime_error,
                         "bad ref counting");
        }
    }
    else {
        set<string>::const_iterator it = m_Strings.find(s);
        if ( it == m_Strings.end() ) {
            ++m_Skipped;
            return;
        }
        ++m_CompressedIn;
        s = *it;
        if ( s.c_str() != it->data() ) {
            THROW1_TRACE(runtime_error,
                         "bad ref counting");
        }
    }
}


CPackStringClassHook::CPackStringClassHook(size_t length_limit,
                                           size_t count_limit)
    : CPackStringHook(length_limit, count_limit)
{
}


CPackStringClassHook::~CPackStringClassHook(void)
{
}


void CPackStringClassHook::ReadClassMember(CObjectIStream& in,
                                           const CObjectInfo::CMemberIterator& member)
{
    ReadString(in, *CType<string>::GetUnchecked(*member));
}


CPackStringChoiceHook::CPackStringChoiceHook(size_t length_limit,
                                             size_t count_limit)
    : CPackStringHook(length_limit, count_limit)
{
}


CPackStringChoiceHook::~CPackStringChoiceHook(void)
{
}


void CPackStringChoiceHook::ReadChoiceVariant(CObjectIStream& in,
                                              const CObjectInfo::CChoiceVariant& variant)
{
    ReadString(in, *CType<string>::GetUnchecked(*variant));
}


END_NCBI_SCOPE

/* $Id$
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
 * Author:  Viatcheslav Gorelenkov
 *
 */

#include <app/project_tree_builder/resolver.hpp>
#include <corelib/ncbistr.hpp>


BEGIN_NCBI_SCOPE


CSymResolver::CSymResolver(void)
{
    Clear();
}


CSymResolver::CSymResolver(const CSymResolver& resolver)
{
    SetFrom(resolver);
}


CSymResolver::CSymResolver(const string& file_path)
{
    LoadFrom(file_path, this);
}


CSymResolver& CSymResolver::operator= (const CSymResolver& resolver)
{
    if (this != &resolver) {
	    Clear();
	    SetFrom(resolver);
    }
    return *this;
}


CSymResolver::~CSymResolver(void)
{
    Clear();
}


static string s_StripDefine(const string& define)
{
    return string(define, 2, define.length() - 3);
}


void CSymResolver::Resolve(const string& define, list<string>* resolved_def)
{
    if ( !IsDefine(define) ) {
	    resolved_def->push_back(define);
	    return;
    }

    string str_define = s_StripDefine(define);

    CSimpleMakeFileContents::TContents::const_iterator m =
        m_Cache.find(str_define);

    if (m != m_Cache.end()) {
	    *resolved_def = m->second;
	    return;
    }
	    
    ITERATE(CSimpleMakeFileContents::TContents, p, m_Data.m_Contents) {
	    if (p->first == str_define) {
            ITERATE(list<string>, n, p->second) {
                Resolve(*n, resolved_def);
            }
	    }
    }

    m_Cache[str_define] = *resolved_def;
}


bool CSymResolver::IsDefine(const string& param)
{
    return NStr::StartsWith(param, "$(")  &&  NStr::EndsWith(param, ")");
}


void CSymResolver::LoadFrom(const string& file_path, 
                            CSymResolver * resolver)
{
    resolver->Clear();
    CSimpleMakeFileContents::LoadFrom(file_path, &resolver->m_Data);
}


bool CSymResolver::IsEmpty(void) const
{
    return m_Data.m_Contents.empty();
}


void CSymResolver::Clear(void)
{
    m_Data.m_Contents.clear();
    m_Cache.clear();
}


void CSymResolver::SetFrom(const CSymResolver& resolver)
{
    m_Data  = resolver.m_Data;
    m_Cache = resolver.m_Cache;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/01/22 17:57:55  gorelenk
 * first version
 *
 * ===========================================================================
 */
 
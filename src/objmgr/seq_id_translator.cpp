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
* Author: Maxim Didenko
*
* File Description:
*   Seq-id translator
*
*/


#include <ncbi_pch.hpp>

#include <objects/seq/seq_id_handle.hpp>
#include <objmgr/seq_id_translator.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

ISeq_id_Translator::~ISeq_id_Translator()
{
}


CSeq_id_Translator_Simple::~CSeq_id_Translator_Simple()
{
}

void CSeq_id_Translator_Simple::AddMapEntry(const CSeq_id_Handle& orig, 
                                            const CSeq_id_Handle& patched)
{
    if (m_OrigToPatched.find(orig) != m_OrigToPatched.end() &&
        m_PatchedToOrig.find(patched) != m_PatchedToOrig.end() )
        return;
    m_OrigToPatched[orig] = patched;
    m_OrigToPatched[patched] = CSeq_id_Handle();
    m_PatchedToOrig[patched] = orig;
    m_PatchedToOrig[orig] = CSeq_id_Handle();
    
}

CSeq_id_Handle 
CSeq_id_Translator_Simple::TranslateToOrig(const CSeq_id_Handle& patched) const
{
    TMap::const_iterator it = m_PatchedToOrig.find(patched);
    if (it != m_PatchedToOrig.end())
        return it->second;
    return patched;
}
CSeq_id_Handle 
CSeq_id_Translator_Simple::TranslateToPatched(const CSeq_id_Handle& orig) const
{
    TMap::const_iterator it = m_OrigToPatched.find(orig);
    if (it != m_OrigToPatched.end())
        return it->second;
    return orig;
}

void CSeq_id_Translator_Simple::TranslateToOrig(const ISeq_id_Translator::TIds& patched, 
                                                ISeq_id_Translator::TIds& orig) const
{
    orig.clear();
    orig.reserve(patched.size());
    ITERATE(ISeq_id_Translator::TIds, it, patched) {
        orig.push_back( TranslateToOrig(*it) );
    }
}
void CSeq_id_Translator_Simple::TranslateToPatched(const ISeq_id_Translator::TIds& orig, 
                                                   ISeq_id_Translator::TIds& patched) const
{
    patched.clear();
    patched.reserve(orig.size());
    ITERATE(ISeq_id_Translator::TIds, it, orig) {
        patched.push_back( TranslateToOrig(*it) );
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/08/25 14:05:37  didenko
 * Restructured TSE loading process
 *
 *
 * ===========================================================================
 */

#ifndef PACK_STRING__HPP_INCLUDED
#define PACK_STRING__HPP_INCLUDED

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
*  File Description: Data reader from Pubseq_OS
*
*/

#include <serial/objhook.hpp>

#include <string>
#include <set>

BEGIN_NCBI_SCOPE

class NCBI_XOBJMGR_EXPORT CPackString
{
public:
    CPackString(size_t length_limit = 32, size_t count_limit = 256);
    ~CPackString(void);

    void ReadString(CObjectIStream& in, string& s);

    static void Assign(string& s, const string& src);

private:
    static void x_RefCounterError(void);
    static void x_Assign(string& s, const string& src);

    size_t m_LengthLimit;
    size_t m_CountLimit;
    size_t m_Skipped;
    size_t m_CompressedIn;
    size_t m_CompressedOut;
    set<string> m_Strings;
};


class NCBI_XOBJMGR_EXPORT CPackStringClassHook : public CReadClassMemberHook
{
public:
    CPackStringClassHook(size_t length_limit = 32, size_t count_limit = 256);
    ~CPackStringClassHook(void);
    
    void ReadClassMember(CObjectIStream& in,
                         const CObjectInfo::CMemberIterator& member);

private:
    CPackString m_PackString;
};


class NCBI_XOBJMGR_EXPORT CPackStringChoiceHook : public CReadChoiceVariantHook
{
public:
    CPackStringChoiceHook(size_t length_limit = 32, size_t count_limit = 256);
    ~CPackStringChoiceHook(void);

    void ReadChoiceVariant(CObjectIStream& in,
                           const CObjectInfo::CChoiceVariant& variant);

private:
    CPackString m_PackString;
};


inline
void CPackString::Assign(string& s, const string& src)
{
    s = src;
    if ( s.c_str() != src.c_str() ) {
        x_Assign(s, src);
    }
}


END_NCBI_SCOPE

/*
 * $Log$
 * Revision 1.4  2003/08/15 19:19:15  vasilche
 * Fixed memory leak in string packing hooks.
 * Fixed processing of 'partial' flag of features.
 * Allow table packing of non-point SNP.
 * Allow table packing of SNP with long alleles.
 *
 * Revision 1.3  2003/08/14 20:05:18  vasilche
 * Simple SNP features are stored as table internally.
 * They are recreated when needed using CFeat_CI.
 *
 * Revision 1.2  2003/07/22 21:55:11  vasilche
 * Allow correct packing with limited reference counter (MSVC - 254 max).
 *
 * Revision 1.1  2003/07/17 20:07:55  vasilche
 * Reduced memory usage by feature indexes.
 * SNP data is loaded separately through PUBSEQ_OS.
 * String compression for SNP data.
 *
 */

#endif // PACK_STRING__HPP_INCLUDED

#if defined(NCBISTR__HPP)  &&  !defined(NCBISTR__INL)
#define NCBISTR__INL

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
* Revision 1.1  2000/12/15 15:36:30  vasilche
* Added header corelib/ncbistr.hpp for all string utility functions.
* Optimized string utility functions.
* Added assignment operator to CRef<> and CConstRef<>.
* Add Upcase() and Locase() methods for automatic conversion.
*
* ===========================================================================
*/

inline
const string& CNcbiEmptyString::Get(void)
{
    const string* str = m_Str;
    return str? *str: FirstGet();
}

inline
int NStr::Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                  const char* pattern, ECase use_case)
{
    return use_case == eCase?
        CompareCase(str, pos, n, pattern): CompareNocase(str, pos, n, pattern);
}

inline
int NStr::Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                  const string& pattern, ECase use_case)
{
    return use_case == eCase?
        CompareCase(str, pos, n, pattern): CompareNocase(str, pos, n, pattern);
}

inline
int NStr::Compare(const char* s1, const char* s2, ECase use_case)
{
    return use_case == eCase? CompareCase(s1, s2): CompareNocase(s1, s2);
}

inline
int NStr::Compare(const string& s1, const char* s2, ECase use_case)
{
    return Compare(s1, 0, NPOS, s2, use_case);
}

inline
int NStr::Compare(const char* s1, const string& s2, ECase use_case)
{
    return Compare(s2, 0, NPOS, s1, use_case);
}

inline
int NStr::Compare(const string& s1, const string& s2, ECase use_case)
{
    return Compare(s1, 0, NPOS, s2, use_case);
}

inline
bool NStr::StartsWith(const string& str, const string& start, ECase use_case)
{
    return str.size() >= start.size()  &&
        Compare(str, 0, start.size(), start, use_case) == 0;
}

inline
bool NStr::EndsWith(const string& str, const string& end, ECase use_case)
{
    return str.size() >= end.size()  &&
        Compare(str, str.size() - end.size(), end.size(), end, use_case) == 0;
}

inline
int PCase::Compare(const string& s1, const string& s2) const
{
    return NStr::Compare(s1, s2, NStr::eCase);
}

inline
bool PCase::Less(const string& s1, const string& s2) const
{
    return Compare(s1, s2) < 0;
}

inline
bool PCase::Equals(const string& s1, const string& s2) const
{
    return Compare(s1, s2) == 0;
}

inline
bool PCase::operator()(const string& s1, const string& s2) const
{
    return Less(s1, s2);
}

inline
int PNocase::Compare(const string& s1, const string& s2) const
{
    return NStr::Compare(s1, s2, NStr::eNocase);
}

inline
bool PNocase::Less(const string& s1, const string& s2) const
{
    return Compare(s1, s2) < 0;
}

inline
bool PNocase::Equals(const string& s1, const string& s2) const
{
    return Compare(s1, s2) == 0;
}

inline
bool PNocase::operator()(const string& s1, const string& s2) const
{
    return Less(s1, s2);
}

#endif /* def NCBISTR__HPP  &&  ndef NCBISTR__INL */

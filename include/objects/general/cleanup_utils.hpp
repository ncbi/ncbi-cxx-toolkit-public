#ifndef OBJECTS_GENERAL___CLEANUP_UTILS__HPP
#define OBJECTS_GENERAL___CLEANUP_UTILS__HPP

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
 * Author:  Mati Shomrat
 *
 * File Description:
 *   General utilities for data cleanup.
 *
 * ===========================================================================
 */
#include <corelib/ncbistd.hpp>
#include <algorithm>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/// convert double quotes to single quotes
inline 
void NCBI_GENERAL_EXPORT ConvertDoubleQuotes(string& str)
{
    if (!str.empty()) {
        replace(str.begin(), str.end(), '\"', '\'');
    }
}


/// truncate spaces and semicolons
void NCBI_GENERAL_EXPORT CleanString(string& str);

/// remove all spaces from a string
void NCBI_GENERAL_EXPORT RemoveSpaces(string& str);

/// clean a container of strings, remove blanks and repeats.
template<typename C>
void CleanStringContainer(C& str_cont)
{
    typename C::iterator it = str_cont.begin();
    while (it != str_cont.end()) {
        CleanString(*it);
        if (NStr::IsBlank(*it)) {
            it = str_cont.erase(it);
        } else {
            ++it;
        }
    }
}


#define TRUNCATE_SPACES(x) \
    if (IsSet##x()) { \
        NStr::TruncateSpacesInPlace(Set##x()); \
        if (NStr::IsBlank(Get##x())) { \
            Reset##x(); \
        } \
    }

#define TRUNCATE_CHOICE_SPACES(x) \
    NStr::TruncateSpacesInPlace(Set##x()); \
    if (NStr::IsBlank(Get##x())) { \
        Reset(); \
    }

#define CONVERT_QUOTES(x) \
    if (IsSet##x()) { \
        ConvertDoubleQuotes(Set##x()); \
    }

#define CLEAN_STRING_MEMBER(x) \
    if (IsSet##x()) { \
        CleanString(Set##x()); \
        if (NStr::IsBlank(Get##x())) { \
            Reset##x(); \
        } \
    }

#define CLEAN_STRING_CHOICE(x) \
        CleanString(Set##x()); \
        if (NStr::IsBlank(Get##x())) { \
            Reset(); \
        }

#define CLEAN_STRING_LIST(x) \
    if (IsSet##x()) { \
        CleanStringContainer(Set##x()); \
        if (Get##x().empty()) { \
            Reset##x(); \
        } \
    }

/// clean a string member 'x' of an internal object 'o'
#define CLEAN_INTERNAL_STRING(o, x) \
    if (o.IsSet##x()) { \
        CleanString(o.Set##x()); \
        if (NStr::IsBlank(o.Get##x())) { \
            o.Reset##x(); \
        } \
    }


/// right now a slightly different cleanup is performed for EMBL/DDBJ and
/// SwissProt records. All other types are handaled as GenBank records.
enum ECleanupMode
{
    eCleanup_EMBL,
    eCleanup_DDBJ,
    eCleanup_SwissProt,
    eCleanup_GenBank
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2005/05/20 13:29:48  shomrat
* Added BasicCleanup()
*
*
* ===========================================================================
*/


#endif  // OBJECTS_GENERAL___CLEANUP_UTILS__HPP

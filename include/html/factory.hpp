#ifndef HTML___FACTORY__HPP
#define HTML___FACTORY__HPP

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
 * Author:  Lewis Geer
 *
 */

/// @file factory.hpp 
/// Class creation factory.

#include <cgi/ncbicgi.hpp>
#include <map>


/** @addtogroup CreationFactory
 *
 * @{
 */


BEGIN_NCBI_SCOPE


// CFactory is used to create objects based on matching strings.
// This is used to create the dispatch table.
template <class Type>
struct SFactoryList {
    // Create the object
    Type* (*pFactory)(void);
    const char* MatchString; // The string to match
    int Style;               // Optional flags
};


template <class Type>
class CFactory {
public:
    int CgiFactory(const TCgiEntries& Cgi, SFactoryList<Type>* List);
};


// List should always end with the m_MatchString = ""
// (visual C 6.0 doesn't allow templates to find the size of arrays).
template <class Type>
int CFactory<Type>::CgiFactory(const TCgiEntries& Cgi,
                               SFactoryList<Type>* List)
{
    int i = 0;
    TCgiEntriesCI iRange, iPageCgi;
    pair<TCgiEntriesCI, TCgiEntriesCI> Range;
    TCgiEntries PageCgi;

    while ( !string(List[i].MatchString).empty() ) {
        PageCgi.erase(PageCgi.begin(), PageCgi.end());
        // Parse the MatchString
        CCgiRequest::ParseEntries(List[i].MatchString, PageCgi);
        bool ThisPage = true;
        for ( iPageCgi = PageCgi.begin(); iPageCgi != PageCgi.end(); iPageCgi++) { 
            Range = Cgi.equal_range(iPageCgi->first);
            for ( iRange = Range.first; iRange != Range.second; iRange++ ) {
                if ( iRange->second == iPageCgi->second)
                    goto equality;
                if ( iPageCgi->second.empty())
                    goto equality;  // wildcard
            }
            ThisPage = false;
        equality:
            ;
        }
        if ( ThisPage ) {
            break;
        }
        i++;
    }
    return i;
}


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2004/01/16 15:12:31  ivanov
 * Minor cosmetic changes
 *
 * Revision 1.9  2003/11/03 17:02:53  ivanov
 * Some formal code rearrangement. Move log to end.
 *
 * Revision 1.8  2003/04/25 13:45:24  siyan
 * Added doxygen groupings
 *
 * Revision 1.7  2002/07/10 18:42:59  ucko
 * Use proper typedefs in order to work with CCgiEntry.
 *
 * Revision 1.6  1999/05/11 02:53:42  vakatov
 * Moved CGI API from "corelib/" to "cgi/"
 *
 * Revision 1.5  1998/12/28 23:29:02  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.4  1998/12/28 21:48:11  vasilche
 * Made Lewis's 'tool' compilable
 *
 * Revision 1.3  1998/12/23 14:28:06  vasilche
 * Most of closed HTML tags made via template.
 *
 * Revision 1.2  1998/12/09 23:02:54  lewisg
 * update to new cgiapp class
 *
 * Revision 1.1  1998/12/01 19:09:07  lewisg
 * uses CCgiApplication and new page factory
 *
 * ===========================================================================
 */

#endif  /* HTML___FACTORY__HPP */

#ifndef FACTORY__HPP
#define FACTORY__HPP

/*  $RCSfile$  $Revision$  $Date$
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
* File Description:
*   Class creation factory
*
* ---------------------------------------------------------------------------
*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <stl.hpp>
#include <ncbicgi.hpp>

BEGIN_NCBI_SCOPE


/////////////////////////////////////
// CFactory is used to create objects based on matching strings

template <class Type>  // this is used to create the dispatch table
struct SFactoryList {
    Type * (* pFactory)(void);  // the function to create the object
    char * MatchString;  // the string to match
    int Style;  // optional flags
};

template <class Type>
class CFactory {
public:
    int CgiFactory( TCgiEntries & Cgi, SFactoryList < Type > * List);
};


template <class Type>
int CFactory<Type>::CgiFactory(TCgiEntries & Cgi, SFactoryList <Type> * List)
    // - List should always end with the m_MatchString = ""
    // (visual C 6.0 doesn't allow templates to find the size
    // of arrays).
{
    int i = 0;
    multimap < string, string >::iterator iRange, iPageCgi;
    pair <multimap < string, string >::iterator, multimap < string, string >:: iterator > Range;
    TCgiEntries PageCgi;

    while(string(List[i].MatchString) != "") {
	PageCgi.erase(PageCgi.begin(), PageCgi.end());
	CCgiRequest::ParseEntries(List[i].MatchString, PageCgi);  // parse the MatchString
	bool ThisPage = true;
	for (iPageCgi = PageCgi.begin(); iPageCgi != PageCgi.end(); iPageCgi++) { 
	    Range = Cgi.equal_range(iPageCgi->first);
	    for(iRange = Range.first; iRange != Range.second; iRange++) {
		if( iRange->second == iPageCgi->second) goto equality;
		if(iPageCgi->second == "") goto equality;  // wildcard
	    }
	    ThisPage = false;
	equality:  // ugh
	    ; // double ugh
	}
	if(ThisPage) break;
	i++;
    }
    return i;
}



END_NCBI_SCOPE
#endif

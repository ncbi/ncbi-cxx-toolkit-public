#ifndef NCBIREG__HPP
#define NCBIREG__HPP

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
* Author:  Denis Vakatov
*
* File Description:
*   Handle info in the NCBI configuration file(s):
*      read and parse config. file
*      search, edit, etc. in the retrieved configuration info
*      dump info back to config. file
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  1998/12/28 17:56:28  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.5  1998/12/10 22:59:46  vakatov
* CNcbiRegistry:: API is ready(and by-and-large tested)
*
* Revision 1.4  1998/12/10 18:05:35  vakatov
* CNcbiReg::  Just passed a draft test.
*
* Revision 1.3  1998/12/08 23:39:29  vakatov
* Comment starts from ';'(rather than '#')
*
* Revision 1.2  1998/12/08 23:32:46  vakatov
* Redesigned to support "transient" parameters.
* Still "a very draft"(compile through only).
*
* Revision 1.1  1998/12/04 23:40:58  vakatov
* Initial revision
* Very draft;  compiles fine but:  never tested!
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <list>
#include <map>


// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


// [section1]
// name1 = value1
// name2 = value2
// ..............
// [section2]
// ; This is a comment...
// name1 = value1
// .............. 

///  Leading and trailing spaces in "section", "name" and "value" get truncated
///  Comments and empty lines are skipped during the parsing
///  "Section" and "name" must contain only [a-z], [A-Z], [0-9] and underscore

class CNcbiRegistry {
public:
    CNcbiRegistry(void);
    CNcbiRegistry(CNcbiIstream& is, bool transient=false);  // see Read()
    ~CNcbiRegistry(void);

    bool Empty(void) const;  // "false" if the registry contains no entries
    // Parse "is" and merge its content to current entries.
    // If "override==true" than in the case of conflict between the current
    // and the loaded entry, replace the current one by the new one.
    // If "transient==true" then store the newly retrieved parameters as
    // transient.
    // Throw CParseException on error.
    void Read(CNcbiIstream& is, bool transient=false, bool override=true);
    bool Write(CNcbiOstream& os) const;  // dump to "os"(only non-transient)
    void Clear(void);  // reset the whole registry content

    // Return empty string if the config. parameter not found
    // If "search_transient==true" then first search in the list of
    // transient parameters;  otherwise, dont search in transients at all
    const string& Get(const string& section, const string& name,
                      bool search_transient=true) const;
    // Set configuration parameter value(unset if "value" is empty)
    // Return "true" if the new "value" is succesfully set(or unset)
    // If there was already an entry with the same <section,name> key:
    //   if "override==true" then override the old value, return "true";
    //   if "override==false"  then do not override old value, return "false"
    // If "transient==true" then store the entry as transient
    bool Set(const string& section, const string& name, const string& value,
             bool transient=true, bool override=true);

    // These functions first erase the passed list, then fill it out by:
    //    name of sections that comprise the whole registry
    void EnumerateSections(list<string>* sections) const;
    //    name of entries(all) that belong to the specified "section"
    void EnumerateEntries(const string& section, list<string>* entries) const;

private:
    struct TRegEntry {
        string persistent;  // non-transient
        string transient;   // transient(thus do not get dumped by Write())
    };
    typedef map<string, TRegEntry>   TRegSection;
    typedef map<string, TRegSection> TRegistry;
    TRegistry m_Registry;

    // prohibit default initialization and assignment
    CNcbiRegistry(const CNcbiRegistry&) { _TROUBLE; }
    CNcbiRegistry& operator=(const CNcbiRegistry&) { _TROUBLE;  return *this; }
};  // CNcbiRegistry


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif  /* NCBIREG__HPP */


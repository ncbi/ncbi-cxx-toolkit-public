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
* Revision 1.5  1998/12/10 22:59:47  vakatov
* CNcbiRegistry:: API is ready(and by-and-large tested)
*
* Revision 1.4  1998/12/10 18:05:37  vakatov
* CNcbiReg::  Just passed a draft test.
*
* Revision 1.3  1998/12/08 23:39:32  vakatov
* Comment starts from ';'(rather than '#')
*
* Revision 1.2  1998/12/08 23:32:47  vakatov
* Redesigned to support "transient" parameters.
* Still "a very draft"(compile through only).
*
* Revision 1.1  1998/12/04 23:41:00  vakatov
* Initial revision
* Very draft;  compiles fine but:  never tested!
*
* ===========================================================================
*/

#include <ncbireg.hpp>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


CNcbiRegistry::CNcbiRegistry(void) {}

CNcbiRegistry::~CNcbiRegistry(void) {}

CNcbiRegistry::CNcbiRegistry(CNcbiIstream& is, bool transient) {
    Read(is, transient, true);
}

bool CNcbiRegistry::Empty(void) const {
    return m_Registry.empty();
}


void CNcbiRegistry::Read(CNcbiIstream& is, bool transient, bool override)
{
    string    str;     // the line being parsed
    SIZE_TYPE line;    // # of the line being parsed
    string    section; // current section name

    for (line = 0;  NcbiGetline(is >> NcbiWs, str, '\n');  line++) {
        _ASSERT( !str.empty()  &&  !isspace(str[0]) );
        switch ( str[0] ) {
        case ';':  { // comment
            break;
        }

        case '[':  { // section name
            SIZE_TYPE beg = 1;
            SIZE_TYPE end = str.find_first_of(']');
            if (end == NPOS)
                throw CParseException("Invalid registry section(']' is "
                                      "missing: `" + str + "'", line);
            while ( isspace(str[beg]) )
                beg++;
            if (str[beg] == ']') {
                section = NcbiEmptyString;
                break;
            }
            
            for (end = beg;  isalnum(str[end])  ||  str[end] == '_';  end++);
            section = str.substr(beg, end - beg);

            // extra validity check
            while ( isspace(str[end]) )
                end++;
            _ASSERT( end <= str.find_first_of(']', 0) );
            if (str[end] != ']')
                throw CParseException("Invalid registry section name: `" +
                                      str + "'", line);
            break;
        }

        default:  { // regular entry
            if (!isalnum(str[0])  ||  str.find_first_of('=') == NPOS)
                throw CParseException("Invalid registry entry format: '" +
                                      str + "'", line);
            // name
            SIZE_TYPE mid;
            for (mid = 0;  isalnum(str[mid])  ||  str[mid] == '_';  mid++);
            string name = str.substr(0, mid);

            // '=' and surrounding spaces
            while ( isspace(str[mid]) )
                mid++;
            if (str[mid] != '=')
                 throw CParseException("Invalid registry entry name: '" +
                                      str + "'", line);
            SIZE_TYPE len = str.length();
            for (mid++;  mid < len  &&  isspace(str[mid]);  mid++);
            _ASSERT( mid <= len );

            // value
            if (mid == len) {
                if ( override )
                    Set(section, name, NcbiEmptyString, transient, true);
                break;
            }
            SIZE_TYPE end;
            for (end = len-1;  isspace(str[end]);  end--);
            _ASSERT( end >= mid );
            Set(section, name, str.substr(mid, end-mid+1),
                transient, override);
        }
        }
    }

    if ( !is.eof() )
        throw CParseException("Error in reading registry", line);
}


bool CNcbiRegistry::Write(CNcbiOstream& os)
const
{
    TRegistry& xx_Registry = const_cast<TRegistry&>(m_Registry); //BW_O1
    for (TRegistry::const_iterator section = xx_Registry.begin();
         section != xx_Registry.end();  section++) {
        // write section header
        os << '[' << section->first << ']' << NcbiEndl;
        if ( !os )
            return false;
        // write section entries
        const TRegSection& reg_section = section->second;
        TRegSection& xx_RegSection =
            const_cast<TRegSection&>(reg_section); //BW_O1
        _ASSERT( !xx_RegSection.empty() );
        for (TRegSection::const_iterator entry = xx_RegSection.begin();
             entry != xx_RegSection.end();  entry++) {
            if ( entry->second.persistent.empty() )
                continue; // dump only persistent values
            os << entry->first << '=' << entry->second.persistent << NcbiEndl;
            if ( !os )
                return false;
        }
    }
    return true;
}


void CNcbiRegistry::Clear(void)
{
    m_Registry.clear();
}


const string& CNcbiRegistry::Get(const string& section, const string& name,
                                 bool search_transient)
const
{
    if ( name.empty() )  // no empty names allowed
        return NcbiEmptyString;

    // find section
    TRegistry& xx_Registry = const_cast<TRegistry&>(m_Registry); //BW_O1
    TRegistry::const_iterator find_section = xx_Registry.find(section);
    if (find_section == xx_Registry.end())
        return NcbiEmptyString;

    // find entry in the section
    const TRegSection& reg_section = find_section->second;
    TRegSection& xx_RegSection = const_cast<TRegSection&>(reg_section); //BW_O1
    _ASSERT( !xx_RegSection.empty() );
    TRegSection::const_iterator find_entry = xx_RegSection.find(name);
    if (find_entry == xx_RegSection.end())
        return NcbiEmptyString;

    // ok -- found the requested entry
    const TRegEntry& entry = find_entry->second;
    _ASSERT( !entry.persistent.empty()  ||  !entry.transient.empty() );
    return (search_transient  &&  !entry.transient.empty()) ?
        entry.transient : entry.persistent;
}


bool CNcbiRegistry::Set(const string& section, const string& name,
                        const string& value, bool transient, bool override)
{
    // find section
    TRegistry::iterator find_section = m_Registry.find(section);
    if (find_section == m_Registry.end()) {
        if ( value.empty() )  // the "unset" case
            return false;
        TRegEntry& entry = m_Registry[section][name]; // new section, new entry
        if ( transient )
            entry.transient = value;
        else
            entry.persistent = value;
        return true;
    }

    // find entry within the found section
    TRegSection& reg_section = find_section->second;
    _ASSERT( !reg_section.empty() );
    TRegSection::iterator find_entry = reg_section.find(name);
    if (find_entry == reg_section.end()) {
        if ( value.empty() )  // the "unset" case
            return false;
        TRegEntry& entry = reg_section[name]; // new entry
        if ( transient )
            entry.transient = value;
        else
            entry.persistent = value;
        return true;
    }

    // modifying an existing entry
    if ( !override )
        return false;  // cannot override

    TRegEntry& entry = find_entry->second;

    // check if it tries to unset an already unset value
    if (value.empty()  &&
        (( transient  &&  entry.transient.empty())  ||
         (!transient  &&  entry.persistent.empty())))
        return false;

    // modify
    if ( transient )
        entry.transient = value;
    else
        entry.persistent = value;

    // unset(remove) the entry, if empty
    if (entry.persistent.empty()  &&  entry.transient.empty()) {
        reg_section.erase(find_entry);
        // remove section, if empty
        if ( reg_section.empty() ) {
            m_Registry.erase(find_section);
        }
    }

    return true;
}


void CNcbiRegistry::EnumerateSections(list<string>* sections)
const
{
    TRegistry& xx_Registry = const_cast<TRegistry&>(m_Registry); //BW_O1
    sections->clear();
    for (TRegistry::const_iterator section = xx_Registry.begin();
         section != xx_Registry.end();  section++) {
        sections->push_back(section->first);
    }
}


void CNcbiRegistry::EnumerateEntries(const string& section,
                                     list<string>* entries)
const
{
    TRegistry& xx_Registry = const_cast<TRegistry&>(m_Registry); //BW_O1

    entries->clear();

    // find section
    TRegistry::const_iterator find_section = xx_Registry.find(section);
    if (find_section == xx_Registry.end())
        return;

    const TRegSection& reg_section = find_section->second;
    TRegSection& xx_RegSection = const_cast<TRegSection&>(reg_section); //BW_O1
    _ASSERT( !xx_RegSection.empty() );

    // enumerate through the entries in the found section
    for (TRegSection::const_iterator entry = xx_RegSection.begin();
         entry != xx_RegSection.end();  entry++) {
        entries->push_back(entry->first);
    }
}


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE


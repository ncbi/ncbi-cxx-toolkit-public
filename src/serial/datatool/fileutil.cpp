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
*   Some file utilities functions/classes.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  1999/12/21 17:44:18  vasilche
* Fixed compilation on SunPro C++
*
* Revision 1.2  1999/12/21 17:18:34  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.1  1999/12/20 21:00:17  vasilche
* Added generation of sources in different directories.
*
* ===========================================================================
*/

#include "fileutil.hpp"
#include <corelib/ncbistre.hpp>

static const int BUFFER_SIZE = 4096;

SourceFile::SourceFile(const string& name, bool binary)
{
    if ( name == "stdin" || name == "-" ) {
        m_StreamPtr = &NcbiCin;
        m_Open = false;
    }
    else {
        m_StreamPtr = new CNcbiIfstream(name.c_str(),
                                        binary?
                                            IOS_BASE::in | IOS_BASE::binary:
                                            IOS_BASE::in);
        if ( !*m_StreamPtr ) {
            delete m_StreamPtr;
            m_StreamPtr = 0;
            ERR_POST(Fatal << "cannot open file " << name);
        }
        m_Open = true;
    }
}

SourceFile::~SourceFile(void)
{
    if ( m_Open ) {
        delete m_StreamPtr;
    }
}

DestinationFile::DestinationFile(const string& name, bool binary)
{
    if ( name == "stdout" || name == "-" ) {
        m_StreamPtr = &NcbiCout;
        m_Open = false;
    }
    else {
        m_StreamPtr = new CNcbiOfstream(name.c_str(),
                                        binary?
                                            IOS_BASE::out | IOS_BASE::binary:
                                            IOS_BASE::out);
        if ( !*m_StreamPtr ) {
            delete m_StreamPtr;
            m_StreamPtr = 0;
            ERR_POST(Fatal << "cannot open file " << name);
        }
        m_Open = true;
    }
}

DestinationFile::~DestinationFile(void)
{
    if ( m_Open ) {
        delete m_StreamPtr;
    }
}

string Path(const string& dir, const string& file)
{
    if ( dir.size() == 0 )
        return file;
    switch ( dir[dir.size() - 1] ) {
    case '/':
    case '\\':
    case ':':
        return dir + file;
    default:
        return dir + '/' + file;
    }
}

string BaseName(const string& path)
{
    SIZE_TYPE dirEnd = path.find_last_of(":/\\");
    string name;
    if ( dirEnd != NPOS )
        name = path.substr(dirEnd + 1);
    else
        name = path;
    SIZE_TYPE extStart = name.rfind('.');
    if ( extStart != NPOS )
        name = name.substr(0, extStart);
    return name;
}

string Identifier(const string& typeName, bool capitalize)
{
    string s;
    s.reserve(typeName.size());
    string::const_iterator i = typeName.begin();
    if ( i != typeName.end() ) {
        s += capitalize? toupper(*i): *i;
        while ( ++i != typeName.end() ) {
            char c = *i;
            if ( c == '-' || c == '.' )
                c = '_';
            s += c;
        }
    }
    return s;
}

CDelayedOfstream::CDelayedOfstream(const char* fileName)
{
    open(fileName);
}

CDelayedOfstream::~CDelayedOfstream(void)
{
    close();
}

void CDelayedOfstream::open(const char* fileName)
{
    close();
    clear();
    seekp(0, IOS_BASE::beg);
    m_FileName = fileName;
    m_Istream.reset(new CNcbiIfstream(fileName));
    if ( !*m_Istream ) {
        _TRACE("cannot open " << m_FileName);
        m_Istream.reset(0);
        m_Ostream.reset(new CNcbiOfstream(fileName));
        if ( !*m_Ostream ) {
            _TRACE("cannot create " << m_FileName);
            setstate(m_Ostream->rdstate());
            m_Ostream.reset(0);
            m_FileName.erase();
        }
    }
}

void CDelayedOfstream::close(void)
{
    if ( !is_open() )
        return;
    if ( !equals() ) {
        if ( !rewrite() )
            setstate(m_Ostream->rdstate());
        m_Ostream.reset(0);
    }
    m_Istream.reset(0);
    m_FileName.erase();
}

bool CDelayedOfstream::equals(void)
{
    if ( !m_Istream.get() )
        return false;
    streamsize count = pcount();
    const char* ptr = str();
    freeze(false);
    while ( count > 0 ) {
        char buffer[BUFFER_SIZE];
        streamsize c = count;
        if ( c > BUFFER_SIZE )
            c = BUFFER_SIZE;
        if ( !m_Istream->read(buffer, c) ) {
            _TRACE("read fault " << m_FileName <<
                   " need: " << c << " was: " << m_Istream->gcount());
            return false;
        }
        if ( memcmp(buffer, ptr, c) != 0 ) {
            _TRACE("file differs " << m_FileName);
            return false;
        }
        ptr += c;
        count -= c;
    }
    if ( m_Istream->get() != -1 ) {
        _TRACE("file too long " << m_FileName);
        return false;
    }
    return true;
}

bool CDelayedOfstream::rewrite(void)
{
    if ( !m_Ostream.get() ) {
        m_Ostream.reset(new CNcbiOfstream(m_FileName.c_str()));
        if ( !*m_Ostream ) {
            _TRACE("rewrite fault " << m_FileName);
            return false;
        }
    }
    streamsize count = pcount();
    const char* ptr = str();
    freeze(false);
    if ( !m_Ostream->write(ptr, count) ) {
        _TRACE("write fault " << m_FileName);
        return false;
    }
    m_Ostream->close();
    if ( !*m_Ostream ) {
        _TRACE("close fault " << m_FileName);
        return false;
    }
    return true;
}

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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbifile.hpp>
#include "fileutil.hpp"
#include "srcutil.hpp"
#include <serial/error_codes.hpp>
#include <set>


#define NCBI_USE_ERRCODE_X   Serial_Util

BEGIN_NCBI_SCOPE

static const int BUFFER_SIZE = 4096;

SourceFile::SourceFile(const string& name, bool binary)
    : m_StreamPtr(0), m_Open(false)
{
    if ( name == "stdin" || name == "-" ) {
        m_StreamPtr = &NcbiCin;
    }
    else {
        if ( !x_Open(name, binary) )
            ERR_POST_X(1, Fatal << "cannot open file " << name);
    }
}

SourceFile::SourceFile(const string& name, const list<string>& dirs,
                       bool binary)
{
    if ( name == "stdin" || name == "-" ) {
        m_StreamPtr = &NcbiCin;
    } else if ( !x_Open(name, binary) ) {
        ITERATE(list<string>, dir, dirs) {
            if ( x_Open(Path(*dir, name), binary) ) {
                return;
            }
        }
        ERR_POST_X(2, Fatal << "cannot open file " << name);
    }
}

SourceFile::~SourceFile(void)
{
    if ( m_Open ) {
        delete m_StreamPtr;
        m_StreamPtr = 0;
        m_Open = false;
    }
}


SourceFile::EType SourceFile::GetType(void) const
{
    CDirEntry entry(m_Name);
    string ext(entry.GetExt());
    if (NStr::CompareNocase(ext,".asn") == 0) {
        return eASN;
    } else if (NStr::CompareNocase(ext,".dtd") == 0) {
        return eDTD;
    } else if (NStr::CompareNocase(ext,".xsd") == 0) {
        return eXSD;
    } else if (NStr::CompareNocase(ext,".wsdl") == 0) {
        return eWSDL;
    }
    return eUnknown;
}


bool SourceFile::x_Open(const string& name, bool binary)
{
    m_Name = name;
    m_StreamPtr = new CNcbiIfstream(name.c_str(),
                                    binary?
                                        IOS_BASE::in | IOS_BASE::binary:
                                        IOS_BASE::in);
    m_Open = m_StreamPtr->good();
    if ( !m_Open ) {
        delete m_StreamPtr;
        m_StreamPtr = 0;
    }
    return m_Open;
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
            ERR_POST_X(3, Fatal << "cannot open file " << name);
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

// default parameters
#undef DIR_SEPARATOR_CHAR
#undef DIR_SEPARATOR_CHAR2
#undef DISK_SEPARATOR_CHAR
#undef ALL_SEPARATOR_CHARS
#define PARENT_DIR ".."

#ifdef NCBI_OS_MSWIN
#  define DIR_SEPARATOR_CHAR '\\'
#  define DIR_SEPARATOR_CHAR2 '/'
#  define DISK_SEPARATOR_CHAR ':'
#  define ALL_SEPARATOR_CHARS ":/\\"
#endif

#ifndef DIR_SEPARATOR_CHAR
#  define DIR_SEPARATOR_CHAR '/'
#endif

#ifndef ALL_SEPARATOR_CHARS
#  define ALL_SEPARATOR_CHARS DIR_SEPARATOR_CHAR
#endif

#ifdef DISK_SEPARATOR_CHAR
inline
bool IsDiskSeparator(char c)
{
    return c == DISK_SEPARATOR_CHAR;
}
#else
inline
bool IsDiskSeparator(char /* c */)
{
    return false;
}
#endif

inline
bool IsDirSeparator(char c)
{
#ifdef DISK_SEPARATOR_CHAR
    if ( c == DISK_SEPARATOR_CHAR )
        return true;
#endif
#ifdef DIR_SEPARATOR_CHAR2
    if ( c == DIR_SEPARATOR_CHAR2 )
        return true;
#endif
    return c == DIR_SEPARATOR_CHAR;
}

bool IsLocalPath(const string& path)
{
    // determine if path is local to current directory
    // exclude pathes like:
    // "../xxx" everywhere
    // "xxx/../yyy" everywhere
    // "/xxx/yyy"  on unix
    // "d:xxx" on windows
    // "HD:folder" on Mac
    if ( path.empty() )
        return false;

    if ( IsDirSeparator(path[0]) )
        return false;

    SIZE_TYPE pos;
#ifdef PARENT_DIR
    SIZE_TYPE parentDirLength = strlen(PARENT_DIR);
    pos = 0;
    while ( (pos = path.find(PARENT_DIR, pos)) != NPOS ) {
        if ( pos == 0 || IsDirSeparator(path[pos - 1]) )
            return false;
        SIZE_TYPE end = pos + parentDirLength;
        if ( end == path.size() || IsDirSeparator(path[end]) )
            return false;
        pos = end + 1;
    }
#endif
#ifdef DISK_SEPARATOR_CHAR
    if ( path.find(DISK_SEPARATOR_CHAR) != NPOS )
        return false;
#endif
    return true;
}

string MakeAbsolutePath(const string& path)
{
    if (!path.empty() && !CDirEntry::IsAbsolutePath(path)) {
        string res = Path(CDir::GetCwd(),path);
        res = CDirEntry::NormalizePath(res);
        return res;
    }
    return path;
}

string Path(const string& dir, const string& file)
{
    if ( dir.empty() )
        return file;
    char lastChar = dir[dir.size() - 1];
    if ( file.empty() )
        _TRACE("Path(\"" << dir << "\", \"" << file << "\")");
    // Avoid duplicate dir separators
    if ( IsDirSeparator(lastChar) ) {
        if ( IsDirSeparator(file[0]) )
            return dir.substr(0, dir.size()-1) + file;
    }
    else {
        if ( !IsDirSeparator(file[0]) )
            return dir + DIR_SEPARATOR_CHAR + file;
    }
    return dir + file;
}

string BaseName(const string& path)
{
    SIZE_TYPE dirEnd = path.find_last_of(ALL_SEPARATOR_CHARS);
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

string DirName(const string& path)
{
    SIZE_TYPE dirEnd = path.find_last_of(ALL_SEPARATOR_CHARS);
    if ( dirEnd != NPOS ) {
        if ( dirEnd == 0 /* "/" root directory */ ||
             IsDiskSeparator(path[dirEnd]) /* disk separator */ ) 
            ++dirEnd; // include separator

        return path.substr(0, dirEnd);
    }
    else {
        return NcbiEmptyString;
    }
}

string GetStdPath(const string& path)
{
    string stdpath = path;
    // Replace each native separator character with the 'standard' one.
    SIZE_TYPE ibeg = NStr::StartsWith(path, "http://", NStr::eNocase) ? 7 : 0;
    for (SIZE_TYPE i=ibeg ; i < stdpath.size(); i++) {
#ifdef NCBI_OS_MSWIN
        if ( i==1 && IsDiskSeparator(stdpath[i]) ) {
            continue;
        }
#endif
        if ( IsDirSeparator(stdpath[i]) )
            stdpath[i] = '/';
    }
    string tmp = NStr::Replace(stdpath,"//","/",ibeg);
    stdpath = NStr::Replace(tmp,"/./","/",ibeg);
    return stdpath;
}


class SSubString
{
public:
    SSubString(const string& value, size_t order)
        : value(value), order(order)
        {
        }

    struct ByOrder {
        bool operator()(const SSubString& s1, const SSubString& s2) const
            {
                return s1.order < s2.order;
            }
    };
    struct ByLength {
        bool operator()(const SSubString& s1, const SSubString& s2) const
            {
                if ( s1.value.size() > s2.value.size() )
                    return true;
                if ( s1.value.size() < s2.value.size() )
                    return false;
                return s1.order < s2.order;
            }
    };
    string value;
    size_t order;
};

string MakeFileName(const string& fname, size_t addLength)
{
    string name = Identifier(fname);
    size_t fullLength = name.size() + addLength;
    if ( fullLength <= MAX_FILE_NAME_LENGTH )
        return name;
    size_t remove = fullLength - MAX_FILE_NAME_LENGTH;
    // we'll have to truncate very long filename

    _TRACE("MakeFileName(\""<<fname<<"\", "<<addLength<<") remove="<<remove);
    // 1st step: parse name dividing by '_' sorting elements by their size
    SIZE_TYPE removable = 0; // removable part of string
    typedef set<SSubString, SSubString::ByLength> TByLength;
    TByLength byLength;
    {
        SIZE_TYPE curr = 0; // current element position in string
        size_t order = 0; // current element order
        for (;;) {
            SIZE_TYPE und = name.find('_', curr);
            if ( und == NPOS ) {
                // end of string
                break;
            }
            _TRACE("MakeFileName: \""<<name.substr(curr, und - curr)<<"\"");
            removable += (und - curr);
            byLength.insert(SSubString(name.substr(curr, und - curr), order));
            curr = und + 1;
            ++order;
        }
        _TRACE("MakeFileName: \""<<name.substr(curr)<<"\"");
        removable += name.size() - curr;
        byLength.insert(SSubString(name.substr(curr), order));
    }
    _TRACE("MakeFileName: removable="<<removable);

    // if removable part of string too small...
    if ( removable - remove < size_t(MAX_FILE_NAME_LENGTH - addLength) / 2 ) {
        // we'll do plain truncate
        _TRACE("MakeFileName: return \""<<name.substr(0, MAX_FILE_NAME_LENGTH - addLength)<<"\"");
        return name.substr(0, MAX_FILE_NAME_LENGTH - addLength);
    }
    
    // 2nd step: shorten elementes beginning with longest
    while ( remove > 0 ) {
        // extract most long element
        SSubString s = *byLength.begin();
        _TRACE("MakeFileName: shorten \""<<s.value<<"\"");
        byLength.erase(byLength.begin());
        // shorten it by one symbol
        s.value = s.value.substr(0, s.value.size() - 1);
        // insert it back
        byLength.insert(s);
        // decrement progress counter
        remove--;
    }
    // 3rd step: reorder elements by their relative order in original string
    typedef set<SSubString, SSubString::ByOrder> TByOrder;
    TByOrder byOrder;
    {
        ITERATE ( TByLength, i, byLength ) {
            byOrder.insert(*i);
        }
    }
    // 4th step: join elements in resulting string
    name.erase();
    {
        ITERATE ( TByOrder, i, byOrder ) {
            if ( !name.empty() )
                name += '_';
            name += i->value;
        }
    }
    _TRACE("MakeFileName: return \""<<name<<"\"");
    return name;
}

CDelayedOfstream::CDelayedOfstream(const string& fileName)
{
    open(fileName);
}

CDelayedOfstream::~CDelayedOfstream(void)
{
    close();
}

void CDelayedOfstream::open(const string& fileName)
{
    close();
    clear();
    seekp(0, IOS_BASE::beg);
    clear(); // eof set?
    m_FileName = MakeAbsolutePath(fileName);
    m_Istream.reset(new CNcbiIfstream(m_FileName.c_str()));
    if ( !*m_Istream ) {
        _TRACE("cannot open " << m_FileName);
        m_Istream.reset(0);
        m_Ostream.reset(new CNcbiOfstream(m_FileName.c_str()));
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
    size_t count = (size_t)pcount();
    const char* ptr = str();
    freeze(false);
    while ( count > 0 ) {
        char buffer[BUFFER_SIZE];
        size_t c = count;
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

void CDelayedOfstream::Discard(void)
{
    if ( is_open() ) {
        m_Ostream.reset(0);
        m_Istream.reset(0);
        CFile(m_FileName).Remove();
        m_FileName.clear();
    }
}

bool Empty(const CNcbiOstrstream& src)
{
    return const_cast<CNcbiOstrstream&>(src).pcount() == 0;
}

CNcbiOstream& Write(CNcbiOstream& out, const CNcbiOstrstream& src)
{
    CNcbiOstrstream& source = const_cast<CNcbiOstrstream&>(src);
    size_t size = (size_t)source.pcount();
    if ( size != 0 ) {
        out.write(source.str(), size);
        source.freeze(false);
    }
    return out;
}

CNcbiOstream& WriteTabbed(CNcbiOstream& out, const CNcbiOstrstream& code,
                          const char* tab)
{
    CNcbiOstrstream& source = const_cast<CNcbiOstrstream&>(code);
    size_t size = (size_t)source.pcount();
    if ( size != 0 ) {
        if ( !tab )
            tab = "    ";
        const char* ptr = source.str();
        source.freeze(false);
        while ( size > 0 ) {
            out << tab;
            const char* endl =
                reinterpret_cast<const char*>(memchr(ptr, '\n', size));
            if ( !endl ) { // no more '\n'
                out.write(ptr, size) << '\n';
                break;
            }
            ++endl; // skip '\n'
            size_t lineSize = endl - ptr;
            out.write(ptr, lineSize);
            ptr = endl;
            size -= lineSize;
        }
    }
    return out;
}

END_NCBI_SCOPE

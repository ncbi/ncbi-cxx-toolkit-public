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
* Revision 1.15  2000/11/20 17:26:32  vasilche
* Fixed warnings on 64 bit platforms.
* Updated names of config variables.
*
* Revision 1.14  2000/11/15 20:34:54  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.13  2000/11/14 21:41:24  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.12  2000/08/25 15:59:22  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.11  2000/04/10 20:01:31  vakatov
* Typo fixed
*
* Revision 1.10  2000/04/10 19:34:02  vakatov
* Get rid of a minor compiler warning
*
* Revision 1.9  2000/04/07 19:26:27  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.8  2000/03/29 15:52:27  vasilche
* Generated files names limited to 31 symbols due to limitations of Mac.
* Removed unions with only one member.
*
* Revision 1.7  2000/02/01 21:47:59  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.6  2000/01/05 19:34:53  vasilche
* Fixed warning on MS VC
*
* Revision 1.5  1999/12/30 21:33:39  vasilche
* Changed arguments - more structured.
* Added intelligence in detection of source directories.
*
* Revision 1.4  1999/12/28 18:55:57  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
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

#include <serial/datatool/fileutil.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiutil.hpp>
#include <set>

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
            ERR_POST(Fatal << "cannot open file " << name);
    }
}

SourceFile::SourceFile(const string& name, const string& dir, bool binary)
{
    if ( name == "stdin" || name == "-" ) {
        m_StreamPtr = &NcbiCin;
    }
    else {
        if ( !x_Open(name, binary) && !x_Open(Path(dir, name), binary) )
            ERR_POST(Fatal << "cannot open file " << name);
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

bool SourceFile::x_Open(const string& name, bool binary)
{
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
    if ( dir.empty() )
        return file;
    if ( file.empty() )
        _TRACE("Path(\"" << dir << "\", \"" << file << "\")");
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

string DirName(const string& path)
{
    SIZE_TYPE dirEnd = path.find_last_of(":/\\");
    if ( dirEnd != NPOS ) {
        if ( dirEnd == 0 ) // "/" root directory
            return path.substr(0, 1);
        else
            return path.substr(0, dirEnd);
    }
    else {
        return NcbiEmptyString;
    }
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
    SIZE_TYPE removeable = 0; // removeable part of string
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
            removeable += (und - curr);
            byLength.insert(SSubString(name.substr(curr, und - curr), order));
            curr = und + 1;
            ++order;
        }
        _TRACE("MakeFileName: \""<<name.substr(curr)<<"\"");
        removeable += name.size() - curr;
        byLength.insert(SSubString(name.substr(curr), order));
    }
    _TRACE("MakeFileName: removeable="<<removeable);

    // if removeable part of string too small...
    if ( removeable - remove < size_t(MAX_FILE_NAME_LENGTH - addLength) / 2 ) {
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
        iterate ( TByLength, i, byLength ) {
            byOrder.insert(*i);
        }
    }
    // 4th step: join elements in resulting string
    name.erase();
    {
        iterate ( TByOrder, i, byOrder ) {
            if ( !name.empty() )
                name += '_';
            name += i->value;
        }
    }
    _TRACE("MakeFileName: return \""<<name<<"\"");
    return name;
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

bool Empty(const CNcbiOstrstream& src)
{
    return const_cast<CNcbiOstrstream&>(src).pcount() == 0;
}

CNcbiOstream& Write(CNcbiOstream& out, const CNcbiOstrstream& src)
{
    CNcbiOstrstream& source = const_cast<CNcbiOstrstream&>(src);
    size_t size = source.pcount();
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
    size_t size = source.pcount();
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

CNcbiOstream& WriteTabbed(CNcbiOstream& out, const string& code,
                          const char* tab)
{
    size_t size = code.size();
    if ( size != 0 ) {
        if ( !tab )
            tab = "    ";
        const char* ptr = code.data();
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

string Tabbed(const string& code, const char* tab)
{
    string out;
    size_t size = code.size();
    if ( size != 0 ) {
        if ( !tab )
            tab = "    ";
        const char* ptr = code.data();
        while ( size > 0 ) {
            out += tab;
            const char* endl =
                reinterpret_cast<const char*>(memchr(ptr, '\n', size));
            if ( !endl ) { // no more '\n'
                out.append(ptr, ptr + size);
                out += '\n';
                break;
            }
            ++endl; // skip '\n'
            size_t lineSize = endl - ptr;
            out.append(ptr, ptr + lineSize);
            ptr = endl;
            size -= lineSize;
        }
    }
    return out;
}

CNcbiOstream& PrintDTDComments(CNcbiOstream& out,
                               const list<string>& comments,
                               int flags)
{
    if ( comments.empty() ) // no comments
        return out;

    if ( (flags & eCommentsDoNotWriteBlankLine) == 0 ) {
        // prepend comments by empty line to separate from previous comments
        out << '\n';
    }

    // comments start
    out <<
        "<!--";

    if ( (flags & eCommentsAlwaysMultiline) == 0 && comments.size() == 1 ) {
        // one line comment
        out << comments.front() << ' ';
    }
    else {
        // multiline comments
        out << '\n';
        iterate ( list<string>, i, comments ) {
            out << *i << '\n';
        }
    }

    // comments end
    out << "-->";
    
    if ( (flags & eCommentsNoEOL) == 0 )
        out << '\n';

    return out;
}

CNcbiOstream& PrintASNComments(CNcbiOstream& out,
                               const list<string>& comments,
                               int indent,
                               int flags)
{
    if ( comments.empty() ) // no comments
        return out;

    bool newLine = (flags & eCommentsDoNotWriteBlankLine) == 0;
    // prepend comments by empty line to separate from previous comments

    iterate ( list<string>, i, comments ) {
        if ( newLine )
            PrintASNNewLine(out, indent);
        out << "--" << *i;
        newLine = true;
    }

    if ( (flags & eCommentsNoEOL) == 0 )
        PrintASNNewLine(out, indent);

    return out;
}

CNcbiOstream& PrintASNNewLine(CNcbiOstream& out, int indent)
{
    out <<
        '\n';
    for ( int i = 0; i < indent; ++i )
        out << "  ";
    return out;
}

END_NCBI_SCOPE

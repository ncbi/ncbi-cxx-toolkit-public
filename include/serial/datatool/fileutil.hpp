#ifndef FILEUTIL_HPP
#define FILEUTIL_HPP

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
*   Several file utility functions/classes.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2000/04/13 14:50:22  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.3  2000/04/07 19:26:09  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.2  2000/03/29 15:51:42  vasilche
* Generated files names limited to 31 symbols due to limitations of Mac.
*
* Revision 1.1  2000/02/01 21:46:18  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
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
* Revision 1.3  1999/12/21 17:44:19  vasilche
* Fixed compilation on SunPro C++
*
* Revision 1.2  1999/12/21 17:18:34  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.1  1999/12/20 21:00:18  vasilche
* Added generation of sources in different directories.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <memory>

BEGIN_NCBI_SCOPE

static const size_t MAX_FILE_NAME_LENGTH = 31;

class SourceFile
{
public:
    SourceFile(const string& name, bool binary = false);
    SourceFile(const string& name, const string& dir, bool binary = false);
    ~SourceFile(void);

    operator CNcbiIstream&(void) const
        {
            return *m_StreamPtr;
        }

private:
    CNcbiIstream* m_StreamPtr;
    bool m_Open;

    bool x_Open(const string& name, bool binary);
};

class DestinationFile
{
public:
    DestinationFile(const string& name, bool binary = false);
    ~DestinationFile(void);

    operator CNcbiOstream&(void) const
        {
            return *m_StreamPtr;
        }

private:
    CNcbiOstream* m_StreamPtr;
    bool m_Open;
};

struct FileInfo {
    FileInfo(void)
        : type(ESerialOpenFlags(0))
        { }
    FileInfo(const string& n, ESerialOpenFlags t)
        : name(n), type(t)
        { }

    operator bool(void) const
        { return !name.empty(); }
    operator const string&(void) const
        { return name; }
    string name;
    ESerialOpenFlags type;
};

class CDelayedOfstream : public CNcbiOstrstream
{
public:
    CDelayedOfstream(const char* fileName);
    virtual ~CDelayedOfstream(void);

    bool is_open(void) const
        {
            return !m_FileName.empty();
        }
    void open(const char* fileName);
    void close(void);

protected:
    bool equals(void);
    bool rewrite(void);

private:
    string m_FileName;
    auto_ptr<CNcbiIfstream> m_Istream;
    auto_ptr<CNcbiOfstream> m_Ostream;
};

// return combined dir and name, inserting if needed '/'
string Path(const string& dir, const string& name);

// file name will be valid after adding at most addLength symbols
string MakeFileName(const string& s, size_t addLength = 0);

// return base name of file i.e. without dir and extension
string BaseName(const string& path);

// return dir name of file
string DirName(const string& path);

// return valid C name
string Identifier(const string& typeName, bool capitalize = true);

bool Empty(const CNcbiOstrstream& code);
CNcbiOstream& Write(CNcbiOstream& out, const CNcbiOstrstream& code);
CNcbiOstream& WriteTabbed(CNcbiOstream& out, const CNcbiOstrstream& code,
                          const char* tab = 0);
CNcbiOstream& WriteTabbed(CNcbiOstream& out, const string& code,
                          const char* tab = 0);
string Tabbed(const string& code, const char* tab = 0);

END_NCBI_SCOPE

#endif

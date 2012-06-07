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
    SourceFile(const string& name, const list<string>& dirs,
               bool binary = false);
    ~SourceFile(void);

    operator CNcbiIstream&(void) const
        {
            return *m_StreamPtr;
        }

    enum EType {
        eUnknown,  // Unknown type
        eASN,      // ASN file
        eDTD,      // DTD file
        eXSD,      // XSD file
        eWSDL      // WSDL file
    };
    EType GetType(void) const;
    string GetFileName(void) const
        {
            return m_Name;
        }

private:
    string m_Name;
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
        : type(ESerialDataFormat(eSerial_None))
        { }
    FileInfo(const string& n, ESerialDataFormat t)
        : name(n), type(t)
        { }

    operator const string&(void) const
        { return name; }

    DECLARE_OPERATOR_BOOL(!name.empty());

    bool operator==(const FileInfo& info) const
    {
        return name == info.name;
    }
    bool operator!=(const FileInfo& info) const
    {
        return name != info.name;
    }

    string name;
    ESerialDataFormat type;
};

class CDelayedOfstream : public CNcbiOstrstream
{
public:
    CDelayedOfstream(const string& fileName);
    virtual ~CDelayedOfstream(void);

    bool is_open(void) const
        {
            return !m_FileName.empty();
        }
    void open(const string& fileName);
    void close(void);
    
    void Discard(void);

protected:
    bool equals(void);
    bool rewrite(void);

private:
    string m_FileName;
    auto_ptr<CNcbiIfstream> m_Istream;
    auto_ptr<CNcbiOfstream> m_Ostream;
};

string MakeAbsolutePath(const string& path);

// return combined dir and name, inserting if needed '/'
string Path(const string& dir, const string& name);

// file name will be valid after adding at most addLength symbols
string MakeFileName(const string& s, size_t addLength = 0);

// return base name of file i.e. without dir and extension
string BaseName(const string& path);

// return dir name of file
string DirName(const string& path);

bool IsLocalPath(const string& path);

// Convert system-dependent path to the standard path
// ('\' ==> '/', ':' ==> '/', etc.)
string GetStdPath(const string& path);

END_NCBI_SCOPE

#endif

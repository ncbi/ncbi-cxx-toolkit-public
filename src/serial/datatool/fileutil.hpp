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
* Revision 1.1  1999/12/20 21:00:18  vasilche
* Added generation of sources in different directories.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

USING_NCBI_SCOPE;

class SourceFile
{
public:
    SourceFile(const string& name, bool binary = false);
    ~SourceFile(void);

    operator CNcbiIstream&(void) const
        {
            return *m_StreamPtr;
        }

private:
    CNcbiIstream* m_StreamPtr;
    bool m_Open;
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

enum EFileType {
    eFileTypeNone,
    eASNText,
    eASNBinary,
    eXMLText
};

struct FileInfo {
    FileInfo(void)
        : type(eFileTypeNone)
        { }
    FileInfo(const string& n, EFileType t)
        : name(n), type(t)
        { }

    operator bool(void) const
        { return !name.empty(); }
    operator const string&(void) const
        { return name; }
    string name;
    EFileType type;
};

// return combined dir and name, inserting if needed '/'
string Path(const string& dir, const string& name);

// return base name of file e.g. without dir and extension
string BaseName(const string& path);

#endif

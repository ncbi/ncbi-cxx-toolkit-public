#ifndef UTIL___LOGROTATE__HPP
#define UTIL___LOGROTATE__HPP

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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   File streams supporting log rotation
*
*/

#include <corelib/ncbistd.hpp>


/** @addtogroup RotatingLog
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CRotatingLogStreamBuf;

class NCBI_XUTIL_EXPORT CRotatingLogStream : public CNcbiOstream {
public:
    // limit is approximate; the actual length may exceed it by a buffer's
    // worth of characters.
    CRotatingLogStream(const string& filename, CNcbiStreamoff limit,
                       openmode mode = app | ate | out);
    virtual ~CRotatingLogStream(void)
        { delete rdbuf(); }

    // Users may call this explicitly to rotate on some basis other
    // than size (such as time since last rotation or a signal from
    // some external process).
    CNcbiStreamoff Rotate(void); // returns number of bytes in old log

protected:
    // Specifies how to rename old logs.  Returning an empty string
    // indicates that any appropriate renaming has already occurred,
    // for instance because this method took care of it directly or
    // because some other program rotated the logs for us and asked us
    // to switch to a new file (traditionally, by sending SIGHUP).
    // Changing "name" is also legitimate, and indicates that the NEW
    // logs should have a different name.

    // The default behavior is to append a timestamp to name.
    virtual string x_BackupName(string& name);

    friend class CRotatingLogStreamBuf;
};


END_NCBI_SCOPE


/* @} */


/*
* ===========================================================================
*
* $Log$
* Revision 1.8  2006/04/21 18:57:36  ucko
* Move CRotatingLogStreamBuf's full declaration from logrotate.hpp to
* logrotate.cpp, as there's no real need to expose it.
*
* Revision 1.7  2004/09/01 15:45:25  ucko
* Use more appropriate typedefs (CNcbiStreamfoo, not CT_FOO_TYPE).
* Rotate now indicates how much data it rotated.
*
* Revision 1.6  2004/02/19 22:57:53  ucko
* Accommodate stricter implementations of CT_POS_TYPE.
*
* Revision 1.5  2003/04/17 17:50:21  siyan
* Added doxygen support
*
* Revision 1.4  2003/02/12 16:41:04  ucko
* Always supply CRotatingLogStreamBuf::sync, and avoid double-counting
* via run-time rather than compile-time logic.
*
* Revision 1.3  2002/12/19 14:51:00  dicuccio
* Added export specifier for Win32 DLL builds.
*
* Revision 1.2  2002/11/25 19:19:13  ucko
* Adjust CRotatingLogStream's constructor to compile under GCC 3, and
* generally be slightly more efficient.
* Remember to clean up the buffer in its destructor.
*
* Revision 1.1  2002/11/25 17:20:59  ucko
* Add support for automatic log rotation (CRotatingLogStream)
*
*
* ===========================================================================
*/

#endif  /* UTIL___LOGROTATE__HPP */

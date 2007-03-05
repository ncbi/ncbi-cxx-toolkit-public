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
*/

/// @file logrotate.hpp
/// This module supplies a file stream variant that supports
/// customizable log rotation.

#include <corelib/ncbistd.hpp>


/** @addtogroup RotatingLog
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CRotatingLogStreamBuf;

/// A file stream variant that automatically rotates its output when
/// it grows beyond a specified size.
///
/// Derived classes have full control over old and new filenames upon
/// rotation.
class NCBI_XUTIL_EXPORT CRotatingLogStream : public CNcbiOstream {
public:
    /// Constructor.
    ///
    /// Construct a rotating log stream with the given parameters:
    /// @param filename
    ///  The active log file's name (at least initially -- see
    ///  x_BackupName).
    /// @param limit
    ///  The approximate size (in bytes) to which the log can grow
    ///  before it is rotated.  (The actual length may exceed the
    ///  limit by up to a bufferful.)
    /// @param mode
    ///  How to open each log file.  The default should generally be
    ///  reasonable.
    /// @sa x_BackupName
    CRotatingLogStream(const string& filename, CNcbiStreamoff limit,
                       openmode mode = app | ate | out);

    /// Destructor.
    virtual ~CRotatingLogStream(void);

    /// Actually rotate the log.
    ///
    /// This happens automatically whenever its size exceeds the
    /// limit, but users may also call it explicitly under other
    /// circumstances -- for instance, to implement time-based
    /// rotation or in response to signals from external processes.
    /// @return Size (in bytes) of old log.
    CNcbiStreamoff Rotate(void); // returns

protected:
    /// Overridable implementation of naming policy.
    ///
    /// By default, the existing log file will be renamed to include a
    /// timestamp, and new logs will go to a new file with the old
    /// (un-timestamped) name.
    /// @param name
    ///  The name of the file to which logs have gone so far, and of
    ///  the new file to which future logs will go.  (Implementations
    ///  may modify this argument to control the latter.)
    /// @return
    ///  The filename to which the existing log should be renamed, or
    ///  the empty string if any appropriate renaming has already
    ///  occurred, typically either because the implementation took
    ///  care of it directly or because some other program did so and then
    ///  sent this process an appropriate signal (traditionally SIGHUP).
    virtual string x_BackupName(string& name);

    friend class CRotatingLogStreamBuf;
};


END_NCBI_SCOPE


/* @} */

#endif  /* UTIL___LOGROTATE__HPP */

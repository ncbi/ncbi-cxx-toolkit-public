#ifndef UTILS__FILEOBSOLETE__HPP__
#define UTILS__FILEOBSOLETE__HPP__

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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  Remove old files from the specified directory
 *
 */

/// @file file_obsolete.hpp
/// Delete old files from the specified directory

#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE

//////////////////////////////////////////////////////////////////
///
/// Find and remove old files
///

class NCBI_XUTIL_EXPORT CFileObsolete
{
public:
    CFileObsolete(const string& path);
    virtual ~CFileObsolete();

    /// Defines if file is deleted based on its creation time 
    /// (attribute modification) or last access time.
    enum ETimeMode
    {
        eLastModification,  ///< Last modification
        eLastAccess         ///< Last access time
    };

    ///
    /// Scan the target directory, find old files matching the specified mask,
    /// files deleted if permission is granted by OnRemove method.
    ///
    /// @param mask
    ///    file mask to search for
    ///
    /// @param age
    ///    file age in seconds. File older than age are deleted.
    ///    Note that age is not the moment in time, but a time interval
    ///    from the moment of function call (back on time axis).
    ///
    /// @param tmode
    ///    Chice between last modification time and last file access time.
    ///
    /// @sa OnRemove
    ///
    void Remove(const string&  mask, 
                unsigned int   age,
                ETimeMode      tmode = eLastAccess);

    /// Reactor function called when file is about to be deleted.
    /// If function returns TRUE file is deleted, otherwise remains.
    ///
    /// @param filename
    ///    Name of the file candidate for deletion
    ///
    /// @return TRUE if file is safe to delete (default).
    ///
    virtual bool OnRemove(const string& filename);

protected:
    string     m_Path; ///< Base path
};

END_NCBI_SCOPE

#endif

#ifndef ASN_CACHE_UNIX_LOCKFILE_HPP__
#define ASN_CACHE_UNIX_LOCKFILE_HPP__
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
 * Authors:  Cheinan Marks
 *
 * File Description:
 * Implementation of the UNIX lockfile command so C++ code can interoperate
 * with scripts using these locks.
 */
 
#include <string>

#if defined(NCBI_OS_UNIX)
#include <unistd.h>
#endif

#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE

class CUnixLockFileException : public CException
{
public:
    enum EErrCode {
        eRetryLimitExceeded,
        eWrongPlatform
    };  

    virtual const char* GetErrCodeString() const
    {   
        switch (GetErrCode()) {
            case eRetryLimitExceeded: return "Tried to acquire the lock too many times.";
            case eWrongPlatform: return "Platform does not allow use of XUnixLockFile class.";
            default:     return CException::GetErrCodeString();
        }   
    }   

    NCBI_EXCEPTION_DEFAULT(CUnixLockFileException, CException);
};


class CUnixLockFile
{
public:
    CUnixLockFile( const std::string & lock_file_path,
                    short sleep_time_seconds = mk_DefaultSleepTime,
                    short retry_number = mk_DefaultRetryCount )
        : m_LockFilePath( lock_file_path ),
            m_LockFile( CDirEntry::GetTmpNameEx( CDirEntry(lock_file_path).GetDir(), "", CDirEntry::eTmpFileCreate ) ),
            m_SleepTime( sleep_time_seconds ),
            m_RetryCount( retry_number )
    {}
    
    ~CUnixLockFile() { m_LockFile.Remove(); }
    
    void    Lock()
    {
#if defined(NCBI_OS_UNIX)
        while( link(m_LockFile.GetPath().c_str(), m_LockFilePath.c_str()) == -1 ) {
            LOG_POST( Warning << "Could not acquire lock at " << m_LockFilePath
                        << ".  Will try " << m_RetryCount << " more times in "
                        << m_SleepTime << " seconds." );
            SleepSec( m_SleepTime );
            if ( 0 == m_RetryCount-- ) {
                NCBI_THROW( CUnixLockFileException, eRetryLimitExceeded, m_LockFilePath );
            }
        }
        m_LockFile.Remove();
        m_LockFile.Reset(m_LockFilePath);
#else
        NCBI_THROW( CUnixLockFileException, eWrongPlatform, m_LockFilePath );
#endif
    }
    
    
    void    Unlock()
    {
        DeleteLock( m_LockFilePath );
    }
    
    static bool DeleteLock( const std::string & path )
    {
        CFile   lock_file( path );
        return  lock_file.Remove();
    }
    
    
private:
    std::string m_LockFilePath;
    CFile   m_LockFile;
    short m_SleepTime;
    short m_RetryCount;
    
    static const short  mk_DefaultSleepTime = 5;
    static const short  mk_DefaultRetryCount = 5;
};


END_NCBI_SCOPE

#endif  //  ASN_CACHE_UNIX_LOCKFILE_HPP__

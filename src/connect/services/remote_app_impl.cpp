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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>

#include "remote_app_impl.hpp"

BEGIN_NCBI_SCOPE



CAtomicCounter IRemoteAppRequest_Impl::sm_DirCounter;
string IRemoteAppRequest_Impl::sm_TmpDirPath;


/*static*/
void IRemoteAppRequest_Impl::SetTempDir(const string& path)
{
    if (CDirEntry::IsAbsolutePath(path))
        sm_TmpDirPath = path;
    else {
        string tmp = CDir::GetCwd()
            + CDirEntry::GetPathSeparator()
            + path;
        sm_TmpDirPath = CDirEntry::NormalizePath(tmp);
    }
}
/*static*/
const string& IRemoteAppRequest_Impl::GetTempDir()
{
    if(sm_TmpDirPath.empty())
        SetTempDir(".");
    return sm_TmpDirPath;
}

void IRemoteAppRequest_Impl::x_CreateWDir()
{
    if( !m_TmpDirName.empty() )
        return;
    m_TmpDirName = GetTempDir() + CDirEntry::GetPathSeparator() +
        NStr::UIntToString(sm_DirCounter.Add(1));
    CDir wdir(m_TmpDirName);
    if (wdir.Exists())
        wdir.Remove();
    CDir(m_TmpDirName).CreatePath();
}

void IRemoteAppRequest_Impl::x_RemoveWDir()
{
    if (m_TmpDirName.empty())
        return;
    CDir dir(m_TmpDirName);
    if (dir.Exists())
        dir.Remove();
    m_TmpDirName = "";
}


void IRemoteAppRequest_Impl::Reset()
{
    m_InBlob->Reset();
    m_CmdLine = "";
    m_Files.clear();
    m_AppRunTimeout = 0;
    x_RemoveWDir();
}

//////////////////////////////////////////////////////////////////////////
//

void IRemoteAppResult_Impl::Reset()
{
    m_OutBlob->Reset();
    m_ErrBlob->Reset();
    m_RetCode = -1;
}


END_NCBI_SCOPE

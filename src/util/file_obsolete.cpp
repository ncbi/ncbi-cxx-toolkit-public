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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Remove old files from the specified directory
 *
 */

#include <ncbi_pch.hpp>
#include <util/file_obsolete.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbifile.hpp>


BEGIN_NCBI_SCOPE


CFileObsolete::CFileObsolete(const string& path)
    : m_Path(path)
{
}


CFileObsolete::~CFileObsolete()
{
}


bool CFileObsolete::OnRemove(const string& /* filename */)
{
    return true;
}


void CFileObsolete::Remove(const string&  mask, 
                           unsigned int   age,
                           ETimeMode      tmode)
{
    CDir dir(m_Path);
    if (!dir.Exists()) {
        ERR_POST(Info << "Directory is not found or access denied:" << m_Path);
        return;
    }

    CTime current(CTime::eCurrent);
    time_t cutoff_time = current.GetTimeT();

    if (cutoff_time < (time_t) age) {
        cutoff_time = 0;
    } else {
        cutoff_time -= age;
    }

    CDir::TEntries  content(dir.GetEntries(mask));
    ITERATE(CDir::TEntries, it, content) {

        if (!(*it)->IsFile()) {
            continue;
        }

        CTime modification;
        CTime creation;
        CTime access;

        bool res = 
            (*it)->GetTime(&modification, &creation, &access);

        if (!res) {
            continue;
        }

        time_t check_time = 0;

        switch (tmode) {
        case eLastModification:
            check_time = modification.GetTimeT();
            break;
        case eLastAccess:
            check_time = access.GetTimeT();
            break;
        default:
            _ASSERT(0);
            continue;
        } // switch

        if (check_time < cutoff_time) {  // Remove file
            (*it)->Remove();
        }

    } // ITERATE
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/02/01 21:47:15  grichenk
 * Fixed warnings
 *
 * Revision 1.4  2004/05/17 21:06:02  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.3  2004/01/09 17:57:15  lavr
 * Resolve signed/unsigned comparison issues
 * Avoid file removal should Remove() get bad "tmode"
 *
 * Revision 1.2  2003/12/05 18:50:23  kuznets
 * Fixed potential overflow on unsigned int arithmetics
 *
 * Revision 1.1  2003/12/01 15:46:48  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

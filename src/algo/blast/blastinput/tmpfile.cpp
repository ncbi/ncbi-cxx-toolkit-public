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
 * Authors: Christiam Camacho
 *
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <algo/blast/blastinput/tmpfile.hpp>
#include <corelib/ncbifile.hpp>
#ifdef NCBI_OS_UNIX
#  include <unistd.h>
#endif

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CTmpFile::CTmpFile(bool delete_file /* = true */)
{
    m_FileName = CFile::GetTmpName();
    m_DeleteFileOnDestruction = delete_file;
}

CTmpFile::CTmpFile(const string& fname, bool delete_file /* = true */)
: m_FileName(fname), m_DeleteFileOnDestruction(delete_file)
{}


CTmpFile::~CTmpFile()
{
    // first, close and delete our streams
    m_InFile.reset();
    m_OutFile.reset();

    if (m_DeleteFileOnDestruction) {
        unlink(m_FileName.c_str());
    }
}


//
// create streams to manage our temp file
//
CNcbiIstream& CTmpFile::AsInputFile(void)
{
    m_InFile.reset(new CNcbiIfstream(m_FileName.c_str()));
    return *m_InFile;
}


CNcbiOstream& CTmpFile::AsOutputFile(void)
{
    m_OutFile.reset(new CNcbiOfstream(m_FileName.c_str()));
    return *m_OutFile;
}

END_SCOPE(blast)
END_NCBI_SCOPE

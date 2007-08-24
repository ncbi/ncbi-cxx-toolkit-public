#ifndef ALGO_BLAST_BLASTINPUT__TMPFILE__HPP
#define ALGO_BLAST_BLASTINPUT__TMPFILE__HPP

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
 * File Description:
 *
 */

#include <corelib/ncbiobj.hpp>
#include <string>
#include <memory>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

class NCBI_XBLAST_EXPORT CTmpFile : public CObject
{
public:
    CTmpFile(bool delete_file = true);
    CTmpFile(const string& fname, bool delete_file = true);
    ~CTmpFile();

    CNcbiIstream& AsInputFile(void);
    CNcbiOstream& AsOutputFile(void);
    const string& GetFileName(void) const;


private:
    // the name of our temporary file
    string m_FileName;
    bool m_DeleteFileOnDestruction;

    auto_ptr<CNcbiIstream> m_InFile;
    auto_ptr<CNcbiOstream> m_OutFile;

    // prohibit copying!
    CTmpFile(const CTmpFile&);
    CTmpFile& operator=(const CTmpFile&);
};


inline
const string& CTmpFile::GetFileName(void) const
{
    return m_FileName;
}

END_SCOPE(blast)
END_NCBI_SCOPE

#endif // ALGO_BLAST_BLASTINPUT___TMPFILE__HPP

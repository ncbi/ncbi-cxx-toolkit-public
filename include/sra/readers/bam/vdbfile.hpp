#ifndef SRA__READER__BAM__VDBFILE__HPP
#define SRA__READER__BAM__VDBFILE__HPP
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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Access to VDB KFile, that supports remote cached file access
 *
 */

#include <corelib/ncbistd.hpp>
#include <sra/readers/bam/bamread.hpp>
#include <kfs/file.h>
#include <vfs/manager.h>
#include <vfs/path.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CVDBMgr;

SPECIALIZE_BAM_REF_TRAITS(VFSManager, );
SPECIALIZE_BAM_REF_TRAITS(VPath, );
SPECIALIZE_BAM_REF_TRAITS(KFile, const);


class NCBI_SRAREAD_EXPORT CVFSManager
    : public CBamRef<VFSManager>
{
    typedef CBamRef<VPath> TParent;
public:
    CVFSManager(void)
        {
            x_Init();
        }

private:
    void x_Init();
};


class NCBI_SRAREAD_EXPORT CVDBPath
    : public CBamRef<VPath>
{
    typedef CBamRef<VPath> TParent;
public:
    CVDBPath(void)
        {
        }
    CVDBPath(ENull /*null*/)
        {
        }
    explicit
    CVDBPath(const CVFSManager& mgr, const string& path)
        {
            x_Init(mgr, path);
        }

    CTempString GetString() const;

private:
    void x_Init(const CVFSManager& mgr, const string& path);
};


class NCBI_BAMREAD_EXPORT CVDBFile
    : public CBamRef<const KFile>
{
public:
    CVDBFile()
        {
        }
    CVDBFile(ENull /*null*/)
        {
        }
    CVDBFile(const CVFSManager& mgr, const CVDBPath& path)
        {
            x_Init(mgr, path);
        }
    explicit
    CVDBFile(const string& path);

    size_t GetSize() const;
    size_t Read(size_t file_pos, char* buffer, size_t buffer_size);
    size_t ReadAll(size_t file_pos, char* buffer, size_t buffer_size);
    void ReadExactly(size_t file_pos, char* buffer, size_t buffer_size);

private:
    void x_Init(const CVFSManager& mgr, const CVDBPath& path);

    CVDBPath m_Path;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SRA__READER__BAM__VDBFILE__HPP

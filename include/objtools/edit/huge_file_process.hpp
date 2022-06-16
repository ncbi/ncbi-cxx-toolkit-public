#ifndef HUGE_FILE_PROCESS_HPP
#define HUGE_FILE_PROCESS_HPP

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
 * Author:  Mati Shomrat
 * File Description:
 *   Utility class for processing Genbank release files.
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

/// forward declarations
class CObjectIStream;
BEGIN_SCOPE(objects)

class CSubmit_block;
class CSeq_entry;
class CSeq_id;

BEGIN_SCOPE(edit)
class CHugeFile;
class CHugeAsnReader;

class NCBI_XOBJEDIT_EXPORT CHugeFileProcess
{
public:

    /// constructors
    CHugeFileProcess();
    CHugeFileProcess(const string& file_name, const set<TTypeInfo>* types = nullptr);
    /// destructor
    virtual ~CHugeFileProcess(void);
    void Open(const string& file_name, const set<TTypeInfo>* types = nullptr);

    using THandler = std::function<void(CConstRef<CSubmit_block>, CRef<CSeq_entry>)>;
    using THandlerTopIds = std::function<void(CHugeAsnReader&, const std::list<CConstRef<CSeq_id>>&)>;

    [[nodiscard]] bool Read(THandler handler, CRef<CSeq_id> seqid);

    CHugeAsnReader& GetReader() { return *m_pReader; }
    CHugeFile& GetFile() { return *m_pHugeFile; }
    static bool IsSupported(TTypeInfo info);

private:
    unique_ptr<CHugeFile> m_pHugeFile;
    unique_ptr<CHugeAsnReader> m_pReader;
};

END_SCOPE(edit)
END_SCOPE(object);
END_NCBI_SCOPE

#endif  ///  NEW_GB_RELEASE_FILE__HPP

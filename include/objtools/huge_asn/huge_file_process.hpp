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
#include <serial/serialdef.hpp>

BEGIN_NCBI_SCOPE

/// forward declarations
class CObjectIStream;
BEGIN_SCOPE(objects)

class CScope;
class CSeq_entry_Handle;
class CBioseq_Handle;
class CSubmit_block;
class CSeq_entry;
class CSeq_id;

BEGIN_SCOPE(edit)
class CHugeFile;
class CHugeAsnReader;

class NCBI_XHUGEASN_EXPORT CHugeFileProcess
{
public:

    static const set<TTypeInfo> g_supported_types;

    /// constructors
    CHugeFileProcess();
    CHugeFileProcess(CHugeAsnReader* pReader);
    CHugeFileProcess(const string& file_name, const set<TTypeInfo>* types = &g_supported_types);
    /// destructor
    virtual ~CHugeFileProcess(void);
    void Open(const string& file_name, const set<TTypeInfo>* types = &g_supported_types);
    void OpenFile(const string& file_name);
    void OpenFile(const string& file_name, const set<TTypeInfo>* types);
    void OpenReader();

    using THandler    = std::function<void(CConstRef<CSubmit_block>, CRef<CSeq_entry>)>;
    using THandlerIds = std::function<bool(CHugeAsnReader*, const std::list<CConstRef<CSeq_id>>&)>;
    using THandlerBlobs   = std::function<bool(CHugeFileProcess&)>;
    using THandlerEntries = std::function<bool(CSeq_entry_Handle& seh)>;

    [[nodiscard]] bool Read(THandler handler, CRef<CSeq_id> seqid);
    [[nodiscard]] bool Read(THandlerIds handler);
    [[nodiscard]] bool ForEachBlob(THandlerBlobs);
    [[nodiscard]] bool ForEachEntry(CRef<CScope> scope, THandlerEntries handler);
    [[nodiscard]] bool ReadNextBlob();
    static CSeq_entry_Handle GetTopLevelEntry(CBioseq_Handle beh);

    CHugeAsnReader& GetReader()           { return *m_pReader; }
    CHugeFile& GetFile()                  { return *m_pHugeFile; }
    const CHugeFile& GetConstFile() const { return *m_pHugeFile; }
    static bool IsSupported(TTypeInfo info);

private:
    CRef<CHugeFile> m_pHugeFile;
    CRef<CHugeAsnReader> m_pReader;
};

END_SCOPE(edit)
END_SCOPE(object)
END_NCBI_SCOPE

#endif  ///  NEW_GB_RELEASE_FILE__HPP

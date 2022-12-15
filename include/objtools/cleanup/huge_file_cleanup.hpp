#ifndef HUGE_FILE_CLEANUP_HPP
#define HUGE_FILE_CLEANUP_HPP

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
 * Author:  Justin Foley
 * File Description:
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objtools/edit/huge_asn_reader.hpp>
#include <objtools/edit/huge_file_process.hpp>
#include <objtools/cleanup/cleanup_change.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeqdesc;

/*
class NCBI_CLEANUP_EXPORT CHugeFileCleanup :
    public edit::CHugeFileProcess
{
public: 
    CHugeFileCleanup();
    virtual ~CHugeFileCleanup() = default;
private:
};
*/


class NCBI_CLEANUP_EXPORT CCleanupHugeAsnReader :
    public edit::CHugeAsnReader
{   
public:
    enum EOptions {
        eExtendedCleanup    = 1,
        eNoNcbiUserObjects  = 1<<1,  
    };
    using TOptions = int;

    CCleanupHugeAsnReader(TOptions options);
    virtual ~CCleanupHugeAsnReader() = default;
    using TParent = edit::CHugeAsnReader;

    void FlattenGenbankSet() override;
    void AddTopLevelDescriptors(CSeq_entry_Handle); 
    const CCleanupChangeCore& GetChanges() const;
private:
    void x_CleanupTopLevelDescriptors();
    bool x_LooksLikeNucProtSet() const;
    void x_AddTopLevelDescriptors(CSeq_entry& entry);
    list<CRef<CSeqdesc>> m_TopLevelBiosources;
    CRef<CSeqdesc> m_pTopLevelMolInfo;
    const TOptions m_CleanupOptions;
    CCleanupChangeCore m_Changes;
};

END_SCOPE(object);
END_NCBI_SCOPE

#endif  ///  HUGE_FILE_CLEANUP

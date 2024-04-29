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
* Authors:  Sergiy Gotvyanskyy
*
* File Description:
*
*
*/

#ifndef _HUGE_ASN_DATALOADER_HPP_INCLUDED_
#define _HUGE_ASN_DATALOADER_HPP_INCLUDED_

#include <corelib/ncbistd.hpp>
#include <objmgr/data_loader.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)

namespace edit
{
    class CHugeAsnReader;
    class CHugeFile;
}

BEGIN_SCOPE(edit)

class NCBI_XHUGEASN_EXPORT CHugeAsnDataLoader: public CDataLoader
{
public:
    typedef SRegisterLoaderInfo<CHugeAsnDataLoader> TRegisterLoaderInfo;
    CHugeAsnDataLoader(const string& name, CHugeAsnReader* reader);
    ~CHugeAsnDataLoader();

    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& loader_name,
        CHugeAsnReader* reader,
        CObjectManager::EIsDefault is_default = CObjectManager::eNonDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_Default);

    TTSE_LockSet GetRecords(const CSeq_id_Handle& idh, EChoice choice) override;
    TBlobId GetBlobId(const CSeq_id_Handle& idh) override;
    TTSE_Lock GetBlobById(const TBlobId& blob_id) override;
    TSeqPos GetSequenceLength(const CSeq_id_Handle& idh) override;
    void GetIds(const CSeq_id_Handle& idh, CDataLoader::TIds& ids) override;
    CSeq_inst::TMol GetSequenceType(const CSeq_id_Handle& idh) override;
    STypeFound GetSequenceTypeFound(const CSeq_id_Handle& idh) override;
    TTaxId GetTaxId(const CSeq_id_Handle& idh) override;

    bool CanGetBlobById(void) const override
    {
        return true;
    }

    CHugeAsnReader& GetReader() { return *m_reader; }

private:
    CRef<CHugeAsnReader> m_reader;
    bool m_owning = false;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _HUGE_ASN_DATALOADER_HPP_INCLUDED_

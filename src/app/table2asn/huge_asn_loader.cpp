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

#include <ncbi_pch.hpp>

#include "huge_asn_loader.hpp"
#include "huge_asn_reader.hpp"
#include <objmgr/impl/tse_loadlock.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

namespace
{

class CHugeAsnDataLoaderMaker : public CLoaderMaker_Base
{
public:
    CHugeAsnDataLoaderMaker(const string& name, const string& filename):
        m_filename(filename)
    {
        m_Name = name;
    }

    virtual CDataLoader* CreateLoader(void) const
    {
        return new CHugeAsnDataLoader(m_Name, m_filename);
    }

    typedef CHugeAsnDataLoader::TRegisterLoaderInfo TRegisterInfo;
    TRegisterInfo GetRegisterInfo(void)
    {
        TRegisterInfo info;
        info.Set(m_RegisterInfo.GetLoader(), m_RegisterInfo.IsCreated());
        return info;
    }
private:
    const string m_filename;
};

}

CHugeAsnDataLoader::CHugeAsnDataLoader(const string& name, const string& filename):
    CDataLoader(name),
    m_filename{filename}
{
}

CDataLoader::TBlobId CHugeAsnDataLoader::GetBlobId(const CSeq_id_Handle& idh)
{
    if (m_reader.get() == nullptr)
    {
#       ifdef _DEBUG
        std::cout << "Indexing " << m_filename << "\n";
#       endif
        //m_reader.reset(new CHugeAsnReader(m_filename));
        //m_reader->PrintAllSeqIds();
    }
    size_t info = m_reader->FindTopObject(idh.GetSeqId());
    if (info != std::numeric_limits<size_t>::max()) {
        TBlobId blob_id = new CBlobIdInt(info);
        return blob_id;
    }
    return {};
}

CDataLoader::TTSE_Lock CHugeAsnDataLoader::GetBlobById(const TBlobId& blob_id)
{
    // Load data, get the lock
    CTSE_LoadLock lock = GetDataSource()->GetTSE_LoadLock(blob_id);
    if ( !lock.IsLoaded() ) {
        auto id = (const CBlobIdInt*)&*blob_id;
        size_t info = id->GetValue();
        auto entry = m_reader->LoadSeqEntry(info);
#       if _DEBUG
        std::cerr << "Loading " << info << "\n";
#       endif
        CTSE_Info& tse_info = *lock;
        tse_info.SetSeq_entry(*entry);
        lock.SetLoaded();
    }
    return lock;
}

CDataLoader::TTSE_LockSet
CHugeAsnDataLoader::GetRecords(const CSeq_id_Handle& idh, EChoice choice)
{
    TTSE_LockSet locks;
    TBlobId blob_id = GetBlobId(idh);
    if ( blob_id ) {
        TTSE_Lock lock = GetBlobById(blob_id);
        if ( lock ) {
            locks.insert(lock);
        }
    }
    return locks;
}

CHugeAsnDataLoader::TRegisterLoaderInfo CHugeAsnDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& loader_name,
    const string& filename,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    CHugeAsnDataLoaderMaker maker(loader_name, filename);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}

END_SCOPE(objects)
END_NCBI_SCOPE

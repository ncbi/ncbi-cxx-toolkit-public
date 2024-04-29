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
* Author:  Sergiy Gotvyanskyy
* File Description:
*   Utility class for processing ASN.1 files using Huge Files approach
*
*/
#include <ncbi_pch.hpp>

#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objtools/edit/huge_file.hpp>
#include <objtools/edit/huge_asn_reader.hpp>
#include <objtools/edit/huge_file_process.hpp>

#include <objects/submit/Submit_block.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/edit/huge_asn_loader.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

namespace
{

    class CAutoRevoker
    {
    public:
        template<class TLoader>
        CAutoRevoker(struct SRegisterLoaderInfo<TLoader>& info)
            : m_loader{ info.GetLoader() } {}
        ~CAutoRevoker()
        {
            CObjectManager::GetInstance()->RevokeDataLoader(*m_loader);
        }
    private:
        CDataLoader* m_loader = nullptr;
    };

}


const set<TTypeInfo> CHugeFileProcess::g_supported_types =
{
    CBioseq_set::GetTypeInfo(),
    CBioseq::GetTypeInfo(),
    CSeq_entry::GetTypeInfo(),
    CSeq_submit::GetTypeInfo(),
};


CHugeFileProcess::CHugeFileProcess():
    m_pHugeFile { new CHugeFile },
    m_pReader{ new CHugeAsnReader }
{}



CHugeFileProcess::CHugeFileProcess(CHugeAsnReader* pReader):
    m_pHugeFile { new CHugeFile },
    m_pReader { pReader }
{}


CHugeFileProcess::CHugeFileProcess(const string& file_name, const set<TTypeInfo>* types)
: CHugeFileProcess()
{
    Open(file_name, types);
}

bool CHugeFileProcess::IsSupported(TTypeInfo info)
{
    return g_supported_types.find(info) != g_supported_types.end();
}

void CHugeFileProcess::Open(const string& file_name, const set<TTypeInfo>* types)
{
    OpenFile(file_name, types);
    OpenReader();
}

void CHugeFileProcess::OpenFile(const string& file_name)
{
    m_pHugeFile->Open(file_name, &g_supported_types);
}

void CHugeFileProcess::OpenFile(const string& file_name, const set<TTypeInfo>* types)
{
    m_pHugeFile->Open(file_name, types);
}

void CHugeFileProcess::OpenReader()
{
    m_pReader->Open(m_pHugeFile.GetPointer(), nullptr);
}

CHugeFileProcess::~CHugeFileProcess()
{
}

bool CHugeFileProcess::Read(THandler handler, CRef<CSeq_id> seqid)
{
    if (!m_pReader->GetNextBlob()) {
        return false;
    }

    do
    {
        m_pReader->FlattenGenbankSet();
        CRef<CSeq_entry> entry;
        do
        {
            entry.Reset();

            if (seqid.Empty())
                entry = m_pReader->GetNextSeqEntry();
            else
            {
                auto seq = m_pReader->LoadBioseq(seqid);
                if (seq.NotEmpty())
                {
                    entry = Ref(new CSeq_entry);
                    entry->SetSeq(*seq);
                    if (auto pTopEntry = m_pReader->GetTopEntry(); pTopEntry) {
                        auto pNewEntry = Ref(new CSeq_entry());
                        pNewEntry->Assign(*pTopEntry);
                        pNewEntry->SetSet().SetSeq_set().push_back(entry);
                        entry = pNewEntry;
                    }
                }
            }

            if (entry)
            {
                handler(m_pReader->GetSubmitBlock(), entry);
            }
        }
        while ( entry && seqid.Empty());
    } while (m_pReader->GetNextBlob());

    return true;
}

bool CHugeFileProcess::Read(THandlerIds handler)
{
    while (m_pReader->GetNextBlob()) {
        m_pReader->FlattenGenbankSet();
        bool processed = handler(m_pReader.GetPointer(), m_pReader->GetTopIds());
        if (!processed)
            return false;
    }

    return true;
}

bool CHugeFileProcess::ReadNextBlob()
{
    if (m_pReader->GetNextBlob()) {
        m_pReader->FlattenGenbankSet();
        return true;
    }

    return false;
}


bool CHugeFileProcess::ForEachBlob(THandlerBlobs handler)
{
    while (m_pReader->GetNextBlob()) {
        m_pReader->FlattenGenbankSet();
        bool processed = handler(*this);
        if (!processed)
            return false;
    }

    return true;
}

bool CHugeFileProcess::ForEachEntry(CRef<CScope> scope, THandlerEntries handler)
{
    if (!handler)
        return false;

    string loader_name = CDirEntry::CreateAbsolutePath(GetFile().m_filename);
    auto info = CHugeAsnDataLoader::RegisterInObjectManager(
        *CObjectManager::GetInstance(), loader_name, &GetReader(), CObjectManager::eNonDefault, 1); //CObjectManager::kPriority_Local);

    CAutoRevoker autorevoker(info);

    if (!scope)
        scope = Ref(new CScope(*CObjectManager::GetInstance()));

    scope->AddDataLoader(loader_name);

    try
    {
        for (auto id: GetReader().GetTopIds())
        {
            {
                auto beh = scope->GetBioseqHandle(*id);
                auto parent = beh.GetTopLevelEntry();
                handler(parent);
            }
            scope->ResetHistory();
        }
    }
    catch(const std::exception& e)
    {
        scope->RemoveDataLoader(loader_name);
        throw;
    }
    scope->RemoveDataLoader(loader_name);

    return true;
}


CSeq_entry_Handle CHugeFileProcess::GetTopLevelEntry(CBioseq_Handle beh)
{
    CSeq_entry_Handle parent = beh.GetParentEntry();
    while(parent)
    {
        if (parent.IsTopLevelEntry())
            break;

        if (auto temp = parent.GetParentEntry(); temp) {
            if (temp.IsSet() && temp.GetSet().IsSetClass() &&
                CHugeAsnReader::IsHugeSet(temp.GetSet().GetClass())) {
                break;
            }

            parent = temp;
        }
        else
            break;
    }

    return parent;
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

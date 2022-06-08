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

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>

#include <objtools/edit/huge_file.hpp>
#include <objtools/edit/huge_asn_reader.hpp>
#include <objtools/edit/huge_file_process.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


CHugeFileProcess::CHugeFileProcess(const string& file_name)
    :   m_pHugeFile { new CHugeFile },
        m_pReader{ new CHugeAsnReader }
{
    static set<TTypeInfo> supported_types =
    {
        CBioseq_set::GetTypeInfo(),
        CBioseq::GetTypeInfo(),
        CSeq_entry::GetTypeInfo(),
        CSeq_submit::GetTypeInfo(),
    };
    m_pHugeFile->m_supported_types = &supported_types;

    m_pHugeFile->Open(file_name);
    m_pReader->Open(m_pHugeFile.get(), nullptr);
}


CHugeFileProcess::~CHugeFileProcess(void)
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
                entry = m_pReader->GetNextEntry();
            else
            {
                auto seq = m_pReader->LoadBioseq(seqid);
                if (seq.NotEmpty())
                {
                    entry = Ref(new CSeq_entry);
                    entry->SetSeq(*seq);
                }
            }

            if (entry)
            {
                if (auto pTopEntry = m_pReader->GetTopEntry(); pTopEntry) {
                    auto pNewEntry = Ref(new CSeq_entry());
                    pNewEntry->Assign(*pTopEntry);
                    pNewEntry->SetSet().SetSeq_set().push_back(entry);
                    entry = pNewEntry;
                }

                handler(m_pReader->GetSubmitBlock(), entry);
            }
        }
        while ( entry && seqid.Empty());
    } while (m_pReader->GetNextBlob());

    return true;
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

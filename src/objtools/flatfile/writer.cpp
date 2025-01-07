/* $Id$
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
 *
 * Author: Justin Foley
 *
 * File Description:
 *      ASN writer for flatfile parser
 *
 */

#include <ncbi_pch.hpp>
#include "writer.hpp"
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objtools/readers/objhook_lambdas.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

class CWriteSubmitEntrysHook : public CWriteChoiceVariantHook
{

public:
    using FGetEntry = CFlat2AsnWriter::FGetEntry;

    CWriteSubmitEntrysHook(FGetEntry fGetEntry) :
        m_fGetEntry(fGetEntry) {}

    virtual ~CWriteSubmitEntrysHook() {}

    void WriteChoiceVariant(CObjectOStream&           objOstr,
                            const CConstObjectInfoCV& variant)
    {
        COStreamContainer outContainer(objOstr, variant.GetVariantType());
        CRef<CSeq_entry>  pEntry;
        while ((pEntry = m_fGetEntry())) {
            outContainer << *pEntry;
        }
    }

private:
    FGetEntry m_fGetEntry;
};


void CFlat2AsnWriter::Write(const CSeq_submit* pSubmit, FGetEntry getEntry)
{
    m_ObjOstr.SetPathWriteVariantHook("Seq-submit.data.entrys",
                                      new CWriteSubmitEntrysHook(getEntry));

    m_ObjOstr.Write(pSubmit, pSubmit->GetThisTypeInfo());
}


void CFlat2AsnWriter::Write(const CBioseq_set* pBioseqSet, FGetEntry getEntry)
{
    size_t level          = 0;
    auto   seq_set_member = CObjectTypeInfo(CBioseq_set::GetTypeInfo()).FindMember("seq-set");
    SetLocalWriteHook(seq_set_member.GetMemberType(), m_ObjOstr, [&level, getEntry](CObjectOStream& objOstr, const CConstObjectInfo& object) {
        ++level;
        if (level == 1) {
            COStreamContainer outContainer(objOstr, object.GetTypeInfo());
            CRef<CSeq_entry>  pEntry;
            while ((pEntry = getEntry())) {
                outContainer << *pEntry;
            }
        } else {
            object.GetTypeInfo()->DefaultWriteData(objOstr, object.GetObjectPtr());
        }
        --level;
    });

    m_ObjOstr.Write(pBioseqSet, pBioseqSet->GetThisTypeInfo());
}


END_NCBI_SCOPE

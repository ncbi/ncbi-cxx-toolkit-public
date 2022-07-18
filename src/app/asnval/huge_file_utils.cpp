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
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/edit/huge_asn_reader.hpp>
#include <objtools/validator/validatorp.hpp>
#include <objtools/validator/validator_context.hpp>
#include <objtools/validator/validerror_bioseq.hpp>
#include <objtools/validator/validerror_format.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/valerr/ValidErrItem.hpp>
#include <objects/valerr/ValidError.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/readers/objhook_lambdas.hpp> 
#include "huge_file_utils.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(edit);
USING_SCOPE(validator);

using TBioseqInfo = CHugeAsnReader::TBioseqInfo;

static bool s_IsTSAContig(const TBioseqInfo& info, const CHugeAsnReader& reader) {
    auto pSeqdesc = reader.GetClosestDescriptor(info, CSeqdesc::e_Molinfo);
    if (pSeqdesc) {
        const auto& molInfo = pSeqdesc->GetMolinfo();
        return (molInfo.IsSetTech() && 
                (molInfo.GetTech() == CMolInfo::eTech_wgs ||
                 molInfo.GetTech() == CMolInfo::eTech_tsa));
    }   
    return false;
}


static bool s_IsGpipe(const TBioseqInfo& info)
{
    for (auto pId : info.m_ids) {
        if (pId->IsGpipe()) {
            return true;
        }
    }
    return false;
}


static bool s_CuratedRefSeq(const string& accession)
{   
    return (NStr::StartsWith(accession, "NM_") ||
            NStr::StartsWith(accession, "NP_") ||
            NStr::StartsWith(accession, "NG_") ||
            NStr::StartsWith(accession, "NR_"));
}


static bool s_IsNoncuratedRefSeq(const list<CConstRef<CSeq_id>>& ids)
{
    for (auto pId : ids) {
        if (pId && pId->IsOther() && pId->GetOther().IsSetAccession()) {
            return !s_CuratedRefSeq(pId->GetOther().GetAccession());
        }
    }
    return false;
}


bool g_IsCuratedRefSeq(const TBioseqInfo& info)
{
    for (auto pId : info.m_ids) {
        if (pId && pId->IsOther() && pId->GetOther().IsSetAccession()) {
            if (s_CuratedRefSeq(pId->GetOther().GetAccession())) {
                return true;
            }
        }
    }
    return false;
}


static bool s_IsMaster(const TBioseqInfo& info) 
{
    if (info.m_repr != CSeq_inst::eRepr_virtual) {
        return false;
    }

    for (auto pId : info.m_ids) {
        if (g_IsMasterAccession(*pId)) {
            return true;
        }
    }
    return false;
}

static bool s_IsWGS(const TBioseqInfo& info, const CHugeAsnReader& reader)
{   
    auto pSeqdesc = reader.GetClosestDescriptor(info, CSeqdesc::e_Molinfo);
    if (pSeqdesc) {
        const auto& molInfo = pSeqdesc->GetMolinfo();
        return (molInfo.IsSetTech() && molInfo.GetTech() == CMolInfo::eTech_wgs);
    }
    return false;
}


static bool s_IsWGSMaster(const TBioseqInfo& info, const CHugeAsnReader& reader) {
    return s_IsMaster(info) && s_IsWGS(info, reader);
}


// IsRefSeq==true indicates that either the record contains RefSeq accessions
// or that RefSeq conventions have been specified.
static bool s_ReportMissingCitSub(const TBioseqInfo& info, const CHugeAsnReader& reader, bool IsRefSeq)
{
    // Does the order matter here? Can this be a RefSeq record and a WGS master?
    if (s_IsWGSMaster(info, reader)) {
        return true;
    }
        
    if (IsRefSeq) {
        return false;
    }

    for (auto pId : info.m_ids) {
        if (CValidError_bioseq::IsWGSAccession(*pId) ||
            CValidError_bioseq::IsTSAAccession(*pId)) {
            return false;
        }
    }   
    return true; 
}


static bool s_ReportMissingPubs(const TBioseqInfo& info, const CHugeAsnReader& reader)
{   
    if (reader.GetBiosets().size() > 1 && 
        next(reader.GetBiosets().begin())->m_class == CBioseq_set::eClass_gen_prod_set) {
        return false;
    }


    if (s_IsNoncuratedRefSeq(info.m_ids) ||
        s_IsGpipe(info) ||
        s_IsTSAContig(info, reader)) { // Note - no need to check for WGS contig because a WGS contig is a type of TSA contig
        return false;
    }
    return true;
}


static string s_GetBioseqAcc(const CSeq_id& id, int* version)
{
    try {
        string label;
        id.GetLabel(&label, version, CSeq_id::eFasta);
        return label;
    }
    catch (CException&){
    }

    return kEmptyStr;
}

static string s_GetIdString(const list<CConstRef<CSeq_id>>& ids, int* version)
{
    list<CRef<CSeq_id>> tmpIds;
    for (auto pId : ids) {
        auto pTmpId = Ref(new CSeq_id());
        pTmpId->Assign(*pId);
        tmpIds.push_back(pTmpId);
    }

    auto pBestId = sequence::GetId(tmpIds, sequence::eGetId_Best).GetSeqId();
    if (pBestId) {
        return s_GetBioseqAcc(*pBestId, version);
    }

    return kEmptyStr; 
}


void s_PostErr(EDiagSev severity,
               EErrType errorType,
               const string& message,
               const string& idString,
               CRef<CValidError>& pErrors)
{

    if (!pErrors) {
        pErrors = Ref(new CValidError());
    }

    const bool suppressContext = false; // Revisit this
    const auto setClass = CBioseq_set::eClass_genbank; // Revisit this
    int version = 0;
    string desc = CValidErrorFormat::GetBioseqSetLabel(idString, 
            setClass, suppressContext);

    pErrors->AddValidErrItem(severity, errorType, message, desc, idString, version);
}


static bool s_IsNa(CSeq_inst::EMol mol)
{
    return (mol == CSeq_inst::eMol_dna ||
            mol == CSeq_inst::eMol_rna ||
            mol == CSeq_inst::eMol_na);
}


string g_GetIdString(const CHugeAsnReader& reader)
{ // Revisit this
    const auto& biosets = reader.GetBiosets();
    if (biosets.size() < 2) {
        return "";
    }
    
    if (auto it = next(biosets.begin()); 
        it->m_class != CBioseq_set::eClass_genbank){
        return "";
    }

    const auto& bioseqs = reader.GetBioseqs();
    const auto& firstBioseq = bioseqs.begin();
    const auto& parentIt = firstBioseq->m_parent_set;
    int version;
    if (parentIt->m_class != CBioseq_set::eClass_nuc_prot) {
        return s_GetIdString(firstBioseq->m_ids, &version);
    }
    // else parent set is nuc-prot set
    for(auto it = firstBioseq; it != bioseqs.end(); ++it) {
        if (s_IsNa(it->m_mol) && it->m_parent_set == parentIt) {
            return s_GetIdString(it->m_ids, &version);
        }
    }
    return s_GetIdString(firstBioseq->m_ids, &version);
}


static bool s_HasPub(const list<CRef<CSeqdesc>>& descriptors)
{   
    for (auto pDesc : descriptors) {
        if (pDesc && pDesc->IsPub()) {
            const auto& pub = pDesc->GetPub();
            if (pub.IsSetPub() && pub.GetPub().IsSet() && !pub.GetPub().Get().empty()) {
                return true;
            }
        }
    } 
    return false;
}

static bool s_HasCitSub(const list<CRef<CSeqdesc>>& descriptors)
{
    for (auto pDesc : descriptors) {
        if (pDesc && pDesc->IsPub()) {
            const auto& pub = pDesc->GetPub();
            if (pub.IsSetPub() && pub.GetPub().IsSet()) {
                for (auto pPub : pub.GetPub().Get()) {
                    if (pPub && pPub->IsSub()) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

static void s_GatherSerialNumbers(const list<CRef<CSeqdesc>>& descriptors,
        set<int>& pubSerialNumbers,
        set<int>& collidingNumbers)
{
    for (auto pDesc : descriptors) {
        if (pDesc && pDesc->IsPub()) {
            const auto& pub = pDesc->GetPub();
            if (pub.IsSetPub() && pub.GetPub().IsSet()) {
                for (auto pPub : pub.GetPub().Get()) {
                    if (pPub->IsGen()) {
                        const auto& gen = pPub->GetGen();
                        if (gen.IsSetSerial_number()) {
                            if (!pubSerialNumbers.insert(gen.GetSerial_number()).second) {
                                collidingNumbers.insert(gen.GetSerial_number());
                            }
                        }
                    }
                }
            }
        }
    }
}


CHugeFileValidator::CHugeFileValidator(const CHugeFileValidator::TReader& reader,
        CHugeFileValidator::TOptions options)
    : m_Reader(reader), m_Options(options) {}


bool CHugeFileValidator::x_HasRefSeqAccession() const
{
    for (auto info : m_Reader.GetBioseqs()) {
        for (auto pId : info.m_ids) {
            if (pId && pId->IsOther()) {
                return true;
            }
        }
    }
    return false;
}


string CHugeFileValidator::x_FindIdString() const
{
    const auto& biosets = m_Reader.GetBiosets();
    if (biosets.size() < 2) {
        return "";
    }
    
    if (auto it = next(biosets.begin()); 
        it->m_class != CBioseq_set::eClass_genbank){
        return "";
    }

    const auto& bioseqs = m_Reader.GetBioseqs();
    const auto& firstBioseq = bioseqs.begin();
    const auto& parentIt = firstBioseq->m_parent_set;
    int version;
    if (parentIt->m_class != CBioseq_set::eClass_nuc_prot) {
        return s_GetIdString(firstBioseq->m_ids, &version);
    }
    // else parent set is nuc-prot set
    for(auto it = firstBioseq; it != bioseqs.end(); ++it) {
        if (s_IsNa(it->m_mol) && it->m_parent_set == parentIt) {
            return s_GetIdString(it->m_ids, &version);
        }
    }
    return s_GetIdString(firstBioseq->m_ids, &version);
}


string CHugeFileValidator::x_GetIdString() const
{
    if (m_pIdString) {
        return *m_pIdString;
    }

    m_pIdString.reset(new string());
    *m_pIdString = x_FindIdString();

    return *m_pIdString;
}


void CHugeFileValidator::ReportCollidingSerialNumbers(const set<int>& collidingNumbers,
        CRef<CValidError>& pErrors) const
{
    for (auto val : collidingNumbers) {
        s_PostErr(eDiag_Warning, eErr_GENERIC_CollidingSerialNumbers,
                "Multiple publications have serial number " + NStr::IntToString(val),
                x_GetIdString(), pErrors);
    }
}


void CHugeFileValidator::ReportMissingPubs(CRef<CValidError>& pErrors) const
{
    if(!(m_Reader.GetSubmitBlock())) {
        if (auto info = m_Reader.GetBioseqs().front(); s_ReportMissingPubs(info, m_Reader)) {
            auto severity = g_IsCuratedRefSeq(info) ? eDiag_Warning : eDiag_Error;
            s_PostErr(severity, eErr_SEQ_DESCR_NoPubFound,
                    "No publications anywhere on this entire record.", 
                    x_GetIdString(), pErrors);
        }
    }
}


void CHugeFileValidator::ReportMissingCitSubs(CRef<CValidError>& pErrors) const
{
    if(!(m_Reader.GetSubmitBlock())) {
        bool isRefSeq = (m_Options & CValidator::eVal_refseq_conventions) ||
            x_HasRefSeqAccession();

        if (auto info = m_Reader.GetBioseqs().front(); s_ReportMissingCitSub(info, m_Reader, isRefSeq)) 
        {
            auto severity = (m_Options & CValidator::eVal_genome_submission) ?
                eDiag_Error : eDiag_Info;
            s_PostErr(severity, eErr_GENERIC_MissingPubRequirement,
                    "No submission citation anywhere on this entire record.",
                    x_GetIdString(), pErrors);
        }
    }
}



void CHugeFileValidator::ReportGlobalErrors(const SGlobalValidatorInfo& globalInfo, CRef<CValidError>& pErrors) const
{
    if (globalInfo.NoPubsFound) {
        ReportMissingPubs(pErrors);
    }

    if (globalInfo.NoCitSubsFound) {
        ReportMissingCitSubs(pErrors);
    }

    if (!globalInfo.conflictingSerialNumbers.empty()) {
        ReportCollidingSerialNumbers(globalInfo.conflictingSerialNumbers, pErrors);
    }
}


static void s_UpdateGlobalInfo(const CPubdesc& pub, SGlobalValidatorInfo& globalInfo) 
{
    if (pub.IsSetPub() && pub.GetPub().IsSet() && !pub.GetPub().Get().empty()) {
        globalInfo.NoPubsFound = false;
        for (auto pPub : pub.GetPub().Get()) {
            if (pPub->IsSub()) {
                globalInfo.NoCitSubsFound = false;
            }
            else if (pPub->IsGen()) {
                const auto& gen = pPub->GetGen();
                if (gen.IsSetSerial_number()) {
                    if (!globalInfo.pubSerialNumbers.insert(gen.GetSerial_number()).second) {
                        globalInfo.conflictingSerialNumbers.insert(gen.GetSerial_number());
                    }
                }
            }
        }
    }
}


void CValidatorHFReader::x_SetHooks(CObjectIStream& objStream, CValidatorHFReader::TContext& context) 
{
    TParent::x_SetHooks(objStream, context);
    // Set Pubdesc skip and read hooks
    SetLocalSkipHook(CType<CPubdesc>(), objStream,
            [this] (CObjectIStream& in, const CObjectTypeInfo& type)
            {
                auto pPubdesc = Ref(new CPubdesc());
                type.GetTypeInfo()->DefaultReadData(in, pPubdesc);
                s_UpdateGlobalInfo(*pPubdesc, m_GlobalInfo);
            });

    SetLocalReadHook(CType<CPubdesc>(), objStream,
            [this](CObjectIStream& in, const CObjectInfo& object)
            {
                auto* pObject = object.GetObjectPtr();
                object.GetTypeInfo()->DefaultReadData(in, pObject);
                CPubdesc* pPubdesc = CTypeConverter<CPubdesc>::SafeCast(pObject);
                s_UpdateGlobalInfo(*pPubdesc, m_GlobalInfo);
            });
}

END_NCBI_SCOPE

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
#include <serial/objhook.hpp>
#include <objtools/readers/objhook_lambdas.hpp>
#include <objtools/validator/huge_file_validator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

USING_SCOPE(edit);
BEGIN_SCOPE(validator)

using TBioseqInfo = CHugeAsnReader::TBioseqInfo;


static bool s_IsTSAContig(const TBioseqInfo& info, const CHugeAsnReader& reader)
{
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
            return ! s_CuratedRefSeq(pId->GetOther().GetAccession());
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


static bool s_IsWGSMaster(const TBioseqInfo& info, const CHugeAsnReader& reader)
{
    return s_IsMaster(info) && s_IsWGS(info, reader);
}


// IsRefSeq==true indicates that either the record contains RefSeq accessions
// or that RefSeq conventions have been specified.
static bool s_x_ReportMissingCitSub(const TBioseqInfo& info, const CHugeAsnReader& reader, bool IsRefSeq)
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


static bool s_x_ReportMissingPubs(const TBioseqInfo& info, const CHugeAsnReader& reader)
{
    if (const auto pSetClass = reader.GetTopLevelClass();
        pSetClass && *pSetClass == CBioseq_set::eClass_gen_prod_set) {
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
    } catch (CException&) {
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


void s_PostErr(EDiagSev           severity,
               EErrType           errorType,
               const string&      message,
               const string&      desc,
               CRef<CValidError>& pErrors)
{

    if (! pErrors) {
        pErrors = Ref(new CValidError());
    }

    int version = 0;
    pErrors->AddValidErrItem(severity, errorType, message, desc, "", version);
}


void s_PostErr(EDiagSev      severity,
               EErrType      errorType,
               const string& message,
               const string& desc,
               IValidError&  errors)
{
    int version = 0;
    errors.AddValidErrItem(severity, errorType, message, desc, "", version);
}


static bool s_IsNa(CSeq_inst::EMol mol)
{
    return (mol == CSeq_inst::eMol_dna ||
            mol == CSeq_inst::eMol_rna ||
            mol == CSeq_inst::eMol_na);
}


CHugeFileValidator::CHugeFileValidator(const CHugeFileValidator::TReader& reader,
                                       CHugeFileValidator::TOptions       options) :
    m_Reader(reader),
    m_Options(options) {}


string CHugeFileValidator::x_GetIdString() const
{
    if (m_pIdString) {
        return *m_pIdString;
    }

    m_pIdString.reset(new string());
    *m_pIdString = g_GetHugeSetIdString(m_Reader);

    return *m_pIdString;
}


string CHugeFileValidator::x_GetHugeSetLabel() const
{
    const auto& biosets = m_Reader.GetBiosets();
    if (biosets.size() < 2) {
        return "";
    }

    auto it = next(biosets.begin());
    if (! CHugeAsnReader::IsHugeSet(it->m_class)) {
        return "";
    }

    const bool suppressContext = false;
    return CValidErrorFormat::GetBioseqSetLabel(x_GetIdString(), it->m_class, suppressContext);
}


bool CHugeFileValidator::IsInBlob(const CSeq_id& id) const
{
    CConstRef<CSeq_id> pId(&id);
    return (m_Reader.FindBioseq(pId) != nullptr);
}


void CHugeFileValidator::x_ReportCollidingSerialNumbers(const set<int>& collidingNumbers,
                                                        IValidError&    errors) const
{
    for (auto val : collidingNumbers) {
        s_PostErr(eDiag_Warning, eErr_GENERIC_CollidingSerialNumbers, "Multiple publications have serial number " + NStr::IntToString(val), x_GetHugeSetLabel(), errors);
    }
}


void CHugeFileValidator::x_ReportMissingPubs(IValidError& errors) const
{
    if (! (m_Reader.GetSubmitBlock())) {
        if (auto info = m_Reader.GetBioseqs().front(); s_x_ReportMissingPubs(info, m_Reader)) {
            auto severity = g_IsCuratedRefSeq(info) ? eDiag_Warning : eDiag_Error;
            s_PostErr(severity, eErr_SEQ_DESCR_NoPubFound, "No publications anywhere on this entire record.", x_GetHugeSetLabel(), errors);
        }
    }
}


void CHugeFileValidator::x_ReportMissingCitSubs(bool hasRefSeqAccession, IValidError& errors) const
{
    if (! (m_Reader.GetSubmitBlock())) {
        bool isRefSeq = hasRefSeqAccession || (m_Options & CValidator::eVal_refseq_conventions);

        if (auto info = m_Reader.GetBioseqs().front(); s_x_ReportMissingCitSub(info, m_Reader, isRefSeq)) {
            auto severity = (m_Options & CValidator::eVal_genome_submission) ? eDiag_Error : eDiag_Info;
            s_PostErr(severity, eErr_GENERIC_MissingPubRequirement, "No submission citation anywhere on this entire record.", x_GetHugeSetLabel(), errors);
        }
    }
}


void CHugeFileValidator::x_ReportMissingBioSources(IValidError& errors) const
{
    s_PostErr(eDiag_Error,
              eErr_SEQ_DESCR_NoSourceDescriptor,
              "No source information included on this record.",
              x_GetHugeSetLabel(),
              errors);
}


void CHugeFileValidator::x_ReportConflictingBiomols(IValidError& errors) const
{
    auto severity = (m_Options & CValidator::eVal_genome_submission) ? eDiag_Error : eDiag_Warning;
    s_PostErr(severity,
              eErr_SEQ_PKG_InconsistentMoltypeSet,
              "Pop/phy/mut/eco set contains inconsistent moltype",
              x_GetHugeSetLabel(),
              errors);
}


void CHugeFileValidator::ReportGlobalErrors(const TGlobalInfo& globalInfo, IValidError& errors) const
{
    if (globalInfo.NoPubsFound) {
        x_ReportMissingPubs(errors);
    }

    if (globalInfo.NoCitSubsFound) {
        x_ReportMissingCitSubs(globalInfo.IsRefSeq, errors);
    }

    if (! globalInfo.conflictingSerialNumbers.empty()) {
        x_ReportCollidingSerialNumbers(globalInfo.conflictingSerialNumbers, errors);
    }


    if (globalInfo.NoBioSource && ! globalInfo.IsPatent && ! globalInfo.IsPDB) {
        x_ReportMissingBioSources(errors);
    }


    if (globalInfo.biomols.size() > 1) {
        if (auto pSetClass = m_Reader.GetTopLevelClass();
            pSetClass &&
            *pSetClass == CBioseq_set::eClass_wgs_set) {
            x_ReportConflictingBiomols(errors);
        }
    }

    if (globalInfo.TpaAssemblyHist > 0 &&
        globalInfo.JustTpaAssembly > 0) {
        s_PostErr(eDiag_Error,
                  eErr_SEQ_INST_TpaAssemblyProblem,
                  "There are " + NStr::SizetToString(globalInfo.TpaAssemblyHist) + " TPAs with history and " + NStr::SizetToString(globalInfo.JustTpaAssembly) + " without history in this record.",
                  x_GetHugeSetLabel(),
                  errors);
    }
    if (globalInfo.TpaNoHistYesGI > 0) {
        s_PostErr(eDiag_Warning,
                  eErr_SEQ_INST_TpaAssemblyProblem,
                  "There are " + NStr::SizetToString(globalInfo.TpaNoHistYesGI) + " TPAs without history in this record where the record has a gi number assignment.",
                  x_GetHugeSetLabel(),
                  errors);
    }
}


void CHugeFileValidator::ReportPostErrors(const SValidatorContext& context,
                                          IValidError&             errors) const
{
    if (context.NumGenes == 0 && context.NumGeneXrefs > 0) {
        s_PostErr(eDiag_Warning,
                  eErr_SEQ_FEAT_OnlyGeneXrefs,
                  "There are " + NStr::SizetToString(context.NumGeneXrefs) + " gene xrefs and no gene features in this record.",
                  x_GetHugeSetLabel(),
                  errors);
    }
    if (context.CumulativeInferenceCount >= InferenceAccessionCutoff) {
        s_PostErr(eDiag_Info,
                  eErr_SEQ_FEAT_TooManyInferenceAccessions,
                  "Skipping validation of remaining /inference qualifiers",
                  x_GetHugeSetLabel(),
                  errors);
    }
}


void CHugeFileValidator::UpdateValidatorContext(const TGlobalInfo& globalInfo, SValidatorContext& context) const
{
    if (m_Reader.GetBiosets().size() < 2) {
        return;
    }

    if (auto it = next(m_Reader.GetBiosets().begin());
        ! CHugeAsnReader::IsHugeSet(it->m_class)) {
        return;
    }

    context.PreprocessHugeFile = true;
    context.HugeSetId          = g_GetHugeSetIdString(m_Reader);

    context.IsPatent        = globalInfo.IsPatent;
    context.IsPDB           = globalInfo.IsPDB;
    context.IsRefSeq        = globalInfo.IsRefSeq;
    context.NoBioSource     = globalInfo.NoBioSource;
    context.NoPubsFound     = globalInfo.NoPubsFound;
    context.NoCitSubsFound  = globalInfo.NoCitSubsFound;
    context.CurrTpaAssembly = globalInfo.CurrTpaAssembly;
    context.JustTpaAssembly += globalInfo.JustTpaAssembly;
    context.TpaAssemblyHist += globalInfo.TpaAssemblyHist;
    context.TpaNoHistYesGI += globalInfo.TpaNoHistYesGI;
    context.CumulativeInferenceCount += globalInfo.CumulativeInferenceCount;

    if (! context.IsIdInBlob) {
        context.IsIdInBlob = [this](const CSeq_id& id) {
            return this->IsInBlob(id);
        };
    }
}


static void s_UpdateGlobalInfo(const CPubdesc& pub, CHugeFileValidator::TGlobalInfo& globalInfo)
{
    if (pub.IsSetPub() && pub.GetPub().IsSet() && ! pub.GetPub().Get().empty()) {
        globalInfo.NoPubsFound = false;
        for (auto pPub : pub.GetPub().Get()) {
            if (pPub->IsSub() && globalInfo.NoCitSubsFound) {
                globalInfo.NoCitSubsFound = false;
            } else if (pPub->IsGen()) {
                const auto& gen = pPub->GetGen();
                if (gen.IsSetSerial_number()) {
                    if (! globalInfo.pubSerialNumbers.insert(gen.GetSerial_number()).second) {
                        globalInfo.conflictingSerialNumbers.insert(gen.GetSerial_number());
                    }
                }
            }
        }
    }
}


static void s_UpdateGlobalInfo(const CSeq_id& id, CHugeFileValidator::TGlobalInfo& globalInfo)
{
    switch (id.Which()) {
    case CSeq_id::e_Patent:
        globalInfo.IsPatent = true;
        break;
    case CSeq_id::e_Pdb:
        globalInfo.IsPDB = true;
        break;
    case CSeq_id::e_Other:
        globalInfo.IsRefSeq = true;
        break;
    case CSeq_id::e_Gi:
        globalInfo.CurrIsGI = true;
        break;
    default:
        break;
    }
}


static void s_UpdateGlobalInfo(const CMolInfo& molInfo, CHugeFileValidator::TGlobalInfo& globalInfo)
{
    if (! molInfo.IsSetBiomol() ||
        molInfo.GetBiomol() == CMolInfo::eBiomol_peptide) {
        return;
    }
    globalInfo.biomols.insert(molInfo.GetBiomol());
}

void CHugeFileValidator::RegisterReaderHooks(CObjectIStream& objStream, CHugeFileValidator::SGlobalInfo& m_GlobalInfo)
{
    // Set MolInfo skip and read hooks
    SetLocalSkipHook(CType<CMolInfo>(), objStream, [&m_GlobalInfo](CObjectIStream& in, const CObjectTypeInfo& type) {
        auto pMolInfo = Ref(new CMolInfo());
        type.GetTypeInfo()->DefaultReadData(in, pMolInfo);
        s_UpdateGlobalInfo(*pMolInfo, m_GlobalInfo);
    });

    SetLocalReadHook(CType<CMolInfo>(), objStream, [&m_GlobalInfo](CObjectIStream& in, const CObjectInfo& object) {
        auto* pObject = object.GetObjectPtr();
        object.GetTypeInfo()->DefaultReadData(in, pObject);
        auto* pMolInfo = CTypeConverter<CMolInfo>::SafeCast(pObject);
        s_UpdateGlobalInfo(*pMolInfo, m_GlobalInfo);
    });

    // Set Pubdesc skip and read hooks
    SetLocalSkipHook(CType<CPubdesc>(), objStream, [&m_GlobalInfo](CObjectIStream& in, const CObjectTypeInfo& type) {
        auto pPubdesc = Ref(new CPubdesc());
        type.GetTypeInfo()->DefaultReadData(in, pPubdesc);
        s_UpdateGlobalInfo(*pPubdesc, m_GlobalInfo);
    });

    SetLocalReadHook(CType<CPubdesc>(), objStream, [&m_GlobalInfo](CObjectIStream& in, const CObjectInfo& object) {
        auto* pObject = object.GetObjectPtr();
        object.GetTypeInfo()->DefaultReadData(in, pObject);
        auto* pPubdesc = CTypeConverter<CPubdesc>::SafeCast(pObject);
        s_UpdateGlobalInfo(*pPubdesc, m_GlobalInfo);
    });

    // Set BioSource skip and read hooks
    SetLocalSkipHook(CType<CBioSource>(), objStream, [&m_GlobalInfo](CObjectIStream& in, const CObjectTypeInfo& type) {
        m_GlobalInfo.NoBioSource = false;
        type.GetTypeInfo()->DefaultSkipData(in);
    });

    SetLocalReadHook(CType<CBioSource>(), objStream, [&m_GlobalInfo](CObjectIStream& in, const CObjectInfo& object) {
        object.GetTypeInfo()->DefaultReadData(in, object.GetObjectPtr());
        m_GlobalInfo.NoBioSource = false;
    });


    SetLocalReadHook(CType<CSeq_id>(), objStream, [&m_GlobalInfo](CObjectIStream& in, const CObjectInfo& object) {
        auto* pObject = object.GetObjectPtr();
        object.GetTypeInfo()->DefaultReadData(in, pObject);
        auto* pSeqId = CTypeConverter<CSeq_id>::SafeCast(pObject);
        s_UpdateGlobalInfo(*pSeqId, m_GlobalInfo);
    });


    // Set UserObject read hook
    SetLocalReadHook(CType<CUser_object>(), objStream, [&m_GlobalInfo](CObjectIStream& in, const CObjectInfo& object) {
        auto* pObject = object.GetObjectPtr();
        object.GetTypeInfo()->DefaultReadData(in, pObject);
        auto* pUser_object = CTypeConverter<CUser_object>::SafeCast(pObject);
        if (pUser_object->IsSetType() && pUser_object->GetType().IsStr() && NStr::EqualNocase(pUser_object->GetType().GetStr(), "TpaAssembly")) {
            m_GlobalInfo.CurrTpaAssembly = true;
            m_GlobalInfo.JustTpaAssembly++;
            if (m_GlobalInfo.CurrIsGI) {
                m_GlobalInfo.TpaNoHistYesGI++;
            }
        }
    });


    // Set History skip hook
    SetLocalSkipHook(CType<CSeq_hist>(), objStream, [&m_GlobalInfo](CObjectIStream& in, const CObjectTypeInfo& type) {
        auto pSeqhist = Ref(new CSeq_hist());
        type.GetTypeInfo()->DefaultReadData(in, pSeqhist);
        if (pSeqhist->IsSetAssembly() && ! pSeqhist->GetAssembly().empty()) {
            if (m_GlobalInfo.CurrTpaAssembly) {
                m_GlobalInfo.JustTpaAssembly--;
                m_GlobalInfo.TpaAssemblyHist++;
            }
        } else if (m_GlobalInfo.CurrTpaAssembly && m_GlobalInfo.CurrIsGI) {
            m_GlobalInfo.TpaNoHistYesGI--;
        }
    });


    // Set Seq-inst skip hook
    SetLocalSkipHook(CType<CSeq_inst>(), objStream, [&m_GlobalInfo](CObjectIStream& in, const CObjectTypeInfo& type) {
        type.GetTypeInfo()->DefaultSkipData(in);
        // clear flags after set by seq-id or user object and after hist examines it
        m_GlobalInfo.CurrTpaAssembly = false;
        m_GlobalInfo.CurrIsGI        = false;
    });

    CObjectTypeInfo gbqual_info = CType<CGb_qual>();

    auto gbqual_qual_mi = gbqual_info.FindMember("qual");

    // Set Gb-qual.qual skip hook
    SetLocalSkipHook(gbqual_qual_mi, objStream,
                     [&m_GlobalInfo](CObjectIStream& in, const CObjectTypeInfo& type)
    {
        string str;
        type.GetTypeInfo()->DefaultReadData(in, &str);
        if (str == "inference") {
            m_GlobalInfo.CumulativeInferenceCount++;
        }
    });

}


static bool s_DropErrorItem(const CHugeFileValidator::TGlobalInfo& globalInfo,
                            const string&                          hugeSetId,
                            const CValidErrItem&                   item)
{
    const auto& errCode = item.GetErrIndex();

    if (errCode == eErr_SEQ_DESCR_NoPubFound) {
        const auto& msg = item.GetMsg();
        if (NStr::StartsWith(msg, "No publications anywhere on this entire record.")) {
            return ! globalInfo.NoPubsFound || (item.GetAccession() != hugeSetId);
        }
        return globalInfo.NoPubsFound &&
               NStr::StartsWith(msg, "No publications refer to this Bioseq.");
    }

    if (errCode == eErr_GENERIC_MissingPubRequirement &&
        NStr::StartsWith(item.GetMsg(), "No submission citation anywhere on this entire record.")) {
        return ! globalInfo.NoCitSubsFound || (item.GetAccession() != hugeSetId);
    }

    if (globalInfo.NoBioSource) {
        return ((errCode == eErr_SEQ_DESCR_NoSourceDescriptor &&
                 (item.GetAccession() != hugeSetId)) ||
                errCode == eErr_SEQ_DESCR_TransgenicProblem ||
                errCode == eErr_SEQ_DESCR_InconsistentBioSources_ConLocation ||
                errCode == eErr_SEQ_INST_MitoMetazoanTooLong ||
                errCode == eErr_SEQ_DESCR_NoOrgFound);
    }
    // else
    return errCode == eErr_SEQ_DESCR_NoSourceDescriptor;
}


string g_GetHugeSetIdString(const CHugeAsnReader& reader)
{
    const auto& biosets = reader.GetBiosets();
    if (biosets.size() < 2) {
        return "";
    }

    if (auto it = next(biosets.begin());
        ! CHugeAsnReader::IsHugeSet(it->m_class)) {
        return "";
    }

    const auto& bioseqs     = reader.GetBioseqs();
    const auto& firstBioseq = bioseqs.begin();
    const auto& parentIt    = firstBioseq->m_parent_set;
    int         version;
    if (parentIt->m_class != CBioseq_set::eClass_nuc_prot) {
        return s_GetIdString(firstBioseq->m_ids, &version);
    }
    // else parent set is nuc-prot set
    for (auto it = firstBioseq; it != bioseqs.end(); ++it) {
        if (s_IsNa(it->m_mol) && it->m_parent_set == parentIt) {
            return s_GetIdString(it->m_ids, &version);
        }
    }
    return s_GetIdString(firstBioseq->m_ids, &version);
}


void g_PostprocessErrors(const CHugeFileValidator::TGlobalInfo& globalInfo,
                         const string&                          hugeSetId,
                         CRef<CValidError>&                     pErrors)
{
    auto pPrunedErrors = Ref(new CValidError());
    for (auto pErrorItem : pErrors->GetErrs()) {
        if (! s_DropErrorItem(globalInfo, hugeSetId, *pErrorItem)) {
            pPrunedErrors->AddValidErrItem(pErrorItem);
        }
    }
    pErrors = pPrunedErrors;
}


void g_PostprocessErrors(const CHugeFileValidator::TGlobalInfo& globalInfo,
                         const string&                          hugeSetId,
                         list<CRef<CValidErrItem>>&             errors)
{
    auto it = remove_if(errors.begin(),
                        errors.end(),
                        [globalInfo, hugeSetId](CRef<CValidErrItem> pItem) { return pItem.Empty() || s_DropErrorItem(globalInfo, hugeSetId, *pItem); });

    errors.erase(it, errors.end());
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

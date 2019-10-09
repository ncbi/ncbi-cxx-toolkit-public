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
*
* Author:  Sergiy Gotvyanskyy, NCBI
* File Description:
*   High level reader for source qualifiers 
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbifile.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/mod_reader.hpp>
#include "src_quals.hpp"


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


const CMemorySrcFileMap::TLineMap& 
CMemorySrcFileMap::GetLineMap(void) const
{
    return m_LineMap;
}


bool CMemorySrcFileMap::Empty(void) const 
{
    return m_LineMap.empty();
}


void CMemorySrcFileMap::x_ProcessLine(const CTempString& line, TModList& mods)
{
    vector<CTempString> tokens;
    NStr::Split(line, "\t", tokens, 0);
    for (size_t i=1; i < tokens.size() && i < m_ColumnNames.size(); ++i) {
        if (!NStr::IsBlank(tokens[i])) {
            mods.emplace_back(m_ColumnNames[i], tokens[i]);
        }
    }
}



bool CMemorySrcFileMap::GetMods(const CBioseq& bioseq, TModList& mods) 
{
    mods.clear();
    if (!m_FileMapped) {
        return false;
    }


    bool retval = false;
    list<string> id_strings;
    for (const auto& pId : bioseq.GetId()) {
        string id;
        pId->GetLabel(&id, nullptr, CSeq_id::eFasta);
        NStr::ToLower(id);
        id_strings.push_back(id);

        id.clear();
        pId->GetLabel(&id, nullptr, CSeq_id::eFastaContent);
        NStr::ToLower(id);
        id_strings.push_back(id);
        if (pId->IsGeneral()) {
            string db, tag;
            NStr::SplitInTwo(id, "|", db, tag);
            id_strings.push_back(tag);
        }
    }

    for (const auto& id : id_strings) {
        auto it = 
            find_if(m_LineMap.begin(),
                    m_LineMap.end(),
                    [id](const TLineMap::value_type& entry) {
                        return (entry.first.find(id) != entry.first.end());
                    });
        if (it != m_LineMap.end()) {
            x_ProcessLine(it->second, mods);
            m_LineMap.erase(it);
            retval = true;
        }
    }

    return retval;
}


void CMemorySrcFileMap::MapFile(const string& fileName, bool allowAcc)
{
    if (m_FileMapped || 
        m_pFileMap ||
        !m_LineMap.empty()) {
        return;
    }

    m_pFileMap.reset(new CMemoryFileMap(fileName));

    size_t fileSize = m_pFileMap->GetFileSize();
    const char* ptr = (const char*)m_pFileMap->Map(0, fileSize);
    const char* end = ptr + fileSize;


    while (ptr < end)
    {
        // search for next non empty line
        if (*ptr == '\r' || *ptr == '\n') {
            ++ptr;
            continue;
        }

        const char* start = ptr;
        // search for end of line
        const char* endline = (const char*)memchr(ptr, '\n', end - ptr);
        if (endline == NULL) {
            endline = ptr;
            ptr = end;
        } 
        else
        {
            ptr = endline + 1;
            endline--;
        }

        while (start < endline && *endline == '\r') {
            endline--;
        }

        // compose line control structure
        if (start < endline)
        {
            CTempString line(start, endline-start+1);
            if (m_ColumnNames.empty()) {
                NStr::Split(line, "\t", m_ColumnNames, 0);
            }
            else 
            // parse regular line
            {
                CTempString idString, remainder;
                NStr::SplitInTwo(line, "\t", idString, remainder);  
                NStr::TruncateSpacesInPlace(idString);

                if (!idString.empty()) {
                    auto parseFlags = 
                        allowAcc ?
                        CSeq_id::fParse_AnyRaw | CSeq_id::fParse_ValidLocal :
                        CSeq_id::fParse_AnyLocal;

                    list<CRef<CSeq_id>> ids;
                    CSeq_id::ParseIDs(ids, idString, parseFlags);
                    // If ids is empty, we should report

                    set<string> idSet;
                    if (ids.size() == 1 &&
                        ids.front()->IsLocal() &&
                        !NStr::StartsWith(idString, "lcl|", NStr::eNocase)) {
                        string idKey = idString;
                        NStr::ToLower(idKey);
                        idSet.emplace(idKey);
                    }
                    else {
                        for (const auto& pSeqId : ids) {
                            string idKey;
                            pSeqId->GetLabel(&idKey, nullptr, CSeq_id::eFasta);
                            NStr::ToLower(idKey);
                            idSet.emplace(idKey); 
                        }
                    }
                    m_LineMap.emplace(idSet, move(line));
                }
            }
        }
    }

    if (m_ColumnNames.empty()) {
        NCBI_THROW(CArgException, eConstraint,
        "source modifiers file header line is not valid");
    }

    m_FileMapped = true;
}

static void sReportUnusedMods(
    ILineErrorListener* pEC,
    const string& fileName,
    const CTempString& seqId)
{
    if (!pEC) {
        // throw an exception
    }

    string message =
        fileName +
        " contains qualifiers for sequence id " +
        seqId +
        " , but no sequence with that id was found.";

    AutoPtr<CLineErrorEx> pErr(
        CLineErrorEx::Create(
            ILineError::eProblem_GeneralParsingError,
            eDiag_Error,
            0, 0, // code and subcode
            seqId,
            0, // lineNumber,
            message));

    pEC->PutError(*pErr);
}

void CMemorySrcFileMap::ReportUnusedIds(ILineErrorListener* pEC)
{
    if (!Empty()) {
        for (const auto& entry : m_LineMap) {
            CTempString seqId, remainder;
            NStr::SplitInTwo(entry.second, "\t", seqId, remainder);
            sReportUnusedMods(pEC, m_pFileMap->GetFileName(), NStr::TruncateSpaces_Unsafe(seqId));
        }
    }
}

static void s_AppendMods(
    const CModHandler::TModList& mods,
    string& title)
{
    for (const auto& mod : mods) {

        title.append(" ["
                + mod.GetName()
                + "="
                + mod.GetValue()
                + "]");
    }
}


static void sReportError(
        ILineErrorListener* pEC,
        EDiagSev severity,
        int subcode,
        const string& seqId,
        const string& message,
        ILineError::EProblem problemType=ILineError::eProblem_GeneralParsingError)
{
    if (!pEC) {
        // throw an exception
    }

    AutoPtr<CLineErrorEx> pErr(
            CLineErrorEx::Create(
                problemType,
                severity,
                EReaderCode::eReader_Mods,
                subcode,
                seqId,
                0, // lineNumber,
                message));

    pEC->PutError(*pErr);
}


static void sReportMissingMods(
        ILineErrorListener* pEC,
        const string& fileName,
        const CBioseq& bioseq)
{

    if (!pEC) {
        // throw an exception
    }

    string seqId = bioseq.GetId().front()->AsFastaString();
    string message = 
        fileName + 
        " doesn't contain qualifiers for sequence id " +
        seqId + 
       "."; 

    AutoPtr<CLineErrorEx> pErr(
            CLineErrorEx::Create(
                ILineError::eProblem_GeneralParsingError,
                eDiag_Error,
                0, 0, // code and subcode
                seqId,
                0, // lineNumber,
                message));

    pEC->PutError(*pErr);
}


static void s_PreprocessNoteMods(CModHandler::TModList& mods)
{
    for (auto& mod : mods) {
        if (CModHandler::GetCanonicalName(mod.GetName()) == "note"){
            string new_value = mod.GetValue();
            NStr::ReplaceInPlace(new_value, "<", "[");
            NStr::ReplaceInPlace(new_value, ">", "]");
            mod.SetValue(new_value);
        }
    }
}


void g_ApplyMods(
    unique_ptr<CMemorySrcFileMap>& namedSrcFileMap,
    const string& namedSrcFile,
    const string& defaultSrcFile,
    const string& commandLineStr,
    bool readModsFromTitle,
    bool allowAcc,
    bool isVerbose,
    ILineErrorListener* pEC,
    CSeq_entry& entry)
{
    using TModList = CModHandler::TModList;
    using TMods = CModHandler::TMods;
    
    TModList rejectedMods;
    string commandLineRemainder;
    TMods commandLineMods;

    if (!NStr::IsBlank(commandLineStr)) {
        TModList mods;
        CTitleParser::Apply(commandLineStr, mods, commandLineRemainder);
        s_PreprocessNoteMods(mods); // RW-928

        auto fReportCommandLineError = 
            [&](const CModData& /* mod */, const string& msg, EDiagSev sev,
                EModSubcode subcode) {
                return sReportError(pEC, sev, subcode, "", msg);
            };


        CModHandler mod_handler;
        mod_handler.AddMods(mods,
            CModHandler::ePreserve,
            rejectedMods, 
            fReportCommandLineError);
        s_AppendMods(rejectedMods, commandLineRemainder);
        commandLineMods = mod_handler.GetMods();
    }


    if (!NStr::IsBlank(namedSrcFile) && CFile(namedSrcFile).Exists()) {
        if (!namedSrcFileMap)
            namedSrcFileMap.reset(new CMemorySrcFileMap);
        namedSrcFileMap->MapFile(namedSrcFile, allowAcc);
    }

    CMemorySrcFileMap defaultSrcFileMap;
    if (!NStr::IsBlank(defaultSrcFile) && CFile(defaultSrcFile).Exists()) {
        defaultSrcFileMap.MapFile(defaultSrcFile, allowAcc);
    }

    auto pScope = Ref(new CScope(*CObjectManager::GetInstance()));
    auto editHandle = pScope->AddTopLevelSeqEntry(entry).GetEditHandle();

    for (CBioseq_CI bioseq_it(editHandle); bioseq_it; ++bioseq_it) {
        auto pBioseq = const_cast<CBioseq*>(
                bioseq_it->GetEditHandle()
                .GetCompleteBioseq()
                .GetPointerOrNull());

        if (pBioseq) {
            CModHandler mod_handler;
            mod_handler.SetMods(commandLineMods);
            string remainder = commandLineRemainder;

            string seqId = pBioseq->GetId().front()->AsFastaString();
            auto fReportError = 
                [&](const CModData& /* mod */, const string& msg, EDiagSev sev,
                    EModSubcode subcode) {
                    return sReportError(pEC, sev, subcode, seqId, msg);
                };
 
            if (namedSrcFileMap && !namedSrcFileMap->Empty()) {
                TModList mods;
                if (namedSrcFileMap->GetMods(*pBioseq, mods)) {
                    s_PreprocessNoteMods(mods); // RW-928
                    mod_handler.AddMods(mods, 
                            CModHandler::ePreserve, 
                            rejectedMods, 
                            fReportError);
                    s_AppendMods(rejectedMods, remainder);
                }
                else if (isVerbose) {
                    sReportMissingMods(pEC, namedSrcFile, *pBioseq);
                }
            }

            if (!defaultSrcFileMap.Empty()) {
                TModList mods;
                if (defaultSrcFileMap.GetMods(*pBioseq, mods)) {
                    s_PreprocessNoteMods(mods); // RW-928
                    mod_handler.AddMods(mods, 
                            CModHandler::ePreserve, 
                            rejectedMods, 
                            fReportError);
                    s_AppendMods(rejectedMods, remainder);
                }
                else if (isVerbose) {
                    sReportMissingMods(pEC, defaultSrcFile, *pBioseq);
                }
            }

            CRef<CSeqdesc> pTitleDesc;
            CSeq_descr::Tdata* pDescriptors = nullptr;
            if ((pBioseq->IsSetDescr() &&
                pBioseq->GetDescr().IsSet()) ||
                !NStr::IsBlank(remainder)) {
                pDescriptors = &(pBioseq->SetDescr().Set());
            }

            CSeq_descr::Tdata::iterator title_it;
            if (pDescriptors) {
                title_it = 
                    find_if(pDescriptors->begin(), pDescriptors->end(),
                        [](CRef<CSeqdesc> pDesc) { return pDesc->IsTitle(); });
                if (title_it != pDescriptors->end()) {
                    pTitleDesc = *title_it;
                    if (readModsFromTitle) { 
                        auto& title = (*title_it)->SetTitle();
                        string titleRemainder;
                        TModList mods;
                        CTitleParser::Apply(title, mods, titleRemainder);
                        title.clear(); 

                        mod_handler.AddMods(mods, 
                                CModHandler::ePreserve, 
                                rejectedMods, 
                                fReportError);
                        s_AppendMods(rejectedMods, titleRemainder);
                        remainder = titleRemainder +  remainder;
                    }
                }
            }


            CModAdder::Apply(mod_handler, *pBioseq, rejectedMods, fReportError);
            s_AppendMods(rejectedMods, remainder);


            NStr::TruncateSpacesInPlace(remainder);
            if (!remainder.empty()) {
                if (!pTitleDesc) {
                    pTitleDesc = Ref(new CSeqdesc());
                    pDescriptors->push_back(pTitleDesc);
                    pTitleDesc->SetTitle() = remainder;
                }
                else {
                    string current_title = 
                        NStr::TruncateSpaces(
                                pTitleDesc->GetTitle(), 
                                NStr::eTrunc_End);
                    pTitleDesc->SetTitle() = current_title.empty() ?
                        remainder : 
                        current_title + " " + remainder;
                }
            }
            else // remainder.empty() 
            if (pDescriptors) {
                if (title_it != pDescriptors->end() &&
                    (*title_it)->GetTitle().empty()) {
                    pDescriptors->erase(title_it);
                } 
                if (pDescriptors->empty()) {
                    pBioseq->ResetDescr();
                }
            }
        }
    }

    if (isVerbose) {
        defaultSrcFileMap.ReportUnusedIds(pEC);
    }
}

END_NCBI_SCOPE


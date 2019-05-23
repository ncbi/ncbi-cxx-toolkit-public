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
* Author:  Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   High level reader for source qualifiers 
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>

#include <util/line_reader.hpp>
#include <objtools/edit/dblink_field.hpp>

#include <objtools/readers/source_mod_parser.hpp>

#include "src_quals.hpp"
#include "struc_cmt_reader.hpp"
#include "table2asn_context.hpp"
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/mod_reader.hpp>

#include <common/test_assert.h>  /* This header must go last */
#include <unordered_set>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

namespace
{
    static const CTempString g_dblink = "DBLink";
};


bool SSrcQuals::AddQualifiers(const CBioseq::TId& ids, TModList& mods)
{
    mods.clear();
    for (const auto& pId : ids) {
        string id = pId->AsFastaString();
        NStr::ToLower(id);
        auto it = m_lines_map.find(id);
        if (it != m_lines_map.end()) {
            vector<CTempString> tokens;
            NStr::Split(it->second.m_unparsed, "\t", tokens, 0);
            for (size_t i=1; i < tokens.size() && i < columnNames.size(); ++i) {
                if (!NStr::IsBlank(tokens[i])) {
                    mods.push_back(CModData(columnNames[i], tokens[i]));
                }
            }
            m_lines_map.erase(it);
            return true;
        }
    }   

    return false;
}

void SSrcQuals::AddQualifiers(CSourceModParser& mod, const vector<CTempString>& values)
{
    // first is always skipped since it's an id 
    for (size_t i = 1; i < values.size() && i < columnNames.size(); i++) {
    //for (size_t i = 1; i < columnNames.size(); i++) {
        // Number of values cannot be greater than the number of columns
        if (!values[i].empty())
        {
            mod.AddMods(columnNames[i], values[i]);
        }
    }
}

bool SSrcQuals::AddQualifiers(objects::CSourceModParser& mod, const string& id)
{
    TLineMap::iterator it = m_lines_map.find(id);

    if (it != m_lines_map.end())
    {
        vector<CTempString> values;
        NStr::Split(it->second.m_unparsed, "\t", values, 0);


        AddQualifiers(mod, values);
        m_lines_map.erase(it);
        return true;
    }

    return false;
}

bool SSrcQuals::AddQualifiers(CSourceModParser& mod, const CBioseq::TId& ids)
{


    if (columnNames.empty())
        return false;

    for (auto pId : ids)
    {
        string id = pId->AsFastaString();
        NStr::ToLower(id);
        if (AddQualifiers(mod, id))
            return true;
    }
    return false;
}



CSourceQualifiersReader::CSourceQualifiersReader(CTable2AsnContext* context) : m_context(context)
{
}

CSourceQualifiersReader::~CSourceQualifiersReader()
{
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


void g_ApplyDeflineMods(CBioseq& bioseq) {
    if (!bioseq.IsSetDescr() || !bioseq.GetDescr().IsSet()) {
        return;
    }

    auto& descriptors = bioseq.SetDescr().Set();
    auto title_it = find_if(descriptors.begin(), descriptors.end(),
            [](CRef<CSeqdesc> pDesc) { return pDesc->IsTitle(); });
    if (title_it == descriptors.end()) {
        return;
    }

    auto& title = (*title_it)->SetTitle();
    
    string remainder;
    CModHandler::TModList mods;
    CTitleParser::Apply(title, mods, remainder);

    auto fReportError = 
        [&](const string& msg, EDiagSev sev, EModSubcode subcode) {
            // dummy function
        };


    CModHandler mod_handler;
    CModHandler::TModList rejected_mods;
    mod_handler.AddMods(mods, CModHandler::eReplace, rejected_mods);
    s_AppendMods(rejected_mods, remainder);


    CModHandler::TModList skipped_mods;
    CModAdder::Apply(mod_handler, bioseq, skipped_mods, fReportError);
    s_AppendMods(skipped_mods, remainder);

    title = remainder;
    NStr::TruncateSpacesInPlace(title);
    if (title.empty()) {
        descriptors.erase(title_it);
    }
}


bool CSourceQualifiersReader::ApplyQualifiers(objects::CSourceModParser& mod, objects::CBioseq& bioseq, objects::ILineErrorListener* listener)
{
    CSourceModParser::TModsRange mods[2];
    mods[0] = mod.FindAllMods("note");
    mods[1] = mod.FindAllMods("notes");
    for (size_t i = 0; i < 2; i++)
    {
        for (CSourceModParser::TModsCI it = mods[i].first; it != mods[i].second; it++)
        {
            NStr::ReplaceInPlace((string&)it->value, "<", "[");
            NStr::ReplaceInPlace((string&)it->value, ">", "]");
        }
    }

    mod.ApplyAllMods(bioseq);
    CSourceModParser::TMods unused_mods = mod.GetMods(CSourceModParser::fUnusedMods);
    for (auto mod : unused_mods)
    {
        if (!x_ParseAndAddTracks(bioseq, mod.key, mod.value))
        {
            if (listener)
                listener->PutError(*auto_ptr<CLineError>(
                CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
                    mod.key)));
            return false;
        }
    }
    return true;
}



bool CSourceQualifiersReader::LoadSourceQualifiers(const string& namedFile, const string& defaultFile)
{
    bool loaded = false;
    if (CFile(defaultFile).Exists())
    {
        loaded = true;
        LoadSourceQualifiers(defaultFile, 
                m_context->m_allow_accession, 
                m_QualsFromDefaultSrcFile); // .src file
    }

    if (!NStr::IsBlank(namedFile) && CFile(namedFile).Exists()) // Should report an error if it doesn't exist
    {
        loaded = true;
        LoadSourceQualifiers(namedFile, 
                m_context->m_allow_accession,
                m_QualsFromNamedSrcFile); // -src-file
    }
    return loaded;
}

void CSourceQualifiersReader::LoadSourceQualifiers(const string& filename, bool allowAcc, SSrcQuals& quals)
{
    unique_ptr<CMemoryFileMap> pFileMap(new CMemoryFileMap(filename));

    size_t fileSize = pFileMap->GetFileSize();
    const char* ptr = (const char*)pFileMap->Map(0, fileSize);
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
            string line(start, endline-start+1);
            if (quals.columnNames.empty()) {
                NStr::Split(line, "\t", quals.columnNames, 0);
            }
    
            else 
            // parse regular line
            {
                string idString, remainder;
                NStr::SplitInTwo(line, "\t", idString, remainder);
                
                if (!idString.empty()) {
                    auto parseFlags = 
                        allowAcc ?
                        CSeq_id::fParse_AnyRaw | CSeq_id::fParse_ValidLocal :
                        CSeq_id::fParse_AnyLocal;
                    auto pSeqId = 
                        Ref(new CSeq_id(idString, parseFlags));
                    string idKey = pSeqId->AsFastaString();
                    NStr::ToLower(idKey);
                    SSrcQualParsed& ref = quals.m_lines_map[idKey];
                    ref.m_id = pSeqId;
                    ref.m_unparsed = line;
                }
            }
        }
    }

    if (quals.columnNames.empty())
       NCBI_THROW(CArgException, eConstraint,
       "source modifiers file header line is not valid");
}


void g_ApplyMods(
        const string& commandLineStr,
        const string& namedSrcFile,
        const string& defaultSrcFile,
        bool allowAcc,
        CSeq_entry& entry) 
{
    using TModList = CModHandler::TModList;

    auto fReportError = 
        [&](const string& msg, EDiagSev sev, EModSubcode subcode) {
            // dummy function
        };
    
    TModList rejectedMods;
    string commandLineRemainder;
    TModList commandLineMods;
    if (!NStr::IsBlank(commandLineStr)) {
        CTitleParser::Apply(commandLineStr, commandLineMods, commandLineRemainder);
    }


    SSrcQuals qualsFromNamedFile;
    if (!NStr::IsBlank(namedSrcFile) && CFile(namedSrcFile).Exists()) {
        CSourceQualifiersReader::LoadSourceQualifiers(
                namedSrcFile, 
                allowAcc, 
                qualsFromNamedFile);
    }

    SSrcQuals qualsFromDefaultFile;
    if (!NStr::IsBlank(defaultSrcFile) && CFile(defaultSrcFile).Exists()) {
        CSourceQualifiersReader::LoadSourceQualifiers(
                defaultSrcFile, 
                allowAcc, 
                qualsFromDefaultFile);
    }
   
    auto pScope = Ref(new CScope(*CObjectManager::GetInstance()));
    auto editHandle = pScope->AddTopLevelSeqEntry(entry).GetEditHandle();

    unordered_set<string> rejectedSet;

    for (CBioseq_CI bioseq_it(editHandle); bioseq_it; ++bioseq_it) {
        auto pBioseq = const_cast<CBioseq*>(bioseq_it->GetEditHandle().GetCompleteBioseq().GetPointerOrNull());
        if (pBioseq) {
            string remainder = commandLineRemainder;
            CModHandler mod_handler(fReportError);
            mod_handler.AddMods(commandLineMods, CModHandler::eAppendReplace, rejectedMods);
            s_AppendMods(rejectedMods, remainder); 
            for (const auto& mod : rejectedMods) {
                rejectedSet.insert(CModHandler::GetCanonicalName(mod.GetName()));
            }

            if (!qualsFromNamedFile.m_lines_map.empty()) {
                TModList mods;
                qualsFromNamedFile.AddQualifiers(pBioseq->GetId(), mods);
                mod_handler.AddMods(mods, CModHandler::ePreserve, rejectedMods);
                rejectedMods.remove_if([&](const CModData& mod) 
                        {  return (rejectedSet.find(CModHandler::GetCanonicalName(mod.GetName())) != rejectedSet.end()); });
                s_AppendMods(rejectedMods, remainder);
                for (const auto& mod : rejectedMods) {
                    rejectedSet.insert(CModHandler::GetCanonicalName(mod.GetName()));
                }
            }

            if (!qualsFromDefaultFile.m_lines_map.empty()) {
                TModList mods;
                qualsFromDefaultFile.AddQualifiers(pBioseq->GetId(), mods);
                mod_handler.AddMods(mods, CModHandler::ePreserve, rejectedMods);
                rejectedMods.remove_if([&](const CModData& mod) 
                        {  return (rejectedSet.find(CModHandler::GetCanonicalName(mod.GetName())) != rejectedSet.end()); });
                s_AppendMods(rejectedMods, remainder);
                for (const auto& mod : rejectedMods) {
                    rejectedSet.insert(CModHandler::GetCanonicalName(mod.GetName()));
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
                    auto& title = (*title_it)->SetTitle();
                    string titleRemainder;
                    TModList mods;
                    CTitleParser::Apply(title, mods, titleRemainder);

                    mod_handler.AddMods(mods, CModHandler::ePreserve, rejectedMods);
                    rejectedMods.remove_if([&](const CModData& mod) 
                        {  return (rejectedSet.find(CModHandler::GetCanonicalName(mod.GetName())) != rejectedSet.end()); });
                    s_AppendMods(rejectedMods, titleRemainder);
                    for (const auto& mod : rejectedMods) {
                        rejectedSet.insert(CModHandler::GetCanonicalName(mod.GetName()));
                    }
                    remainder = titleRemainder +  remainder;
                }
            }
            CModAdder::Apply(mod_handler, *pBioseq, rejectedMods, fReportError);
            rejectedMods.remove_if([&](const CModData& mod) 
                    {  return (rejectedSet.find(CModHandler::GetCanonicalName(mod.GetName())) != rejectedSet.end()); });
            s_AppendMods(rejectedMods, remainder);
            for (const auto& mod : rejectedMods) {
                rejectedSet.insert(CModHandler::GetCanonicalName(mod.GetName()));
            }

            NStr::TruncateSpacesInPlace(remainder);
            if (!remainder.empty()) {
                if (!pTitleDesc) {
                    pTitleDesc = Ref(new CSeqdesc());
                    pDescriptors->push_back(pTitleDesc);
                }
                pTitleDesc->SetTitle() = remainder;
            }
            else // remainder.empty() 
            if (pDescriptors) {
                if (title_it != pDescriptors->end()) {
                    pDescriptors->erase(title_it);
                } 
                if (pDescriptors->empty()) {
                    pBioseq->ResetDescr();
                }
            }
        }
    }
}


void CSourceQualifiersReader::ProcessSourceQualifiers(CSeq_entry& entry)
{

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_EditHandle h_entry = scope.AddTopLevelSeqEntry(entry).GetEditHandle();

    


    // apply for all sequences
    for (CBioseq_CI bioseq_it(h_entry); bioseq_it; ++bioseq_it)
    {
        CSourceModParser mod;

        CBioseq* pBioseq = (CBioseq*)bioseq_it->GetEditHandle().GetCompleteBioseq().GetPointerOrNull();

        if (!m_context->mCommandLineMods.empty())
           mod.ParseTitle(m_context->mCommandLineMods, CConstRef<CSeq_id>(pBioseq->GetFirstId()));

        bool handled = m_QualsFromNamedSrcFile.AddQualifiers(mod, pBioseq->GetId());
        handled |= m_QualsFromDefaultSrcFile.AddQualifiers(mod, pBioseq->GetId());


        if (!ApplyQualifiers(mod, *pBioseq, m_context->m_logger))
          NCBI_THROW(CArgException, eConstraint,
             "there are found unrecognised source modifiers");



        if (m_context->m_verbose && !handled)
        {
            m_context->m_logger->PutError(*auto_ptr<CLineError>(
                CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
                    "Source qualifiers file doesn't contain qualifiers for sequence id " + pBioseq->GetId().front()->AsFastaString())));
        }
    }

    // Since entries
    if (m_context->m_verbose)
    {

            for (auto line : m_QualsFromDefaultSrcFile.m_lines_map)
            {
                m_context->m_logger->PutError(*auto_ptr<CLineError>(
                    CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
                    "File " + m_context->m_current_file + " doesn't contain sequence with id " + line.first)));
            }


            for (auto line : m_QualsFromNamedSrcFile.m_lines_map)
            {
                m_context->m_logger->PutError(*auto_ptr<CLineError>(
                    CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
                    "File " + m_context->m_current_file + " doesn't contain sequence with id " + line.first)));
            }
    }
}


bool CSourceQualifiersReader::x_ParseAndAddTracks(CBioseq& container, const string& name, const string& value)
{
    if (name == "ft-url" ||
        name == "ft-map")
        CTable2AsnContext::AddUserTrack(container.SetDescr(), "FileTrack", "Map-FileTrackURL", value);
    else
    if (name == "ft-mod")
        CTable2AsnContext::AddUserTrack(container.SetDescr(), "FileTrack", "BaseModification-FileTrackURL", value);
    else
        return false;

    return true;
}


END_NCBI_SCOPE


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

#if 0
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objtools/readers/source_mod_parser.hpp>


#endif

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>

#include <util/line_reader.hpp>
#include <objtools/edit/dblink_field.hpp>

#include <objtools/readers/source_mod_parser.hpp>

#include "src_quals.hpp"
#include "struc_cmt_reader.hpp"
#include "table2asn_context.hpp"

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

void TSrcQuals::x_AddQualifiers(CSourceModParser& mod, const vector<CTempString>& values)
{
    // first is always skipped since it's an id 
    for (size_t i = 0; i < values.size() && i < m_cols.size(); i++)
    {
        if (!values[i].empty())
        {
            mod.AddMods(m_cols[i], values[i]);
        }
    }
}

bool TSrcQuals::AddQualifiers(objects::CSourceModParser& mod, const string& id)
{
    TLineMap::const_iterator it = m_lines_map.find(id);
    if (it != m_lines_map.end())
    {
        vector<CTempString> values;
        NStr::Split(it->second, "\t", values);

        x_AddQualifiers(mod, values);
        return true;
    }

    return false;
}

bool TSrcQuals::AddQualifiers(CSourceModParser& mod, const CBioseq& id)
{
    if (m_cols.empty())
        return false;

    ITERATE(CBioseq::TId, id_it, id.GetId())
    {
        string id = (**id_it).AsFastaString();
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

void CSourceQualifiersReader::x_ApplyAllQualifiers(objects::CSourceModParser& mod, objects::CBioseq& bioseq)
{
    // first is always skipped since it's an id 
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
    NON_CONST_ITERATE(CSourceModParser::TMods, mod, unused_mods)
    {
        if (NStr::CompareNocase(mod->key, "bioproject") == 0)
            edit::CDBLink::SetBioProject(CTable2AsnContext::SetUserObject(bioseq.SetDescr(), "DBLink"), mod->value);
        else
            if (NStr::CompareNocase(mod->key, "biosample") == 0)
                edit::CDBLink::SetBioSample(CTable2AsnContext::SetUserObject(bioseq.SetDescr(), "DBLink"), mod->value);

        x_ParseAndAddTracks(bioseq, mod->key, mod->value);
    }
}

void CSourceQualifiersReader::LoadSourceQualifiers(const string& filename, const string& opt_map_filename)
{
    if (CFile(filename).Exists())
        x_LoadSourceQualifiers(m_quals[0], filename, opt_map_filename);

    if (!m_context->m_single_source_qual_file.empty())
        x_LoadSourceQualifiers(m_quals[1], m_context->m_single_source_qual_file, opt_map_filename);
}

void CSourceQualifiersReader::x_LoadSourceQualifiers(TSrcQuals& quals, const string& filename, const string& opt_map_filename)
{
    quals.m_id_col = 0;
    quals.m_filemap.reset(new CMemoryFileMap(filename));

    size_t sz = quals.m_filemap->GetFileSize();
    const char* ptr = (const char*)quals.m_filemap->Map(0, sz);
    const char* end = ptr + sz;
    while (ptr < end)
    {
        if (*ptr == '\r' || *ptr == '\n')
            continue;

        const char* start = ptr;

        const char* endline = (const char*)memchr(ptr, '\n', end - ptr);
        if (endline == 0) // this is the last line
        {
            endline = ptr;
            ptr = end;
        }
        else
        {
            ptr = endline + 1;
            endline--;
        }

        while (start < endline && *endline == '\r')
            endline--;

        if (start < endline)
        {
            CTempString newline(start, endline - start + 1);
            if (quals.m_cols.empty())
            {
                NStr::Split(newline, "\t", quals.m_cols);
                if (quals.m_cols.empty())
                    NCBI_THROW(CArgException, eConstraint,
                    "source modifiers file header line is not valid");

                if (!opt_map_filename.empty())
                {
                    ITERATE(vector<CTempString>, it, quals.m_cols)
                    {
                        if (*it == "id" ||
                            *it == "seqid" ||
                            NStr::CompareNocase(*it, "Filename") == 0 ||
                            NStr::CompareNocase(*it, "File name") == 0)
                        {
                            quals.m_id_col = (it - quals.m_cols.begin());
                            break;
                        }
                    }
                }
            }
            else
            {
                const char* endid = (const char*)memchr(start, '\t', endline - start);
                if (endid)
                {
                    CTempString id_text(start, endid - start);
                    if (opt_map_filename.empty())
                    {
                        CSeq_id id(id_text, CSeq_id::fParse_AnyLocal);
                        quals.m_lines_map[id.AsFastaString()] = newline;
                    }
                    else
                    {
                        quals.m_lines_map[id_text] = newline;
                    }
                }
            }
        }
    }

}

void CSourceQualifiersReader::ProcessSourceQualifiers(CSeq_entry& entry, const string& opt_map_filename)
{
    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();

    CSeq_entry_EditHandle h_entry = scope.AddTopLevelSeqEntry(entry).GetEditHandle();

    // apply for all sequences
    for (CBioseq_CI bioseq_it(h_entry); bioseq_it; ++bioseq_it)
    {
        CSourceModParser mod;

        CBioseq* dest = (CBioseq*)bioseq_it->GetEditHandle().GetCompleteBioseq().GetPointerOrNull();

        if (!m_context->m_source_mods.empty())
           mod.ParseTitle(m_context->m_source_mods, CConstRef<CSeq_id>(dest->GetFirstId()));

        if (opt_map_filename.empty())
        {
            m_quals[0].AddQualifiers(mod, *dest);
            m_quals[1].AddQualifiers(mod, *dest);
        }
        else
        {
            m_quals[0].AddQualifiers(mod, opt_map_filename);
            m_quals[1].AddQualifiers(mod, opt_map_filename);
        }

        x_ApplyAllQualifiers(mod, *dest);
    }
}


void CSourceQualifiersReader::x_AddQualifiers(CSourceModParser& mod, const string& filename)
{
    CRef<ILineReader> reader(ILineReader::New(filename));

    vector<CTempString> cols;

    size_t filename_id = string::npos;
    while (!reader->AtEOF())
    {
        reader->ReadLine();
        // First line is a collumn definitions
        CTempString current = reader->GetCurrentLine();
        if (current.empty())
            continue;

        if (cols.empty())
        {
            NStr::Split(current, "\t", cols);
#if 0
            if (!opt_map_filename.empty())
            {
                ITERATE(vector<CTempString>, it, cols)
                {
                    if (*it == "id" ||
                        *it == "seqid" ||
                        NStr::CompareNocase(*it, "Filename") == 0 ||
                        NStr::CompareNocase(*it, "File name") == 0)
                    {
                        filename_id = (it - cols.begin());
                        break;
                    }
                }
            }
#endif
            if (cols.empty())
                NCBI_THROW(CArgException, eConstraint,
                "source modifiers file header line is not valid");
            continue;
        }

        if (current.empty())
            continue;

        // Each line except first is a set of values, first collumn is a sequence id
        vector<CTempString> values;
        NStr::Split(current, "\t", values);
#if 0
        string id;

        if (opt_map_filename.empty())
        {
            id = values[0];
        }
        else
        {
            if (filename_id < values.size())
                id = values[filename_id];
        }
#endif

        //x_AddQualifiers(mod, cols, values);
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


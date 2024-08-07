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
* Authors: Sergiy Gotvyanskyy
*
* File Description:
*   Fast, indexed accessible five column feature table reader
*
*/
#include <ncbi_pch.hpp>
#include "annot_match.hpp"
#include <objtools/huge_asn/huge_file.hpp>
#include <objtools/readers/readfeat.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <util/memory_streambuf.hpp>

namespace {
struct CFast5colReaderIndexer
{
    size_t m_current_pos = 0;
    std::string_view m_whole_file;
    size_t m_current_lineno = 0;
    size_t m_prev_line_pos = 0;
    ncbi::CFast5colReader::CMemBlockInfo m_current_block;
    std::list<ncbi::CFast5colReader::CMemBlockInfo>* m_blocks = nullptr;
    size_t GetNextLine(std::string_view& next);
    void MakeNewBlock(size_t pos, std::string_view seqid);
};

void CFast5colReaderIndexer::MakeNewBlock(size_t pos, std::string_view seqid)
{
    if (m_current_block.line_no) {
        m_current_block.size = pos - m_current_block.start_pos;
        m_blocks->push_back(m_current_block);
        m_current_block.index++;
        m_current_block.start_pos = pos;
    }
    m_current_block.seqid = seqid;
    m_current_block.line_no = m_current_lineno;
}

size_t CFast5colReaderIndexer::GetNextLine(std::string_view& next_line)
{
    if (m_current_pos>=m_whole_file.size())
        return std::string_view::npos;

    auto nl_pos = m_whole_file.find('\n', m_current_pos);
    if (nl_pos == m_whole_file.npos)
        nl_pos = m_whole_file.size()-1;
    auto cr_pos = nl_pos;
    while (cr_pos>0 && m_whole_file[cr_pos-1]=='\r')
    {
        cr_pos--;
    }
    next_line = m_whole_file.substr(m_current_pos, cr_pos-m_current_pos);
    size_t start = m_current_pos;
    m_current_pos = nl_pos + 1;

    ++m_current_lineno;
    return start;
}

}

BEGIN_NCBI_SCOPE

using namespace objects;

CFast5colReader::CFast5colReader() {}
CFast5colReader::~CFast5colReader() {}

void CFast5colReader::Init(const std::string& genome_center_id, long reader_flags, TLogger* logger)
{
    m_reader_flags = reader_flags;
    m_logger = logger;
    m_seqid_parse_flags = CSeq_id::fParse_RawText | CSeq_id::fParse_ValidLocal | CSeq_id::fParse_PartialOK;

    if (genome_center_id.empty())
        m_seqid_prefix.clear();
    else {
        m_seqid_prefix = "gnl|" + genome_center_id + ":";
        NStr::ToLower(m_seqid_prefix);
    }
}

void CFast5colReader::Open(const std::string& filename)
{
    auto file = std::make_unique<TFile>();
    file->Open(filename, nullptr);
    Open(std::move(file));
}

void CFast5colReader::Open(std::unique_ptr<TFile> file)
{
    m_hugefile = std::move(file);
    x_IndexFile(std::string_view(m_hugefile->m_memory, m_hugefile->m_filesize));
}

void CFast5colReader::Open(std::string_view memory)
{
    x_IndexFile(memory);
}

CFast5colReader::TRange CFast5colReader::x_FindAnnots(CRef<CSeq_id> id) const
{
    std::list<CRef<CSeq_annot>> annots;

    std::string label_nover, label_with_ver;

    id->GetLabel(&label_with_ver, CSeq_id::eBoth, CSeq_id::fLabel_Version);
    NStr::ToLower(label_with_ver);

    auto range = m_blocks_map.equal_range(label_with_ver);
    if (range.first == range.second) {
        id->GetLabel(&label_nover, CSeq_id::eBoth, 0);
        NStr::ToLower(label_nover);
        if (label_nover != label_with_ver)
            range = m_blocks_map.equal_range(label_nover);
    }
    return range;
}


std::list<CRef<CSeq_annot>> CFast5colReader::GetAndUseAnnot(CRef<CSeq_id> seqid)
{
    std::list<CRef<CSeq_annot>> annots;

    auto range = x_FindAnnots(seqid);

    for (auto it = range.first; it != range.second; ++it)
    {
        if (m_used_annots.reset(it->second->index)) {

            CMemoryLineReader line_reader(m_memory.data()+it->second->start_pos, it->second->size);

            auto annot = CFeature_table_reader::ReadSequinFeatureTable(
                line_reader, m_reader_flags, m_logger);

            if (annot && annot->IsFtable() && annot->GetData().GetFtable().size() > 0) {
                annots.push_back(annot);
            }
        }
    }

    return annots;
}

std::list<CRef<objects::CSeq_id>> CFast5colReader::FindAnnots(CRef<objects::CSeq_id> seqid) const
{
    std::list<CRef<CSeq_id>> seqids;

    auto range = x_FindAnnots(seqid);

    for (auto it = range.first; it != range.second; ++it)
    {
        CBioseq::TId ids;
        CSeq_id::ParseIDs(ids, it->second->seqid, CSeq_id::fParse_RawText | CSeq_id::fParse_ValidLocal | CSeq_id::fParse_PartialOK);
        seqids.splice(seqids.end(), ids);
    }
    return seqids;

}

void CFast5colReader::x_IndexFile(std::string_view memory)
{
    m_memory = memory;
    CStopWatch watches;
    watches.Start();
    CFast5colReaderIndexer state;
    state.m_blocks = &m_blocks;
    state.m_whole_file = memory;

    std::string_view next_line;
    size_t start;
    while((start = state.GetNextLine(next_line)) != std::string_view::npos)
    {
        if (next_line[0]=='>') {
            CTempStringEx defline(next_line);
            CTempStringEx seqid;
            CTempStringEx annot_name;
            if (CFeature_table_reader::ParseInitialFeatureLine(defline, seqid, annot_name))
            {
                state.MakeNewBlock(start, seqid);
            }
        }
    }
    state.MakeNewBlock(memory.size(), {});

    for (auto& rec: m_blocks)
    {
        CBioseq::TId ids;
        CSeq_id::ParseIDs(ids, rec.seqid, m_seqid_parse_flags);
        for (auto id: ids) {
            std::string label_nover, label_with_ver, label_only;

            id->GetLabel(&label_with_ver, CSeq_id::eBoth, CSeq_id::fLabel_Version);
            NStr::ToLower(label_with_ver);
            id->GetLabel(&label_nover, CSeq_id::eBoth, 0);
            NStr::ToLower(label_nover);
            id->GetLabel(&label_only, CSeq_id::eContent, CSeq_id::fLabel_Version);
            NStr::ToLower(label_only);

            m_blocks_map.emplace(std::make_pair(label_with_ver, &rec));
            if (label_nover != label_with_ver)
                m_blocks_map.emplace(std::make_pair(label_nover, &rec));

            if (!m_seqid_prefix.empty()) {
                if (rec.seqid.find('|') == std::string_view::npos || id->IsLocal())
                    m_blocks_map.emplace(std::make_pair(m_seqid_prefix + label_only, &rec));
            }

            if (rec.seqid.find('|') == std::string_view::npos && !id->IsLocal())
                m_blocks_map.emplace(std::make_pair("lcl|" + label_only, &rec));

        }
    }
    m_used_annots.Init(m_blocks.size());

    watches.Stop();

}

END_NCBI_SCOPE

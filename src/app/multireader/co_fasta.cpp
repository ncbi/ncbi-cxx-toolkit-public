#include <ncbi_pch.hpp>
#include "co_fasta.hpp"
#include <objtools/huge_asn/huge_file.hpp>
#include "impl/co_readers.hpp"


using namespace ncbi::objtools::hugeasn;
using enum CFastaSource::ParseFlags;

BEGIN_LOCAL_NAMESPACE;

size_t x_FindNextDefline(std::string_view blob, size_t start_pos)
{
    while(true) {
        start_pos = blob.find('>', start_pos);
        if (start_pos == std::string_view::npos || blob[start_pos-1] == '\n')
            return start_pos;

        start_pos++;
    }
}
std::optional<size_t> x_CheckGapEstimation(std::string_view line)
{
    if (line.size()>1 && line[0] == '>' && line[1] == '?') {
        size_t gapsize = 0;
        ncbi::NStr::StringToNumeric(line.substr(2), &gapsize);
        return gapsize;
    } else {
        return std::nullopt;
    }
}

size_t x_FindNextTrueDefline(std::string_view blob, bool is_delta_seq)
{
    size_t current_pos = 0;

    bool is_data_segment = true;
    std::string_view defline;
    while(true)
    {
        current_pos = x_FindNextDefline(blob, current_pos);

        while (current_pos != std::string_view::npos &&
               current_pos + 1 <= blob.size() &&
               blob[current_pos+1] == '?') {
            current_pos = impl::SkipLine(blob, current_pos, defline);
            is_data_segment = false;
        }

        if (current_pos == std::string_view::npos)
            return blob.size();

        if (is_data_segment || !is_delta_seq)
            return current_pos;

        current_pos = impl::SkipLine(blob, current_pos, defline);
        is_data_segment = true;
    }
}

bool x_FillLiteralBuffer(size_t max_allowed, std::string& buffer, std::string_view& lit)
{
    if (buffer.size() < max_allowed && lit.size()) {
        size_t can_add = max_allowed - buffer.size();
        can_add = std::min(can_add, lit.size());
        auto to_copy = lit.substr(0, can_add);
        buffer.append(to_copy);
        lit = lit.substr(can_add);
    }

    return buffer.size() == max_allowed;
}

END_LOCAL_NAMESPACE;

BEGIN_SCOPE(ncbi::objtools::hugeasn)

Generator<std::pair<size_t, std::string_view>>
BasicReadLinesGenerator(size_t start_line, std::string_view blob)
{
    using TView = std::string_view;
    if (blob.empty())
        co_return;

    TView current = blob;

    size_t lineno = start_line;

    if (current[0] == '\n') {
        co_yield std::make_pair(lineno++, TView{});
        current = current.substr(1);
    }
    while(!current.empty()) {
        auto next_nl = current.find('\n');
        if (next_nl == TView::npos) {
            next_nl = current.size();
        }
        auto newline_size = (current[next_nl-1] == '\r') ? next_nl-1 : next_nl;
        auto to_yield = current.substr(0, newline_size);
        co_yield std::make_pair(lineno++, to_yield);
        current = current.substr(next_nl+1);
    }
    co_return;
}

class CFastaSourceImpl
{
public:
    using TFile = CFastaSource::TFile;
    using TFastaBlob = CFastaSource::TFastaBlob;
    using TView = CFastaSource::TView;
    using ParseFlagsSet = CFastaSource::ParseFlagsSet;

    CFastaSourceImpl(const std::string& filename);
    CFastaSourceImpl(TFile file);
    CFastaSourceImpl(TView blob) : m_blob {blob} {}
    static Generator<std::shared_ptr<TFastaBlob>> ReadBlobsQuick(std::shared_ptr<CFastaSourceImpl> self, ParseFlagsSet flags);
    static Generator<std::shared_ptr<TFastaBlob>> ReadBlobs(std::shared_ptr<CFastaSourceImpl> self, ParseFlagsSet flags);
    ~CFastaSourceImpl()
    {
    }
    ParseFlagsSet Flags() const { return m_flags; }

private:
    static void xTrimBlob(TFastaBlob& blob, TView current);
    TFile m_file;
    TView m_blob;
    std::atomic<bool> m_blobs_loaded;
    std::atomic<bool> m_is_loading;
    ParseFlagsSet m_flags;
};

CFastaSourceImpl::CFastaSourceImpl(const std::string& filename)
{
    m_file.reset(new TFile::element_type);
    m_file->Open(filename, nullptr);
    if (m_file->m_format != CFormatGuess::eFasta)
        throw std::runtime_error("Wrong file format used");
    m_blob = TView(m_file->m_memory, m_file->m_filesize);
}

CFastaSourceImpl::CFastaSourceImpl(CFastaSource::TFile file) : m_file(file)
{
    if (m_file->m_format != CFormatGuess::eFasta)
        throw std::runtime_error("Wrong file format used");
    m_blob = TView(m_file->m_memory, m_file->m_filesize);
}

Generator<std::shared_ptr<CFastaSourceImpl::TFastaBlob>>
CFastaSourceImpl::ReadBlobsQuick(std::shared_ptr<CFastaSourceImpl> self, ParseFlagsSet flags)
{
    self->m_flags = flags;

    std::shared_ptr<TFastaBlob> prev_blob;

    auto current = self->m_blob;

    while (!current.empty()) {
        if (current[0] != '>') {
            throw std::runtime_error("No defline start found");
        }

        TView defline;
        auto next_nl = impl::SkipLine(current, 0, defline);
        if (next_nl == TView::npos) {
            throw std::runtime_error("Defline end was not found");
        }

        current = current.substr(next_nl);

        auto next_defline_pos = x_FindNextTrueDefline(current, flags.test(IsDeltaSeq));
        auto next_blob = current.substr(0, next_defline_pos);
        auto to_yield = std::make_shared<TFastaBlob> (self);
        to_yield->m_defline = defline;
        to_yield->m_data = next_blob;
        co_yield to_yield;
        current = current.substr(next_defline_pos);
    }
    co_return;
}

void CFastaSourceImpl::xTrimBlob(TFastaBlob& blob, TView current)
{
    auto it = current.begin();
    size_t len = std::distance(blob.m_data.begin(), it);
    blob.m_data = blob.m_data.substr(0, len);

    len = std::distance(blob.m_blob.begin(), it);
    blob.m_blob = blob.m_blob.substr(0, len);
}


Generator<std::shared_ptr<CFastaSourceImpl::TFastaBlob>>
CFastaSourceImpl::ReadBlobs(std::shared_ptr<CFastaSourceImpl> self, ParseFlagsSet flags)
{
    self->m_flags = flags;

    std::shared_ptr<TFastaBlob> prev_blob;

    auto current = self->m_blob;

    TFastaBlob blob{self};

    size_t lineno = 0;
    blob.m_blob = blob.m_data = current;

    bool inside_deltaseq = false;

    while (!current.empty()) {
        TView line;
        auto nextnl = impl::SkipLine(current, 0, line);
        lineno++;
        if (!line.empty() && line[0] == '>') {
            if (line.size()>1 && line[1] == '?') {
                inside_deltaseq = true;
                // do nothing
            } else {
                if (flags.test(IsDeltaSeq) && inside_deltaseq) {
                    inside_deltaseq = false;
                } else {
                    inside_deltaseq = false;
                    if (blob.m_numlines>0) {
                        xTrimBlob(blob, current);
                        co_yield std::make_shared<TFastaBlob>(blob);
                    }
                    blob.m_defline = line;
                    blob.m_blob = current;
                    current = current.substr(nextnl);
                    blob.m_data = current;
                    blob.m_lineno_handicap = lineno-1;
                    blob.m_seq_length = 0;
                    blob.m_numlines = 1;
                    continue;
                }
            }
        }
        if (line.size()>0 && line[0] != '>') {
            blob.m_seq_length += line.size();
        }
        blob.m_numlines++;
        current = current.substr(nextnl);
    }
    if (blob.m_numlines>0) {
        xTrimBlob(blob, current);
        co_yield std::make_shared<TFastaBlob>(blob);
    }
    co_return;
}

Generator<std::shared_ptr<CFastaSource::TFastaBlob>> CFastaSource::ReadBlobsQuick()
{
    return CFastaSourceImpl::ReadBlobsQuick(std::move(m_impl), m_flags);
}

Generator<std::shared_ptr<CFastaSource::TFastaBlob>> CFastaSource::ReadBlobs()
{
    return CFastaSourceImpl::ReadBlobs(std::move(m_impl), m_flags);
}

CFastaSource::CFastaSource(const std::string& filename)
{
    m_impl = std::make_shared<CFastaSourceImpl>(filename);
}

CFastaSource::CFastaSource(TFile file)
{
    m_impl = std::make_shared<CFastaSourceImpl>(file);
}

CFastaSource::CFastaSource(TView blob)
{
    m_impl = std::make_shared<CFastaSourceImpl>(blob);
}

CFastaSource::ParseFlagsSet CFastaSource::TFastaBlob::Flags() const
{
    return m_impl->Flags();
}

END_SCOPE(ncbi::objtools::hugeasn)


#include <ncbi_pch.hpp>
#include "co_gff.hpp"
#include <objtools/huge_asn/huge_file.hpp>
#include <util/regexp/ctre/ctre.hpp>
#include <atomic>
#include <forward_list>
#include <optional>
#include <util/compile_time.hpp>
#include "impl/co_readers.hpp"


BEGIN_LOCAL_NAMESPACE;

template<size_t...Ints>
constexpr auto MakeMapping()
{
    using result_type = std::array<size_t, std::max({Ints...}) + 1>;
    result_type result = {};
    for (auto& rec: result) {
        rec = std::numeric_limits<size_t>::max();
    }
    size_t pos = 0;
    for (auto rec: {Ints...}) {
        result[rec] = pos;
        pos++;
    }
    return result;
}


template<typename _Finder, size_t current, auto indices, typename _Input, typename _Output>
void ExtractTokensToTupleImpl(_Input&& input, _Output&& output)
{
    constexpr auto pos = indices[current];

    auto nextpos = _Finder{}(input);
    if (nextpos == std::string_view::npos)
        nextpos = input.size();

    if constexpr (pos != std::numeric_limits<size_t>::max()) {
        std::get<pos>(output) = input.substr(0, nextpos);
    }
    if constexpr(current + 1 < indices.size()) {
        if (nextpos != input.size()) {
            ExtractTokensToTupleImpl<_Finder, current + 1, indices>(input.substr(nextpos + 1), output);
        } else {
            throw std::runtime_error("split_string: failed to fill all elements");
        }
    }
}

template<typename _Finder, auto indices, typename _Input, typename _Output>
void ExtractTokensToTuple(_Input&& input, _Output&& output)
{
    ExtractTokensToTupleImpl<_Finder, 0, indices>(std::forward<_Input>(input), std::forward<_Output>(output));
}

template<typename _CharType, _CharType _C>
struct string_finder
{
    std::size_t operator()(std::basic_string_view<_CharType> s) const
    {
        return s.find(_C);
    }
};

template<ct::fixed_string _RE>
struct regex_finder
{
    using _CharType = typename decltype(_RE)::char_type;
    std::size_t operator()(std::basic_string_view<_CharType> s) const
    {
        auto m = ctre::search<_RE.get_array()>(s);
        if (m)
            return std::distance(s.begin(), m.begin());
        else
            return std::basic_string_view<_CharType>::npos;
    }
};

template<ct::fixed_string _RE, size_t...Ints, typename...TArgs>
void extract_tokens(
    std::integer_sequence<size_t, Ints...>,
    std::basic_string_view<typename decltype(_RE)::char_type> input,
    TArgs&&...tokens)
{
    static_assert(sizeof...(Ints) == sizeof...(TArgs));

    using _Finder = std::conditional_t<_RE.size() == 1,
        string_finder<typename decltype(_RE)::char_type, _RE[0]>,
        regex_finder<_RE>>;

    constexpr auto indices = MakeMapping<Ints...>();

    std::tuple<std::decay_t<TArgs>&...> output(std::ref(tokens)...);

    ExtractTokensToTuple<_Finder, indices>(input, output);
}

template<ct::fixed_string _RE, size_t...Ints, typename...TArgs>
void extract_tokens(
    std::basic_string_view<typename decltype(_RE)::char_type> input,
    TArgs&&...tokens)
{
    extract_tokens<_RE>(std::index_sequence<Ints...>{}, input, std::forward<TArgs>(tokens)...);
}


END_LOCAL_NAMESPACE;

BEGIN_SCOPE(ncbi::objtools::hugeasn)

class CGffSourceImpl
{
public:
    using TFile = CGffSource::TFile;
    using TGffBlob = CGffSource::TGffBlob;
    using TView = CGffSource::TView;

    CGffSourceImpl(const std::string& filename);
    CGffSourceImpl(TFile file);
    CGffSourceImpl(TView blob) : m_whole_blob {blob} {}
    static CGffSource::TGenerator ReadBlobs(std::shared_ptr<CGffSourceImpl> self);
    ~CGffSourceImpl()
    {
    }

private:

    struct TGffLine
    {
        TView m_line;
        TView m_seqid;
        TView m_parent;
        TView m_id;
    };

    struct TCompareNodes
    {
        bool operator ()(const TGffLine* l, const TGffLine* r) const
        {
            return operator()(*l, *r);
        }
        bool operator ()(std::forward_list<TGffLine>::const_iterator l, std::forward_list<TGffLine>::const_iterator r) const
        {
            return operator()(*l, *r);
        }
        bool operator ()(const TGffLine& l, const TGffLine& r) const
        {
            int cmp;

            cmp = l.m_seqid.compare(r.m_seqid);
            if (cmp != 0)
                return cmp<0;

            cmp = l.m_parent.compare(r.m_parent);
            if (cmp != 0)
                return cmp<0;

            cmp = l.m_id.compare(r.m_id);
            if (cmp != 0)
                return cmp<0;

            return l.m_line.compare(r.m_line) < 0;
        }
    };

    template<typename _It>
    static std::vector<_It> x_SortGffLines(size_t size, _It begin, _It end);

    template<typename _It>
    static std::shared_ptr<TGffBlob> x_PopulateBlob(std::shared_ptr<CGffSourceImpl> self, _It begin, _It end);

    static void x_GetColumn1and9(TView line, TView& col1, TView& col9);
    static bool x_GetParentAndID(TView col9, TView& id, TView& parent);
    static bool x_GetGtfGeneId(TView col9, TView& geneid);

    void x_LoadGffLines(
        std::vector<TView>& comments,
        std::size_t& all_lines_size,
        std::forward_list<TGffLine>& all_lines,
        TView& fasta) const;

    void x_OpenFile();

    TFile m_file;
    TView m_whole_blob;
};

CGffSourceImpl::CGffSourceImpl(const std::string& filename)
    : m_file{std::make_shared<TFile::element_type>()}
{
    m_file->Open(filename, nullptr);
    x_OpenFile();
}

CGffSourceImpl::CGffSourceImpl(CGffSource::TFile file)
    : m_file{file}
{
    x_OpenFile();
}

void CGffSourceImpl::x_OpenFile()
{
    if (!ct::inline_bitset<CFormatGuess::eGff3, CFormatGuess::eGtf>.test(m_file->m_format))
        throw std::runtime_error("Wrong file format used");
    m_whole_blob = TView(m_file->m_memory, m_file->m_filesize);
}

template<typename _It>
std::shared_ptr<CGffSourceImpl::TGffBlob> CGffSourceImpl::x_PopulateBlob(std::shared_ptr<CGffSourceImpl> self, _It begin, _It end)
{
    std::shared_ptr<TGffBlob> new_blob;
    auto size = std::distance(begin, end);
    if (size>0) {
        new_blob = std::make_shared<TGffBlob>(self);
        new_blob->m_blob_type = CGffSource::blob_type::lines;
        new_blob->m_lines.reserve(size);
        new_blob->m_seqid = (**begin).m_seqid;
        for_each(begin, end, [b = new_blob](auto rec) {
            b->m_lines.push_back(rec->m_line);
        });
    }
    return new_blob;
}

template<typename _It>
std::vector<_It> CGffSourceImpl::x_SortGffLines(size_t size, _It begin, _It end)
{
    std::multiset<_It, TCompareNodes> s_set;
    for(_It node = begin; node != end; node++) {
        s_set.emplace(node);
    }

    std::vector<_It> sorted; sorted.reserve(size);

    for (auto& node: s_set) {
        sorted.push_back(node);
    }

    return sorted;
}


void CGffSourceImpl::x_GetColumn1and9(TView line, TView& col1, TView& col9)
{
    for (size_t i=0; i<9; i++) {
        auto tab_pos = line.find('\t');
        if (i==8) {
            col9 = line.substr(0, tab_pos);
            break;
        }
        if (tab_pos == TView::npos)
            throw std::runtime_error("wrong format");
        if (i==0)
            col1 = line.substr(0, tab_pos);
        line = line.substr(tab_pos + 1);
    }
    if (col1.empty() || col9.empty())
        throw std::runtime_error("wrong format");
}

bool CGffSourceImpl::x_GetGtfGeneId(TView col9, TView& geneid)
{
    auto geneid_search = ctre::search<"gene_id \"([^\"]+)\"">(col9);
    if (geneid_search) {
        auto c = *std::prev(geneid_search.begin());
        if (c==';' || c=='\t')
            geneid= geneid_search.get<1>().to_view();
    }
    return true;
}

bool CGffSourceImpl::x_GetParentAndID(TView col9, TView& id, TView& parent)
{
    auto parent_search = ctre::search<"Parent=([^;]+)">(col9);
    if (parent_search) {
        auto c = *std::prev(parent_search.begin());
        if (c==';' || c=='\t')
            parent = parent_search.get<1>().to_view();
    }
    auto id_search = ctre::search<"ID=([^;]+)">(col9);
    if (id_search) {
        auto c = *std::prev(id_search.begin());
        if (c==';' || c=='\t')
            id = id_search.get<1>().to_view();
    }
    return true;
}

void CGffSourceImpl::x_LoadGffLines(
    std::vector<TView>& comments,
    std::size_t& all_lines_size,
    std::forward_list<TGffLine>& all_lines,
    TView& fasta) const
{
    std::forward_list<TView> dash_lines;
    size_t dash_lines_size = 0;
    all_lines_size = 0;

    TView current = m_whole_blob;

    fasta = {};

    while(!current.empty()) {
        TView line;
        auto next_pos = impl::SkipLine(current, 0, line);
        if (line.empty()) {
            current = current.substr(next_pos);
            continue;
        }

        if (line[0] == '#') {
            dash_lines.push_front(line);
            dash_lines_size++;
        } else if (line[0] == '>') {
            fasta = current;
            break;
        }
        else {
            std::string_view seqid;
            std::string_view id;
            std::string_view parent;
            TView column9;

            extract_tokens<"\t", 0, 8>(line, seqid, column9);

            if (m_file && m_file->m_format == CFormatGuess::eGtf)
            {
                x_GetGtfGeneId(column9, parent);
            } else {
                x_GetParentAndID(column9, id, parent);
                if (parent.empty())
                    parent = id;
            }

            all_lines.emplace_front(line, seqid, parent, id);
            all_lines_size++;
        }
        current = current.substr(next_pos);
    }

    if (dash_lines_size) {
        comments.reserve(dash_lines_size);
        dash_lines.reverse();
        while(!dash_lines.empty()) {
            comments.push_back(dash_lines.front());
            dash_lines.pop_front();
        }
    }
}


CGffSource::TGenerator
CGffSourceImpl::ReadBlobs(std::shared_ptr<CGffSourceImpl> self)
{
    std::vector<TView> comments;
    std::forward_list<TGffLine> all_lines;
    size_t all_lines_size;
    TView fasta;

    self->x_LoadGffLines(comments, all_lines_size, all_lines, fasta);

    if (!comments.empty()) {
        auto new_blob = std::make_shared<TGffBlob>(self);
        new_blob->m_blob_type = CGffSource::blob_type::comments;
        new_blob->m_lines = std::move(comments);

        co_yield new_blob;
    }

    if (all_lines_size) {
        auto sorted = x_SortGffLines(all_lines_size, all_lines.begin(), all_lines.end());

        TView current_seqid;
        std::vector<TView> current_lines;

        auto begin = sorted.begin();

        std::shared_ptr<TGffBlob> new_blob;

        for (auto it = sorted.begin(); it< sorted.end(); it++) {
            if (current_seqid != (**it).m_seqid ) {
                if ((new_blob = x_PopulateBlob(self, begin, it)))
                    co_yield new_blob;

                current_seqid = (**it).m_seqid;
                begin = it;
            }
        }
        if ((new_blob = x_PopulateBlob(self, begin, sorted.end())))
            co_yield new_blob;
    }

    if (!fasta.empty()) {
        auto new_blob = std::make_shared<TGffBlob>();
        new_blob->m_blob_type = CGffSource::blob_type::fasta;
        new_blob->m_lines.push_back(fasta);

        co_yield new_blob;
    }

    co_return;
}

Generator<std::shared_ptr<CGffSource::TGffBlob>> CGffSource::ReadBlobs() const
{
    return CGffSourceImpl::ReadBlobs(m_impl);
}
CGffSource::CGffSource(const std::string& filename)
{
    m_impl = std::make_shared<CGffSourceImpl>(filename);
}
CGffSource::CGffSource(TFile file)
{
    m_impl = std::make_shared<CGffSourceImpl>(file);
}
CGffSource::CGffSource(TView blob)
{
    m_impl = std::make_shared<CGffSourceImpl>(blob);
}
CGffSource::TGenerator CGffSource::ReadBlobs(const std::string& filename)
{
    return CGffSourceImpl::ReadBlobs(std::make_shared<CGffSourceImpl>(filename));
}
CGffSource::TGenerator CGffSource::ReadBlobs(TFile file)
{
    return CGffSourceImpl::ReadBlobs(std::make_shared<CGffSourceImpl>(file));
}
CGffSource::TGenerator CGffSource::ReadBlobs(TView blob)
{
    return CGffSourceImpl::ReadBlobs(std::make_shared<CGffSourceImpl>(blob));
}


END_SCOPE(ncbi::objtools::hugeasn)


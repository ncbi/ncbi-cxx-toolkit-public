#ifndef _CO_FASTA_READER_HPP_INCLUDED_
#define _CO_FASTA_READER_HPP_INCLUDED_

#include <objtools/readers/co_generator.hpp>
#include <util/compile_time.hpp>

BEGIN_SCOPE(ncbi::objects::edit)
class CHugeFile;
END_SCOPE(ncbi::objects::edit)

BEGIN_SCOPE(ncbi::objtools::hugeasn)

class CFastaSourceImpl;

class CFastaSource
{
public:
    using TFile = std::shared_ptr<objects::edit::CHugeFile>;
    using TView = std::string_view;

    enum class ParseFlags
    {
        IsDeltaSeq,
        NIsGap,
        MaxValue,
    };

    using ParseFlagsSet = ct::const_bitset<int(ParseFlags::MaxValue), ParseFlags>;

    struct TFastaBlob
    {
        TFastaBlob() = default;
        TFastaBlob(std::shared_ptr<CFastaSourceImpl> impl) : m_impl{impl} {}
        TView    m_defline;
        TView    m_blob;
        TView    m_data;
        size_t   m_numlines = 0;
        size_t   m_lineno_handicap = 0;
        size_t   m_seq_length = 0;

        ParseFlagsSet Flags() const;

    private:
        std::shared_ptr<CFastaSourceImpl> m_impl;
    };

    Generator<std::shared_ptr<TFastaBlob>> ReadBlobsQuick();
    Generator<std::shared_ptr<TFastaBlob>> ReadBlobs();

    explicit CFastaSource(const std::string& filename);
    explicit CFastaSource(TFile file);
    explicit CFastaSource(TView blob);

    void SetFlags(ParseFlagsSet flags)    { m_flags  = flags; }
    void AddFlags(ParseFlagsSet flags)    { m_flags += flags; }
    const ParseFlagsSet& GetFlags() const { return m_flags;   }

protected:
    std::shared_ptr<CFastaSourceImpl> m_impl;
    ParseFlagsSet m_flags;
};

END_SCOPE(ncbi::objtools::hugeasn)

#endif


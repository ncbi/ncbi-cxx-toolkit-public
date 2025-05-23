#ifndef _CO_FASTA_COMPAT_HPP_INCLUDED_
#define _CO_FASTA_COMPAT_HPP_INCLUDED_

#include "co_fasta.hpp"
#include <objtools/readers/fasta.hpp>

BEGIN_SCOPE(ncbi::objects::edit)
//class CHugeFile;
END_SCOPE(ncbi::objects::edit)

BEGIN_SCOPE(ncbi::objtools::hugeasn)

class CCompatLineReader: public ILineReader
{
public:
    explicit CCompatLineReader(CFastaSource::TFastaBlob blob) :
        m_blob{std::move(blob)},
        m_current_lineno{blob.m_lineno_handicap}
    {
    }
    bool AtEOF(void) const override;
    char PeekChar(void) const override;
    ILineReader& operator++(void) override;
    void UngetLine(void) override;
    CTempString operator*(void) const override;
    CT_POS_TYPE GetPosition(void) const override;
    Uint8 GetLineNumber(void) const override;

private:
    CFastaSource::TFastaBlob m_blob;
    size_t m_current_pos    = 0;
    size_t m_current_lineno = 0;
    size_t m_previous_pos   = 0;
    CFastaSource::TView m_current_line;
    CFastaSource::TView m_prev_line;
};

END_SCOPE(ncbi::objtools::hugeasn)

#endif


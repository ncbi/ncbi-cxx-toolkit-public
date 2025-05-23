#include <ncbi_pch.hpp>
#include "co_fasta_compat.hpp"
#include "impl/co_readers.hpp"


using namespace ncbi::objtools::hugeasn;
using enum CFastaSource::ParseFlags;

BEGIN_LOCAL_NAMESPACE;


END_LOCAL_NAMESPACE;

BEGIN_SCOPE(ncbi::objtools::hugeasn)

bool CCompatLineReader::AtEOF(void) const
{
    return m_current_pos >= m_blob.m_blob.size();
}

char CCompatLineReader::PeekChar(void) const
{
    return m_blob.m_blob.at(m_current_pos);
}

ILineReader& CCompatLineReader::operator++(void)
{
    CFastaSource::TView line;
    size_t nextnl = impl::SkipLine(m_blob.m_blob, m_current_pos, line);
    if (nextnl != CFastaSource::TView::npos) {
        m_current_lineno++;
        m_prev_line = m_current_line;
        m_previous_pos = m_current_pos;
        m_current_line = line;
        m_current_pos = nextnl;
    }
    return *this;
}

void CCompatLineReader::UngetLine(void)
{
    if (m_current_lineno != 0) {
        m_current_lineno--;
        m_current_pos = m_previous_pos;
        m_current_line = m_prev_line;
    }
}

CTempString CCompatLineReader::operator*(void) const
{
    return m_current_line;
}

CT_POS_TYPE CCompatLineReader::GetPosition(void) const
{
    return m_current_pos;
}

Uint8 CCompatLineReader::GetLineNumber(void) const
{
    return m_current_lineno;
}

END_SCOPE(ncbi::objtools::hugeasn)


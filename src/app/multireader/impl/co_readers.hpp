#ifndef _CO_READERS_HPP_INCLUDED_
#define _CO_READERS_HPP_INCLUDED_

#include <objtools/readers/co_generator.hpp>

BEGIN_SCOPE(ncbi::objtools::hugeasn)

Generator<std::pair<size_t, std::string_view>>
BasicReadLinesGenerator(size_t start_line, std::string_view blob);

BEGIN_SCOPE(impl)

inline size_t SkipLine(std::string_view blob, std::string_view::size_type start_pos, std::string_view& line)
{
    auto next_nl = blob.find('\n', start_pos);
    if (next_nl == std::string_view::npos) {
        line = blob.substr(start_pos);
        return blob.size();
    }

    auto line_end = blob[next_nl-1] == '\r' ? next_nl - 1 : next_nl;
    line = blob.substr(start_pos, line_end-start_pos);
    return next_nl + 1;
}

END_SCOPE(impl)

END_SCOPE(ncbi::objtools::hugeasn)

#endif


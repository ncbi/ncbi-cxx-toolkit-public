#ifndef __HGVS_SPECIAL_VARIANT_PARSER_HPP__
#define __HGVS_SPECIAL_VARIANT_PARSER_HPP__

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistd.hpp>
#include <objtools/readers/hgvs/hgvs_lexer.hpp>
#include <boost/spirit/include/qi.hpp>
#include <objtools/readers/hgvs/hgvs_parser_common.hpp>
#include <objects/varrep/varrep__.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

using TParseIterator = SHgvsLexer::iterator_type;

//struct SHgvsSpecialVariantGrammar :  boost::spirit::qi::grammar<TParseIterator, string()>
struct SHgvsSpecialVariantGrammar :  boost::spirit::qi::grammar<TParseIterator, ESpecialVariant()>
{
    SHgvsSpecialVariantGrammar(const SHgvsLexer& tok);
    using TRule = boost::spirit::qi::rule<TParseIterator, ESpecialVariant()>;
    TRule variant_expression;
};


END_NCBI_SCOPE


#endif // __HGVS_SPECIAL_VARIANT_PARSER_HPP__

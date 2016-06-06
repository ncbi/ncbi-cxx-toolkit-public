#ifndef _HGVS_PARSER_HPP_
#define _HGVS_PARSER_HPP_

#include <objtools/readers/hgvs/hgvs_protein_parser.hpp>
#include <objtools/readers/hgvs/hgvs_nucleic_acid_parser.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

struct SHgvsVariantGrammar : boost::spirit::qi::grammar<SHgvsLexer::iterator_type, CRef<CVariantExpression>()>
{
    SHgvsVariantGrammar(const SHgvsLexer& tok);

    using TParseIterator = SHgvsLexer::iterator_type;
    template<typename T> using TRule = boost::spirit::qi::rule<TParseIterator, CRef<T>()>;

    TRule<CVariantExpression> variant_expression;
    SHgvsProteinGrammar protein_variant;
    SHgvsNucleicAcidGrammar na_variant;
};


class CHgvsParser {
public:
    CHgvsParser() : m_Lexer(), m_Grammar(m_Lexer) {}

    bool Apply(const string& hgvs_expression, CRef<CVariantExpression>& varient_irep) const;

private:
    SHgvsLexer m_Lexer;
    SHgvsVariantGrammar m_Grammar;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _HGVS_PARSER_HPP_


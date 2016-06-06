#include <ncbi_pch.hpp>
#include <objtools/readers/hgvs/hgvs_parser.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

SHgvsVariantGrammar::SHgvsVariantGrammar(const SHgvsLexer& tok) : 
    protein_variant(tok),
    na_variant(tok),
    SHgvsVariantGrammar::base_type(variant_expression)
{
    variant_expression = protein_variant | na_variant;
}




// Success failure partial success
bool CHgvsParser::Apply(const string& hgvs_expression, CRef<CVariantExpression>& variant_irep) const
{
    if (hgvs_expression.empty()) {
        return true;
    }

    TLexIterator first = hgvs_expression.c_str();
    TLexIterator last = &first[hgvs_expression.size()];

    TLexer::iterator_type lex_first = m_Lexer.begin(first,last);
    TLexer::iterator_type lex_last = m_Lexer.end();

    bool parse_success = boost::spirit::qi::parse(lex_first, lex_last, m_Grammar, variant_irep);

    if (parse_success && 
        lex_first == lex_last) { // Successful parse
        variant_irep->SetInput_expr(hgvs_expression);
        return true;
    }

    return false;
}


END_NCBI_SCOPE



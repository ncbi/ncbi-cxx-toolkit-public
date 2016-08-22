#include <ncbi_pch.hpp>
#include <objtools/readers/hgvs/hgvs_parser.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <objtools/readers/hgvs/semantic_actions.hpp>

using boost::spirit::qi::_1;
using boost::spirit::qi::_val;
using boost::phoenix::bind;

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

SHgvsVariantGrammar::SHgvsVariantGrammar(const SHgvsLexer& tok) : 
    simple_protein_variant(tok),
    simple_na_variant(tok),
    special_variant(tok),
    SHgvsVariantGrammar::base_type(variant_expression)
{
    variant_expression = protein_expression | na_expression;

    protein_expression = tok.identifier ACTION1(AssignRefSeqIdentifier) 
                         >> protein_seq_variants ACTION1(AssignSequenceVariant);

    protein_seq_variants = protein_simple_seq_variant | protein_mosaic | protein_chimera;

    protein_simple_seq_variant = tok.protein_tag ACTION0(AssignSequenceType) >>
                                 protein_variant ACTION1(AssignSingleVariation);

    protein_mosaic = ( (tok.protein_tag ACTION0(AssignSequenceType) >>
                     ( "[" >> (protein_variant >> tok.slash) ACTION1(AssignSingleVariation)
                     >> (protein_variant ACTION1(AssignSingleVariation)) >> "]" ) ) ACTION0(TagAsMosaic));

    protein_chimera =  ( (tok.protein_tag ACTION0(AssignSequenceType) >>
                       ( "[" >> (protein_variant >> tok.double_slash) ACTION1(AssignSingleVariation)
                       >> (protein_variant ACTION1(AssignSingleVariation)) >> "]" ) ) ACTION0(TagAsChimera));

    protein_variant = simple_protein_variant ACTION1(AssignSimpleVariant) |
                      special_variant ACTION1(AssignSpecialVariant);

    na_expression = tok.identifier ACTION1(AssignRefSeqIdentifier) >>
                    na_seq_variants ACTION1(AssignSequenceVariant);

    na_seq_variants = na_simple_seq_variant | na_chimera | na_mosaic;

    na_simple_seq_variant = tok.na_tag ACTION1(AssignSequenceType) >>  
                            na_variant ACTION1(AssignSingleVariation);

    na_mosaic = ( (tok.na_tag ACTION1(AssignSequenceType) >>  
                ( "[" >> (na_variant >> tok.slash) ACTION1(AssignSingleVariation) 
                >> na_variant ACTION1(AssignSingleVariation) >> "]" ) ) ) ACTION0(TagAsMosaic);

    na_chimera = ( (tok.na_tag ACTION1(AssignSequenceType) >>  
                 ( "[" >> (na_variant >> tok.double_slash) ACTION1(AssignSingleVariation) 
                 >> na_variant ACTION1(AssignSingleVariation) >> "]" ) ) ) ACTION0(TagAsChimera);

    na_variant = simple_na_variant ACTION1(AssignSimpleVariant) |
                 special_variant ACTION1(AssignSpecialVariant);
}




// Success failure partial success
bool CHgvsParser::Apply(const string& hgvs_expression, CRef<CVariantExpression>& variant_irep) const
{
    if (NStr::IsBlank(hgvs_expression)) {
        return false; // LCOV_EXCL_LINE
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



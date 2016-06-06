#ifndef __HGVS_NUCLEIC_ACID_PARSER_HPP__
#define __HGVS_NUCLEIC_ACID_PARSER_HPP__

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistd.hpp>
#include <objtools/readers/hgvs/hgvs_lexer.hpp>
#include <boost/spirit/include/qi.hpp>
#include <objtools/readers/hgvs/hgvs_parser_common.hpp>
#include <objects/varrep/varrep__.hpp>
#include <objtools/readers/hgvs/hgvs_special_variant_parser.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

using TParseIterator = SHgvsLexer::iterator_type;

struct SHgvsNucleicAcidGrammar : boost::spirit::qi::grammar<TParseIterator, CRef<CVariantExpression>()>
{
    SHgvsNucleicAcidGrammar(const SHgvsLexer& tok);

    using TTerminal = boost::spirit::qi::rule<TParseIterator, std::string()>;
    template<typename T> using TRule = boost::spirit::qi::rule<TParseIterator, CRef<T>()>;

    TRule<CVariantExpression> dna_expression;
    TRule<CSequenceVariant> dna_seq_variants;
    TRule<CSequenceVariant> dna_simple_seq_variant;
    TRule<CSequenceVariant> dna_mosaic;
    TRule<CSequenceVariant> dna_chimera;
    TRule<CVariant> variant;
    TRule<CSimpleVariant> simple_variation;
    TRule<CSimpleVariant> fuzzy_simple_variation;
    TRule<CSimpleVariant> confirmed_simple_variation;
    TRule<CSimpleVariant> sub;
    TRule<CSimpleVariant> dup;
    TRule<CSimpleVariant> delins;
    TRule<CSimpleVariant> del;
    TRule<CSimpleVariant> ins;
    TRule<CSimpleVariant> conv;
    TRule<CSimpleVariant> inv;
    TRule<CSimpleVariant> ssr;
    TRule<CSimpleVariant> identity;
    TRule<CNtLocation> remote_nucleotide_interval;
    TRule<CNtLocation> nucleotide_location;
    TRule<CNtLocation> nucleotide_site_interval;
    TRule<CNtLocation> nucleotide_site;
    TRule<CNtLocation> nucleotide_site_range;
    TRule<CNtSite> nucleotide_uncertain_site;
    TRule<CNtSite> nucleotide_single_site;
    TRule<CNtSite> simple_coding_site;
    TRule<CNtSite> pretranslation_site;
    TRule<CNtSite> posttranslation_site;
    TRule<CNtSite> intron_site; 
    TRule<CNtSite> simple_site;
    TTerminal intron_offset; 
    TTerminal offset_length;
    TTerminal nucleotide_seq;
    TTerminal nucleotide;
    TRule<CCount> count_range;
    TRule<CCount> count;
    TRule<CCount> fuzzy_count;
    TTerminal val_or_unknown;
    TTerminal nn_int;

    SHgvsSpecialVariantGrammar special_variant;
};

END_SCOPE(objects)
END_NCBI_SCOPE


#endif // _HGVS_NUCLEIC_ACID_PARSER_HPP_

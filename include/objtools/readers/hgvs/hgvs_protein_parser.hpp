#ifndef _HGVS_PROTEIN_PARSER_HPP_
#define _HGVS_PROTEIN_PARSER_HPP_

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistd.hpp>
#include <objtools/readers/hgvs/hgvs_lexer.hpp>
#include <boost/spirit/include/qi.hpp>
#include <objtools/readers/hgvs/hgvs_parser_common.hpp>
#include <objects/varrep/varrep__.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

using TParseIterator = SHgvsLexer::iterator_type;
//using TTerminal = boost::spirit::qi::rule<TParseIterator, std::string()>;
//template<typename T> using TRule = boost::spirit::qi::rule<TParseIterator, CRef<T>()>;


struct SHgvsProteinGrammar : boost::spirit::qi::grammar<TParseIterator, CRef<CVariantExpression>()>
{

    SHgvsProteinGrammar(const SHgvsLexer& tok);


    using TTerminal = boost::spirit::qi::rule<TParseIterator, std::string()>;
    template<typename T> using TRule = boost::spirit::qi::rule<TParseIterator, CRef<T>()>;

    boost::spirit::qi::rule<TParseIterator, ESpecialVariant()> protein_special_variant;
 
    TRule<CVariantExpression> protein_expression;
    TRule<CSeqVariants> protein_seq_variants; 
    TRule<CSeqVariants> protein_mosaic;
    TRule<CSeqVariants> protein_chimera;
    TRule<CSeqVariants> protein_simple_seq_variant;
    TRule<CSimpleVariantSeq> protein_unknown_chrom_variant;
    TRule<CSimpleVariantSeq> protein_confirmed_unknown_chrom_variant;
    TRule<CSimpleVariantSeq> protein_fuzzy_unknown_chrom_variant;
    TRule<CSimpleVariantSeq> protein_chrom_variant;
    TRule<CSimpleVariantSeq> protein_confirmed_chrom_variant;
    TRule<CSimpleVariantSeq> protein_fuzzy_chrom_variant;
    TRule<CSimpleVariant> protein_simple_variation;
    TRule<CSimpleVariant> protein_fuzzy_simple_variation;
    TRule<CSimpleVariant> protein_confirmed_simple_variation;
    TRule<CSimpleVariant> missense;
    TRule<CSimpleVariant> nonsense;
    TRule<CSimpleVariant> unknown_sub;
    TRule<CSimpleVariant> aa_dup;
    TRule<CSimpleVariant> aa_delins;
    TRule<CSimpleVariant> aa_del;
    TRule<CSimpleVariant> aa_ins;
    TRule<CSimpleVariant> aa_ssr;
    TRule<CCount> aa_repeat;
    TRule<CCount> aa_repeat_precise;
    TRule<CCount> aa_repeat_fuzzy;
    TRule<CCount> aa_repeat_range;
    TRule<CSimpleVariant> frameshift;
    TRule<CSimpleVariant> protein_extension;
    TRule<CSimpleVariant> nterm_extension;
    TRule<CSimpleVariant> cterm_extension;
    TRule<CSimpleVariant> frameshift_long_form;
    TRule<CSimpleVariant> frameshift_nonstandard;
    TRule<CSimpleVariant> frameshift_short_form;
    TRule<CCount> end_codon_shift;
    TRule<CCount> seq_size;  
    TRule<CAaLocation> aa_loc;
    TRule<CAaInterval> aa_interval;
    TRule<CAaSite> aa_site;
    TRule<CAaLocation> aa3_loc;
    TRule<CAaInterval> aa3_interval;
    TRule<CAaSite> aa3_site;
    TRule<CAaLocation> aa1_loc;
    TRule<CAaInterval> aa1_interval;
    TRule<CAaSite> aa1_site;
    TTerminal aa3_stop_seq;
    TTerminal aa1_stop_seq;
    TTerminal aa3_seq;
    TTerminal aa1_seq;
    TTerminal aa1;
    TTerminal nn_int;
};

END_NCBI_SCOPE

#endif // _HGVS_PROTEIN_PARSER_HPP_

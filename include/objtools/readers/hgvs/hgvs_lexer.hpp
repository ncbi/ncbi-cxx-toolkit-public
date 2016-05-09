#ifndef _HGVS_LEXER_HPP_
#define _HGVS_LEXER_HPP_


#include <boost/spirit/include/lex_lexertl.hpp>


namespace lex = boost::spirit::lex;

typedef char const* TLexIterator;
typedef lex::lexertl::token<TLexIterator, boost::mpl::vector<std::string> > TToken;
typedef lex::lexertl::lexer<TToken> TLexer;


struct SHgvsLexer : lex::lexer<TLexer>
{

    SHgvsLexer();

    typedef lex::token_def<lex::omit> TSimpleToken;       // This token type only has an identifier 
    typedef lex::token_def<std::string> TAttributedToken; // This token also includes attribute

    TSimpleToken dup;
    TSimpleToken del;
    TSimpleToken delins;
    TSimpleToken ins;
    TSimpleToken inv;
    TSimpleToken con;
    TSimpleToken ext;
    TSimpleToken fs;
    TAttributedToken ACGT;
    TAttributedToken acgu;
    TAttributedToken definite_aa1;
    TAttributedToken aa3;
    TSimpleToken stop;
    TAttributedToken pos_int;
    TAttributedToken fuzzy_pos_int;
    TAttributedToken unknown_val;
    TSimpleToken nochange;
    TSimpleToken zero;
    TSimpleToken unknown_chrom_separator; // (;)
    TSimpleToken protein_tag;
    TAttributedToken na_tag;
    TAttributedToken identifier;
    TSimpleToken slash;
    TSimpleToken double_slash;
};

#endif // _HGVS_LEXER_HPP_

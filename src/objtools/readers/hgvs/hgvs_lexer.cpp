#include <ncbi_pch.hpp>
#include <objtools/readers/hgvs/hgvs_lexer.hpp>


SHgvsLexer::SHgvsLexer()
    : dup("dup"),
    del("del"),
    delins("delins"),
    ins("ins"),
    inv("inv"),
    con("con"),
    ext("ext"),
    fs("fs"),
    ACGT("[ACGT]"),
    acgu("[acgu]"),
    definite_aa1("D|E|F|H|I|K|L|M|N|P|Q|R|S|V|W|Y|U|O"),
    aa3("Ala|Cys|Asp|Glu|Phe|Gly|His|Ile|Lys|Leu|Met|Asn|Pro|Gln|Arg|Ser|Thr|Val|Trp|Tyr|Sec|Pyl"),
    stop("Ter|\\*|X"),
    pos_int("[1-9][0-9]*"),
    fuzzy_pos_int("\\([1-9][0-9]*\\)"),
    unknown_val("\\?"),
    nochange("\\="),
    zero("0"),
    unknown_chrom_separator("(;)"),
    protein_tag("p."),
    na_tag("g.|c.|m.|n.|r."),
    identifier("o?([A-Z]|rs|ss|chr)([a-zA-Z0-9_.]+):")
{

    this->self = lex::token_def<>( '(' ) | ')' | '{' | '}' | '[' | ']' | ';' | ':' | ',' | '_' |
        '-'| '+' | '>';


    this->self.add
        (dup)
        (delins)
        (del)
        (ins)
        (inv)
        (con)
        (ext)
        (fs)
        (ACGT)
        (acgu)
        (definite_aa1)
        (aa3)
        (stop)
        (pos_int)
        (fuzzy_pos_int)
        (unknown_val)
        (nochange)
        (zero)
        (unknown_chrom_separator)
        (protein_tag)
        (na_tag)
        (identifier)
        ;
}


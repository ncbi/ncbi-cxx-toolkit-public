#include <ncbi_pch.hpp>
#include <objtools/readers/hgvs/hgvs_special_variant_parser.hpp>
#include <objtools/readers/hgvs/semantic_actions.hpp>
#include <boost/spirit/include/phoenix.hpp>

using boost::spirit::qi::_val;
using boost::phoenix::bind;

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

template<class T>
CRef<T> MakeResultIfNull(CRef<T> result)
{
    return result.IsNull() ? Ref(new T()) : result;
}

void s_SimpleAssign(CRef<CVariant>& result)
{
    result = MakeResultIfNull(result);
}


SHgvsSpecialVariantGrammar::SHgvsSpecialVariantGrammar(const SHgvsLexer& tok) :
    SHgvsSpecialVariantGrammar::base_type(variant_expression)
{

    variant_expression = tok.unknown_val [ _val = eSpecialVariant_unknown ] |
                         ( "(" >> tok.unknown_val >> ")" ) [ _val = eSpecialVariant_not_analyzed ] |
                         tok.nochange [ _val = eSpecialVariant_nochange ] |
                         ( "(" >> tok.nochange >> ")" ) [ _val = eSpecialVariant_nochange_expected ] |
                         ( tok.zero >> tok.unknown_val ) [ _val = eSpecialVariant_noseq_expected ] |
                         tok.zero [ _val =  eSpecialVariant_noseq ] |
                         tok.splice [ _val = eSpecialVariant_splice_expected ] |
                         ( "(" >> tok.splice >> ")" ) [ _val = eSpecialVariant_splice_possible ];

}


END_NCBI_SCOPE

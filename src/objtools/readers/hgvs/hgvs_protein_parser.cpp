#include <ncbi_pch.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <objtools/readers/hgvs/hgvs_protein_parser.hpp>
#include <objtools/readers/hgvs/semantic_actions.hpp>
#include <objtools/readers/hgvs/hgvs_special_variant_parser.hpp>

using boost::spirit::qi::_1;
using boost::spirit::qi::_2;
using boost::spirit::qi::_3;
using boost::spirit::qi::_val;
using boost::spirit::qi::_a;
using boost::phoenix::bind;

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

void IsMet1(const CRef<CAaSite>& aa_site, boost::spirit::unused_type context, bool& match)
{
    match = false;
    if (aa_site->IsSetAa() &&
        aa_site->IsSetIndex() &&
        (aa_site->GetIndex() == 1 &&
         (NStr::Equal(aa_site->GetAa(), "Met") 
          || NStr::Equal(aa_site->GetAa(), "M")))) 
       {
           match = true;   
       }
}


SHgvsProteinGrammar::SHgvsProteinGrammar(const SHgvsLexer& tok) : 
    special_variant(tok),
    SHgvsProteinGrammar::base_type(protein_expression)
{
 // Also need to account for variants involving different genes on different chromosomes. 
    protein_expression = tok.identifier ACTION1(AssignRefSeqIdentifier) 
                       >> protein_seq_variants ACTION1(AssignSequenceVariant);

    protein_seq_variants = protein_simple_seq_variant | protein_mosaic | protein_chimera;

    protein_simple_seq_variant = tok.protein_tag ACTION0(AssignSequenceType) >>
                                 variant ACTION1(AssignSingleVariation);

    protein_mosaic = ( (tok.protein_tag ACTION0(AssignSequenceType) >>
                     ( "[" >> (variant >> tok.slash) ACTION1(AssignSingleVariation)
                     >> (variant ACTION1(AssignSingleVariation)) >> "]" ) ) ACTION0(TagAsMosaic));

    protein_chimera =  ( (tok.protein_tag ACTION0(AssignSequenceType) >>
                       ( "[" >> (variant >> tok.double_slash) ACTION1(AssignSingleVariation)
                        >> (variant ACTION1(AssignSingleVariation)) >> "]" ) ) ACTION0(TagAsChimera));

    variant = protein_simple_variation ACTION1(AssignSimpleVariant) |
              special_variant ACTION1(AssignSpecialVariant);

    protein_simple_variation = protein_confirmed_simple_variation | protein_fuzzy_simple_variation;

    protein_fuzzy_simple_variation = ("(" >> protein_confirmed_simple_variation >> ")") ACTION1(AssignFuzzyLocalVariation);

    protein_confirmed_simple_variation =
                                         frameshift |
                                         protein_extension |
                                         missense |
                                         nonsense |
                                         unknown_sub |
                                         aa_dup  |
                                         aa_delins |
                                         aa_del 	|
                                         aa_ins  |
                                         aa_ssr;
							
    missense = (aa3_site >> tok.aa3) ACTION2(AssignMissense) |
               (aa1_site >> aa1) ACTION2(AssignMissense);
    
    nonsense = (aa_site >> tok.stop) ACTION1(AssignNonsense);

    unknown_sub = (aa_site >> tok.unknown_val) ACTION1(AssignUnknownSub);

    aa_delins = (aa3_loc >> tok.delins >> aa3_stop_seq) ACTION2(AssignAaDelins) |
                (aa3_loc >> tok.delins >> aa3_seq) ACTION2(AssignAaDelins) |
                (aa1_loc >> tok.delins >> aa1_stop_seq) ACTION2(AssignAaDelins) |
                (aa1_loc >> tok.delins >> aa1_seq) ACTION2(AssignAaDelins) |
                (aa_loc >> tok.delins >> seq_size) ACTION2(AssignAaDelinsSize);

    aa1_stop_seq = (aa1_seq >> tok.stop) [_val = _1 + "*"];

    aa3_stop_seq = (aa3_seq >> tok.stop) [_val = _1 + "*"];

    aa_dup = (aa_loc >> tok.dup) ACTION1(AssignAaDup);

    aa_del = (aa_loc >> tok.del) ACTION1(AssignAaDel);

    aa_ins =  (aa3_interval >> tok.ins >>  aa3_seq) ACTION2(AssignAaInsertion) |
              (aa1_interval >> tok.ins >> aa1_seq) ACTION2(AssignAaInsertion) |
              (aa_interval >> tok.ins >> seq_size) ACTION2(AssignAaInsertionSize);
              // Need to be able to insert a "remote" sequence			 

    aa_ssr = (aa_loc >> aa_repeat) ACTION2(AssignAaSSR);

    aa_repeat = aa_repeat_fuzzy | aa_repeat_precise | aa_repeat_range;

    aa_repeat_precise = ("[" >> tok.pos_int >> "]") ACTION1(AssignCount);

    aa_repeat_fuzzy = tok.fuzzy_pos_int ACTION1(AssignFuzzyCount);

    aa_repeat_range = ("(" >> nn_int >> "_" >> tok.pos_int >> ")") ACTION2(AssignCountRange) |
                      ("(" >> nn_int >> "_" >> tok.unknown_val >> ")") ACTION2(AssignCountRange) |
                      ("(" >> tok.unknown_val >> "_" >> tok.pos_int >> ")") ACTION2(AssignCountRange);

    frameshift = frameshift_long_form |
                 frameshift_nonstandard |
                 frameshift_short_form;

    frameshift_long_form = (aa1_site >> aa1 >> tok.fs >> tok.stop >> (tok.pos_int | tok.unknown_val )) ACTION1(AssignFrameshift) |
                           (aa3_site >> tok.aa3 >> tok.fs >> tok.stop >> (tok.pos_int | tok.unknown_val)) ACTION1(AssignFrameshift) |
                           (tok.stop >> (tok.aa3 | aa1) >> tok.fs >> tok.stop >> (tok.pos_int | tok.unknown_val)) ACTION0(AssignFrameshiftFromStopcodon);

    frameshift_nonstandard  = (aa1_site >> aa1 >> tok.fs) ACTION1(AssignFrameshift) |
                              (aa3_site >> tok.aa3 >> tok.fs) ACTION1(AssignFrameshift) |
                              (tok.stop >> (tok.aa3 | aa1) >> tok.fs) ACTION0(AssignFrameshiftFromStopcodon);
                              
    frameshift_short_form = (aa_site >> tok.fs) ACTION1(AssignFrameshift) |
                            (tok.stop >> tok.fs) ACTION0(AssignFrameshiftFromStopcodon);

    protein_extension = cterm_extension | nterm_extension;

    cterm_extension = (tok.stop >> tok.pos_int >> (aa1 | tok.aa3) >> tok.ext >> tok.stop >> end_codon_shift) 
                      ACTION3(AssignCtermExtension);

    nterm_extension = (aa_site [IsMet1] >> tok.ext >> "-" >> end_codon_shift) 
                      ACTION2(AssignNtermExtension) |
                      (aa3_site [IsMet1] >> tok.aa3 >> tok.ext >> "-" >> end_codon_shift) 
                      ACTION3(AssignNtermExtension) |
                      (aa1_site [IsMet1] >> aa1 >> tok.ext >> "-" >> end_codon_shift) 
                      ACTION3(AssignNtermExtension);
         
    end_codon_shift = tok.pos_int ACTION1(AssignCount) | 
                      tok.fuzzy_pos_int ACTION1(AssignFuzzyCount);

    seq_size = tok.pos_int ACTION1(AssignCount);

    aa_loc = aa3_loc | aa1_loc;

    aa_interval = aa3_interval | aa1_interval;

    aa_site = aa3_site | aa1_site;

    aa3_loc = aa3_interval ACTION1(AssignAaIntervalLocation) |
              aa3_site ACTION1(AssignAaSiteLocation);

    aa3_interval = (aa3_site >> "_" >> aa3_site) ACTION2(AssignAaInterval);

    aa3_site = (tok.aa3 >> tok.pos_int) ACTION2(AssignAaSite);

    aa3_seq = +(tok.aa3) [_val += _1];

    aa1_loc = aa1_interval ACTION1(AssignAaIntervalLocation) | 
              aa1_site ACTION1(AssignAaSiteLocation);

    aa1_interval = (aa1_site >> "_" >> aa1_site) ACTION2(AssignAaInterval);

    aa1_site = (aa1 >> tok.pos_int) ACTION2(AssignAaSite);

    aa1_seq = +(aa1) [_val += _1];

    aa1 = tok.ACGT | tok.definite_aa1;

    nn_int = tok.zero [_val = "0"] |
             tok.pos_int [_val = _1]; 
}

END_NCBI_SCOPE

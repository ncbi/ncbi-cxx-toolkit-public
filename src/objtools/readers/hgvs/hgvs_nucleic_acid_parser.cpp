#include <ncbi_pch.hpp>
#include <objtools/readers/hgvs/hgvs_nucleic_acid_parser.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <objtools/readers/hgvs/semantic_actions.hpp>


using boost::spirit::qi::_1;
using boost::spirit::qi::_2;
using boost::spirit::qi::_3;
using boost::spirit::qi::_val;
using boost::phoenix::bind;


BEGIN_NCBI_SCOPE


SHgvsNucleicAcidGrammar::SHgvsNucleicAcidGrammar(const SHgvsLexer& tok) : SHgvsNucleicAcidGrammar::base_type(dna_expression)
{
    dna_expression = tok.identifier ACTION1(AssignRefSeqIdentifier) >>
                     dna_seq_variants ACTION1(AppendSeqVariant);

    dna_seq_variants = tok.na_tag ACTION1(AssignSequenceType) >>
                       simple_variation ACTION1(AssignSingleLocalVariation);

    simple_variation = fuzzy_simple_variation | confirmed_simple_variation;

    fuzzy_simple_variation = ("(" >> confirmed_simple_variation >> ")") ACTION1(AssignFuzzyLocalVariation);

    confirmed_simple_variation = sub |
                                 dup |
                                 delins |
                                 del |
                                 ins |
                                 inv |
                                 conv |
                                 ssr |
                                 identity;

    sub = (nucleotide_site >> nucleotide_seq >> ">" >> nucleotide_seq) ACTION3(AssignNtSub);

    dup = (nucleotide_location >> tok.dup >> nucleotide_seq) ACTION2(AssignNtDup) | // Need to change this to allow remote sequences
          (nucleotide_location >> tok.dup) ACTION1(AssignNtDup);

    delins = (nucleotide_location >> tok.del >> nucleotide_seq >> tok.ins >> nucleotide_seq) ACTION3(AssignNtDelins) | 
             (nucleotide_location >> tok.delins >> nucleotide_seq) ACTION2(AssignNtDelins);

    del = (nucleotide_location >> tok.del >> nucleotide_seq) ACTION2(AssignNtDeletion) | 
          (nucleotide_location >> tok.del) ACTION1(AssignNtDeletion);

    ins = (nucleotide_site_interval >> tok.ins >> nucleotide_seq) ACTION2(AssignNtInsertion); // Need to change this to allow remote sequences

    conv = (nucleotide_site_interval >> tok.con >> remote_nucleotide_interval) ACTION2(AssignNtConversion); 

    inv = (nucleotide_site_interval >> tok.inv >> nucleotide_seq) ACTION2(AssignNtInv) |
          (nucleotide_site_interval >> tok.inv) ACTION1(AssignNtInv); // What about expressions specifying the size of the inversion?

    ssr = (nucleotide_location >> "[" >> count >> "]") ACTION2(AssignNtSSR) |
          (nucleotide_location >> nucleotide_seq >> "[" >> count >> "]") ACTION3(AssignNtSSR) |
          (nucleotide_location >> count_range ) ACTION2(AssignNtSSR) |
          (nucleotide_location >> nucleotide_seq >> count_range) ACTION3(AssignNtSSR) |
          (nucleotide_location >> fuzzy_count) ACTION2(AssignNtSSR) |
          (nucleotide_location >> nucleotide_seq >> fuzzy_count) ACTION3(AssignNtSSR);

    identity = (nucleotide_site >> nucleotide >> tok.nochange) ACTION2(AssignNtIdentity) |
               (nucleotide_site >> tok.nochange) ACTION1(AssignNtIdentity);

    remote_nucleotide_interval = (tok.identifier >> tok.na_tag >> nucleotide_site_interval) ACTION3(AssignNtRemoteLocation) |
                                 (tok.identifier >> nucleotide_site_interval) ACTION2(AssignNtRemoteLocation);

    nucleotide_location = nucleotide_site_interval |
                          nucleotide_site;

    nucleotide_site_interval = (nucleotide_site >> "_" >> nucleotide_site) 
                               ACTION2(AssignNtInterval);

    nucleotide_site =  nucleotide_site_range VALASSIGN | 
                       nucleotide_single_site ACTION1(AssignNtSite) |
                       nucleotide_uncertain_site ACTION1(AssignNtSite);

  
    nucleotide_site_range = ("(" >> nucleotide_single_site >> "_" >> nucleotide_single_site >> ")") 
                            ACTION2(AssignNtSiteRange);

    nucleotide_uncertain_site = ("(" >> nucleotide_single_site >> ")") ACTION1(AssignFuzzyNtSite);

    nucleotide_single_site = pretranslation_site  |
                             posttranslation_site |
                             intron_site |
                             simple_site;

    pretranslation_site = "-" >> intron_site ACTION1(Assign5primeUTRSite) | 
                          "-" >> simple_site ACTION1(Assign5primeUTRSite);

    posttranslation_site = tok.stop >> intron_site ACTION1(Assign3primeUTRSite) | 
                           tok.stop >> simple_site ACTION1(Assign3primeUTRSite);

    intron_site = (tok.pos_int >> intron_offset) ACTION2(AssignIntronSite);

    intron_offset = "+" >> offset_length [_val = "+" + _1] |
                    "-" >> offset_length [_val = "-" + _1];
  
    offset_length = tok.pos_int | tok.fuzzy_pos_int | tok.unknown_val;

    simple_site = tok.pos_int ACTION1(AssignSimpleNtSite) |
                  tok.fuzzy_pos_int ACTION1(AssignFuzzySimpleNtSite) |
                  tok.unknown_val ACTION1(AssignSimpleNtSite);

    nucleotide_seq = +nucleotide [_val += _1];

    nucleotide = tok.acgu | tok.ACGT;

    count = val_or_unknown ACTION1(AssignCount) | 
            count_range VALASSIGN; 

    fuzzy_count = tok.fuzzy_pos_int ACTION1(AssignFuzzyCount);

    count_range = ("(" >> val_or_unknown >> "_" >> val_or_unknown >> ")") ACTION2(AssignCountRange);

    val_or_unknown = nn_int | tok.unknown_val;

    nn_int = tok.zero [_val = "0"] |
             tok.pos_int VALASSIGN;
}

END_NCBI_SCOPE

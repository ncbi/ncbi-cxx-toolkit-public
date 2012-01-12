/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* File Description:
*
*   Translate HGVS expression to Variation-ref seq-feats.
*   HGVS nomenclature rules: http://www.hgvs.org/mutnomen/
*
* ===========================================================================
*/

#ifndef HGVSPARSER_HPP_
#define HGVSPARSER_HPP_

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistd.hpp>

#include <boost/version.hpp>
#if BOOST_VERSION >= 103800
#include <boost/spirit/include/classic.hpp>
#include <boost/spirit/include/classic_core.hpp>
#include <boost/spirit/include/classic_ast.hpp>
#include <boost/spirit/include/classic_parse_tree.hpp>
#include <boost/spirit/include/classic_tree_to_xml.hpp>

using namespace BOOST_SPIRIT_CLASSIC_NS;
#else
//older boost
#include <boost/spirit.hpp>
#include <boost/spirit/core.hpp>
#include <boost/spirit/tree/ast.hpp>
#include <boost/spirit/tree/parse_tree.hpp>
#include <boost/spirit/tree/tree_to_xml.hpp>
using namespace boost::spirit;
#endif

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seq/Seq_data.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

#define HGVS_THROW(err_code, message) NCBI_THROW(CHgvsParser::CHgvsParserException, err_code, message)

namespace variation_ref {

class CHgvsParser : public CObject
{
public:
    CHgvsParser(CScope& scope)
       : m_scope(&scope)
    {}

    enum EOpFlags
    {
        fOpFlags_RelaxedAA = 1 << 0, ///< try assumbing all-uppercase three-letter AA representation
        fOpFlags_Default = fOpFlags_RelaxedAA
    };
    typedef int TOpFlags;


    CRef<CSeq_feat> AsVariationFeat(const string& hgvs_expression, TOpFlags = fOpFlags_Default);
    string AsHgvsExpression(const CSeq_feat& feat);

    CScope& SetScope()
    {
        return *m_scope;
    }

    class CHgvsParserException : public CException
    {
    public:
        enum EErrCode {
            eLogic,         ///<Problem with the code
            eGrammatic,     ///<Expression is not a valid language
            eSemantic,      ///<Expression is invalid in some way
            eContext,       ///<Some problem with context
            eAlignment,     ///<Some problem with getting alignment
            ePrecondition,  ///<Precondition is not met
            eOther,
        };

        virtual const char* GetErrCodeString(void) const
        {
            switch(GetErrCode()) {
            case eLogic:            return "eLogic";
            case eGrammatic:        return "eGrammatic";
            case eSemantic:         return "eSemantic";
            case eContext:          return "eContext";
            case eAlignment:        return "eAlignment";
            case ePrecondition:     return "ePrecondition";
            case eOther:            return "eOther";
            default:                return CException::GetErrCodeString();
            }
        }
        NCBI_EXCEPTION_DEFAULT(CHgvsParserException, CException);
    };


protected:

    struct SFuzzyInt
    {
        SFuzzyInt()
        {
            Reset();
        }

        void Assign(const SFuzzyInt& other) {
            value = other.value;
            if(!other.fuzz) {
                fuzz.Reset();
            } else {
                if(!fuzz) {
                    fuzz.Reset(new CInt_fuzz);
                }
                fuzz->Assign(*other.fuzz);
            }
        }

        void Reset()
        {
            value = 0;
            fuzz.Reset();
        }

        long value;
        CRef<CInt_fuzz> fuzz; //can be null;
    };

    struct SOffsetPoint
    {
        SOffsetPoint()
        {
            Reset();
        }

        bool IsOffset() const {
            return offset.value || offset.fuzz;
        }

        void Reset()
        {
            pnt.Reset();
            offset.Reset();
        }

        void Assign(const SOffsetPoint& other)
        {
            offset.Assign(other.offset);
            if(!other.pnt) {
                pnt.Reset();
            } else {
                if(!pnt) {
                    pnt.Reset(new CSeq_point);
                }
                pnt->Assign(*other.pnt);
            }
        }

        string asserted_sequence;
        CRef<CSeq_point> pnt;
        SFuzzyInt offset;
    };

    struct SOffsetLoc
    {
        SOffsetLoc()
        {
            Reset();
        }

        void Reset()
        {
            loc.Reset();
            start_offset.Reset();
            stop_offset.Reset();
        }

        void Assign(const SOffsetLoc& other)
        {
            start_offset.Assign(other.start_offset);
            stop_offset.Assign(other.stop_offset);
            if(!other.loc) {
                loc.Reset();
            } else {
                if(!loc) {
                    loc.Reset(new CSeq_loc);
                }
                loc->Assign(*other.loc);
            }
        }

        bool IsOffset() const
        {
            return start_offset.value || start_offset.value || stop_offset.fuzz || stop_offset.fuzz;
        }

        TSeqPos GetLength() const;

        string asserted_sequence;
        CRef<CSeq_loc> loc;
        SFuzzyInt start_offset;
        SFuzzyInt stop_offset;
    };

    /*!
     * CContext encapsulates sequence or location context for an hgvs sub-expression.
     * E.g. given an expression id:c.5_10delinsAT, when creating a variation-ref
     * for delinsAT the context will refer to sequence "id" and location "5_10"
     */
    class CContext
    {
    public:
        CContext(CRef<CScope> scope)
          : m_scope(scope)
        {
            Clear();
        }

        CContext(const CContext& other)
        {
            *this = other; //shallow copy will suffice.
        }

        enum EMolType {
            eMol_not_set,
            eMol_g,
            eMol_c,
            eMol_r,
            eMol_p,
            eMol_mt
        };

        void Clear()
        {
            m_bsh.Reset();
            m_mol_type = eMol_not_set;
            m_cds.Reset();
            m_seq_id.Reset();
            m_loc.Reset();
        }

        TSeqPos GetLength() const
        {
            return m_bsh.GetBioseqLength();
        }

        /*!
         * Clear the context and reset it for the given seq-id.
         * If the sequence is cdna and we're working with "c." coordinates,
         * also find the CDS, as the coordinates are (start|stop)codon-relative.
         */
        void SetId(const CSeq_id& id, EMolType mol_type);

        void Validate(const CSeq_literal& literal) const
        {
            if(!m_loc.IsOffset()) {
                //Can only validate normal locs, as with offset loc the asserted
                //allele does not correspond to the base loc.
                Validate(literal, GetLoc());
            } else {
                //LOG_POST("Ignoring validation of literal due to offset location");
            }
        }

        void Validate(const CSeq_literal& literal, const CSeq_loc& loc) const;

        void SetLoc(const SOffsetLoc& loc)
        {
            m_loc.Assign(loc);
        }

        bool IsSetLoc() const
        {
            return !m_loc.loc.IsNull();
        }

        CScope& GetScope() const
        {
            return *m_scope;
        }

        const CSeq_loc& GetLoc() const;

        const SOffsetLoc& GetOffsetLoc() const;

        const CSeq_id& GetId() const;

        const CSeq_feat& GetCDS() const;

        EMolType GetMolType(bool check=true) const;


    private:
        CBioseq_Handle m_bsh;
        EMolType m_mol_type;
        CRef<CSeq_feat> m_cds;
        CRef<CSeq_id> m_seq_id;
        SOffsetLoc m_loc;
        mutable CRef<CScope> m_scope;
    };

    struct SGrammar: public grammar<SGrammar>
    {
        /*!
         * Deviations from the recommendations:
         *
         * Flattened compound lists are not supported (a list must have same delimiter type)
         *     [[[a,b];c;f](+)[d,e]] - OK
         *     [a,b;c;f(+)d,e] - NOT OK
         *
         * No mixing of different expression types within a list.
         * Note: this example is not mentioned in specifications, but found it in Mutalyzer docs.
         * AB026906:c.[1del;6_7insoAL449423.14(CDKN2A):c.[1_10;5del]]
         *    1_10 specifies a location
         *    5del specifies a variation instance
         *    [1_10;5del] is not a valid expression
         *
         * Only seq-id header containing actual seq-id (gi or acc.ver) are supported:
         * Y : NM_004006.1:c.3G>T - uses a GenBank file as indicator
           N : GJB2:c.76A>C - uses a HGNC-approved gene symbol as indicator
           Y : DMD{NM_004006.1}:c.3G>T - uses both a HGNC-approved gene symbol and a GenBank file as indicator
           N : chrX:g.32,218,983_32,984,039del - uses a chromosome indicator (here X)
           N : rs2306220:A>G - using a dbSNP-identifier as indicator
           N : DXS1219:g.CA[18] (or AFM297yd1:g.CA[18]) - uses marker DXS1219 / AFM297yd1 as indicator
           Note: cases below are not described in recommendations, but found in Mutalyzer docs.
           N : AL449423.14(CDKN2A_v003):g.10del - not in symbol{seq_id} format
           N : CDKN2A_v003{AL449423.14}:c.1_*3352del cDNA coordinates on a genomic sequence.
         *
         */

        enum E_NodeIds {
            eID_NONE = 0,
            eID_root,
            eID_list1a,
            eID_list2a,
            eID_list3a,
            eID_list1b,
            eID_list2b,
            eID_list3b,
            eID_expr1,
            eID_expr2,
            eID_expr3,
            eID_translocation,
            eID_header,
            eID_seq_id,
            eID_mut_list,
            eID_mut_ref,
            eID_mol,
            eID_int_fuzz,
            eID_abs_pos,
            eID_general_pos,
            eID_fuzzy_pos,
            eID_pos_spec,
            eID_location,
            eID_nuc_range,
            eID_prot_range,
            eID_mut_inst,
            eID_raw_seq,
            eID_aminoacid,
            eID_nuc_subst,
            eID_deletion,
            eID_insertion,
            eID_delins,
            eID_duplication,
            eID_nuc_inv,
            eID_ssr,
            eID_conversion,
            eID_seq_loc,
            eID_seq_ref,
            eID_prot_pos,
            eID_prot_fs,
            eID_prot_missense,
            eID_prot_ext
        };


        typedef std::map<parser_id, std::string> TRuleNames;
        static TRuleNames s_rule_names;
        static TRuleNames& s_GetRuleNames()
        {
            if(s_rule_names.size() == 0) {
                s_rule_names[eID_NONE]              = "NONE";
                s_rule_names[eID_root]              = "root";
                s_rule_names[eID_list1a]            = "list1a";
                s_rule_names[eID_list2a]            = "list2a";
                s_rule_names[eID_list3a]            = "list3a";
                s_rule_names[eID_list1b]            = "list1b";
                s_rule_names[eID_list2b]            = "list2b";
                s_rule_names[eID_list3b]            = "list3b";
                s_rule_names[eID_expr1]             = "expr1";
                s_rule_names[eID_expr2]             = "expr2";
                s_rule_names[eID_expr3]             = "expr3";
                s_rule_names[eID_translocation]     = "translocation";
                s_rule_names[eID_header]            = "header";
                s_rule_names[eID_location]          = "location";
                s_rule_names[eID_mol]               = "mol";
                s_rule_names[eID_seq_id]            = "seq_id";
                s_rule_names[eID_mut_list]          = "mut_list";
                s_rule_names[eID_mut_ref]           = "mut_ref";
                s_rule_names[eID_nuc_range]         = "nuc_range";
                s_rule_names[eID_prot_range]        = "prot_range";
                s_rule_names[eID_mut_inst]          = "mut_inst";
                s_rule_names[eID_int_fuzz]          = "int_fuzz";
                s_rule_names[eID_abs_pos]           = "abs_pos";
                s_rule_names[eID_general_pos]       = "general_pos";
                s_rule_names[eID_fuzzy_pos]         = "fuzzy_pos";
                s_rule_names[eID_pos_spec]          = "pos_spec";
                s_rule_names[eID_raw_seq]           = "raw_seq";
                s_rule_names[eID_aminoacid]         = "aminoacid";
                s_rule_names[eID_nuc_subst]         = "nuc_subst";
                s_rule_names[eID_deletion]          = "deletion";
                s_rule_names[eID_insertion]         = "insertion";
                s_rule_names[eID_delins]            = "delins";
                s_rule_names[eID_duplication]       = "duplication";
                s_rule_names[eID_nuc_inv]           = "nuc_inv";
                s_rule_names[eID_ssr]               = "ssr";
                s_rule_names[eID_conversion]        = "conversion";
                s_rule_names[eID_seq_loc]           = "seq_loc";
                s_rule_names[eID_seq_ref]           = "seq_ref";
                s_rule_names[eID_prot_pos]          = "prot_pos";
                s_rule_names[eID_prot_fs]           = "prot_fs";
                s_rule_names[eID_prot_missense]     = "prot_missense";
                s_rule_names[eID_prot_ext]          = "prot_ext";
            }
            return s_rule_names;
        }

        static const string& s_GetRuleName(parser_id id);

        template <typename ScannerT>
        struct definition
        {
            rule<ScannerT, parser_context<>, parser_tag<eID_root> >             root;
            rule<ScannerT, parser_context<>, parser_tag<eID_list1a> >           list1a;
            rule<ScannerT, parser_context<>, parser_tag<eID_list2a> >           list2a;
            rule<ScannerT, parser_context<>, parser_tag<eID_list3a> >           list3a;
            rule<ScannerT, parser_context<>, parser_tag<eID_list1b> >           list1b;
            rule<ScannerT, parser_context<>, parser_tag<eID_list2b> >           list2b;
            rule<ScannerT, parser_context<>, parser_tag<eID_list3b> >           list3b;
            rule<ScannerT, parser_context<>, parser_tag<eID_expr1> >            expr1;
            rule<ScannerT, parser_context<>, parser_tag<eID_expr2> >            expr2;
            rule<ScannerT, parser_context<>, parser_tag<eID_expr3> >            expr3;
            rule<ScannerT, parser_context<>, parser_tag<eID_translocation> >    translocation;
            rule<ScannerT, parser_context<>, parser_tag<eID_header> >           header;
            rule<ScannerT, parser_context<>, parser_tag<eID_seq_id> >           seq_id;
            rule<ScannerT, parser_context<>, parser_tag<eID_mol> >              mol;
            rule<ScannerT, parser_context<>, parser_tag<eID_mut_list > >        mut_list;
            rule<ScannerT, parser_context<>, parser_tag<eID_mut_ref> >          mut_ref;
            rule<ScannerT, parser_context<>, parser_tag<eID_mut_inst> >         mut_inst;
            rule<ScannerT, parser_context<>, parser_tag<eID_int_fuzz> >         int_fuzz;
            rule<ScannerT, parser_context<>, parser_tag<eID_abs_pos> >          abs_pos;
            rule<ScannerT, parser_context<>, parser_tag<eID_general_pos> >      general_pos;
            rule<ScannerT, parser_context<>, parser_tag<eID_fuzzy_pos> >        fuzzy_pos;
            rule<ScannerT, parser_context<>, parser_tag<eID_pos_spec> >         pos_spec;
            rule<ScannerT, parser_context<>, parser_tag<eID_location> >         location;
            rule<ScannerT, parser_context<>, parser_tag<eID_nuc_range> >        nuc_range;
            rule<ScannerT, parser_context<>, parser_tag<eID_prot_range> >       prot_range;
            rule<ScannerT, parser_context<>, parser_tag<eID_raw_seq> >          raw_seq;
            rule<ScannerT, parser_context<>, parser_tag<eID_aminoacid> >        aminoacid;
            rule<ScannerT, parser_context<>, parser_tag<eID_nuc_subst> >        nuc_subst;
            rule<ScannerT, parser_context<>, parser_tag<eID_deletion> >         deletion;
            rule<ScannerT, parser_context<>, parser_tag<eID_insertion> >        insertion;
            rule<ScannerT, parser_context<>, parser_tag<eID_delins> >           delins;
            rule<ScannerT, parser_context<>, parser_tag<eID_duplication> >      duplication;
            rule<ScannerT, parser_context<>, parser_tag<eID_nuc_inv> >          nuc_inv;
            rule<ScannerT, parser_context<>, parser_tag<eID_ssr> >              ssr;
            rule<ScannerT, parser_context<>, parser_tag<eID_conversion> >       conversion;
            rule<ScannerT, parser_context<>, parser_tag<eID_seq_loc> >          seq_loc;
            rule<ScannerT, parser_context<>, parser_tag<eID_seq_ref> >          seq_ref;
            rule<ScannerT, parser_context<>, parser_tag<eID_prot_pos> >         prot_pos;
            rule<ScannerT, parser_context<>, parser_tag<eID_prot_missense> >    prot_missense;
            rule<ScannerT, parser_context<>, parser_tag<eID_prot_ext> >         prot_ext;
            rule<ScannerT, parser_context<>, parser_tag<eID_prot_fs> >          prot_fs;

            definition(SGrammar const&)
            {
                aminoacid       = str_p("Ala")
                                | str_p("Asx")
                                | str_p("Cys")
                                | str_p("Asp")
                                | str_p("Glu")
                                | str_p("Phe")
                                | str_p("Gly")
                                | str_p("His")
                                | str_p("Ile")
                                | str_p("Lys")
                                | str_p("Leu")
                                | str_p("Met")
                                | str_p("Asn")
                                | str_p("Pro")
                                | str_p("Gln")
                                | str_p("Arg")
                                | str_p("Ser")
                                | str_p("Thr")
                                | str_p("Val")
                                | str_p("Trp")
                                | str_p("Tyr")
                                | str_p("Glx")
                                | chset<>("XARNDCEQGHILKMFPSTWYV") //"X" as in p.X110GlnextX17
                                ;

                raw_seq         = leaf_node_d[+aminoacid | +chset<>("ACGTN") | +chset<>("acgun")];
                    /*
                     * Note: there's no separation between protein, DNA and RNA sequences, as
                     * at the parse time it is not known without context which sequence it is - i.e.
                     * "AC" could be a dna sequence, or AlaCys. Hence, raw_seq will match any sequence
                     * and at the parse-tree transformation stage we'll create proper seq-type as
                     * the mol will be known then.
                     *
                     * Note: +aminoacid precedes +chset<>("ACGTN") so that we don't prematurely match T in Trp etc.
                     */


                /*
                 * Positions and Locations
                 */

                int_fuzz        = ch_p('(') >> (ch_p('?')|int_p) >> ch_p('_') >> (ch_p('?')|int_p) >> ch_p(')')
                                | ch_p('(') >> int_p >> ch_p(')') //note: not ch_p('?') here as this makes grammar amgiguous
                                | (ch_p('?')|int_p);

                abs_pos         = !ch_p('*') >> int_fuzz;
                    //Note: '*' means the location is CDS-stop-relative; cdna context required.
                    //Otherwise it is CDS-start-relative iff in cdna-context.

                general_pos     = (str_p("IVS") >> int_p | abs_pos) >> sign_p >> int_fuzz
                                | abs_pos;
                    //Warning: offset-pos must be followed by a sign because otherwise
                    //it is ambiguous whether c.123(1_2) denotes a SSR or a fuzzy intronic location


                fuzzy_pos       = discard_node_d[ch_p('(')]
                                >> general_pos
                                >> discard_node_d[ch_p('_')]
                                >> general_pos
                                >> discard_node_d[ch_p(')')];

                pos_spec        = general_pos   //intronic offset-pos
                                | fuzzy_pos     //(generalpos_generalpos)
                                | !ch_p('o') >> header >> pos_spec;               //far-loc, as in id:c.88+101_id2:c.355-1045del


                prot_pos        = raw_seq >> pos_spec; //raw_seq must be of length one (single aminoacid)

                prot_range      = prot_pos >> discard_node_d[ch_p('_')] >> prot_pos;

                nuc_range       = pos_spec >> discard_node_d[ch_p('_')] >> pos_spec;

                location        = nuc_range | pos_spec | prot_range | prot_pos;
                    //Note that this describes "local" coordinates within a sequence context, not a seq-loc

                /*
                 * Seq-ids and seq-locs
                 */
                seq_id          = leaf_node_d[alpha_p >> +(alnum_p | chset<>("._-|"))];

                mol             = str_p("mt") | chset<>("gcrpm"); //note: for 'mt.' also supporting 'm.'

                header          = seq_id
                                  >> !(discard_node_d[ch_p('{')]
                                       >> seq_id
                                       >> discard_node_d[ch_p('}')])
                                  >> discard_node_d[ch_p(':')]
                                  >> mol
                                  >> discard_node_d[ch_p('.')];
                   /*
                    * A the parsing stage we'll not require that the mutations for
                    * different mol-types don't mix, i.e. protein mutation type for a
                    * mol-type "c." - this will be deferred to the semantic check that will
                    * validate the parsed expression. The reason is that the g/c/r/p specs mostly
                    * overlap, so we can avoid exploding the grammar.
                    */

                seq_loc         = !ch_p('o') >> header >> location;

                seq_ref         = seq_loc       //far-loc
                                | (nuc_range|prot_range)      //local-loc of range-type, e.g. c.17_18ins5_16 http://www.hgvs.org/mutnomen/FAQ.html
                                                              //This is to exclude point-locs (see below)
                                | raw_seq       //literal sequence
                                | int_fuzz;     //unknown sequence of some length
                                                //  WARNING: this is ambiguous WRT local-loc!
                                                //  e.g. p.Glu5Valins2fsX3 - 2 in ins2 indicates sequence of length two, NOT the sequence at position 2.
                                                //  Hence, the local-locs above must be specified via range-types only.

                nuc_subst       = raw_seq >> ch_p('>') >> raw_seq; //semantic check: must be of length 1

                deletion        = str_p("del") >> !(raw_seq | int_p);

                duplication     = str_p("dup") >> !seq_ref;

                insertion       = str_p("ins") >> seq_ref;

                conversion      = str_p("con") >> seq_loc;

                delins          = str_p("del") >> !raw_seq >> str_p("ins") >> seq_ref;

                nuc_inv         = str_p("inv") >> !int_p;

                ssr             = !raw_seq >> ( int_fuzz - (ch_p('?')|int_p) //don't want to interpret 'id:5_6?' as ssr
                                              | list_p(discard_node_d[ch_p('[')]
                                                    >> int_p
                                                    >> discard_node_d[ch_p(']')],
                                                       discard_node_d[ch_p('+')]));
                    /*
                     * Note: It is not correct to match [5] and [3] in NM_000815.2:c.101TC[5]+[3]
                     * individually as list3a within c.101 location context, because
                     * the location-spec for an ssr may point to the first repeat-unit base only,
                     * in which case we need to calculate the actual range-loc based on the sequence literal,
                     * and when processing "[3]" later the "TC" literal will not be in context,
                     * and it will appear as if the context is NM_000815.2:c.101 rather than c.101_102.
                     *
                     * Hence, the ssr rule will have to consume the list of multipliers [5]+[3]
                     * and generate compound variation-ref "manually"
                     */


                prot_fs         = str_p("fs") >> !(ch_p('X') >> int_p);

                prot_ext        = (str_p("extMet") | str_p("extX")) >> int_p;

                prot_missense   = aminoacid;

                //ISCN expression followed by a seq-loc
                translocation = str_p("t(")
                                  >> leaf_node_d[*(print_p - ch_p('(') - ch_p(')'))]
                                  >> str_p(")(")
                                  >> leaf_node_d[*(print_p - ch_p('(') - ch_p(')'))]
                                  >> str_p(")")
                                  >> seq_loc;

                mut_inst        = ch_p('?') //can exist within +mut_inst, e.g. p.Met1?extMet-5
                                | ch_p('=')
                                | delins    //delins must precede del
                                | deletion
                                | insertion
                                | duplication
                                | nuc_subst
                                | nuc_inv
                                | ssr
                                | conversion
                                | prot_fs
                                | prot_missense
                                | prot_ext //this may occur inside location context, e.g. p.Met1ValextMet-12
                                ;
                    //Note: '?' and '=' can exist both within a location context
                    //(i.e. expr3) or within a sequence context (i.e. expr2)
                    //additionally, prot_ext may exist as expr2 (outside of location context) as well.


                root            = list_p(expr1, ch_p('+'));
                    //At the root level the '+'-delimited expressions are not required to be bracketed, i.e.
                    //NM_004004.2:c.[35delG]+NM_006783.1:c.[689_690insT] instead of
                    //[NM_004004.2:c.35delG]+[NM_006783.1:c.689_690insT]


                expr1           = ch_p('(') >> expr1 >> ch_p(')')
                                | list1a
                                | header >> expr2
                                | translocation;
                list1a          = list_p(discard_node_d[ch_p('[')] >> list1b >> discard_node_d[ch_p(']')], ch_p('+'));
                list1b          = list_p(expr1, chset<>(",;") | str_p("(+)"));


                expr2           = ch_p('(') >> expr2 >> ch_p(')')
                                | list2a
                                | str_p("0?") //note: precdes location>>expr3 such that not matched as unknown variation at pos=0 here
                                | ch_p('0')
                                | location >> expr3
                                | prot_ext    //can also exist within location context (mut_inst)
                                | ch_p('?')   //note: follows location>>expr3 such that variation at unknown pos is not partially-matched as unknown variation
                                | ch_p('=');
                list2a          = list_p(discard_node_d[ch_p('[')] >> list2b >> discard_node_d[ch_p(']')], ch_p('+'));
                list2b          = list_p(expr2, chset<>(",;") | str_p("(+)"));


                expr3           = ch_p('(') >> expr3 >> ch_p(')')
                                | list3a
                                | +mut_inst;
                    /*
                     * Note: Multiple mut_insts that are not delimited, i.e.
                     * abc instead of [a;b;c] are legit, e.g.
                     *
                     * a) p.X110GlnextX17
                     * b) NM_012345.3:c.123+45_123+51dupinsAB012345.3:g.393_1295
                     */
                list3a          = list_p(discard_node_d[ch_p('[')] >> list3b >> discard_node_d[ch_p(']')], ch_p('+'));
                list3b          = list_p(expr3, chset<>(",;") | str_p("(+)"));

                //BOOST_SPIRIT_DEBUG_RULE(expr1);
            }

            rule<ScannerT, parser_context<>, parser_tag<eID_root> > const& start() const
            {
                return root;
            }
        };

        static bool s_is_list(parser_id id)
        {
            return id == SGrammar::eID_list1a
                || id == SGrammar::eID_list2a
                || id == SGrammar::eID_list3a
                || id == SGrammar::eID_list1b
                || id == SGrammar::eID_list2b
                || id == SGrammar::eID_list3b
                || id == SGrammar::eID_root;
        }
    };
    static SGrammar s_grammar;


private:
    typedef tree_match<char const*> TParseTreeMatch;
    typedef TParseTreeMatch::const_tree_iterator TIterator;
    typedef CVariation_inst::TDelta::value_type TDelta;
    typedef CVariation_ref::TData::TSet TVariationSet;


    static SFuzzyInt x_int_fuzz (TIterator const& i, const CContext& context);

    static CRef<CSeq_point>      x_abs_pos         (TIterator const& i, const CContext& context);
    static SOffsetPoint          x_general_pos     (TIterator const& i, const CContext& context);
    static SOffsetPoint          x_fuzzy_pos       (TIterator const& i, const CContext& context);
    static SOffsetPoint          x_pos_spec        (TIterator const& i, const CContext& context);
    static SOffsetPoint          x_prot_pos        (TIterator const& i, const CContext& context);

    static SOffsetLoc            x_range           (TIterator const& i, const CContext& context);
    static SOffsetLoc            x_location        (TIterator const& i, const CContext& context);

    static CRef<CSeq_loc>        x_seq_loc         (TIterator const& i, const CContext& context);
    static CRef<CSeq_literal>    x_raw_seq         (TIterator const& i, const CContext& context);
    static TDelta                x_seq_ref         (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_identity        (const CContext& context);
    static CRef<CVariation_ref>  x_delins          (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_deletion        (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_insertion       (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_duplication     (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_nuc_subst       (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_nuc_inv         (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_ssr             (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_conversion      (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_prot_ext        (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_prot_missense   (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_translocation   (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_mut_inst        (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_expr1           (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_expr2           (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_expr3           (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_prot_fs         (TIterator const& i, const CContext& context);
    static CRef<CVariation_ref>  x_list            (TIterator const& i, const CContext& context);
    static CContext              x_header          (TIterator const& i, const CContext& context);
    static CRef<CSeq_feat>       x_root            (TIterator const& i, const CContext& context);

    static CRef<CVariation_ref>  x_unwrap_iff_singleton(CVariation_ref& v);


    ///Convert HGVS amino-acid code to ncbieaa
    static string s_hgvsaa2ncbieaa(const string& hgvsaa);

    ///Convert non-HGVS compliant all-uppercase AAs to UpLow, e.g. ILECYS ->IleCys
    static string s_hgvsUCaa2hgvsUL(const string& hgvsaa);

private:
    //functions to create hgvs expression from a variation-ref

    /// variatino must have seq-loc specified
    string x_AsHgvsExpression(const CVariation_ref& variation,
                            const CSeq_loc& parent_loc, //if variation has seq-loc set, it will be used instead.
                            bool is_top_level);


    //Calculate the length of the inst, multipliers taken into account.
    TSeqPos x_GetInstLength(const CVariation_inst& inst, const CSeq_loc& this_loc);

    string x_GetInstData(const CVariation_inst& inst, const CSeq_loc& this_loc);


    /// Only subset of insts can be expressed as HGVS, this will throw otherwise.
    /// loc is required to capture sequence at loc for snps, e.g. 'A' in 'A>G'
    ///
    /// loc is modified to conform to HGVS flavor (on plus strand, modify
    /// starts/stops for ins or microsatellite locatinos to conform to HGVS
    /// convention where differes from Variation-ref standard)
    string x_InstToString(const CVariation_inst& inst, CSeq_loc& loc);

    string x_LocToSeqStr(const CSeq_loc& loc);

    /*
     * Convert a loc to hgvs representation.
     *
     *
     * if alignment is provided (spliced-seg), then it is assumed that loc is the
     * genomic loc but the output should be represented in cdna context.
     */
    string x_SeqLocToStr(const CSeq_loc& loc, bool with_header);

    // force-map: remap via alignment
    string x_SeqPntToStr(const CSeq_point& pnt, TSeqPos first_pos);

    /// Convert seq-id to HGVS seq-id header, e.g. "NM_123456.7:c." or "NG_123456.7:p"
    string x_SeqIdToHgvsHeader(const CSeq_id& id);

    string x_IntWithFuzzToStr(int value, const CInt_fuzz* fuzz = NULL);

    //string Delta_itemToStr(const CDelta_item& delta, bool flip_strand);

    string x_SeqLiteralToStr(const CSeq_literal& literal, bool flip_strand);



    static CRef<CVariation_ref> s_ProtToCdna(const CVariation_ref& vr, CScope& scope);

private:
    CRef<CScope> m_scope;
};

};

END_NCBI_SCOPE;


#endif /* HGVSPARSER_HPP_ */

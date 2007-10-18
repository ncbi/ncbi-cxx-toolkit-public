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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/readers/glimmer_reader.hpp>
#include <objtools/error_codes.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objmgr/util/sequence.hpp>

#include <corelib/ncbiutil.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_Glimmer

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


CGlimmerReader::CGlimmerReader()
{
}


CRef<CSeq_entry> CGlimmerReader::Read(CNcbiIstream& istr, CScope& scope,
                                      int genetic_code_idx)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    CRef<CSeq_annot> annot(new CSeq_annot);
    entry->SetSet().SetSeq_set();
    entry->SetSet().SetAnnot().push_back(annot);

    /// parse line by line
    /// we will be lax and skip enpty lines; we will also permit a few comments
    string line;
    string defline;
    CSeq_id_Handle idh;
    TSeqPos seq_length = 0;

    size_t errs = 0;
    size_t count = 0;
    while (NcbiGetlineEOL(istr, line)) {
        ++count;
        if (line.empty()  ||  line[0] == '#'  ||
            (line.size() >= 2  &&  line[0] == '/'  &&  line[1] == '/')) {
            continue;
        }

        if (defline.empty()) {
            if (line[0] == '>') {
                defline = line;
                string s = defline;
                string::size_type pos = s.find_first_of(" ");
                if (pos != string::npos) {
                    s.erase(pos);
                }
                s.erase(0, 1);

                CBioseq::TId ids;
                CSeq_id::ParseFastaIds(ids, s);

                CRef<CSeq_id> best = FindBestChoice(ids, CSeq_id::Score);
                idh = CSeq_id_Handle::GetHandle(*best);

                CBioseq_Handle bsh = scope.GetBioseqHandle(idh);
                if ( !bsh ) {
                    NCBI_THROW(CException, eUnknown,
                               "Failed to find sequence: " + s);
                }
                seq_length = bsh.GetBioseqLength();
            } else {
                CNcbiOstrstream ostr;
                ostr << "CGlimmerReader::ReadAnnot(): line "
                    << count << ": failed to identify defline: " << line;
                string msg = string(CNcbiOstrstreamToString(ostr));
                LOG_POST_X(1, Error << msg);
                NCBI_THROW(CException, eUnknown, msg);
            }
        } else {
            list<string> toks;
            NStr::Split(line, " \t", toks);
            if (toks.size() != 5) {
                CNcbiOstrstream ostr;
                ostr << "CGlimmerReader::ReadAnnot(): line "
                    << count << ": invalid number of tokens: "
                    << "found " << toks.size() << ", expected 5: " << line;
                string msg = string(CNcbiOstrstreamToString(ostr));
                LOG_POST_X(2, Error << msg);
                ++errs;
                if (errs > 5) {
                    NCBI_THROW(CException, eUnknown, msg);
                }
            }

            list<string>::iterator it = toks.begin();

            /// token 1: ORF identifier
            string orf_name = *it++;

            /// token 2: start position
            TSeqPos start_pos = 0;
            try {
                start_pos = NStr::StringToInt(*it++);
                start_pos -= 1;
            }
            catch (CException&) {
                CNcbiOstrstream ostr;
                ostr << "CGlimmerReader::ReadAnnot(): line "
                    << count << ": failed to identify start pos: " << line;
                string msg = string(CNcbiOstrstreamToString(ostr));
                LOG_POST_X(3, Error << msg);

                ++errs;
                if (errs > 5) {
                    NCBI_THROW(CException, eUnknown, msg);
                } else {
                    continue;
                }
            }

            /// token 3: stop position
            TSeqPos stop_pos = 0;
            try {
                stop_pos = NStr::StringToInt(*it++);
                stop_pos -= 1;
            }
            catch (CException&) {
                CNcbiOstrstream ostr;
                ostr << "CGlimmerReader::ReadAnnot(): line "
                    << count << ": failed to identify stop pos: " << line;
                string msg = string(CNcbiOstrstreamToString(ostr));
                LOG_POST_X(4, Error << msg);

                ++errs;
                if (errs > 5) {
                    NCBI_THROW(CException, eUnknown, msg);
                } else {
                    continue;
                }
            }

            /// stop may be less than start!

            /// token 4: frame + strand
            ENa_strand strand = eNa_strand_plus;
            try {
                int frame = NStr::StringToInt(*it++);
                if (frame > 3  ||  frame < -3) {
                    NCBI_THROW(CException, eUnknown, "frame out of range");
                }

                if (frame < 0) {
                    strand = eNa_strand_minus;
                }
            }
            catch (CException&) {
                CNcbiOstrstream ostr;
                ostr << "CGlimmerReader::ReadAnnot(): line "
                    << count << ": failed to identify frame: " << line;
                string msg = string(CNcbiOstrstreamToString(ostr));
                LOG_POST_X(5, Error << msg);

                ++errs;
                if (errs > 5) {
                    NCBI_THROW(CException, eUnknown, msg);
                } else {
                    continue;
                }
            }

            /// token 5: score
            double score = 0;
            try {
                score = NStr::StringToDouble(*it++);
            }
            catch (CException&) {
                CNcbiOstrstream ostr;
                ostr << "CGlimmerReader::ReadAnnot(): line "
                    << count << ": failed to identify score: " << line;
                string msg = string(CNcbiOstrstreamToString(ostr));
                LOG_POST_X(6, Error << msg);

                ++errs;
                if (errs > 5) {
                    NCBI_THROW(CException, eUnknown, msg);
                } else {
                    continue;
                }
            }

            ///
            /// build our features
            ///

            /// CDS feat
            CRef<CSeq_feat> cds_feat(new CSeq_feat());
            if (strand == eNa_strand_plus  &&  start_pos > stop_pos) {
                /// circular cds_feature; make two intervals
                CRef<CSeq_interval> ival;

                ival.Reset(new CSeq_interval);
                ival->SetFrom(start_pos);
                ival->SetTo  (seq_length - 1);
                cds_feat->SetLocation().SetPacked_int().Set().push_back(ival);

                ival.Reset(new CSeq_interval);
                ival->SetFrom(0);
                ival->SetTo  (stop_pos);
                cds_feat->SetLocation().SetPacked_int().Set().push_back(ival);

            } else if (strand == eNa_strand_minus  &&  start_pos < stop_pos) {
                /// circular cds_feature; make two intervals
                CRef<CSeq_interval> ival;

                ival.Reset(new CSeq_interval);
                ival->SetFrom(0);
                ival->SetTo  (start_pos);
                cds_feat->SetLocation().SetPacked_int().Set().push_back(ival);

                ival.Reset(new CSeq_interval);
                ival->SetFrom(stop_pos);
                ival->SetTo  (seq_length - 1);
                cds_feat->SetLocation().SetPacked_int().Set().push_back(ival);

            } else {
                cds_feat->SetLocation().SetInt().SetFrom(min(start_pos, stop_pos));
                cds_feat->SetLocation().SetInt().SetTo  (max(start_pos, stop_pos));
            }
            cds_feat->SetLocation().SetStrand(strand);
            cds_feat->SetLocation().SetId(*idh.GetSeqId());

            CCdregion& cdr = cds_feat->SetData().SetCdregion();
            if (genetic_code_idx) {
                CRef<CGenetic_code::C_E> d(new CGenetic_code::C_E);
                d->SetId(genetic_code_idx);
                cdr.SetCode().Set().push_back(d);
            }

            CRef<CSeq_feat> gene_feat(new CSeq_feat);
            gene_feat->SetData().SetGene().SetLocus(orf_name);
            gene_feat->SetLocation().Assign(cds_feat->GetLocation());

            annot->SetData().SetFtable().push_back(gene_feat);
            annot->SetData().SetFtable().push_back(cds_feat);
        }
    }
    LOG_POST_X(7, Info << "CGlimmerReader::Read(): parsed " << count << " lines, " << errs << " errors");

    string prefix("lcl|prot");
    count = 0;
    NON_CONST_ITERATE (CSeq_annot::TData::TFtable, it, annot->SetData().SetFtable()) {
        CSeq_feat& feat = **it;
        if (feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_cdregion) {
            continue;
        }

        CRef<CSeq_entry> sub_entry(new CSeq_entry);
        CBioseq& bioseq = sub_entry->SetSeq();

        /// establish our inst
        CSeq_inst& inst = bioseq.SetInst();
        CCdregion_translate::TranslateCdregion(inst.SetSeq_data().SetIupacaa().Set(),
                                               **it, scope,
                                               false /* trim trailing stop */);
        inst.SetRepr(CSeq_inst::eRepr_raw);
        inst.SetMol(CSeq_inst::eMol_aa);
        inst.SetLength(inst.SetSeq_data().SetIupacaa().Set().size());

        /// create a readable seq-id
        CNcbiOstrstream ostr;
        ostr << prefix << setw(7) << setfill('0') << ++count;
        string id_str = string(CNcbiOstrstreamToString(ostr));

        CRef<CSeq_id> id(new CSeq_id(id_str));
        bioseq.SetId().push_back(id);

        /// set the product on the feature
        feat.SetProduct().SetWhole().Assign(*id);

        /// save our bioseq
        /// this is done last to preserve our data in a serializable form
        entry->SetSet().SetSeq_set().push_back(sub_entry);
    }

    return entry;
}


END_NCBI_SCOPE

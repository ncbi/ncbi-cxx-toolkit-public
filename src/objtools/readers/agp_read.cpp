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
 * Authors: Josh Cherry
 *
 * File Description:  Read agp file
 */


#include <ncbi_pch.hpp>
#include <objtools/readers/agp_read.hpp>
#include <objtools/readers/reader_exception.hpp>

#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/general/Object_id.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


CRef<CBioseq_set> AgpRead(CNcbiIstream& is, EAgpRead_IdRule component_id_rule)
{
    vector<CRef<CSeq_entry> > entries;
    AgpRead(is, entries, component_id_rule);
    CRef<CBioseq_set> bioseq_set(new CBioseq_set);
    ITERATE (vector<CRef<CSeq_entry> >, iter, entries) {
        bioseq_set->SetSeq_set().push_back(*iter);
    }
    return bioseq_set;
}


void AgpRead(CNcbiIstream& is,
             vector<CRef<CSeq_entry> >& entries,
             EAgpRead_IdRule component_id_rule)
{
    vector<CRef<CBioseq> > bioseqs;
    AgpRead(is, bioseqs, component_id_rule);
    NON_CONST_ITERATE (vector<CRef<CBioseq> >, bioseq, bioseqs) {
        CRef<CSeq_entry> entry(new CSeq_entry);
        entry->SetSeq(**bioseq);
        entries.push_back(entry);
    }
}


void AgpRead(CNcbiIstream& is,
             vector<CRef<CBioseq> >& bioseqs,
             EAgpRead_IdRule component_id_rule)
{
    string line;
    vector<string> fields;
    string current_object;
    CRef<CBioseq> bioseq;
    CRef<CSeq_inst> seq_inst;
    int last_to;
    int part_num, last_part_num;
    TSeqPos length = 0;  // initialize to avoid compilation warning

    int line_num = 0;
    while (NcbiGetlineEOL(is, line)) {
        line_num++;
        if (line[0] == '#') {
            // comment line; skip
            continue;
        }
        if (line.find_first_not_of(" \t\n\r") == string::npos) {
            // skip lines containing only white space
            continue;
        }

        fields.clear();
        NStr::Tokenize(line, "\t", fields);

        // eliminate any empty fields at the end of the line
        int index;
        for (index = (int) fields.size() - 1;  index > 0;  --index) {
            if (!fields[index].empty()) {
                break;
            }
        }
        fields.resize(index + 1);

        // Number of fields can be 9 or 8, but 8 is valid
        // only if field[4] == "N" or "U".
        if (fields.size() != 9) {
            if (fields.size() >= 5 && fields[4] != "N" && fields[4] != "U") {
                NCBI_THROW2(CObjReaderParseException, eFormat,
                            string("error at line ") + 
                            NStr::IntToString(line_num) + ": found " +
                            NStr::IntToString(fields.size()) +
                            " columns; there should be 9",
                            is.tellg() - CT_POS_TYPE(0));
            } else if (fields.size() != 8) {
                NCBI_THROW2(CObjReaderParseException, eFormat,
                            string("error at line ") + 
                            NStr::IntToString(line_num) + ": found " +
                            NStr::IntToString(fields.size()) +
                            " columns; there should be 8 or 9",
                            is.tellg() - CT_POS_TYPE(0));
            }
        }

        if (fields[0] != current_object || !bioseq) {
            // close out old one, start a new one
            if (bioseq) {
                seq_inst->SetLength(length);
                bioseq->SetInst(*seq_inst);
                bioseqs.push_back(bioseq);
            }

            current_object = fields[0];

            seq_inst.Reset(new CSeq_inst);
            seq_inst->SetRepr(CSeq_inst::eRepr_delta);
            seq_inst->SetMol(CSeq_inst::eMol_dna);

            bioseq.Reset(new CBioseq);
            CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Local,
                                         current_object, current_object));
            bioseq->SetId().push_back(id);

            last_to = 0;
            last_part_num = 0;
            length = 0;
        }

        // validity checks
        part_num = NStr::StringToInt(fields[3]);
        if (part_num != last_part_num + 1) {
            NCBI_THROW2(CObjReaderParseException, eFormat,
                        string("error at line ") + 
                        NStr::IntToString(line_num) +
                        ": part number out of order",
                        is.tellg() - CT_POS_TYPE(0));
        }
        last_part_num = part_num;
        if (NStr::StringToInt(fields[1]) != last_to + 1) {
            NCBI_THROW2(CObjReaderParseException, eFormat,
                        string("error at line ") + 
                         NStr::IntToString(line_num) +
                         ": begining not equal to previous end + 1",
                         is.tellg() - CT_POS_TYPE(0));
        }
        last_to = NStr::StringToInt(fields[2]);


        // build a Delta-seq, either a Seq-literal (for a gap) or a Seq-loc 

        CRef<CDelta_seq> delta_seq(new CDelta_seq);

        if (fields[4] == "N" || fields[4] == "U") {
            // a gap
            TSeqPos gap_len = NStr::StringToInt(fields[5]);
            delta_seq->SetLiteral().SetLength(gap_len);
            if (fields[4] == "U") {
                delta_seq->SetLiteral().SetFuzz().SetLim();
            }
            length += gap_len;
        } else if (fields[4].size() == 1 && 
                   fields[4].find_first_of("ADFGPOW") == 0) {
            CSeq_loc& loc = delta_seq->SetLoc();

            // Component ID
            CRef<CSeq_id> comp_id;
            if (component_id_rule != eAgpRead_ForceLocalId) {
                try {
                    comp_id.Reset(new CSeq_id(fields[5]));
                }
                catch (invalid_argument& /*e*/) {
                    comp_id.Reset(new CSeq_id);
                }
            } else {
                comp_id.Reset(new CSeq_id);
            }
            if (comp_id->Which() == CSeq_id::e_not_set) {
                // not a recognized format, or request to force a local id
                comp_id->SetLocal().SetStr(fields[5]);
            }
            loc.SetInt().SetId(*comp_id);

            loc.SetInt().SetFrom(NStr::StringToInt(fields[6]) - 1);
            loc.SetInt().SetTo  (NStr::StringToInt(fields[7]) - 1);
            length += loc.GetInt().GetTo() - loc.GetInt().GetFrom() + 1;
            if (fields[8] == "+") {
                loc.SetInt().SetStrand(eNa_strand_plus);
            } else if (fields[8] == "-") {
                loc.SetInt().SetStrand(eNa_strand_minus);
            } else if (fields[8] == "0") {
                loc.SetInt().SetStrand(eNa_strand_unknown);
            } else if (fields[8] == "na") {
                loc.SetInt().SetStrand(eNa_strand_other);
            } else {
                NCBI_THROW2(CObjReaderParseException, eFormat,
                            string("error at line ") + 
                            NStr::IntToString(line_num) + ": invalid "
                            "orientation " + fields[8],
                            is.tellg() - CT_POS_TYPE(0));
            }
        } else {
            NCBI_THROW2(CObjReaderParseException, eFormat,
                        string("error at line ") + 
                        NStr::IntToString(line_num) + ": invalid "
                        "component type " + fields[4],
                        is.tellg() - CT_POS_TYPE(0));
        }
        seq_inst->SetExt().SetDelta().Set().push_back(delta_seq);
    }

    // deal with the last one
    if (bioseq) {
        seq_inst->SetLength(length);
        bioseq->SetInst(*seq_inst);
        bioseqs.push_back(bioseq);
    }
}


END_NCBI_SCOPE


/*
 * =====================================================================
 * $Log$
 * Revision 1.13  2005/04/26 15:36:18  vasilche
 * Fixed warning about unused variable.
 *
 * Revision 1.12  2005/02/28 16:05:31  jcherry
 * Initialize variable to eliminate compilation warning
 *
 * Revision 1.11  2005/01/26 21:24:58  jcherry
 * Fix for "force local id" case
 *
 * Revision 1.10  2005/01/26 20:58:48  jcherry
 * More robust and controllable handling of component ids
 *
 * Revision 1.9  2004/07/08 14:22:19  jcherry
 * Handle Seq-ids of unrecognized format as local ids
 *
 * Revision 1.8  2004/06/30 20:57:54  jcherry
 * Handle new component type "U", which specifies a gap whose
 * length is unknown.
 *
 * Revision 1.7  2004/06/14 18:16:28  jcherry
 * Tolerate tabs at ends of lines
 *
 * Revision 1.6  2004/05/25 20:49:58  jcherry
 * Let the last column of an "N" line be missing, not just empty
 *
 * Revision 1.5  2004/05/21 21:42:55  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.4  2004/02/19 22:57:52  ucko
 * Accommodate stricter implementations of CT_POS_TYPE.
 *
 * Revision 1.3  2004/01/05 23:01:37  jcherry
 * Support unknown ("0") or irrelevant ("na") strand designation
 *
 * Revision 1.2  2003/12/08 23:39:20  jcherry
 * Set length of Seq-inst
 *
 * Revision 1.1  2003/12/08 15:49:32  jcherry
 * Initial version
 *
 * =====================================================================
 */

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
 * Authors:  Denis Vakatov
 *
 * File Description:
 *   PSG seq ids utils
 *
 */

#include <ncbi_pch.hpp>

#include "psgs_seq_id_utils.hpp"
#include "ipsgs_processor.hpp"
#include "pubseq_gateway_logging.hpp"
#include "pubseq_gateway.hpp"

#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_id.hpp>
USING_NCBI_SCOPE;
using namespace ncbi::objects;



// The lower the return value the higher the priority
static int s_PSGRank(const CSeq_id& seq_id)
{
    switch (seq_id.Which()) {
    case CSeq_id::e_not_set:            return 83;
    case CSeq_id::e_Giim:               return 20;
    case CSeq_id::e_General:
    case CSeq_id::e_Gibbsq:
    case CSeq_id::e_Gibbmt:             return 15;
    case CSeq_id::e_Local:
    case CSeq_id::e_Patent:             return 10;
    case CSeq_id::e_Gpipe:
    case CSeq_id::e_Named_annot_track:  return  9;
    case CSeq_id::e_Gi:                 return  7;
    default:
        if (const CTextseq_id* text_id = seq_id.GetTextseq_Id()) {
            if ( !text_id->IsSetVersion() )
                return 8;
        }
        return  5;
    }
}

int PSGRank(const CSeq_id& seq_id)
{
    return seq_id.AdjustScore( s_PSGRank(seq_id) );
}


static bool s_SortSeqIdPredicate(const CRef<CSeq_id> &  lhs,
                                 const CRef<CSeq_id> &  rhs)
{
    return PSGRank(*lhs) < PSGRank(*rhs);
}


void PSGSortSeqIds(list<SPSGSeqId>& seq_ids,
                   IPSGS_Processor *  processor)
{
    list<SPSGSeqId>         unparsed_seq_ids;
    list<CRef<CSeq_id>>     parsed_seq_ids;

    for (const auto &  item : seq_ids) {
        CRef<CSeq_id>     seq_id(new CSeq_id);
        if (processor->ParseInputSeqId(*seq_id, item.seq_id,
                                       item.seq_id_type, nullptr) == ePSGS_ParsedOK) {
            parsed_seq_ids.push_back(seq_id);
        } else {
            unparsed_seq_ids.push_back(item);
        }
    }

    // Sort the parsed ones
    parsed_seq_ids.sort(s_SortSeqIdPredicate);

    // Repopulate in the appropriate order. First the parsed seq_ids
    seq_ids.clear();
    for (const auto & item : parsed_seq_ids) {
        auto    parsed_seq_id_type = item->Which();
        if (parsed_seq_id_type == CSeq_id_Base::e_not_set) {
            seq_ids.push_back(SPSGSeqId{-1, item->AsFastaString()});
        } else {
            seq_ids.push_back(SPSGSeqId{static_cast<int16_t>(parsed_seq_id_type),
                                        item->AsFastaString()});
        }
    }

    // Then the not parseable seq_ids
    seq_ids.splice(seq_ids.end(), unparsed_seq_ids);
}


string StripTrailingVerticalBars(const string &  seq_id)
{
    string      stripped_v_bars = seq_id;
    while (!stripped_v_bars.empty() && stripped_v_bars[stripped_v_bars.size() - 1] == '|') {
        stripped_v_bars.erase(stripped_v_bars.size() - 1, 1);
    }
    return stripped_v_bars;
}


CSeq_id_Base::E_Choice   DetectSeqIdTypeForIPG(const string &  seq_id)
{
    try {
        CSeq_id     parsed_seq_id(seq_id, CSeq_id::fParse_RawGI|CSeq_id::fParse_RawText);
        return parsed_seq_id.Which();
    } catch (...) {
        return CSeq_id_Base::e_not_set;
    }
    return CSeq_id_Base::e_not_set;
}


// The function returns true if the user provided string looks like it could be
// an accession. It is called only if the CSeq_id parsing failed so some of the
// return values are not expected, thus there will be a log message and an
// alert.
bool IsAccessionLike(const string &  user_input)
{
    auto    accession = CSeq_id::AssessAccession(user_input);
    switch (accession) {
        case CSeq_id::eInvalid:
        case CSeq_id::eLocalOnly:
            return false;
        case CSeq_id::ePlausible:
        case CSeq_id::eUnreserved:
            return true;
        case CSeq_id::eIdentifiable:
        case CSeq_id::eTagged:
            {
                string  err_msg =
                    "Inconsistency detected: CSeq_id parsing failed but "
                    "CSeq_id::AssessAccession() returned eIdentifiable or eTagged: " +
                    to_string(accession) +
                    ".";
                PSG_ERROR(err_msg +
                          " Continue with an additional try to lookup in BIOSEQ_INFO table.");

                CPubseqGatewayApp *     app = CPubseqGatewayApp::GetInstance();
                app->GetAlerts().Register(ePSGS_SeqIdInconsistencyParsingAndAssess,
                                          err_msg);
                return true;
            }
        default:
            PSG_ERROR("Unexpected accession received from CSeq_id::AssessAccession(): " +
                      to_string(accession) +
                      ". Continue with an additional try to lookup in BIOSEQ_INFO table.");
            return true;
    }
}


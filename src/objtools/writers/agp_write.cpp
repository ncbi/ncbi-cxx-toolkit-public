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
 * Authors:  Josh Cherry
 *
 * File Description:  Write agp file
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/writers/agp_write.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objmgr/seq_map_ci.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

static char s_DetermineComponentType(const CSeq_id& id, CScope& scope);

/// Write to stream in agp format.
/// Based on www.ncbi.nlm.nih.gov/Genbank/WGS.agpformat.html
void AgpWrite(CNcbiOstream& os,
              const CSeqMap& seq_map,
              const string& object_id,
              const string& gap_type,
              bool linkage,
              CScope& scope)
{
    int part_num = 1;
    int next_start = 1;
    for (CSeqMap_CI iter = seq_map.Begin(&scope);
         iter != seq_map.End(&scope);  iter.Next()) {

        // col 1
        os << object_id;
        // col 2
        os << '\t' << next_start;
        // col 3
        os << '\t' << next_start + iter.GetLength() - 1;
        next_start += iter.GetLength();
        // col 4
        os << '\t' << part_num;
        part_num += 1;

        if (iter.GetType() == CSeqMap::eSeqGap) {
            // col 5
            os << "\tN";
            // col 6b
            os << '\t' << iter.GetLength();
            // col 7b
            os << "\t" << gap_type;
            // col 8b
            os << "\t" << (linkage ? "yes" : "no");
            // col 9; write an empty column (Or should there be no column,
            //        i.e., no tab?  Our reader doesn't care.)
            os << '\t';
        } else if (iter.GetType() == CSeqMap::eSeqRef) {
            // col 5; Should be A, D, F, G, P, O, or W
            os << "\t" <<
                s_DetermineComponentType(*iter.GetRefSeqid().GetSeqId(),
                                         scope);
            // col 6a
            os << '\t' << iter.GetRefSeqid().GetSeqId()->GetSeqIdString(true);
            // col 7a
            os << '\t' << iter.GetRefPosition() + 1;
            // col 8a
            os << '\t' << iter.GetRefEndPosition();
            // col 9
            if (iter.GetRefMinusStrand()) {
                os << "\t-";
            } else {
                os << "\t+";
            }
        }
        os << endl;
    }
}


// This function tries to figure out the "component type"
// to put into column 5 (F, A, D, P, W, or O) by examining
// the referenced Bioseq.
// The relevant attributes are htgs-[0123] and wgs in MolInfo.tech
// and the GB-block keywords HTGS_DRAFT, HTGS_FULLTOP, and HTGS_ACTIVEFIN.
// The rules are as follows. wgs trumps everything, yieding W.
// htgs-[012] indicate P (pre-draft), htgs-3 indicates F (finished),
// HTGS_DRAFT and HTGS_FULLTOP indicate D (draft), and HTGS_ACTIVEFIN
// indicates A (active finishing).  If more than one of F, A, D, P
// are indicated, the "more finished" takes precedence, i.e.,
// F > A > D > P, and the maximum according to this ordering is used.
// If none of these attributes are present, an O (other) is returned.

enum EAgpType
{
    eO,
    eP,
    eD,
    eA,
    eF
};

static char s_DetermineComponentType(const CSeq_id& id, CScope& scope)
{
    EAgpType type = eO;

    CConstRef<CBioseq> bioseq = scope.GetBioseqHandle(id).GetCompleteBioseq();
    if (bioseq->CanGetDescr()) {
        const CSeq_descr::Tdata& descs = bioseq->GetDescr().Get();
        ITERATE (CSeq_descr::Tdata, desc, descs) {
            if ((*desc)->IsMolinfo()) {
                switch ((*desc)->GetMolinfo().GetTech()) {
                case CMolInfo::eTech_wgs:
                    return 'W';
                case CMolInfo::eTech_htgs_0:
                case CMolInfo::eTech_htgs_1:
                case CMolInfo::eTech_htgs_2:
                    type = max(type, eP);
                    break;
                case CMolInfo::eTech_htgs_3:
                    type = max(type, eF);
                    break;
                default:
                    break;
                }
            }
            if ((*desc)->IsGenbank()) {
                if ((*desc)->GetGenbank().CanGetKeywords()) {
                    ITERATE (CGB_block::TKeywords, kw,
                             (*desc)->GetGenbank().GetKeywords()) {
                        if (*kw == "HTGS_DRAFT" || *kw == "HTGS_FULLTOP") {
                            type = max(type, eD);
                        }
                        if (*kw == "HTGS_ACTIVEFIN") {
                            type = max(type, eA);
                        }
                    }
                }
            }
        }
    }
    switch (type) {
    case eP:
        return 'P';
    case eD:
        return 'D';
    case eA:
        return 'A';
    case eF:
        return 'F';
    default:
        return 'O';
    }
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/07/09 11:54:52  dicuccio
 * Dropped version of AgpWrite() that takes the object manager - use only one API,
 * taking a CScope
 *
 * Revision 1.4  2004/07/08 16:53:31  yazhuk
 * commented #include <objtools/data_loaders/genbank/gbloader.hpp>
 *
 * Revision 1.3  2004/07/07 21:45:07  jcherry
 * Removed form of AgpWrite that creates its own object manager
 *
 * Revision 1.2  2004/06/29 16:26:59  ucko
 * Move E(Agp)Type's definition out of s_DetermineComponentType because
 * templates (namely, max) can't necessarily use local types.
 *
 * Revision 1.1  2004/06/29 13:29:29  jcherry
 * Initial version
 *
 * ===========================================================================
 */


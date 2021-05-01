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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   validation of Seq_feat splice sites
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <objtools/validator/utilities.hpp>
#include <objtools/validator/splice_problems.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqfeat/OrgName.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;



void CSpliceProblems::ValidateDonorAcceptorPair
(ENa_strand strand,
 TSeqPos stop,
 const CSeqVector& vec_donor,
 TSeqPos seq_len_donor,
 TSeqPos start,
 const CSeqVector& vec_acceptor,
 TSeqPos seq_len_acceptor)
{
    char donor[2];
    char acceptor[2];

    ESpliceSiteRead good_donor = ReadDonorSpliceSite(strand, stop, vec_donor, seq_len_donor, donor);
    ESpliceSiteRead good_acceptor = ReadAcceptorSpliceSite(strand, start, vec_acceptor, seq_len_acceptor, acceptor);
    bool donor_ok = (good_donor == eSpliceSiteRead_OK  || good_donor == eSpliceSiteRead_WrongNT);
    bool acceptor_ok = (good_acceptor == eSpliceSiteRead_OK || good_acceptor == eSpliceSiteRead_WrongNT);

    if (donor_ok && acceptor_ok) {
        // Check canonical adjacent splice sites: "GT-AG"
        // Check non-canonical adjacent splice sites: "GC-AG"
        // Check non-canonical adjacent splice sites: "AT-AC"
        if (CheckAdjacentSpliceSites(kSpliceSiteGTAG, strand, donor, acceptor) ||
            CheckAdjacentSpliceSites(kSpliceSiteGCAG, strand, donor, acceptor) ||
            CheckAdjacentSpliceSites(kSpliceSiteATAC, strand, donor, acceptor)) {
            return; // canonical splice site found
        }
        m_DonorProblems.push_back(TSpliceProblem(good_donor, stop));
        m_AcceptorProblems.push_back(TSpliceProblem(good_acceptor, start));
    } else {
        m_DonorProblems.push_back(TSpliceProblem(good_donor, stop));
        m_AcceptorProblems.push_back(TSpliceProblem(good_acceptor, start));
    }
}


CSpliceProblems::ESpliceSiteRead
CSpliceProblems::ReadDonorSpliceSite(ENa_strand strand, TSeqPos stop, const CSeqVector& vec, TSeqPos seq_len, TSpliceSite& site)
{
    try {
        bool in_gap;
        bool bad_seq = false;

        if (strand == eNa_strand_minus) {
            // check donor and acceptor on minus strand
            if (stop > 1 && stop <= seq_len) {
                in_gap = (vec.IsInGap(stop - 2) && vec.IsInGap(stop - 1));
                if (!in_gap) {
                    bad_seq = (vec[stop - 1] > 250 || vec[stop - 2] > 250);
                }

                if (in_gap) {
                    return eSpliceSiteRead_Gap;
                } else if (bad_seq) {
                    return eSpliceSiteRead_BadSeq;
                }

                // Read splice site seq
                site[0] = vec[stop - 2];
                site[1] = vec[stop - 1];
            } else {
                return eSpliceSiteRead_OutOfRange;
            }
        }
        // Read donor splice site from plus strand
        else {
            if (stop < seq_len - 2) {
                in_gap = (vec.IsInGap(stop + 1) && vec.IsInGap(stop + 2));
                if (!in_gap) {
                    bad_seq = (vec[stop + 1] > 250 || vec[stop + 2] > 250);
                }
                if (in_gap) {
                    return eSpliceSiteRead_Gap;
                } else if (bad_seq) {
                    return eSpliceSiteRead_BadSeq;
                }
                site[0] = vec[stop + 1];
                site[1] = vec[stop + 2];
            } else {
                return eSpliceSiteRead_OutOfRange;
            }
        }

        // Check canonical donor site: "GT" and non-canonical donor site: "GC"
        if (CheckSpliceSite(kSpliceSiteGT, strand, site) || CheckSpliceSite(kSpliceSiteGC, strand, site)) {
            return eSpliceSiteRead_OK;
        } else {
            return eSpliceSiteRead_WrongNT;
        }
    } catch (CException&) {
        return eSpliceSiteRead_OK;
    }
}


CSpliceProblems::ESpliceSiteRead
CSpliceProblems::ReadDonorSpliceSite(ENa_strand strand, TSeqPos stop, const CSeqVector& vec, TSeqPos seq_len)
{
    char site[2];

    return ReadDonorSpliceSite(strand, stop, vec, seq_len, site);
}



CSpliceProblems::ESpliceSiteRead
CSpliceProblems::ReadAcceptorSpliceSite
(ENa_strand strand,
 TSeqPos start,
 const CSeqVector& vec,
 TSeqPos seq_len,
 TSpliceSite& site)
{
    try {
        bool in_gap;
        bool bad_seq = false;

        if (strand == eNa_strand_minus) {
            // check donor and acceptor on minus strand
            if (start < seq_len - 2) {
                in_gap = (vec.IsInGap(start + 1) && vec.IsInGap(start + 2));
                if (!in_gap) {
                    bad_seq = (vec[start + 1] > 250 || vec[start + 2] > 250);
                }

                if (in_gap) {
                    return eSpliceSiteRead_Gap;
                } else if (bad_seq) {
                    return eSpliceSiteRead_BadSeq;
                }
                site[0] = vec[start + 1];
                site[1] = vec[start + 2];
            } else {
                return eSpliceSiteRead_OutOfRange;
            }
        }
        // read acceptor splice site from plus strand
        else {
            if (start > 1 && start <= seq_len) {
                in_gap = (vec.IsInGap(start - 2) && vec.IsInGap(start - 1));
                if (!in_gap) {
                    bad_seq = (vec[start - 2] > 250 || vec[start - 1] > 250);
                }

                if (in_gap) {
                    return eSpliceSiteRead_Gap;
                } else if (bad_seq) {
                    return eSpliceSiteRead_BadSeq;
                }
                site[0] = vec[start - 2];
                site[1] = vec[start - 1];
            } else {
                return eSpliceSiteRead_OutOfRange;
            }
        }
        // Check canonical acceptor site: "AG"
        if (CheckSpliceSite(kSpliceSiteAG, strand, site)) {
            return eSpliceSiteRead_OK;
        } else {
            return eSpliceSiteRead_WrongNT;
        }
    } catch (CException&) {
        return eSpliceSiteRead_BadSeq;
    }
}


CSpliceProblems::ESpliceSiteRead
CSpliceProblems::ReadAcceptorSpliceSite
(ENa_strand strand,
TSeqPos start,
const CSeqVector& vec,
TSeqPos seq_len)
{
    char site[2];
    return ReadAcceptorSpliceSite(strand, start, vec, seq_len, site);
}


bool CSpliceProblems::SpliceSitesHaveErrors()
{
    bool has_errors = false;
    // donors
    for (auto it = m_DonorProblems.begin(); it != m_DonorProblems.end() && !has_errors; it++) {
        if (it->first == eSpliceSiteRead_BadSeq || it->first == eSpliceSiteRead_Gap ||
            it->first == eSpliceSiteRead_WrongNT) {
            has_errors = true;
        }
    }
    // acceptors
    for (auto it = m_AcceptorProblems.begin(); it != m_AcceptorProblems.end() && !has_errors; it++) {
        if (it->first == eSpliceSiteRead_BadSeq || it->first == eSpliceSiteRead_Gap ||
            it->first == eSpliceSiteRead_WrongNT) {
            has_errors = true;
        }
    }

    return has_errors;
}


void CSpliceProblems::CalculateSpliceProblems(const CSeq_feat& feat, bool check_all, bool pseudo, CBioseq_Handle loc_handle)
{
    m_DonorProblems.clear();
    m_AcceptorProblems.clear();
    m_ExceptionUnnecessary = false;
    m_ErrorsNotExpected = true;

    bool has_errors = false, ribo_slip = false;

    const CSeq_loc& loc = feat.GetLocation();

    // skip if organelle
    if (!loc_handle || IsOrganelle(loc_handle)) {
        return;
    }

    // suppress for specific biological exceptions
    if (feat.IsSetExcept() && feat.IsSetExcept_text()
        && (NStr::FindNoCase(feat.GetExcept_text(), "low-quality sequence region") != string::npos)) {
        return;
    }
    if (feat.IsSetExcept() && feat.IsSetExcept_text()
        && (NStr::FindNoCase(feat.GetExcept_text(), "ribosomal slippage") != string::npos)) {
        m_ErrorsNotExpected = false;
        ribo_slip = true;
    }
    if (feat.IsSetExcept() && feat.IsSetExcept_text()
        && (NStr::FindNoCase(feat.GetExcept_text(), "artificial frameshift") != string::npos
        || NStr::FindNoCase(feat.GetExcept_text(), "nonconsensus splice site") != string::npos
        || NStr::FindNoCase(feat.GetExcept_text(), "adjusted for low-quality genome") != string::npos
        || NStr::FindNoCase(feat.GetExcept_text(), "heterogeneous population sequenced") != string::npos
        || NStr::FindNoCase(feat.GetExcept_text(), "low-quality sequence region") != string::npos
        || NStr::FindNoCase(feat.GetExcept_text(), "artificial location") != string::npos)) {
        m_ErrorsNotExpected = false;
    }


    // look for mixed strands, skip if found
    ENa_strand strand = eNa_strand_unknown;

    int num_parts = 0;
    for (CSeq_loc_CI si(loc); si; ++si) {
        if (si.IsSetStrand()) {
            ENa_strand tmp = si.GetStrand();
            if (tmp == eNa_strand_plus || tmp == eNa_strand_minus) {
                if (strand == eNa_strand_unknown) {
                    strand = si.GetStrand();
                } else if (strand != tmp) {
                    return;
                }
            }
        }
        num_parts++;
    }

    if (!check_all && num_parts < 2) {
        return;
    }

    // Default value for a strand is '+'
    if (eNa_strand_unknown == strand) {
        strand = eNa_strand_plus;
    }

    // only check for errors if overlapping gene is not pseudo
    if (!pseudo) {
        CSeqFeatData::ESubtype subtype = feat.GetData().GetSubtype();
        switch (subtype) {
        case CSeqFeatData::eSubtype_exon:
            ValidateSpliceExon(feat, loc_handle, strand);
            break;
        case CSeqFeatData::eSubtype_mRNA:
            ValidateSpliceMrna(feat, loc_handle, strand);
            break;
        case CSeqFeatData::eSubtype_cdregion:
            ValidateSpliceCdregion(feat, loc_handle, strand);
            break;
        default:
            break;
        }
    }
    has_errors = SpliceSitesHaveErrors();

    if (!m_ErrorsNotExpected  &&  !has_errors  &&  !ribo_slip) {
        m_ExceptionUnnecessary = true;
    }
}


void CSpliceProblems::ValidateSpliceExon(const CSeq_feat& feat, const CBioseq_Handle& bsh, ENa_strand strand)
{
    const CSeq_loc& loc = feat.GetLocation();

    // Find overlapping feature - mRNA or gene - to identify start / stop exon
    bool overlap_feat_partial_5 = false; // set to true if 5'- most start of overlapping feature is partial
    bool overlap_feat_partial_3 = false; // set to true if 3'- most end of overlapping feature is partial
    TSeqPos  overlap_feat_start = 0; // start position of overlapping feature
    TSeqPos  overlap_feat_stop = 0;  // stop position of overlapping feature

    bool overlap_feat_exists = false;
    // Locate overlapping mRNA feature
    CConstRef<CSeq_feat> mrna = GetBestOverlappingFeat(
        loc,
        CSeqFeatData::eSubtype_mRNA,
        eOverlap_Contained,
        bsh.GetScope());
    if (mrna) {
        overlap_feat_exists = true;
        overlap_feat_partial_5 = mrna->GetLocation().IsPartialStart(eExtreme_Biological);
        overlap_feat_start = mrna->GetLocation().GetStart(eExtreme_Biological);

        overlap_feat_partial_3 = mrna->GetLocation().IsPartialStop(eExtreme_Biological);
        overlap_feat_stop = mrna->GetLocation().GetStop(eExtreme_Biological);
    }
    else {
        // Locate overlapping gene feature.
        CConstRef<CSeq_feat> gene = GetBestOverlappingFeat(
            loc,
            CSeqFeatData::eSubtype_gene,
            eOverlap_Contained,
            bsh.GetScope());
        if (gene) {
            overlap_feat_exists = true;
            overlap_feat_partial_5 = gene->GetLocation().IsPartialStart(eExtreme_Biological);
            overlap_feat_start = gene->GetLocation().GetStart(eExtreme_Biological);

            overlap_feat_partial_3 = gene->GetLocation().IsPartialStop(eExtreme_Biological);
            overlap_feat_stop = gene->GetLocation().GetStop(eExtreme_Biological);
        }
    }

    CSeq_loc_CI si(loc);
    try{
        CSeq_loc::TRange range = si.GetRange();
        CConstRef<CSeq_loc> cur_int = si.GetRangeAsSeq_loc();
        if (cur_int) {
            CBioseq_Handle bsh_si = bsh.GetScope().GetBioseqHandle(*cur_int);

            if (bsh_si) {
                CSeqVector vec = bsh_si.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

                TSeqPos start, stop;
                if (eNa_strand_minus == strand) {
                    start = range.GetTo();
                    stop = range.GetFrom();
                } else {
                    start = range.GetFrom();
                    stop = range.GetTo();
                }

                if (overlap_feat_exists) {
                    if (!cur_int->IsPartialStop(eExtreme_Biological)) {
                        if (stop == overlap_feat_stop) {
                            if (overlap_feat_partial_3) {
                                ESpliceSiteRead good_donor = ReadDonorSpliceSite(strand, stop, vec, bsh_si.GetInst_Length());
                                m_DonorProblems.push_back(TSpliceProblem(good_donor, stop));
                            }
                        } else {
                            ESpliceSiteRead good_donor = ReadDonorSpliceSite(strand, stop, vec, bsh_si.GetInst_Length());
                            m_DonorProblems.push_back(TSpliceProblem(good_donor, stop));
                        }
                    }

                    if (!cur_int->IsPartialStart(eExtreme_Biological)) {
                        if (start == overlap_feat_start) {
                            if (overlap_feat_partial_5) {
                                ESpliceSiteRead good_acceptor = ReadAcceptorSpliceSite(strand, start, vec, bsh_si.GetInst_Length());
                                m_AcceptorProblems.push_back(TSpliceProblem(good_acceptor, start));
                            }
                        } else {
                            ESpliceSiteRead good_acceptor = ReadAcceptorSpliceSite(strand, start, vec, bsh_si.GetInst_Length());
                            m_AcceptorProblems.push_back(TSpliceProblem(good_acceptor, start));
                        }
                    }
                } else {
                    // Overlapping feature - mRNA or gene - not found.
                    if (!cur_int->IsPartialStop(eExtreme_Biological)) {
                        ESpliceSiteRead good_donor = ReadDonorSpliceSite(strand, stop, vec, bsh_si.GetInst_Length());
                        m_DonorProblems.push_back(TSpliceProblem(good_donor, stop));
                    }
                    if (!cur_int->IsPartialStart(eExtreme_Biological)) {
                        ESpliceSiteRead good_acceptor = ReadAcceptorSpliceSite(strand, start, vec, bsh_si.GetInst_Length());
                        m_AcceptorProblems.push_back(TSpliceProblem(good_acceptor, start));
                    }
                }
            }
        }
    } catch (CException ) {
        ;
    } catch (std::exception ) {
        ;// could get errors from CSeqVector
    }
}

void CSpliceProblems::ValidateSpliceMrna(const CSeq_feat& feat, const CBioseq_Handle& bsh, ENa_strand strand)
{
    const CSeq_loc& loc = feat.GetLocation();

    bool ignore_mrna_partial5 = false;
    bool ignore_mrna_partial3 = false;

    // Retrieve overlapping cdregion
    CConstRef<CSeq_feat> cds = GetBestOverlappingFeat(
        loc,
        CSeqFeatData::eSubtype_cdregion,
        eOverlap_Contains,
        bsh.GetScope());
    if (cds) {
        // If there is no UTR information, then the mRNA location should agree with its CDS location,
        // but the mRNA should be marked partial at its 5' and 3' ends
        // Do not check splice site (either donor or acceptor) if CDS location's start / stop is complete.
        if (!cds->GetLocation().IsPartialStart(eExtreme_Biological)
            && cds->GetLocation().GetStart(eExtreme_Biological) == feat.GetLocation().GetStart(eExtreme_Biological)) {
            ignore_mrna_partial5 = true;
        }
        if (!cds->GetLocation().IsPartialStop(eExtreme_Biological)
            && cds->GetLocation().GetStop(eExtreme_Biological) == feat.GetLocation().GetStop(eExtreme_Biological)) {
            ignore_mrna_partial3 = true;
        }
    }

    TSeqPos start;
    TSeqPos stop;

    CSeq_loc_CI head(loc);
    if (head) {
        // Validate acceptor site of 5'- most feature
        const CSeq_loc& part = head.GetEmbeddingSeq_loc();
        CSeq_loc::TRange range = head.GetRange();
        CBioseq_Handle bsh_head = bsh.GetScope().GetBioseqHandle(*head.GetRangeAsSeq_loc());
        if (bsh_head) {
            CSeqVector vec = bsh_head.GetSeqVector (CBioseq_Handle::eCoding_Iupac);

            if (strand == eNa_strand_minus) {
                start = range.GetTo();
            } else {
                start = range.GetFrom();
            }
            if (part.IsPartialStart(eExtreme_Biological) && !ignore_mrna_partial5) {
                ESpliceSiteRead good_acceptor = ReadAcceptorSpliceSite(strand, start, vec, bsh_head.GetInst_Length());
                m_AcceptorProblems.push_back(TSpliceProblem(good_acceptor, start));
            }
        }

        CSeq_loc_CI tail(loc);
        ++tail;

        // Validate adjacent (donor...acceptor) splice sites.
        // @head is a location of exon that contibutes `donor site`
        // @tail is a location of exon that contibutes `acceptor site`
        for(; tail; ++head, ++tail) {
            CSeq_loc::TRange range_head = head.GetRange();
            CSeq_loc::TRange range_tail = tail.GetRange();
            CBioseq_Handle bsh_head = bsh.GetScope().GetBioseqHandle(*head.GetRangeAsSeq_loc());
            CBioseq_Handle bsh_tail = bsh.GetScope().GetBioseqHandle(*tail.GetRangeAsSeq_loc());
            if (bsh_head && bsh_tail) {
                try {
                    CSeqVector vec_head = bsh_head.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
                    CSeqVector vec_tail = bsh_tail.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

                    if (strand == eNa_strand_minus) {
                        start = range_tail.GetTo();
                        stop = range_head.GetFrom();
                    } else {
                        start = range_tail.GetFrom();
                        stop = range_head.GetTo();
                    }
                    ValidateDonorAcceptorPair(strand,
                        stop, vec_head, bsh_head.GetInst_Length(),
                        start, vec_tail, bsh_tail.GetInst_Length());
                } catch (CSeqVectorException& ) {
                }
            }
        }
    }

    // Validate donor site of 3'most feature
    if(head) {
        const CSeq_loc& part = head.GetEmbeddingSeq_loc();
        CSeq_loc::TRange range = head.GetRange();
        CBioseq_Handle bsh_head = bsh.GetScope().GetBioseqHandle(*head.GetRangeAsSeq_loc());
        if (bsh_head) {
            CSeqVector vec = bsh_head.GetSeqVector (CBioseq_Handle::eCoding_Iupac);

            if (strand == eNa_strand_minus) {
                stop = range.GetFrom();
            } else {
                stop = range.GetTo();
            }
            if (part.IsPartialStop(eExtreme_Biological) && !ignore_mrna_partial3) {
                ESpliceSiteRead good_donor = ReadDonorSpliceSite(strand, stop, vec, bsh_head.GetInst_Length());
                m_DonorProblems.push_back(TSpliceProblem(good_donor, stop));
            }
        }
    }
}

void CSpliceProblems::ValidateSpliceCdregion(const CSeq_feat& feat, const CBioseq_Handle& bsh, ENa_strand strand)
{
    const CSeq_loc& loc = feat.GetLocation();

    TSeqPos start;
    TSeqPos stop;

    CSeq_loc_CI head(loc);
    if (head) {
        // Validate acceptor site of 5'- most feature
        const CSeq_loc& part = head.GetEmbeddingSeq_loc();
        CSeq_loc::TRange range = head.GetRange();
        CBioseq_Handle bsh_head = bsh.GetScope().GetBioseqHandle(*head.GetRangeAsSeq_loc());
        if (bsh_head) {
            try {
                CSeqVector vec = bsh_head.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
                if (part.IsPartialStart(eExtreme_Biological)) {
                    if (strand == eNa_strand_minus) {
                        start = range.GetTo();
                    } else {
                        start = range.GetFrom();
                    }
                    ESpliceSiteRead good_acceptor = ReadAcceptorSpliceSite(strand, start, vec, bsh_head.GetInst_Length());
                    m_AcceptorProblems.push_back(TSpliceProblem(good_acceptor, start));
                }
            } catch (CSeqVectorException&) {
            }
        }

        CSeq_loc_CI tail(loc);
        ++tail;

        // Validate adjacent (donor...acceptor) splice sites.
        // @head is a location of exon that contibutes `donor site`
        // @tail is a location of exon that contibutes `acceptor site`
        for(; tail; ++head, ++tail) {
            CSeq_loc::TRange range_head = head.GetRange();
            CSeq_loc::TRange range_tail = tail.GetRange();
            CBioseq_Handle bsh_head = bsh.GetScope().GetBioseqHandle(*head.GetRangeAsSeq_loc());
            CBioseq_Handle bsh_tail = bsh.GetScope().GetBioseqHandle(*tail.GetRangeAsSeq_loc());
            if (bsh_head && bsh_tail) {
                try {
                    CSeqVector vec_head = bsh_head.GetSeqVector (CBioseq_Handle::eCoding_Iupac);
                    CSeqVector vec_tail = bsh_tail.GetSeqVector (CBioseq_Handle::eCoding_Iupac);


                    if (strand == eNa_strand_minus) {
                        start = range_tail.GetTo();
                        stop  = range_head.GetFrom();
                    } else {
                        start = range_tail.GetFrom();
                        stop = range_head.GetTo();
                    }
                    ValidateDonorAcceptorPair(strand,
                                               stop, vec_head, bsh_head.GetInst_Length(),
                                               start, vec_tail, bsh_tail.GetInst_Length());
                } catch (CSeqVectorException&) {
                }
            }
        }
    }

    // Validate donor site of 3'most feature
    if(head) {
        const CSeq_loc& part = head.GetEmbeddingSeq_loc();
        CSeq_loc::TRange range = head.GetRange();
        CBioseq_Handle bsh_head = bsh.GetScope().GetBioseqHandle(*head.GetRangeAsSeq_loc());
        if (bsh_head) {
            try {
                CSeqVector vec = bsh_head.GetSeqVector (CBioseq_Handle::eCoding_Iupac);

                if (strand == eNa_strand_minus) {
                    stop = range.GetFrom();
                } else {
                    stop = range.GetTo();
                }
                if (part.IsPartialStop(eExtreme_Biological)) {
                    ESpliceSiteRead good_donor = ReadDonorSpliceSite(strand, stop, vec, bsh_head.GetInst_Length());
                    m_DonorProblems.push_back(TSpliceProblem(good_donor, stop));
                }
            } catch (CSeqVectorException&) {
            }
        }
    }

}


bool s_EqualsG(Char c)
{
    return c == 'G';
}

bool s_EqualsC(Char c)
{
    return c == 'C';
}

bool s_EqualsA(Char c)
{
    return c == 'A';
}

bool s_EqualsT(Char c)
{
    return c == 'T';
}

bool CheckAdjacentSpliceSites(const string& signature, ENa_strand strand, TConstSpliceSite donor, TConstSpliceSite acceptor)
{
    static
    struct tagSpliceSiteInfo
    {
        const string& id;
        ENa_strand strand;
        bool(*check_donor0)(Char);
        bool(*check_donor1)(Char);
        bool(*check_acceptor0)(Char);
        bool(*check_acceptor1)(Char);
    }
    SpliceSiteInfo[] = {
        // 5' << GT...AG <<
        { kSpliceSiteGTAG, eNa_strand_plus, ConsistentWithG, ConsistentWithT, ConsistentWithA, ConsistentWithG },
        //    >> CT...AC >>, reverse complement
        { kSpliceSiteGTAG, eNa_strand_minus, ConsistentWithA, ConsistentWithC, ConsistentWithC, ConsistentWithT },
        // 5' << GC...AG <<
        { kSpliceSiteGCAG, eNa_strand_plus, s_EqualsG, s_EqualsC, s_EqualsA, s_EqualsG },
        //    >> CT...GC >>, reverse complement
        { kSpliceSiteGCAG, eNa_strand_minus, s_EqualsG, s_EqualsC, s_EqualsC, s_EqualsT },
        // 5' << AT...AC <<
        { kSpliceSiteATAC, eNa_strand_plus, s_EqualsA, s_EqualsT, s_EqualsA, s_EqualsC },
        //    >> GT...AT >>, reverse complement
        { kSpliceSiteATAC, eNa_strand_minus, s_EqualsA, s_EqualsT, s_EqualsG, s_EqualsT }
    };
    static int size = sizeof(SpliceSiteInfo) / sizeof(struct tagSpliceSiteInfo);

    for (int i = 0; i < size; ++i) {
        struct tagSpliceSiteInfo* entry = &SpliceSiteInfo[i];
        if (strand == entry->strand && entry->id == signature) {
            return (entry->check_donor0(donor[0]) && entry->check_donor1(donor[1]) &&
                entry->check_acceptor0(acceptor[0]) && entry->check_acceptor1(acceptor[1]));
        }
    }

    NCBI_THROW(CCoreException, eCore, "Unknown splice site signature.");
}


bool CheckSpliceSite(const string& signature, ENa_strand strand, TConstSpliceSite site)
{
    static
    struct tagSpliceSiteInfo
    {
        const string& id;
        ENa_strand strand;
        bool(*check_site0)(Char);
        bool(*check_site1)(Char);
    }
    SpliceSiteInfo[] = {
        // 5' << GT... <<
        { kSpliceSiteGT, eNa_strand_plus, ConsistentWithG, ConsistentWithT },
        //    >> ...AC >>, reverse complement
        { kSpliceSiteGT, eNa_strand_minus, ConsistentWithA, ConsistentWithC },
        // 5' << ...AG <<
        { kSpliceSiteAG, eNa_strand_plus, ConsistentWithA, ConsistentWithG },
        //    >> CT...>>, reverse complement
        { kSpliceSiteAG, eNa_strand_minus, ConsistentWithC, ConsistentWithT },
        // 5' << GC... <<
        { kSpliceSiteGC, eNa_strand_plus, s_EqualsG, s_EqualsC },
        //    >> ...GC >>, reverse complement
        { kSpliceSiteGC, eNa_strand_minus, s_EqualsG, s_EqualsC }
    };
    static int size = sizeof(SpliceSiteInfo) / sizeof(struct tagSpliceSiteInfo);

    for (int i = 0; i < size; ++i) {
        struct tagSpliceSiteInfo* entry = &SpliceSiteInfo[i];
        if (strand == entry->strand && entry->id == signature) {
            return (entry->check_site0(site[0]) && entry->check_site1(site[1]));
        }
    }

    NCBI_THROW(CCoreException, eCore, "Unknown splice site signature.");
}


bool CheckIntronSpliceSites(ENa_strand strand, TConstSpliceSite donor, TConstSpliceSite acceptor)
{
    return (CheckAdjacentSpliceSites(kSpliceSiteGTAG, strand, donor, acceptor) ||
        CheckAdjacentSpliceSites(kSpliceSiteGCAG, strand, donor, acceptor) ||
        CheckAdjacentSpliceSites(kSpliceSiteATAC, strand, donor, acceptor));
}

bool CheckIntronDonor(ENa_strand strand, TConstSpliceSite donor)
{
    return (CheckSpliceSite(kSpliceSiteGT, strand, donor) ||
        CheckSpliceSite(kSpliceSiteGC, strand, donor));
}

bool CheckIntronAcceptor(ENa_strand strand, TConstSpliceSite acceptor)
{
    return CheckSpliceSite(kSpliceSiteAG, strand, acceptor);
}

END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

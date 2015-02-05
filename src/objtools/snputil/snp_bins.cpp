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
 * Author:  Melvin Quintos, Dmitry Rudnev
 *
 * File Description:
 *  Implements the functions in snp_bins.hpp
 */

#include <ncbi_pch.hpp>

#include <objmgr/annot_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/table_field.hpp>

#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqres/Byte_graph.hpp>

#include <objtools/snputil/snp_bins.hpp>

#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>

#include <util/checksum.hpp>

#include <cmath>

#include <objtools/snputil/snp_bins.hpp>
#include <objtools/snputil/snp_utils.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


string NSnpBins::SourceAsString(TSource Source)
{
    switch(Source)
    {
		case eSource_dbGAP:
			return "dbGaP";
		case eSource_NHGRI_GWAS:
			return "NHGRI GWAS catalog";
		case eSource_NHLBI_GRASP:
			return "NHLBI GRASP";
    }
    return "dbGaP";
}

void NSnpBins::ReadAnnotDesc(const CSeq_annot_Handle& handle, string& title, string& comment)
{
    if (handle.Seq_annot_CanGetDesc()) {
        // Extract the Annotations properties
        ITERATE( CAnnot_descr::Tdata, it, handle.Seq_annot_GetDesc().Get()) {
            const CAnnotdesc &desc = **it;

            if (desc.IsComment()) {
                comment = desc.GetComment();
            }
            else if (desc.IsTitle()) {
                title = desc.GetTitle();
            }
        }
    }
}


// get a selector for a bin given a NA track accession with some selector parameters
void NSnpBins::GetBinSelector(const string& sTrackAccession,
    bool isAdaptive,
    int depth,
    SAnnotSelector& sel)
{
    sel.SetOverlapTotalRange().SetResolveAll();
    sel.SetAnnotType(CSeq_annot::TData::e_Seq_table);
    sel.IncludeNamedAnnotAccession(sTrackAccession);
    sel.AddNamedAnnots(sTrackAccession);
    // copied from  CSeqUtils::SetResolveDepth()
    if(isAdaptive) {
        sel.SetAdaptiveDepth(true);
        sel.SetExactDepth(false);
        //!!: watch out
        // Maybe there is bug inside selector, we have to call SetResolveAll() even
        // for cases where we only want to resolve up to a given depth.
        sel.SetResolveAll();
        if (depth >=0) sel.SetResolveDepth(depth);
    } else if (depth >= 0){
        sel.SetResolveDepth(depth);
        sel.SetExactDepth(true);
        sel.SetAdaptiveDepth(false);
    }
}

// get an annotation handle that is needed to load a bin from an existing selector and loc and bioseq handle
// returns false if a handle cannot be obtained
bool NSnpBins::GetBinHandle(CScope& scope,
    const SAnnotSelector& sel,
    const CSeq_loc &loc,
    CSeq_annot_Handle& handle)
{
    CAnnot_CI iter(scope, loc, sel);
    if(iter.size() != 1) {
        return false;
    }
    handle = *iter;
    return true;
}




// choose a more significant entry of the two offered
// returns 1 of entry1 is more significant or 2 if entry2 is more
int NSnpBins::ChooseSignificant(const SBinEntry* entry1, const SBinEntry* entry2, TBinType type)
{
	// significance is determined using different metrics depending on the bin type
    // for eCLIN, the most significant is pathogenic, then probably pathogenic, then everything else
    // for all other bins, the significance is determined by the largest pvalue
	if(type == eCLIN) {
        return (entry1->ClinSigID == CPhenotype::eClinical_significance_pathogenic ||
                                (entry1->ClinSigID == CPhenotype::eClinical_significance_probable_pathogenic && entry2->ClinSigID != CPhenotype::eClinical_significance_pathogenic))
                            ?   1
                            :   2;
    } else {
        return (entry1->pvalue > entry2->pvalue)
                                ? 1
                                : 2;
    }
}


CRef<NSnpBins::SBin> NSnpBins::GetBin(const objects::CSeq_annot_Handle& annot,
								  TSeqRange range)
{
    const CTableFieldHandle<int>      col_type("trackType");
    int     pos_start, pos_end;
    int type;
    string title, comment;
    CRef<SBin>  res(new SBin);
    FindPosIndexRange(annot, (int)range.GetFrom(), (int)range.GetTo(), pos_start, pos_end);
    ReadAnnotDesc(annot, title, comment);
    if (!col_type.TryGet(annot, 0, type)) {
		type = NSnpBins::eGAP;
	}
    res->count = 0;
    res->range = range;
    res->title = title;
    res->type = type;

    for(int row = pos_start; row < pos_end; ++row ) {
        CRef<NSnpBins::SBinEntry> BinEntry(GetEntry(annot, row));
        if(res->m_SigEntry.Empty()) {
            res->m_SigEntry = BinEntry;
        } else {
            if(ChooseSignificant(res->m_SigEntry, BinEntry, type) == 2) {
                res->m_SigEntry = BinEntry;
            }
        }
        res->m_EntryList.push_back(BinEntry);
        res->count++;
    }
    return res;
}

CRef<NSnpBins::SBinEntry> NSnpBins::GetEntry(const objects::CSeq_annot_Handle& annot,
								             int row)
{
    const CTableFieldHandle<int>      col_pos("pos");
    const CTableFieldHandle<int>      col_pos_end("pos_end");
    const CTableFieldHandle<double>   col_val("pvalue");
    const CTableFieldHandle<double>   col_val_synonym("Pvalue");
    const CTableFieldHandle<string>   col_trait("trait");
    const CTableFieldHandle<string>   col_pmids("pmids");
    const CTableFieldHandle<string>   col_rgenes("reportedGenes");
    const CTableFieldHandle<string>   col_mgenes("mappedGenes");
    const CTableFieldHandle<int>      col_snpid("snpId");
    const CTableFieldHandle<string>   col_sub_type("trackSubType");
    const CTableFieldHandle<int>      col_clinsigid("clinSigID");
    const CTableFieldHandle<string>   col_hgvs("HGVS");
    const CTableFieldHandle<string>   col_dbgaptext("dbgaptext");
    const CTableFieldHandle<string>   col_context("context");
    const CTableFieldHandle<int>      col_source("source");
    const CTableFieldHandle<string>   col_population("population");
    const CTableFieldHandle<int>      col_geneId("geneId");
    const CTableFieldHandle<string>   col_geneName("geneName");

    string  trackSubType;
    int pos, pos_end;
    int snpid, ClinSigID;
    double  pvalue;
    string  trait, pmids, rgenes, mgenes;
    string  title, comment, population;
    string  sHGVS;
	int		source, geneId;
    string  dbgaptext, geneName;
	string  context;

    CRef<NSnpBins::SBinEntry> entry;
    if(col_pos.TryGet(annot, row, pos)) {
        entry.Reset(new NSnpBins::SBinEntry());
        entry->pos = (TSeqPos)pos;
        entry->pos_end  = col_pos_end.TryGet(annot, row, pos_end) ? (TSeqPos)pos_end : kInvalidSeqPos;
        entry->trackSubType = col_sub_type.TryGet(annot, row, trackSubType) ? trackSubType : "";
        entry->snpid  = col_snpid.TryGet(annot, row, snpid) ? (unsigned int)snpid : 0;
        entry->pvalue = (col_val.TryGet(annot, row, pvalue) || col_val_synonym.TryGet(annot, row, pvalue)) ? -log10(pvalue) : 0;
        entry->trait = col_trait.TryGet(annot, row, trait) ? trait : "";
        entry->pmids  = col_pmids.TryGet(annot, row, pmids) ? pmids : "";
        entry->genes_reported = col_rgenes.TryGet(annot, row, rgenes) ? rgenes : "";
        entry->genes_mapped = col_mgenes.TryGet(annot, row, mgenes) ? mgenes : "";
        entry->ClinSigID = col_clinsigid.TryGet(annot, row, ClinSigID) ? ClinSigID : -1;
        entry->sHGVS = col_hgvs.TryGet(annot, row, sHGVS) ? sHGVS : "";
        entry->dbgaptext = col_dbgaptext.TryGet(annot, row, dbgaptext) ? dbgaptext : "";
        entry->context = col_context.TryGet(annot, row, context) ? context : "";
        entry->source = col_source.TryGet(annot, row, source) ? source : -1;
        entry->population = col_population.TryGet(annot, row, population) ? population : "";
        entry->geneName = col_geneName.TryGet(annot, row, geneName) ? geneName : "";
        entry->geneId = col_geneId.TryGet(annot, row, geneId) ? geneId : -1;
    }
    return entry;
}

void NSnpBins::CGeneMap::x_Init(const string& sSrc)
{
    m_GeneMap.clear();
    list<string> GeneSymIDPairsList;

    NStr::Split(sSrc, ":", GeneSymIDPairsList);

    ITERATE(list<string>, iGeneSymIDPairsList, GeneSymIDPairsList) {
        list<string> GeneSymIDPair;

        NStr::Split(*iGeneSymIDPairsList, "^", GeneSymIDPair);

        m_GeneMap[GeneSymIDPair.front()] = GeneSymIDPair.size() == 2 ? GeneSymIDPair.back() : string();
    }
}

string NSnpBins::CGeneMap::AsString() const
{
    string sRes;

    ITERATE(TGeneMap, iGeneMap, m_GeneMap) {
        sRes += (sRes.empty() ? "" : ":") + iGeneMap->first + "^" + iGeneMap->second;
    }

    return sRes;
}

void NSnpBins::FindPosIndexRange(const CSeq_annot_Handle& annot,
                         int pos_value_from, int pos_value_to,
                          int& pos_index_begin, int& pos_index_end)
{
    size_t rows = annot.GetSeq_tableNumRows();
    const CTableFieldHandle<int> col_pos("pos");
    const CTableFieldHandle<string>   col_sub_type("trackSubType");

    pos_index_begin = -1;
    pos_index_end   = rows-1;

    string  trackSubType;
    col_sub_type.TryGet(annot, 0, trackSubType);

    const CTableFieldHandle<int>  col_pos_end(NSnpBins::isGeneMarker(trackSubType) ? "pos_end" : "pos");


    // Find 'pos_value_from'
    int lower_pos_index_bound = 0;
    int upper_pos_index_bound = rows-1;
    int pos_index_k = 0;
    int pos_value_k = 0;
    do {
        pos_index_k = (lower_pos_index_bound + upper_pos_index_bound)/2;
        col_pos_end.TryGet(annot, pos_index_k, pos_value_k);
        if(pos_value_k < pos_value_from) {
            pos_index_begin = pos_index_k;
            lower_pos_index_bound = pos_index_k+1;
        } else {
            upper_pos_index_bound = pos_index_k-1;
        }
    } while (pos_value_k != pos_value_from && lower_pos_index_bound <= upper_pos_index_bound);

    // position start to be inclusive (catch boundary condition)
    pos_index_begin = (pos_value_from == pos_value_k ? pos_index_k : pos_index_begin+1);

    // slide the start down for cases when there are several entries with the same position
    int SlidingBegin(pos_index_begin-1);
    while(SlidingBegin >= 0) {
        col_pos_end.TryGet(annot, SlidingBegin, pos_value_k);
        if(pos_value_k < pos_value_from)
            break;
        pos_index_begin = SlidingBegin;
        --SlidingBegin;
    }

    // find the 'pos_value_to' value
    lower_pos_index_bound=0;
    upper_pos_index_bound=rows-1;
    pos_value_k = 0;
    do {
        pos_index_k = (lower_pos_index_bound+upper_pos_index_bound)/2;
        col_pos.TryGet(annot, pos_index_k, pos_value_k);
        if (pos_value_k < pos_value_to) {
            lower_pos_index_bound = pos_index_k+1;
        }
        else {
            pos_index_end = pos_index_k;
            upper_pos_index_bound = pos_index_k-1;
        }
    } while (pos_value_k != pos_value_to && lower_pos_index_bound <= upper_pos_index_bound);

    // increase end to include in range up until the latest entry with "pos".
    size_t SlidingEnd(pos_index_end);
    while(SlidingEnd < rows) {
        col_pos.TryGet(annot, SlidingEnd, pos_value_k);
        if(pos_value_k > pos_value_to)
            break;
        pos_index_end = SlidingEnd+1;
        ++SlidingEnd;
    }
}




END_NCBI_SCOPE


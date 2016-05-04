/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

Author: Tom Madden

******************************************************************************/

/** @file vecscreen_run.cpp
 * Run vecscreen, produce output
*/

#include <ncbi_pch.hpp>
#include <algo/blast/format/blast_format.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/local_blast.hpp>

#include <objmgr/util/seq_loc_util.hpp>

#include <algo/blast/format/vecscreen_run.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
USING_SCOPE(sequence);
#endif

// static const string kGifLegend[] = {"Strong", "Moderate", "Weak", "Suspect"};

CVecscreenRun::CVecscreenRun(CRef<CSeq_loc> seq_loc, CRef<CScope> scope, const string & db)
 : m_SeqLoc(seq_loc), m_Scope(scope), m_DB(db), m_Vecscreen(0)
{
    m_Queries.Reset(new CBlastQueryVector());
    CRef<CBlastSearchQuery> q(new CBlastSearchQuery(*seq_loc, *scope));
    m_Queries->push_back(q);
    x_RunBlast();
}

void CVecscreenRun::x_RunBlast()
{
   //_ASSERT(m_Queries.NotEmpty());
   //_ASSERT(m_Queries->Size() != 0);

   // Load blast queries
   CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(*m_Queries));

   // BLAST optiosn needed for vecscreen.
   CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::CreateTask("vecscreen"));

   // Sets Vecscreen database.
   const CSearchDatabase target_db(m_DB, CSearchDatabase::eBlastDbIsNucleotide);

   // Constructor for blast run.
   CLocalBlast blaster(query_factory, opts, target_db);

   // BLAST run.
   m_RawBlastResults = blaster.Run();
   _ASSERT(m_RawBlastResults->size() == 1);
   CRef<CBlastAncillaryData> ancillary_data((*m_RawBlastResults)[0].GetAncillaryData());

   // The vecscreen stuff follows.
   m_Vecscreen = new CVecscreen(*((*m_RawBlastResults)[0].GetSeqAlign()), GetLength(*m_SeqLoc, m_Scope));

   // This actually does the vecscreen work.
   m_Seqalign_set = m_Vecscreen->ProcessSeqAlign();

   CConstRef<CSeq_id> id(m_SeqLoc->GetId());
   CRef<CSearchResults> results(new CSearchResults(id, m_Seqalign_set,
                                                   TQueryMessages(),
                                                   ancillary_data));
   m_RawBlastResults->clear();
   m_RawBlastResults->push_back(results);
}

CRef<blast::CSearchResultSet> 
CVecscreenRun::GetSearchResultSet() const
{
    return m_RawBlastResults;
}

struct SVecscreenMatchFinder : public unary_function<bool, CVecscreenRun::SVecscreenSummary>
{
    SVecscreenMatchFinder(const string& match_type)
        : m_MatchType(match_type) {}

    bool operator()(const CVecscreenRun::SVecscreenSummary& rhs) {
        return (rhs.match_type == m_MatchType);
    }

private:
    string m_MatchType;
};

void
CVecscreenRun::CFormatter::FormatResults(CNcbiOstream& out,
                                         CRef<CBlastOptionsHandle> vs_opts)
{
    const bool kPrintAlignments = static_cast<bool>(m_Outfmt == eShowAlignments);
    const CFormattingArgs::EOutputFormat fmt(CFormattingArgs::ePairwise);
    const bool kBelieveQuery(false);
    const bool kShowGi(false);
    const CSearchDatabase dbinfo(m_Screener.m_DB,
                                 CSearchDatabase::eBlastDbIsNucleotide);
    CLocalDbAdapter dbadapter(dbinfo);
    const int kNumDescriptions(0);
    const int kNumAlignments(50);   // FIXME: find this out
    const bool kIsTabular(false);

    CBlastFormat blast_formatter(vs_opts->GetOptions(), dbadapter, fmt,
                                 kBelieveQuery, out, kNumDescriptions,
                                 kNumAlignments, m_Scope, BLAST_DEFAULT_MATRIX,
                                 kShowGi, m_HtmlOutput);
    blast_formatter.PrintProlog();
    list<SVecscreenSummary> match_list = m_Screener.GetList();

    // Acknowledge the query if the alignments section won't be printed (this
    // does the acknowledgement)
    if (kPrintAlignments == false) {
        CBioseq_Handle bhandle =
            m_Scope.GetBioseqHandle(*m_Screener.m_SeqLoc->GetId(),
                                    CScope::eGetBioseq_All);
        if( !bhandle  ){
            string message = "Failed to resolve SeqId: "+m_Screener.m_SeqLoc->GetId()->AsFastaString();
            ERR_POST(message);
            NCBI_THROW(CException, eUnknown, message);
        }
        CConstRef<CBioseq> bioseq = bhandle.GetBioseqCore();
        CBlastFormatUtil::AcknowledgeBlastQuery(*bioseq, 
                                                CBlastFormat::kFormatLineLength,
                                                out, kBelieveQuery,
                                                m_HtmlOutput, kIsTabular);
    }
    if (m_HtmlOutput) {
        m_Screener.m_Vecscreen->VecscreenPrint(out);
        if (match_list.empty() && !kPrintAlignments) {
            out << "<b>***** No hits found *****</b><br>\n";
        }
    } else {
        if (match_list.empty() && !kPrintAlignments) {
            out << "No hits found\n";
        } else {
            typedef pair<string, string> TLabels;
            vector<TLabels> match_labels;
            match_labels.push_back(TLabels("Strong", "Strong match"));
            match_labels.push_back(TLabels("Moderate", "Moderate match"));
            match_labels.push_back(TLabels("Weak", "Weak match"));
            match_labels.push_back(TLabels("Suspect", "Suspect origin"));

            ITERATE(vector<TLabels>, label, match_labels) {
                list<SVecscreenSummary>::iterator boundary, itr;
                boundary = stable_partition(match_list.begin(), match_list.end(),
                                            SVecscreenMatchFinder(label->first));
                if (boundary != match_list.begin()) {
                    out << label->second << "\n";
                    for (itr = match_list.begin(); itr != boundary; ++itr) {
                        out << itr->range.GetFrom()+1 << "\t" 
                            << itr->range.GetTo()+1 << "\n";
                    }
                    match_list.erase(match_list.begin(), boundary);
                }
            }
        }
    }

    if (kPrintAlignments) {
        CRef<CSearchResultSet> result_set = m_Screener.GetSearchResultSet();
        _ASSERT(result_set->size() == 1);
        blast_formatter.PrintOneResultSet((*result_set)[0],
                                          m_Screener.m_Queries);
        blast_formatter.PrintEpilog(vs_opts->GetOptions());
    }
}

list<CVecscreenRun::SVecscreenSummary>
CVecscreenRun::GetList() const
{
    _ASSERT(m_Vecscreen != NULL);
    list<CVecscreenRun::SVecscreenSummary> retval;

    const list<CVecscreen::AlnInfo*>* aln_info_ptr = m_Vecscreen->GetAlnInfoList();
    list<CVecscreen::AlnInfo> aln_info;
    ITERATE(list<CVecscreen::AlnInfo*>, ai, *aln_info_ptr) {
        if ((*ai)->type == CVecscreen::eNoMatch) 
            continue;
        CVecscreen::AlnInfo align_info((*ai)->range, (*ai)->type);
        aln_info.push_back(align_info);
    }
    aln_info.sort();

    ITERATE(list<CVecscreen::AlnInfo>, ai, aln_info) {
       SVecscreenSummary summary;
       summary.seqid = m_SeqLoc->GetId();
       summary.range = ai->range;
       summary.match_type = CVecscreen::GetStrengthString(ai->type);
       retval.push_back(summary);
    }
    return retval;
}

CRef<objects::CSeq_align_set>
CVecscreenRun::GetSeqalignSet() const
{
    return m_Seqalign_set;
}

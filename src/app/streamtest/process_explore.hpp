#ifndef __process_explore__hpp__
#define __process_explore__hpp__

#include <objmgr/util/indexer.hpp>

#include <objmgr/seq_vector.hpp>

//  ============================================================================
class CExploreProcess
//  ============================================================================
    : public CScopedProcess
{
public:
    //  ------------------------------------------------------------------------
    CExploreProcess()
    //  ------------------------------------------------------------------------
        : CScopedProcess()
        , m_out( 0 ), m_from( 0 ), m_to( 0 ), m_revcomp( false )
    {};

    //  ------------------------------------------------------------------------
    ~CExploreProcess()
    //  ------------------------------------------------------------------------
    {
    };

    //  ------------------------------------------------------------------------
    void ProcessInitialize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        CScopedProcess::ProcessInitialize( args );

        m_out = args["o"] ? &(args["o"].AsOutputFile()) : &cout;
        m_accn = args["accn"].AsString();
        if (args["from"]) {
            m_from = args["from"].AsInteger();
        }
        if (args["to"]) {
            m_to = args["to"].AsInteger();
        }
        if (args["revcomp"]) {
            m_revcomp = args["revcomp"];
        }
    };

    //  ------------------------------------------------------------------------
    void ProcessFinalize()
    //  ------------------------------------------------------------------------
    {
        // delete m_out;
    }

    //  ------------------------------------------------------------------------
    virtual void SeqEntryInitialize(
        CRef<CSeq_entry>& se )
    //  ------------------------------------------------------------------------
    {
        CScopedProcess::SeqEntryInitialize( se );
    };

    //  ------------------------------------------------------------------------
    void SeqEntryProcess()
    //  ------------------------------------------------------------------------
    {
        try {
  
            // Create master index variable on stack
            CSeqEntryIndex idx( CSeqEntryIndex::fDefaultIndexing );

            // Initialize with top-level Seq-entry
            idx.Initialize(*m_entry);

            // IterateBioseqs visits each Bioseq in blob
            int num_seqs = 0;
            idx.IterateBioseqs([this, &num_seqs](CBioseqIndex& bsx) {
                // Must pass &num_segs as closure argument for it to be visible and editable inside lambda function
                num_seqs++;
            });
            *m_out << "Bioseq count " << NStr::IntToString(num_seqs) << endl;

            // If no -accn argument, set with accession from first Bioseq
            if (m_accn.empty()) {
                // get first Bioseq
                idx.ProcessBioseq([this](CBioseqIndex& bsx) {
                    // Compiler does not allow passing m_accn member variable as closure argument
                    m_accn = bsx.GetAccession();
                });
            }

            if (m_from > 0 && m_to > 0) {
                // create temporary delta on Bioseq range, pass surrogate CBioseqIndex to lambda function
                bool ok = idx.ProcessBioseq(m_accn, m_from - 1, m_to - 1, m_revcomp, [this](CBioseqIndex& bsx) {

                    int num_feats = 0;
                    // IterateFeatures causes feature vector to be initialized by CFeat_CI
                    // ProcessBioseq on subregion provides delta sequence pointing to portion of original Bioseq
                    bsx.IterateFeatures([this, &bsx, &num_feats](CFeatureIndex& sfx) {

                        num_feats++;
                        CSeqFeatData::ESubtype sbt = sfx.GetSubtype();
                        if (bsx.IsNA() && sbt != CSeqFeatData::ESubtype::eSubtype_gene) {

                            // Print feature eSubtype number
                            *m_out << NStr::IntToString(sbt) << endl;

                            // Get referenced or overlapping gene using internal Bioseq-specific CFeatTree
                            string locus;
                            bool has_locus = sfx.GetBestGene([this, &sfx, sbt, &locus](CFeatureIndex& fsx){
                                CMappedFeat best = fsx.GetMappedFeat();
                                const CGene_ref& gene = best.GetData().GetGene();
                                if (gene.IsSetLocus()) {
                                    locus = gene.GetLocus();
                                }
                            });
                            if (has_locus) {
                                *m_out << locus << endl;
                            } else {
                                *m_out << "--" << endl;
                            }

                            // Print feature location
                            const CSeq_loc& loc = sfx.GetMappedFeat().GetOriginalFeature().GetLocation();
                            string loc_str;
                            loc.GetLabel(&loc_str);
                            *m_out << "Original: " << loc_str << endl;

                            CConstRef<CSeq_loc> sloc = sfx.GetMappedLocation();
                            if (sloc) {
                                string sloc_str;
                                sloc->GetLabel(&sloc_str);
                                *m_out << "MappedLoc: " << sloc_str << endl;
                            }

                            *m_out << endl;
                        }

                    });

                    *m_out << "Feature count " << NStr::IntToString(num_feats) << endl;
                });

                if (! ok) {
                    LOG_POST(Error << "Unable to find accession: " << m_accn);
                }
            }
        }
        catch (CException& e) {
            LOG_POST(Error << "Error processing seqentry: " << e.what());
        }
    }

protected:
    CNcbiOstream* m_out;
    string m_accn;
    int m_from;
    int m_to;
    bool m_revcomp;
    // CSeqEntryIndex* m_explore;
};

#endif



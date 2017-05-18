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
            CSeqEntryIndex idx(CSeqEntryIndex::fSkipRemoteFeatures);

            // Initialize with top-level Seq-entry
            idx.Initialize(*m_entry);

            // IterateBioseqs visits each Bioseq in blob
            int num_seqs = 0;
            idx.IterateBioseqs([this, &num_seqs](CBioseqIndex& bsx) {
                // Must pass &num_segs as closure argument for it to be visible inside lambda function
                num_seqs++;
            });
            *m_out << "Bioseq count " << NStr::IntToString(num_seqs) << endl;

            // If no -accn argument, set with accession from FirstBioseq
            if (m_accn.empty()) {
                idx.FirstBioseq([this](CBioseqIndex& bsx) {
                    // Compiler does not allow passing m_accn as closure argument
                    m_accn = bsx.GetAccession();
                    // *m_out << m_accn << endl;
                });
            }

            // Find Bioseq by -accn accession argument (ensures match with optional -from and -to arguments)
            bool ok = idx.FindBioseq(m_accn, [this](CBioseqIndex& bsx) {

                if (m_from > 0 && m_to > 0) {
                    // Force feature collection on specified subrange (ignores fSkipRemoteFeatures flag)
                    bsx.InitializeFeatures(m_from - 1, m_to - 1, m_revcomp);
                }

                int num_feats = 0;
                // IterateFeatures causes feature vector to be initialized by CFeat_CI,
                // if not already done on a subrange with a prior call to InitializeFeatures
                bsx.IterateFeatures([this, &bsx, &num_feats](CFeatureIndex& sfx) {
                    num_feats++;
                    CSeqFeatData::ESubtype sbt = sfx.GetSubtype();
                    if (bsx.IsNA() && sbt != CSeqFeatData::ESubtype::eSubtype_gene) {

                        // Print feature eSubtype number
                        *m_out << NStr::IntToString(sbt);

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
                            *m_out << "\t" << locus;
                        } else {
                            *m_out << "\t" << "--";
                        }

                        // Print feature location
                        *m_out << "\t" << SeqLocString(sfx.GetMappedFeat().GetLocation());

                        // If coding region, print underlying nucleotide sequence
                        if (sbt == CSeqFeatData::ESubtype::eSubtype_cdregion) {
                            const CMappedFeat cds = sfx.GetMappedFeat();
                            CSeqVector vec(cds.GetLocation(), *m_scope);
                            vec.SetCoding(CBioseq_Handle::eCoding_Iupac);
                            string coding;
                            vec.GetSeqData(0, vec.size(), coding);
                            *m_out << "\t" << coding;
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
        catch (CException& e) {
            LOG_POST(Error << "Error processing seqentry: " << e.what());
        }
    }

    //  ------------------------------------------------------------------------
    string SeqLocString( const CSeq_loc& loc )
    //  ------------------------------------------------------------------------
    {
        string str;
        loc.GetLabel(&str);
        return str;
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



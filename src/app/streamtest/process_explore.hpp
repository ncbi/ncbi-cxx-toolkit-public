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
        // Create master index variable on stack
        CSeqEntryIndex idx( CSeqEntryIndex::fDefaultIndexing );


        // Initialize with top-level Seq-entry, builds Bioseq index
        idx.Initialize(*m_entry);


        // IterateBioseqs visits each Bioseq in blob
        int num_nucs = 0;
        int num_prots = 0;
        idx.IterateBioseqs([&num_nucs, &num_prots](CBioseqIndex& bsx) {
            // Must pass &num_nucs and &num_prots as closure arguments
            if (bsx.IsNA()) {
                num_nucs++;
            } else if (bsx.IsAA()) {
                num_prots++;
            }
        });
        *m_out << "Record has " << num_nucs << " nucleotide" << ((num_nucs != 1) ? "s" : "");
        *m_out << " and " << num_prots << " protein" << ((num_prots != 1) ? "s" : "");
        *m_out << endl;


        bool ok = false;

        // Select desired sequence record, call application DoOneBioseq method
        if (m_from > 0 && m_to > 0) {

            // If no -accn argument, set with accession from first Bioseq
            if (m_accn.empty()) {
                // get first Bioseq
                idx.ProcessBioseq([this](CBioseqIndex& bsx) {
                    // Must pass "this" to see m_accn member variable
                    m_accn = bsx.GetAccession();
                });
            }

            // Create temporary delta on Bioseq range
            ok = idx.ProcessBioseq(m_accn, m_from - 1, m_to - 1, m_revcomp,
                                   [this](CBioseqIndex& bsx) {
                DoOneBioseq(bsx);
            });

        } else if (! m_accn.empty()) {

            // Process selected Bioseq
            ok = idx.ProcessBioseq(m_accn, [this](CBioseqIndex& bsx) {
                DoOneBioseq(bsx);
            });

        } else {

            // Process first Bioseq
            ok = idx.ProcessBioseq([this](CBioseqIndex& bsx) {
                DoOneBioseq(bsx);
            });

        }

        if (! ok) {
            LOG_POST(Error << "Unable to process Bioseq");
        }
    }

    //  ------------------------------------------------------------------------
    void DoOneBioseq(
        CBioseqIndex& bsx )
    //  ------------------------------------------------------------------------
    {
        *m_out << "Accession: " << bsx.GetAccession() << endl;
        *m_out << "Length: " << bsx.GetLength() << endl;
        *m_out << "Taxname: " << bsx.GetTaxname() << endl;

        *m_out << "Title: " << bsx.GetTitle() << endl;
        *m_out << "Defline: " << bsx.GetDefline(sequence::CDeflineGenerator::fIgnoreExisting) << endl;

        *m_out << endl;

        CRef<CSeqsetIndex> prnt = bsx.GetParent();
        if (prnt) {
            *m_out << "Parent Bioseq-set class: " << prnt->GetClass() << endl;
        }


        int num_feats = 0;
        // IterateFeatures causes feature vector to be initialized by CFeat_CI
        bsx.IterateFeatures([this, &bsx, &num_feats](CFeatureIndex& sfx) {

            num_feats++;
            CSeqFeatData::ESubtype sbt = sfx.GetSubtype();

            // Print feature eSubtype number
            *m_out << "Feature subtype: " << sbt << endl;

            if (bsx.IsNA()) {

                if (sbt != CSeqFeatData::ESubtype::eSubtype_gene) {

                    // Get referenced or overlapping gene using internal Bioseq-specific CFeatTree
                    string locus;
                    bool has_locus = sfx.GetBestGene([this, &sfx, &locus](CFeatureIndex& fsx){
                        const CGene_ref& gene = fsx.GetMappedFeat().GetData().GetGene();
                        if (gene.IsSetLocus()) {
                            locus = gene.GetLocus();
                        }
                    });
                    if (has_locus) {
                        *m_out << "Best gene: " << locus << endl;
                    }


                    // Print feature location
                    const CSeq_loc& loc = sfx.GetMappedFeat().GetOriginalFeature().GetLocation();
                    string loc_str;
                    loc.GetLabel(&loc_str);
                    *m_out << "Location: " << loc_str << endl;


                    CConstRef<CSeq_loc> sloc = sfx.GetMappedLocation();
                    if (sloc) {
                        string sloc_str;
                        sloc->GetLabel(&sloc_str);
                        if (! NStr::EqualNocase(loc_str, sloc_str)) {
                            // Print mapped location if different (i.e., using Bioseq sublocation)
                            *m_out << "MappedLoc: " << sloc_str << endl;
                        }
                    }


                    if (sbt == CSeqFeatData::ESubtype::eSubtype_cdregion) {
                        // Print nucleotide sequence under coding region
                        string feat_seq = sfx.GetSequence();
                        *m_out << "CdRegion seq: " << feat_seq << endl;
                    }
                }

                *m_out << endl;

            } else if (bsx.IsAA()) {

                if (sbt == CSeqFeatData::ESubtype::eSubtype_prot) {
                    // Print sequence under protein feature
                    string feat_seq = sfx.GetSequence();
                    *m_out << "Protein seq: " << feat_seq << endl;
                }

                *m_out << endl;
            }
        });

        *m_out << "Feature count " << num_feats << endl;
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



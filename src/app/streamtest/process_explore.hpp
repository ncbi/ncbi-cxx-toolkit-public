#ifndef __process_explore__hpp__
#define __process_explore__hpp__

#include <objmgr/util/indexer.hpp>

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
            CSeqEntryIndex idx(CSeqEntryIndex::fSkipRemoteFeatures);
            idx.Initialize(*m_entry);
            int num_seqs = 0;
            idx.IterateBioseqs([this, &num_seqs](CBioseqIndex& bsx) {
                num_seqs++;
            });
            *m_out << "Bioseq count " << NStr::IntToString(num_seqs) << endl;
            if (! m_accn.empty()) {
                idx.FindBioseq(m_accn, [this](CBioseqIndex& bsx) {
                    *m_out << bsx.GetAccession() << endl;
                    if (m_from > 0 && m_to > 0) {
                        *m_out << "InitializeFeatures from " << NStr::IntToString(m_from) << " to " << NStr::IntToString(m_to) << endl;
                        bsx.InitializeFeatures(m_from - 1, m_to - 1, m_revcomp);
                    }
                    int num_feats = 0;
                    bsx.IterateFeatures([this, &bsx, &num_feats](CFeatureIndex& sfx) {
                        num_feats++;
                        CSeqFeatData::ESubtype sbt = sfx.GetSubtype();
                        if (sbt != CSeqFeatData::ESubtype::eSubtype_gene) {
                            sfx.GetBestGene([this, &sfx, sbt](CFeatureIndex& fsx){
                                *m_out << NStr::IntToString(sbt);
                                CMappedFeat best = fsx.GetMappedFeat();
                                const CGene_ref& gene = best.GetData().GetGene();
                                if (gene.IsSetLocus()) {
                                    *m_out << "  " << gene.GetLocus();
                                }
                                *m_out << "  " << SeqLocString(sfx.GetMappedFeat().GetLocation());
                                *m_out << endl;
                            });
                        }
                    });
                    *m_out << "Feature count " << NStr::IntToString(num_feats) << endl;
                });
            }
        }
        catch (CException& e) {
            LOG_POST(Error << "error processing seqentry: " << e.what());
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



#ifndef __process_explore__hpp__
#define __process_explore__hpp__

#include <objmgr/util/indexer.hpp>

#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_map_ci.hpp>

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
        m_policy = 0;
        if (args["policy"]) {
            m_policy = args["policy"].AsInteger();
        }
        m_flags = 0;
        if (args["flags"]) {
            m_flags = args["flags"].AsInteger();
        }
        m_depth = -1;
        if (args["depth"]) {
            m_depth = args["depth"].AsInteger();
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
        // Create master index variable on stack, initialize with top-level Seq-entry
        CSeqEntryIndex idx( m_topseh, (CSeqEntryIndex::EPolicy) m_policy, (CSeqEntryIndex::TFlags) m_flags );
        idx.SetFeatDepth(m_depth);


        // IterateBioseqs visits each Bioseq in blob
        int num_nucs = 0;
        int num_prots = 0;

        CNcbiOstream* x_out = m_out;
       // Must pass &num_nucs and &num_prots references as closure arguments
       idx.IterateBioseqs([x_out, &num_nucs, &num_prots](CBioseqIndex& bsx) {
            // Lambda function executed for each Bioseq
            if (bsx.IsNA()) {
                SSeqMapSelector sel;
                sel.SetFlags(CSeqMap::fFindAny);
                CBioseq_Handle bsh = bsx.GetBioseqHandle();
                for ( CSeqMap_CI seg(ConstRef(&bsh.GetSeqMap()), &bsh.GetScope(), sel); seg; ++seg ) {
                    *x_out << "# " << seg.GetType() << " @ " << seg.GetPosition() << " - " << seg.GetEndPosition() << endl;
                }
                num_nucs++;
            } else if (bsx.IsAA()) {
                num_prots++;
            }
        });

        *m_out << "Record has " << num_nucs << " nucleotide" << ((num_nucs != 1) ? "s" : "");
        *m_out << " and " << num_prots << " protein" << ((num_prots != 1) ? "s" : "");
        *m_out << '\n';


        // Alternative method uses access to vector of CRef<CBioseqIndex> in for loop
        num_nucs = 0;
        num_prots = 0;
        for (auto &bsx : idx.GetBioseqIndices()) {
            if (bsx->IsNA()) {
                num_nucs++;
            } else if (bsx->IsAA()) {
                num_prots++;
            }
        }

        *m_out << "Second has " << num_nucs << " nucleotide" << ((num_nucs != 1) ? "s" : "");
        *m_out << " and " << num_prots << " protein" << ((num_prots != 1) ? "s" : "");
        *m_out << '\n';

        // Find Bioseq from arguments, call application DoOneBioseq method
        CRef<CBioseqIndex> bsx;

        // Select desired sequence record
        if (m_from > 0 && m_to > 0) {

            // subrange moved to IterateFeatures

          } else if (! m_accn.empty()) {

              // Process selected Bioseq
              bsx = idx.GetBioseqIndex(m_accn);
              if (bsx) {
                  DoOneBioseq(*bsx);
              }

        } else {

            if (num_nucs == 1 && num_prots > 0) {
                // Process second Bioseq (protein if nuc-prot set)
                // (GetFeatureForProduct will find the relevant CDS and index nucleotide features)
                bsx = idx.GetBioseqIndex(2);
                if (bsx) {
                    DoOneBioseq(*bsx);
                }
            }

            // Process first Bioseq (nucleotide if nuc-prot set)
            bsx = idx.GetBioseqIndex();
            if (bsx) {
                DoOneBioseq(*bsx);
            }
        }

        if (! bsx) {
            LOG_POST(Error << "Unable to process Bioseq");
        }

        // Report far fetch failure
        if (idx.IsFetchFailure()) {
            *m_out << "IsFetchFailure" << '\n';
        }
    }

    //  ------------------------------------------------------------------------
    void DoOneBioseq(
        CBioseqIndex& bsx )
    //  ------------------------------------------------------------------------
    {
        *m_out << "\nBIOSEQ\n";

        *m_out << "Accession: " << bsx.GetAccession() << '\n';
        *m_out << "Length: " << bsx.GetLength() << '\n';

        // Title, MolInfo, and BioSource fields are saved when descriptors are indexed (on first request)
        *m_out << "Taxname: " << bsx.GetTaxname() << '\n';
        *m_out << "Title: " << bsx.GetTitle() << '\n';

        *m_out << '\n';


        // Determine class of immediate Bioseq-set parent
        CRef<CSeqsetIndex> prnt = bsx.GetParent();
        if (prnt) {
            *m_out << "Parent Bioseq-set class: " << prnt->GetClass() << '\n';
        } else {
            *m_out << "Unable to process GetParent" << '\n';
        }


        // Get first part of underlying sequence
        string subseq = bsx.GetSequence(0, 9);
        *m_out << "First 10 bases '" << subseq << "'" << '\n';


        if (bsx.IsAA()) {
            CRef<CFeatureIndex> bestprot = bsx.GetBestProteinFeature();
            if (bestprot) {
                *m_out << "Best Protein subtype: " << bestprot->GetSubtype() << '\n';
            }
        }


        /*
        int num_gaps = bsx.IterateGaps([this, &bsx](CGapIndex& sgx) {

            const string& type = sgx.GetGapType();
            TSeqPos start = sgx.GetStart();
            TSeqPos end = sgx.GetEnd();
            *m_out << "Gap type: " << type << ", from: " << start << ", to: " << end << '\n';
        });

        *m_out << "Gap count " << num_gaps << '\n';
        */


        const vector<CRef<CGapIndex>>& gaps = bsx.GetGapIndices();
        string gap_type = "";
        int num_gaps = gaps.size();
        int next_gap = 0;
        TSeqPos gap_start = 0;
        TSeqPos gap_end = 0;
        bool has_gap = false;
        if (num_gaps > 0) {
            CRef<CGapIndex> sgr = gaps[next_gap];
            gap_start = sgr->GetStart();
            gap_end = sgr->GetEnd();
            gap_type = sgr->GetGapType();
            has_gap = true;
            next_gap++;
        }

        // IterateFeatures causes feature vector to be initialized by CFeat_CI, locations mapped to master
        int num_feats = bsx.IterateFeatures([this, &bsx, gaps, num_gaps, &next_gap, &has_gap, &gap_start, &gap_end, &gap_type](CFeatureIndex& sfx) {

            // Alternative to passing &bsx reference in closure
            // CBioseqIndex& bsx = sfx.GetBioseqIndex();

            TSeqPos feat_start = sfx.GetStart();

            while (has_gap && gap_start < feat_start) {
                *m_out << "Gap type: " << gap_type << ", from: " << gap_start << ", to: " << gap_end << '\n';
                if (next_gap < num_gaps) {
                    CRef<CGapIndex> sgr = gaps[next_gap];
                    gap_start = sgr->GetStart();
                    gap_end = sgr->GetEnd();
                    gap_type = sgr->GetGapType();
                    has_gap = true;
                    next_gap++;
                } else {
                    has_gap = false;
                }
            }

            CSeqFeatData::ESubtype sbt = sfx.GetSubtype();

            // Print feature eSubtype number
            *m_out << "Feature subtype: " << sbt << ", Start: " << feat_start << ", Stop: " << sfx.GetEnd() << '\n';


            if (bsx.IsNA()) {

                if (sbt != CSeqFeatData::ESubtype::eSubtype_gene) {

                    // Get referenced or overlapping gene using internal Bioseq-specific CFeatTree
                    string locus;
                    CRef<CFeatureIndex> fsx = sfx.GetBestGene();
                    if (fsx) {
                        const CGene_ref& gene = fsx->GetMappedFeat().GetData().GetGene();
                        if (gene.IsSetLocus()) {
                            locus = gene.GetLocus();
                            *m_out << "Best gene: " << locus << '\n';
                        } else if (gene.IsSetLocus_tag()) {
                            locus = gene.GetLocus_tag();
                            *m_out << "Best gene: " << locus << '\n';
                        }
                    }


                    // Print feature location
                    const CSeq_loc& loc = sfx.GetMappedFeat().GetOriginalFeature().GetLocation();
                    string loc_str;
                    loc.GetLabel(&loc_str);
                    *m_out << "Location: " << loc_str << '\n';


                    CConstRef<CSeq_loc> sloc = sfx.GetMappedLocation();
                    if (sloc) {
                        string sloc_str;
                        sloc->GetLabel(&sloc_str);
                        if (! NStr::EqualNocase(loc_str, sloc_str)) {
                            // Print mapped location if different (i.e., using Bioseq sublocation)
                            *m_out << "MappedLoc: " << sloc_str << '\n';
                        }
                    }


                    if (sbt == CSeqFeatData::ESubtype::eSubtype_cdregion) {
                        // Print nucleotide sequence under coding region
                        string feat_seq = sfx.GetSequence();
                        *m_out << "CdRegion seq: " << feat_seq << '\n';
                    }

                    const CMappedFeat mf = sfx.GetMappedFeat();
                    const CSeq_feat& mpped = mf.GetMappedFeature();
                    *m_out << "MappedFeat: " << MSerial_AsnText << mpped << '\n';

                    const CSeq_feat& sf = mf.GetOriginalFeature();
                    *m_out << "Original: " << MSerial_AsnText << sf << '\n';
                }

                *m_out << '\n';

            } else if (bsx.IsAA()) {

                if (sbt == CSeqFeatData::ESubtype::eSubtype_prot) {
                    // Print sequence under protein feature
                    string feat_seq = sfx.GetSequence();
                    *m_out << "Protein seq: " << feat_seq << '\n';
                }

                CRef<CFeatureIndex> sfxp = bsx.GetFeatureForProduct();
                if (sfxp) {
                    string locus;
                    CRef<CFeatureIndex> fsx = sfxp->GetBestGene();
                    if (fsx) {
                        const CGene_ref& gene = fsx->GetMappedFeat().GetData().GetGene();
                        if (gene.IsSetLocus()) {
                            locus = gene.GetLocus();
                            *m_out << "CDS gene: " << locus << '\n';
                        }
                    }
                }

                *m_out << '\n';
            }
        });

        *m_out << "Feature count " << num_feats << '\n';
    }

protected:
    CNcbiOstream* m_out;
    string m_accn;
    int m_from;
    int m_to;
    bool m_revcomp;
    int m_policy;
    int m_flags;
    int m_depth;
    // CSeqEntryIndex* m_explore;
};


//  ============================================================================
class CWordPairProcess
//  ============================================================================
    : public CScopedProcess
{
public:
    //  ------------------------------------------------------------------------
    CWordPairProcess()
    //  ------------------------------------------------------------------------
        : CScopedProcess()
        , m_out( 0 )
    {};

    //  ------------------------------------------------------------------------
    ~CWordPairProcess()
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
        // Test word pair indexer
        string ucode = "β-carotene 1¾ CO₂ H<sub>2</sub>O NH&lt;sub&gt;3&lt;/sub&gt; Tn&amp;lt;i&amp;gt;3&amp;lt;/i&amp;gt; ç ł å ß í č";
        string phrase = "selective serotonin reuptake inhibitor and monoamine-oxidase inhibitor and ((NH<sub>4</sub>+)3) hydroxide";

        CWordPairIndexer wpi;
        *m_out << "\nUcode: " << ucode << '\n';
        string ascii = wpi.ConvertUTF8ToAscii(ucode);
        *m_out << "Ascii: " << ascii << '\n';
        string mixed = wpi.TrimMixedContent(ascii);
        *m_out << "Clean: " << mixed << '\n';

        *m_out << "\nPhrase:\n" << phrase << '\n';
        wpi.PopulateWordPairIndex(phrase);

        *m_out << "\nNORM:\n";
        wpi.IterateNorm([this](string& norm) {
            *m_out << norm << '\n';
        });

        *m_out << "\nPAIR:\n";
        wpi.IteratePair([this](string& pair) {
            *m_out << pair << '\n';
        });

        *m_out << '\n';
    }

protected:
    CNcbiOstream* m_out;
};


#endif



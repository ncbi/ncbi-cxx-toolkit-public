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
        // Create master index variable on stack, initialize with top-level Seq-entry
        CSeqEntryIndex idx( *m_entry, CSeqEntryIndex::fSkipRemoteFeatures );


        // IterateBioseqs visits each Bioseq in blob
        int num_nucs = 0;
        int num_prots = 0;

       // Must pass &num_nucs and &num_prots references as closure arguments
       idx.IterateBioseqs([&num_nucs, &num_prots](CBioseqIndex& bsx) {
            // Lambda function executed for each Bioseq
            if (bsx.IsNA()) {
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

            // Create surrogate delta sequence pointing to Bioseq subrange (mixed indexing)
            if (m_accn.empty()) {
                // If no -accn argument, use first Bioseq
                bsx = idx.GetBioseqIndex(m_from - 1, m_to - 1, m_revcomp);
            } else {
                bsx = idx.GetBioseqIndex(m_accn, m_from - 1, m_to - 1, m_revcomp);
            }
            if (bsx) {
                DoOneBioseq(*bsx);
            }

        } else if (! m_accn.empty()) {

            // Process selected Bioseq
            bsx = idx.GetBioseqIndex(m_accn);
            if (bsx) {
                DoOneBioseq(*bsx);
            }

        } else {

            // Process first Bioseq
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
        *m_out << "Accession: " << bsx.GetAccession() << '\n';
        *m_out << "Length: " << bsx.GetLength() << '\n';

        // Title, MolInfo, and BioSource fields are saved when descriptors are indexed (on first request)
        *m_out << "Taxname: " << bsx.GetTaxname() << '\n';
        *m_out << "Title: " << bsx.GetTitle() << '\n';

        // Passes Bioseq-specific CFeatTree to defline generator for efficiency
        *m_out << "Defline: " << bsx.GetDefline(sequence::CDeflineGenerator::fIgnoreExisting) << '\n';

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


        // IterateFeatures causes feature vector to be initialized by CFeat_CI, locations mapped to master
        int num_feats = bsx.IterateFeatures([this, &bsx](CFeatureIndex& sfx) {

            // Alternative to passing &bsx reference in closure
            // CBioseqIndex& bsx = sfx.GetBioseqIndex();

            CSeqFeatData::ESubtype sbt = sfx.GetSubtype();

            // Print feature eSubtype number
            *m_out << "Feature subtype: " << sbt << '\n';


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
                }

                *m_out << '\n';

            } else if (bsx.IsAA()) {

                if (sbt == CSeqFeatData::ESubtype::eSubtype_prot) {
                    // Print sequence under protein feature
                    string feat_seq = sfx.GetSequence();
                    *m_out << "Protein seq: " << feat_seq << '\n';
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



#ifndef OBJTOOLS_FORMAT___FLAT_FILE_CONFIG__HPP
#define OBJTOOLS_FORMAT___FLAT_FILE_CONFIG__HPP

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
 * Authors:  Mati Shomrat, NCBI
 *
 * File Description:
 *    Configuration class for flat-file generator
 */
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <util/icanceled.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CBioseqContext;
class CStartSectionItem;
class CHtmlAnchorItem;
class CLocusItem;
class CDeflineItem;
class CAccessionItem;
class CVersionItem;
class CGenomeProjectItem;
class CDBSourceItem;
class CKeywordsItem;
class CSegmentItem;
class CSourceItem;
class CReferenceItem;
class CCommentItem;
class CPrimaryItem;
class CFeatHeaderItem;
class CSourceFeatureItem;
class CFeatureItem;
class CGapItem;
class CBaseCountItem;
class COriginItem;
class CSequenceItem;
class CContigItem;
class CWGSItem;
class CTSAItem;
class CEndSectionItem;
class IFlatItem;

// --- Flat File configuration class

class NCBI_FORMAT_EXPORT CFlatFileConfig
{
public:
    
    enum EFormat {
        // formatting styles    
        eFormat_GenBank,
        eFormat_EMBL,
        eFormat_DDBJ,
        eFormat_GBSeq,
        eFormat_FTable,
        eFormat_GFF, // version 2, w/GTF
        eFormat_GFF3,
        eFormat_FeaturesOnly,
        eFormat_SAM
    };

    enum EMode {
        // determines the tradeoff between strictness and completeness
        eMode_Release = 0, // strict -- for official public releases
        eMode_Entrez,      // somewhat laxer -- for CGIs
        eMode_GBench,      // even laxer -- for editing submissions
        eMode_Dump         // shows everything, regardless of validity
    };


    enum EStyle {
        // determines handling of segmented records
        eStyle_Normal,  // default -- show segments iff they're near
        eStyle_Segment, // always show segments
        eStyle_Master,  // merge segments into a single virtual record
        eStyle_Contig   // just an index of segments -- no actual sequence
    };

    enum EFlags {
        // customization flags
        fDoHTML                = 1,
        fShowContigFeatures    = 1 << 1,  // not just source features
        fShowContigSources     = 1 << 2,  // not just focus
        fShowFarTranslations   = 1 << 3,
        fTranslateIfNoProduct  = 1 << 4,
        fAlwaysTranslateCDS    = 1 << 5,
        fOnlyNearFeatures      = 1 << 6,
        fFavorFarFeatures      = 1 << 7,  // ignore near feats on segs w/far feats
        fCopyCDSFromCDNA       = 1 << 8,  // for gen-prod sets
        fCopyGeneToCDNA        = 1 << 9,  // for gen-prod sets
        fShowContigInMaster    = 1 << 10,
        fHideImpFeatures       = 1 << 11,
        fHideRemoteImpFeatures = 1 << 12,
        fHideSNPFeatures       = 1 << 13,
        fHideExonFeatures      = 1 << 14,
        fHideIntronFeatures    = 1 << 15,
        fHideMiscFeatures      = 1 << 16,
        fHideCDSProdFeatures   = 1 << 17,
        fHideCDDFeatures       = 1 << 18,
        fShowTranscript        = 1 << 19,
        fShowPeptides          = 1 << 20,
        fHideGeneRIFs          = 1 << 21,
        fOnlyGeneRIFs          = 1 << 22,
        fLatestGeneRIFs        = 1 << 23,
        fShowContigAndSeq      = 1 << 24,
        fHideSourceFeatures    = 1 << 25,
        fShowFtableRefs        = 1 << 26,
        fOldFeaturesOrder      = 1 << 27,
        fHideGapFeatures       = 1 << 28,
        fNeverTranslateCDS     = 1 << 29,
        fShowSeqSpans          = 1 << 30
    };

    enum EView {
        // determines which Bioseqs in an entry to view
        fViewNucleotides  = 0x1,
        fViewProteins     = 0x2,
        fViewAll          = (fViewNucleotides | fViewProteins)
    };

    enum EGffOptions {
        // specifies special features of GFF(3) generation
        fGffGenerateIdTags     = 1 << 0,
        fGffGTFCompat          = 1 << 1, ///< Represent CDSs (and exons) per GTF.
        fGffGTFOnly            = 1 << 2, ///< Omit all other features.
        fGffShowSeq            = 1 << 3, ///< Show the actual sequence in a "##" comment.
        // There are at least 3 flavours of GFF3, discounting multiple
        // versions of each:
        // - "Official specifications" by the Sequence Ontology group (SO),
        //   see http://www.sequenceontology.org/gff3.shtml
        // - GFF3 as modified by the Interoperability Working Group (IOWG),
        //   see http://www.pathogenportal.org/gff3-usage-conventions.html
        // - GFF3 as modified for exchange with Flybase. This alters
        //   such critical information as how phase is represented.
        fGffForFlybase         = 1 << 4, ///< Flybase flavour of GFF3.
    };

    // These flags are used to select the GenBank sections to print or skip.
    enum FGenbankBlocks {
        // default is all sections
        fGenbankBlocks_All        = (~0u),
        fGenbankBlocks_None       = 0,

        // or, specify individual sections to print.
        // or, specify individual sections to skip
        // (via (fGenbankBlocks_All & ~fGenbankBlocks_Locus), for example)
        // Note that these flags do NOT have a one-to-one relationship
        // with the notify() functions in CGenbankBlockCallback.
        // For example, fGenbankBlocks_FeatAndGap is used to turn
        // on both the CFeatureItem and CGapItem notify functions.
        fGenbankBlocks_Head       = (1u <<  0),
        fGenbankBlocks_Locus      = (1u <<  1),
        fGenbankBlocks_Defline    = (1u <<  2),
        fGenbankBlocks_Accession  = (1u <<  3),
        fGenbankBlocks_Version    = (1u <<  4),
        fGenbankBlocks_Project    = (1u <<  5),
        fGenbankBlocks_Dbsource   = (1u <<  6),
        fGenbankBlocks_Keywords   = (1u <<  7),
        fGenbankBlocks_Segment    = (1u <<  8),
        fGenbankBlocks_Source     = (1u <<  9),
        fGenbankBlocks_Reference  = (1u << 10),
        fGenbankBlocks_Comment    = (1u << 11),
        fGenbankBlocks_Primary    = (1u << 12), 
        fGenbankBlocks_Featheader = (1u << 13),
        fGenbankBlocks_Sourcefeat = (1u << 14),
        fGenbankBlocks_FeatAndGap = (1u << 15),
        fGenbankBlocks_Basecount  = (1u << 16),
        fGenbankBlocks_Origin     = (1u << 17),
        fGenbankBlocks_Sequence   = (1u << 18),
        fGenbankBlocks_Contig     = (1u << 19),
        fGenbankBlocks_Wgs        = (1u << 20),
        fGenbankBlocks_Tsa        = (1u << 21),
        fGenbankBlocks_Slash      = (1u << 22)
    };

    // types
    typedef EFormat         TFormat;
    typedef EMode           TMode;
    typedef EStyle          TStyle;
    typedef unsigned int    TFlags; // binary OR of "EFlatFileFlags"
    typedef EView           TView;
    typedef unsigned int    TGffOptions;
    typedef unsigned int    TGenbankBlocks;
    
    class NCBI_FORMAT_EXPORT CGenbankBlockCallback : public CObject {
    public:
        enum EAction {
            /// the normal case
            eAction_Default,
            /// skip this block (i.e. don't print it)
            eAction_Skip,
            /// If for some reason you don't want the rest of the flatfile generated, 
            /// this will trigger a CFlatException of type eHaltRequested
            eAction_HaltFlatfileGeneration
        };

        enum EBioseqSkip {
            eBioseqSkip_No,
            eBioseqSkip_Yes
        };

        virtual ~CGenbankBlockCallback(void) { }

        // It is intentional that CBioseqContext is non-const here, 
        // but const in the other notify functions.
        virtual EBioseqSkip notify_bioseq( CBioseqContext& ctx );

        // please note that these notify functions let you change the block_text
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CStartSectionItem & head_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CHtmlAnchorItem & anchor_item );
        virtual EAction notify( string & block_text, 
                                const CBioseqContext& ctx,
                                const CLocusItem &locus_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CDeflineItem & defline_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CAccessionItem & accession_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CVersionItem & version_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CGenomeProjectItem & project_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CDBSourceItem & dbsource_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CKeywordsItem & keywords_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CSegmentItem & segment_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CSourceItem & source_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CReferenceItem & ref_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CCommentItem & comment_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CPrimaryItem & primary_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CFeatHeaderItem & featheader_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CSourceFeatureItem & sourcefeat_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CFeatureItem & feature_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CGapItem & feature_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CBaseCountItem & basecount_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const COriginItem & origin_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CSequenceItem & sequence_chunk_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CContigItem & contig_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CWGSItem & wgs_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CTSAItem & tsa_item );
        virtual EAction notify( string & block_text,
                                const CBioseqContext& ctx,
                                const CEndSectionItem & slash_item );

        // add more overridable functions if more blocks are invented


        // ...or override
        // this if you want only a single entry-point for
        // notifications.
        virtual EAction unified_notify( 
            string & block_text,
            const CBioseqContext& ctx, 
            const IFlatItem & flat_item, 
            FGenbankBlocks which_block ) { return eAction_Default; }
    };

    // constructors
    CFlatFileConfig(TFormat format = eFormat_GenBank,
                    TMode   mode = eMode_GBench,
                    TStyle  style = eStyle_Normal,
                    TFlags  flags = 0,
                    TView   view = fViewNucleotides,
                    TGffOptions gff_options = fGffGTFCompat,
                    TGenbankBlocks genbank_blocks = fGenbankBlocks_All,
                    CGenbankBlockCallback* pGenbankBlockCallback = NULL,
                    const ICanceled * pCanceledCallback = NULL,
                    bool basicCleanup = false );
    // destructor
    ~CFlatFileConfig(void);

    // -- Format
    // getters
    const TFormat& GetFormat(void) const { return m_Format; }
    bool IsFormatGenbank(void) const { return m_Format == eFormat_GenBank; }
    bool IsFormatEMBL   (void) const { return m_Format == eFormat_EMBL;    }
    bool IsFormatDDBJ   (void) const { return m_Format == eFormat_DDBJ;    }
    bool IsFormatGBSeq  (void) const { return m_Format == eFormat_GBSeq;   }
    bool IsFormatFTable (void) const { return m_Format == eFormat_FTable;  }
    bool IsFormatGFF    (void) const { return m_Format == eFormat_GFF;     }
    bool IsFormatGFF3   (void) const { return m_Format == eFormat_GFF3;    }
    // setters
    void SetFormat(const TFormat& format) { m_Format = format; }
    void SetFormatGenbank(void) { m_Format = eFormat_GenBank; }
    void SetFormatEMBL   (void) { m_Format = eFormat_EMBL;    }
    void SetFormatDDBJ   (void) { m_Format = eFormat_DDBJ;    }
    void SetFormatGBSeq  (void) { m_Format = eFormat_GBSeq;   }
    void SetFormatFTable (void) { m_Format = eFormat_FTable;  }
    void SetFormatGFF    (void) { m_Format = eFormat_GFF;     }
    void SetFormatGFF3   (void) { m_Format = eFormat_GFF3;     }

    // -- Mode
    // getters
    const TMode& GetMode(void) const { return m_Mode; }
    bool IsModeRelease(void) const { return m_Mode == eMode_Release; }
    bool IsModeEntrez (void) const { return m_Mode == eMode_Entrez;  }
    bool IsModeGBench (void) const { return m_Mode == eMode_GBench;  }
    bool IsModeDump   (void) const { return m_Mode == eMode_Dump;    }
    // setters
    void SetMode(const TMode& mode) { m_Mode = mode; }
    void SetModeRelease(void) { m_Mode = eMode_Release; }
    void SetModeEntrez (void) { m_Mode = eMode_Entrez;  }
    void SetModeGBench (void) { m_Mode = eMode_GBench;  }
    void SetModeDump   (void) { m_Mode = eMode_Dump;    }

    // -- Style
    // getters
    const TStyle& GetStyle(void) const { return m_Style; }
    bool IsStyleNormal (void) const { return m_Style == eStyle_Normal;  }
    bool IsStyleSegment(void) const { return m_Style == eStyle_Segment; }
    bool IsStyleMaster (void) const { return m_Style == eStyle_Master;  }
    bool IsStyleContig (void) const { return m_Style == eStyle_Contig;  }
    // setters
    void SetStyle(const TStyle& style) { m_Style = style;  }
    void SetStyleNormal (void) { m_Style = eStyle_Normal;  }
    void SetStyleSegment(void) { m_Style = eStyle_Segment; }
    void SetStyleMaster (void) { m_Style = eStyle_Master;  }
    void SetStyleContig (void) { m_Style = eStyle_Contig;  }

    // -- View
    // getters
    const TView& GetView(void) const { return m_View; }
    bool IsViewNuc (void) const { return (m_View & fViewNucleotides) != 0; }
    bool IsViewProt(void) const { return (m_View & fViewProteins)    != 0; }
    bool IsViewAll (void) const { return IsViewNuc()  &&  IsViewProt();    }
    // setters
    void SetView(const TView& view) { m_View = view; }
    void SetViewNuc (void) { m_View = fViewNucleotides; }
    void SetViewProt(void) { m_View = fViewProteins;    }
    void SetViewAll (void) { m_View = fViewAll;         } 

    // -- Flags
    // getters
    const TFlags& GetFlags(void) const { return m_Flags; }
    // customizable flags
    bool DoHTML                (void) const;
    bool HideImpFeatures       (void) const;
    bool HideSNPFeatures       (void) const;
    bool HideExonFeatures      (void) const;
    bool HideIntronFeatures    (void) const;
    bool HideMiscFeatures      (void) const;
    bool HideRemoteImpFeatures (void) const;
    bool HideGeneRIFs          (void) const;
    bool OnlyGeneRIFs          (void) const;
    bool HideCDSProdFeatures   (void) const;
    bool HideCDDFeatures       (void) const;
    bool LatestGeneRIFs        (void) const;
    bool ShowContigFeatures    (void) const;
    bool ShowContigSources     (void) const;
    bool ShowContigAndSeq      (void) const;
    bool CopyGeneToCDNA        (void) const;
    bool ShowContigInMaster    (void) const;
    bool CopyCDSFromCDNA       (void) const;
    bool HideSourceFeatures    (void) const;
    bool AlwaysTranslateCDS    (void) const;
    bool OnlyNearFeatures      (void) const;
    bool FavorFarFeatures      (void) const;
    bool ShowFarTranslations   (void) const;
    bool TranslateIfNoProduct  (void) const;
    bool ShowTranscript        (void) const;
    bool ShowPeptides          (void) const;
    bool ShowFtableRefs        (void) const;
    bool OldFeaturesOrder      (void) const;
    bool HideGapFeatures       (void) const;
    bool NeverTranslateCDS     (void) const;
    bool ShowSeqSpans          (void) const;
    // mode dependant flags
    bool SuppressLocalId     (void) const;
    bool ValidateFeatures    (void) const;
    bool IgnorePatPubs       (void) const;
    bool DropShortAA         (void) const;
    bool AvoidLocusColl      (void) const;
    bool IupacaaOnly         (void) const;
    bool DropBadCitGens      (void) const;
    bool NoAffilOnUnpub      (void) const;
    bool DropIllegalQuals    (void) const;
    bool CheckQualSyntax     (void) const;
    bool NeedRequiredQuals   (void) const;
    bool NeedOrganismQual    (void) const;
    bool NeedAtLeastOneRef   (void) const;
    bool CitArtIsoJta        (void) const;
    bool DropBadDbxref       (void) const;
    bool UseEmblMolType      (void) const;
    bool HideBankItComment   (void) const;
    bool CheckCDSProductId   (void) const;
    bool FrequencyToNote      (void) const;
    bool SrcQualsToNote      (void) const;
    bool HideEmptySource     (void) const;
    bool GoQualsToNote       (void) const;
    bool SelenocysteineToNote(void) const;
    bool ForGBRelease        (void) const;
    bool HideUnclassPartial  (void) const;
    bool CodonRecognizedToNote(void) const;
    bool GoQualsEachMerge    (void) const;
    bool ShowOutOfBoundsFeats(void) const;
    bool HideSpecificGeneMaps(void) const;
    
    // adjust mode dependant flags for RefSeq
    void SetRefSeqConventions(void);

    // setters (for customization flags)
    void SetFlags(const TFlags& flags) { m_Flags = flags; }
    CFlatFileConfig& SetDoHTML               (bool val = true);
    CFlatFileConfig& SetHideImpFeatures      (bool val = true);
    CFlatFileConfig& SetHideSNPFeatures      (bool val = true);
    CFlatFileConfig& SetHideExonFeatures     (bool val = true);
    CFlatFileConfig& SetHideIntronFeatures   (bool val = true);
    CFlatFileConfig& SetHideMiscFeatures     (bool val = true);
    CFlatFileConfig& SetHideRemoteImpFeatures(bool val = true);
    CFlatFileConfig& SetHideGeneRIFs         (bool val = true);
    CFlatFileConfig& SetOnlyGeneRIFs         (bool val = true);
    CFlatFileConfig& SetHideCDSProdFeatures  (bool val = true);
    CFlatFileConfig& SetHideCDDFeatures      (bool val = true);
    CFlatFileConfig& SetLatestGeneRIFs       (bool val = true);
    CFlatFileConfig& SetShowContigFeatures   (bool val = true);
    CFlatFileConfig& SetShowContigSources    (bool val = true);
    CFlatFileConfig& SetShowContigAndSeq     (bool val = true);
    CFlatFileConfig& SetCopyGeneToCDNA       (bool val = true);
    CFlatFileConfig& SetShowContigInMaster   (bool val = true);
    CFlatFileConfig& SetCopyCDSFromCDNA      (bool val = true);
    CFlatFileConfig& SetHideSourceFeatures   (bool val = true);
    CFlatFileConfig& SetAlwaysTranslateCDS   (bool val = true);
    CFlatFileConfig& SetOnlyNearFeatures     (bool val = true);
    CFlatFileConfig& SetFavorFarFeatures     (bool val = true);
    CFlatFileConfig& SetShowFarTranslations  (bool val = true);
    CFlatFileConfig& SetTranslateIfNoProduct (bool val = true);
    CFlatFileConfig& SetShowTranscript       (bool val = true);
    CFlatFileConfig& SetShowPeptides         (bool val = true);
    CFlatFileConfig& SetShowFtableRefs       (bool val = true);
    CFlatFileConfig& SetOldFeaturesOrder     (bool val = true);
    CFlatFileConfig& SetHideGapFeatures      (bool val = true);
    CFlatFileConfig& SetNeverTranslateCDS    (bool val = true);
    CFlatFileConfig& SetShowSeqSpans         (bool val = true);
    
    // -- GffOptions
    // getters
    bool GffGenerateIdTags   (void) const 
    { 
        return (0 != (m_GffOptions & fGffGenerateIdTags));
    };

    bool GffGTFCompat        (void) const 
    { 
        return (0 != (m_GffOptions & fGffGTFCompat));
    };

    bool GffGTFOnly          (void) const 
    { 
        return (0 != (m_GffOptions & fGffGTFOnly));
    };

    bool GffShowSeq          (void) const 
    { 
        return (0 != (m_GffOptions & fGffShowSeq));
    };

    bool GffForFlybase       (void) const 
    { 
        return (0 != (m_GffOptions & fGffForFlybase));
    };

    // setters
    void SetGffGenerateIdTags (bool val = true)
    {
        m_GffOptions |= fGffGenerateIdTags;
    };

    void SetGffGTFCompat      (bool val = true)
    {
        m_GffOptions |= fGffGTFCompat;
    };

    void SetGffGTFOnly        (bool val = true)
    {
        m_GffOptions |= fGffGTFOnly;
    };

    void SetGffShowSeq        (bool val = true)
    {
        m_GffOptions |= fGffShowSeq;
    };

    void SetGffForFlybase     (bool val = true)
    {
        m_GffOptions |= fGffForFlybase;
    };

    // check if the given section is shown
    bool IsShownGenbankBlock(FGenbankBlocks fTGenbankBlocksMask) const
    {
        return (m_fGenbankBlocks & fTGenbankBlocksMask) == fTGenbankBlocksMask;
    }

    // set the given section to be shown
    void ShowGenbankBlock(FGenbankBlocks fTGenbankBlocksMask)
    {
        m_fGenbankBlocks |= (fTGenbankBlocksMask);
    }

    // set the given section to be skipped
    // (that is, not shown or even processed)
    void SkipGenbankBlock(FGenbankBlocks fTGenbankBlocksMask)
    {
        m_fGenbankBlocks &= (~fTGenbankBlocksMask);
    }

    // throws on error
    static FGenbankBlocks StringToGenbankBlock(const string & str);
    // returns the set of all possible genbank strings
    // "head, "locus", etc.  Guaranteed to be sorted.
    static const vector<string> & GetAllGenbankStrings(void);

    // return non-const callback even if the CFlatFileConfig is const
    CRef<CGenbankBlockCallback> GetGenbankBlockCallback(void) const { return m_GenbankBlockCallback; }

    const ICanceled * GetCanceledCallback(void) const {
        return m_pCanceledCallback;
    }

    // -- BasicCleanup
    // getters
    const bool BasicCleanup(void) const { return m_BasicCleanup; }
    // setters
    void BasicCleanup(bool clean) { m_BasicCleanup = clean; }

    /// This throws a CFlatException if 
    /// flatfile generation cancellation has been requested via ICanceled.
    void ThrowIfCanceled(void) const {
        if( m_pCanceledCallback && m_pCanceledCallback->IsCanceled() ) {
            // halt requested, so throw the appropriate exception
            x_ThrowHaltNow();
        }
    }

public:
    static const size_t SMARTFEATLIMIT = 1000000;

private:
    // mode specific flags
    static const bool sm_ModeFlags[4][32];

    void x_ThrowHaltNow(void) const;

    // data
    TFormat     m_Format;
    TMode       m_Mode;
    TStyle      m_Style;
    TView       m_View;
    TFlags      m_Flags;  // custom flags
    bool        m_RefSeqConventions;
    TGffOptions m_GffOptions;
    TGenbankBlocks m_fGenbankBlocks;
    CRef<CGenbankBlockCallback> m_GenbankBlockCallback;
    const ICanceled * m_pCanceledCallback; // instance does NOT own it
    bool        m_BasicCleanup;
};


/////////////////////////////////////////////////////////////////////////////
// inilne methods

// custom flags
#define CUSTOM_FLAG_GET(x) \
inline \
bool CFlatFileConfig::x(void) const \
{ \
    return (m_Flags & f##x) != 0; \
}

#define CUSTOM_FLAG_SET(x) inline \
CFlatFileConfig& CFlatFileConfig::Set##x(bool val) \
{ \
    if ( val ) { \
        m_Flags |= f##x; \
    } else { \
        m_Flags &= ~f##x; \
    } \
    return *this; \
}

#define CUSTOM_FLAG_IMP(x) \
CUSTOM_FLAG_GET(x) \
CUSTOM_FLAG_SET(x)

CUSTOM_FLAG_IMP(DoHTML)
CUSTOM_FLAG_IMP(HideImpFeatures)
CUSTOM_FLAG_IMP(HideSNPFeatures)
CUSTOM_FLAG_IMP(HideExonFeatures)
CUSTOM_FLAG_IMP(HideIntronFeatures)
CUSTOM_FLAG_IMP(HideMiscFeatures)
CUSTOM_FLAG_IMP(HideRemoteImpFeatures)
CUSTOM_FLAG_IMP(HideGeneRIFs)
CUSTOM_FLAG_IMP(OnlyGeneRIFs)
CUSTOM_FLAG_IMP(HideCDSProdFeatures)
CUSTOM_FLAG_IMP(HideCDDFeatures)
CUSTOM_FLAG_IMP(LatestGeneRIFs)
CUSTOM_FLAG_IMP(ShowContigFeatures)
CUSTOM_FLAG_IMP(ShowContigSources)
CUSTOM_FLAG_IMP(ShowContigAndSeq)
CUSTOM_FLAG_IMP(CopyGeneToCDNA)
CUSTOM_FLAG_IMP(ShowContigInMaster)
CUSTOM_FLAG_IMP(CopyCDSFromCDNA)
CUSTOM_FLAG_IMP(HideSourceFeatures)
CUSTOM_FLAG_IMP(AlwaysTranslateCDS)
CUSTOM_FLAG_IMP(OnlyNearFeatures)
CUSTOM_FLAG_IMP(FavorFarFeatures)
CUSTOM_FLAG_IMP(ShowFarTranslations)
CUSTOM_FLAG_IMP(TranslateIfNoProduct)
CUSTOM_FLAG_IMP(ShowTranscript)
CUSTOM_FLAG_IMP(ShowPeptides)
CUSTOM_FLAG_IMP(ShowFtableRefs)
CUSTOM_FLAG_IMP(OldFeaturesOrder)
CUSTOM_FLAG_IMP(HideGapFeatures)
CUSTOM_FLAG_IMP(NeverTranslateCDS)
CUSTOM_FLAG_IMP(ShowSeqSpans)

#undef CUSTOM_FLAG_IMP
#undef CUSTOM_FLAG_GET
#undef CUSTOM_FLAG_SET

inline
void CFlatFileConfig::SetRefSeqConventions(void)
{
    m_RefSeqConventions = true;
}

// end of inline methods
/////////////////////////////////////////////////////////////////////////////

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT___FLAT_FILE_CONFIG__HPP */

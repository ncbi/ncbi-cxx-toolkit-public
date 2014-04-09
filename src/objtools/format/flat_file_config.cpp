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
* Author:  Mati Shomrat
*
* File Description:
*   Configuration class for flat-file generator
*   
*/
#include <ncbi_pch.hpp>
#include <objtools/format/flat_file_config.hpp>
#include <util/static_map.hpp>
#include <corelib/ncbistd.hpp>

#include <objtools/format/items/accession_item.hpp>
#include <objtools/format/items/basecount_item.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/items/contig_item.hpp>
#include <objtools/format/items/ctrl_items.hpp>
#include <objtools/format/items/dbsource_item.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/items/gap_item.hpp>
#include <objtools/format/items/genome_project_item.hpp>
#include <objtools/format/items/html_anchor_item.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/origin_item.hpp>
#include <objtools/format/items/primary_item.hpp>
#include <objtools/format/items/reference_item.hpp>
#include <objtools/format/items/segment_item.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/items/source_item.hpp>
#include <objtools/format/items/tsa_item.hpp>
#include <objtools/format/items/version_item.hpp>
#include <objtools/format/items/wgs_item.hpp>
#include <objtools/format/flat_expt.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CFlatFileConfig::CGenbankBlockCallback::EBioseqSkip
CFlatFileConfig::CGenbankBlockCallback::notify_bioseq( 
    CBioseqContext& ctx )
{
    // default is to do nothing; feel free to override it
    return eBioseqSkip_No;
}

CFlatFileConfig::CGenbankBlockCallback::EAction 
CFlatFileConfig::CGenbankBlockCallback::notify( 
    string & block_text,
    const CBioseqContext& ctx,
    const CStartSectionItem & head_item ) 
{ 
    return unified_notify(block_text, ctx, head_item, fGenbankBlocks_Head); 
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CHtmlAnchorItem & anchor_item )
{
    return unified_notify(block_text, ctx, anchor_item, fGenbankBlocks_None); 
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text, 
    const CBioseqContext& ctx,
    const CLocusItem &locus_item ) 
{
    return unified_notify(block_text, ctx, locus_item, fGenbankBlocks_Locus);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CDeflineItem & defline_item ) 
{
    return unified_notify(block_text, ctx,  defline_item, fGenbankBlocks_Defline);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CAccessionItem & accession_item ) 
{
    return unified_notify(block_text, ctx,  accession_item, fGenbankBlocks_Accession);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CVersionItem & version_item ) 
{
    return unified_notify(block_text, ctx,  version_item, fGenbankBlocks_Version);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CGenomeProjectItem & project_item ) 
{
    return unified_notify(block_text, ctx,  project_item, fGenbankBlocks_Project);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CDBSourceItem & dbsource_item ) 
{
    return unified_notify(block_text, ctx,  dbsource_item, fGenbankBlocks_Dbsource);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CKeywordsItem & keywords_item ) 
{
    return unified_notify(block_text, ctx,  keywords_item, fGenbankBlocks_Keywords);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CSegmentItem & segment_item ) 
{
    return unified_notify(block_text, ctx,  segment_item, fGenbankBlocks_Segment);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CSourceItem & source_item ) 
{
    return unified_notify(block_text, ctx,  source_item, fGenbankBlocks_Source);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CReferenceItem & ref_item ) 
{
    return unified_notify(block_text, ctx,  ref_item, fGenbankBlocks_Reference);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CCommentItem & comment_item ) 
{
    return unified_notify(block_text, ctx,  comment_item, fGenbankBlocks_Comment);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CPrimaryItem & primary_item ) 
{
    return unified_notify(block_text, ctx,  primary_item, fGenbankBlocks_Primary);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CFeatHeaderItem & featheader_item ) 
{
    return unified_notify(block_text, ctx,  featheader_item, fGenbankBlocks_Featheader);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CSourceFeatureItem & sourcefeat_item ) 
{
    return unified_notify(block_text, ctx,  sourcefeat_item, fGenbankBlocks_Sourcefeat);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CFeatureItem & feature_item ) 
{
    return unified_notify(block_text, ctx,  feature_item, fGenbankBlocks_FeatAndGap);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CGapItem & feature_item ) 
{
    return unified_notify(block_text, ctx,  feature_item, fGenbankBlocks_FeatAndGap);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CBaseCountItem & basecount_item ) 
{
    return unified_notify(block_text, ctx,  basecount_item, fGenbankBlocks_Basecount);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const COriginItem & origin_item ) 
{
    return unified_notify(block_text, ctx,  origin_item, fGenbankBlocks_Origin);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CSequenceItem & sequence_chunk_item ) 
{
    return unified_notify(block_text, ctx,  sequence_chunk_item, fGenbankBlocks_Sequence);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CContigItem & contig_item ) 
{
    return unified_notify(block_text, ctx,  contig_item, fGenbankBlocks_Contig);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CWGSItem & wgs_item ) 
{
    return unified_notify(block_text, ctx,  wgs_item, fGenbankBlocks_Wgs);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CTSAItem & tsa_item ) 
{
    return unified_notify(block_text, ctx,  tsa_item, fGenbankBlocks_Tsa);
}

CFlatFileConfig::CGenbankBlockCallback::EAction
CFlatFileConfig::CGenbankBlockCallback::notify(
    string & block_text,
    const CBioseqContext& ctx,
    const CEndSectionItem & slash_item ) 
{
    return unified_notify(block_text, ctx,  slash_item, fGenbankBlocks_Slash);
}

// -- constructor
CFlatFileConfig::CFlatFileConfig(
    TFormat format,
    TMode mode,
    TStyle style,
    TFlags flags,
    TView view,
    TGffOptions gff_options,
    TGenbankBlocks genbank_blocks,
    CGenbankBlockCallback* pGenbankBlockCallback,
    const ICanceled * pCanceledCallback,
    bool basicCleanup ) :
    m_Format(format), m_Mode(mode), m_Style(style), m_View(view),
    m_Flags(flags), m_RefSeqConventions(false), m_GffOptions(gff_options),
    m_fGenbankBlocks(genbank_blocks),
    m_GenbankBlockCallback(pGenbankBlockCallback),
    m_pCanceledCallback(pCanceledCallback),
    m_BasicCleanup(basicCleanup)
{
    // GFF/GFF3 and FTable always require master style
    if (m_Format == eFormat_GFF  ||  m_Format == eFormat_GFF3  ||
        m_Format == eFormat_FTable) {
        m_Style = eStyle_Master;
    }
}

// -- destructor
CFlatFileConfig::~CFlatFileConfig(void)
{
}


// -- mode flags

// mode flags initialization
const bool CFlatFileConfig::sm_ModeFlags[4][32] = {
    // Release
    { 
        true, true, true, true, true, true, true, true, true, true,
        true, true, true, true, true, true, true, true, true, true,
        true, true, true, true, true, true, true, false, false, true, 
        false, false
    },
    // Entrez
    {
        false, true, true, true, true, false, true, true, true, true,
        true, true, true, true, true, true, true, false, true, true,
        true, true, true, true, false, true, true, true, false, true, 
        false, false
    },
    // GBench
    {
        false, false, false, false, false, false, false, true, false, false,
        false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, true, false, 
        false, false
    },
    // Dump
    {
        false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, true, false, true, false, 
        false, false
    }
};


#define MODE_FLAG_GET(x, y) \
bool CFlatFileConfig::x(void) const \
{ \
    return sm_ModeFlags[static_cast<size_t>(m_Mode)][y]; \
} \
    
MODE_FLAG_GET(SuppressLocalId, 0);
MODE_FLAG_GET(ValidateFeatures, 1);
MODE_FLAG_GET(IgnorePatPubs, 2);
MODE_FLAG_GET(DropShortAA, 3);
MODE_FLAG_GET(AvoidLocusColl, 4);
MODE_FLAG_GET(IupacaaOnly, 5);
MODE_FLAG_GET(DropBadCitGens, 6);
MODE_FLAG_GET(NoAffilOnUnpub, 7);
MODE_FLAG_GET(DropIllegalQuals, 8);
MODE_FLAG_GET(CheckQualSyntax, 9);
MODE_FLAG_GET(NeedRequiredQuals, 10);
MODE_FLAG_GET(NeedOrganismQual, 11);
MODE_FLAG_GET(NeedAtLeastOneRef, 12);
MODE_FLAG_GET(CitArtIsoJta, 13);
MODE_FLAG_GET(DropBadDbxref, 14);
MODE_FLAG_GET(UseEmblMolType, 15);
MODE_FLAG_GET(HideBankItComment, 16);
MODE_FLAG_GET(CheckCDSProductId, 17);
MODE_FLAG_GET(FrequencyToNote, 18);
//MODE_FLAG_GET(SrcQualsToNote, 19); // implementation below
MODE_FLAG_GET(HideEmptySource, 20);
// MODE_FLAG_GET(GoQualsToNote, 21); // implementation below
//MODE_FLAG_GET(SelenocysteineToNote, 23); // implementation below
MODE_FLAG_GET(ForGBRelease, 24);
MODE_FLAG_GET(HideUnclassPartial, 25);
// MODE_FLAG_GET(CodonRecognizedToNote, 26); // implementation below
MODE_FLAG_GET(GoQualsEachMerge, 27);
MODE_FLAG_GET(ShowOutOfBoundsFeats, 28);
MODE_FLAG_GET(HideSpecificGeneMaps, 29);

#undef MODE_FLAG_GET

bool CFlatFileConfig::SrcQualsToNote(void) const 
{
    return m_RefSeqConventions ? false : sm_ModeFlags[static_cast<size_t>(m_Mode)][19];
}

bool CFlatFileConfig::GoQualsToNote(void) const
{
    return m_RefSeqConventions ? false : sm_ModeFlags[static_cast<size_t>(m_Mode)][21];
}

bool CFlatFileConfig::SelenocysteineToNote(void) const 
{
    return m_RefSeqConventions ? false : sm_ModeFlags[static_cast<size_t>(m_Mode)][23];
}

bool CFlatFileConfig::CodonRecognizedToNote(void) const
{
    return m_RefSeqConventions ? false : sm_ModeFlags[static_cast<size_t>(m_Mode)][26];
}

typedef SStaticPair<const char *, CFlatFileConfig::FGenbankBlocks>  TBlockElem;
static const TBlockElem sc_block_map[] = {
    { "accession",  CFlatFileConfig::fGenbankBlocks_Accession },
    { "all",        CFlatFileConfig::fGenbankBlocks_All },
    { "basecount",  CFlatFileConfig::fGenbankBlocks_Basecount },
    { "comment",    CFlatFileConfig::fGenbankBlocks_Comment },
    { "contig",     CFlatFileConfig::fGenbankBlocks_Contig },
    { "dbsource",   CFlatFileConfig::fGenbankBlocks_Dbsource },
    { "defline",    CFlatFileConfig::fGenbankBlocks_Defline },
    { "featandgap", CFlatFileConfig::fGenbankBlocks_FeatAndGap },
    { "featheader", CFlatFileConfig::fGenbankBlocks_Featheader },
    { "head",       CFlatFileConfig::fGenbankBlocks_Head },
    { "keywords",   CFlatFileConfig::fGenbankBlocks_Keywords },
    { "locus",      CFlatFileConfig::fGenbankBlocks_Locus },
    { "origin",     CFlatFileConfig::fGenbankBlocks_Origin },
    { "primary",    CFlatFileConfig::fGenbankBlocks_Primary },
    { "project",    CFlatFileConfig::fGenbankBlocks_Project },
    { "reference",  CFlatFileConfig::fGenbankBlocks_Reference },
    { "segment",    CFlatFileConfig::fGenbankBlocks_Segment },
    { "sequence",   CFlatFileConfig::fGenbankBlocks_Sequence },
    { "slash",      CFlatFileConfig::fGenbankBlocks_Slash },
    { "source",     CFlatFileConfig::fGenbankBlocks_Source },
    { "sourcefeat", CFlatFileConfig::fGenbankBlocks_Sourcefeat },
    { "tsa",        CFlatFileConfig::fGenbankBlocks_Tsa },
    { "version",    CFlatFileConfig::fGenbankBlocks_Version },
    { "wgs",        CFlatFileConfig::fGenbankBlocks_Wgs }
};
typedef CStaticArrayMap<const char *, CFlatFileConfig::FGenbankBlocks, PNocase_CStr> TBlockMap;
DEFINE_STATIC_ARRAY_MAP(TBlockMap, sc_BlockMap, sc_block_map);

// static
CFlatFileConfig::FGenbankBlocks CFlatFileConfig::StringToGenbankBlock(const string & str)
{
    TBlockMap::const_iterator find_iter = sc_BlockMap.find(str.c_str());
    if( find_iter == sc_BlockMap.end() ) {
        throw runtime_error("Could not translate this string to a Genbank block type: " + str);
    }
    return find_iter->second;
}

// static 
const vector<string> & 
CFlatFileConfig::GetAllGenbankStrings(void)
{
    static vector<string> s_vecOfGenbankStrings;
    static CFastMutex s_mutex;

    CFastMutexGuard guard(s_mutex);
    if( s_vecOfGenbankStrings.empty() ) {
        // use "set" for sorting and uniquing
        set<string> setOfGenbankStrings;
        ITERATE(TBlockMap, map_iter, sc_BlockMap) {
            setOfGenbankStrings.insert(map_iter->first);
        }
        copy( setOfGenbankStrings.begin(),
            setOfGenbankStrings.end(), 
            back_inserter(s_vecOfGenbankStrings) );
    }

    return s_vecOfGenbankStrings;
}

void
CFlatFileConfig::x_ThrowHaltNow(void) const
{
    NCBI_THROW(CFlatException, eHaltRequested,
        "FlatFile Generation canceled" );
}

END_SCOPE(objects)
END_NCBI_SCOPE

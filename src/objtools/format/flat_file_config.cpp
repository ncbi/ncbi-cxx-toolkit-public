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
#include <util/static_map.hpp>
#include <corelib/ncbistd.hpp>
#include <objtools/format/flat_file_config.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// -- constructor
CFlatFileConfig::CFlatFileConfig(
    TFormat format,
    TMode mode,
    TStyle style,
    TFlags flags,
    TView view,
    TGffOptions gff_options,
    TGenbankBlocks genbank_blocks,
    CGenbankBlockCallback* pGenbankBlockCallback ) :
    m_Format(format), m_Mode(mode), m_Style(style), m_View(view),
    m_Flags(flags), m_RefSeqConventions(false), m_GffOptions(gff_options),
    m_fGenbankBlocks(genbank_blocks),
    m_GenbankBlockCallback(pGenbankBlockCallback)
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
        true, true, true, true, true, true, false, false, true, true,
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

END_SCOPE(objects)
END_NCBI_SCOPE

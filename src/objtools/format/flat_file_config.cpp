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
#include <corelib/ncbistd.hpp>

#include <objtools/format/flat_file_config.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// -- constructor
CFlatFileConfig::CFlatFileConfig
(TFormat format,
 TMode mode,
 TStyle style,
 TFlags flags,
 TView view) :
    m_Format(format), m_Mode(mode), m_Style(style), m_View(view),
    m_Flags(flags), m_RefSeqConventions(false)
{
}

// -- destructor
CFlatFileConfig::~CFlatFileConfig(void)
{
}


// -- mode flags

// mode flags initialization
const bool CFlatFileConfig::sm_ModeFlags[4][26] = {
    // Release
    { 
        true, true, true, true, true, true, true, true, true, true,
        true, true, true, true, true, true, true, true, true, true,
        true, true, true, true, true, true
    },
    // Entrez
    {
        false, true, true, true, true, false, true, true, true, true,
        true, false, true, true, true, true, false, false, true, true,
        true, true, true, true, false, true
    },
    // GBench
    {
        false, false, false, false, false, false, false, true, false, false,
        false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false
    },
    // Dump
    {
        false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false
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
MODE_FLAG_GET(SuppressSegLoc, 18);
//MODE_FLAG_GET(SrcQualsToNote, 19);
MODE_FLAG_GET(HideEmptySource, 20);
MODE_FLAG_GET(GoQualsToNote, 21);
//MODE_FLAG_GET(GeneSynsToNote, 22);
//MODE_FLAG_GET(SelenocysteineToNote, 23);
MODE_FLAG_GET(ForGBRelease, 24);
MODE_FLAG_GET(HideUnclassPartial, 25);

#undef MODE_FLAG_GET

bool CFlatFileConfig::SrcQualsToNote(void) const 
{
    return m_RefSeqConventions ? false : sm_ModeFlags[static_cast<size_t>(m_Mode)][19];
}

bool CFlatFileConfig::GeneSynsToNote(void) const 
{
    return !m_RefSeqConventions ? true : sm_ModeFlags[static_cast<size_t>(m_Mode)][22];
}

bool CFlatFileConfig::SelenocysteineToNote(void) const 
{
    return m_RefSeqConventions ? false : sm_ModeFlags[static_cast<size_t>(m_Mode)][23];
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2005/04/27 17:11:37  shomrat
* Modify for RefSeq
*
* Revision 1.3  2004/11/24 16:52:50  shomrat
* Standardize flat-file customization flags
*
* Revision 1.2  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.1  2004/04/22 16:03:23  shomrat
* Initial Revision (from context)
*
*
* ===========================================================================
*/

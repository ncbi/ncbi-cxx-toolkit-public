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
 * Author:  Frank Ludwig
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>

// Objects includes
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objtools/readers/ucscid.hpp>
#include <objtools/readers/idmapper.hpp>
#include "idmap.hpp"
#include "idmapper_builtin.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

    
//  ============================================================================
CIdMapperBuiltin::CIdMapperBuiltin()
//  ============================================================================
{
    //
    //  Rat build 4.1
    //
    AddMapping( "rn4", "chr1",      "gi|62750345" ); 
    AddMapping( "rn4", "chr2",      "gi|62750359" ); 
    AddMapping( "rn4", "chr3",      "gi|62750360" ); 
    AddMapping( "rn4", "chr4",      "gi|62750804" ); 
    AddMapping( "rn4", "chr5",      "gi|62750805" ); 
    AddMapping( "rn4", "chr6",      "gi|62750806" ); 
    AddMapping( "rn4", "chr7",      "gi|62750807" ); 
    AddMapping( "rn4", "chr8",      "gi|62750808" ); 
    AddMapping( "rn4", "chr9",      "gi|62750809" ); 
    AddMapping( "rn4", "chr10",     "gi|62750810" ); 
    AddMapping( "rn4", "chr11",     "gi|62750811" ); 
    AddMapping( "rn4", "chr12",     "gi|62750812" ); 
    AddMapping( "rn4", "chr13",     "gi|62750813" ); 
    AddMapping( "rn4", "chr14",     "gi|62750814" ); 
    AddMapping( "rn4", "chr15",     "gi|62750815" ); 
    AddMapping( "rn4", "chr16",     "gi|62750816" ); 
    AddMapping( "rn4", "chr17",     "gi|62750817" ); 
    AddMapping( "rn4", "chr18",     "gi|62750818" ); 
    AddMapping( "rn4", "chr19",     "gi|62750819" ); 
    AddMapping( "rn4", "chr20",     "gi|62750820" ); 
    AddMapping( "rn4", "chrX",      "gi|62750821" ); 
    
    //
    //  Mouse build 36
    //
    AddMapping( "mm8", "chr1",      "gi|94471495" ); 
    AddMapping( "mm8", "chr2",      "gi|94471506" ); 
    AddMapping( "mm8", "chr3",      "gi|94471518" ); 
    AddMapping( "mm8", "chr4",      "gi|94471531" ); 
    AddMapping( "mm8", "chr5",      "gi|94471532" ); 
    AddMapping( "mm8", "chr6",      "gi|94471533" ); 
    AddMapping( "mm8", "chr7",      "gi|94471604" ); 
    AddMapping( "mm8", "chr8",      "gi|94471605" ); 
    AddMapping( "mm8", "chr9",      "gi|94471614" ); 
    AddMapping( "mm8", "chr10",     "gi|94471496" ); 
    AddMapping( "mm8", "chr11",     "gi|94471497" ); 
    AddMapping( "mm8", "chr12",     "gi|94471498" ); 
    AddMapping( "mm8", "chr13",     "gi|94471499" ); 
    AddMapping( "mm8", "chr14",     "gi|94471500" ); 
    AddMapping( "mm8", "chr15",     "gi|94471501" ); 
    AddMapping( "mm8", "chr16",     "gi|94471502" ); 
    AddMapping( "mm8", "chr17",     "gi|94471503" ); 
    AddMapping( "mm8", "chr18",     "gi|94471504" ); 
    AddMapping( "mm8", "chr19",     "gi|94471505" ); 
    AddMapping( "mm8", "chrX",      "gi|94471643" ); 
    AddMapping( "mm8", "chrY",      "gi|94490724" ); 
    
    //
    //  Mouse build 37
    //
    AddMapping( "mm9", "chr1",      "gi|149288852" ); 
    AddMapping( "mm9", "chr2",      "gi|149338249" ); 
    AddMapping( "mm9", "chr3",      "gi|149352351" ); 
    AddMapping( "mm9", "chr4",      "gi|149354223" ); 
    AddMapping( "mm9", "chr5",      "gi|149354224" ); 
    AddMapping( "mm9", "chr6",      "gi|149361431" ); 
    AddMapping( "mm9", "chr7",      "gi|149361432" ); 
    AddMapping( "mm9", "chr8",      "gi|149361523" ); 
    AddMapping( "mm9", "chr9",      "gi|149361524" ); 
    AddMapping( "mm9", "chr10",     "gi|149288869" ); 
    AddMapping( "mm9", "chr11",     "gi|149288871" ); 
    AddMapping( "mm9", "chr12",     "gi|149292731" ); 
    AddMapping( "mm9", "chr13",     "gi|149292733" ); 
    AddMapping( "mm9", "chr14",     "gi|149292735" ); 
    AddMapping( "mm9", "chr15",     "gi|149301884" ); 
    AddMapping( "mm9", "chr16",     "gi|149304713" ); 
    AddMapping( "mm9", "chr17",     "gi|149313536" ); 
    AddMapping( "mm9", "chr18",     "gi|149321426" ); 
    AddMapping( "mm9", "chr19",     "gi|149323268" ); 
    AddMapping( "mm9", "chrX",      "gi|149361525" ); 
    AddMapping( "mm9", "chrY",      "gi|149361526" ); 
    
    //
    //  Human build 35
    //
    AddMapping( "hg17", "chr1",     "gi|51511461" ); 
    AddMapping( "hg17", "chr2",     "gi|51511462" ); 
    AddMapping( "hg17", "chr3",     "gi|51511463" ); 
    AddMapping( "hg17", "chr4",     "gi|51511464" ); 
    AddMapping( "hg17", "chr5",     "gi|51511721" ); 
    AddMapping( "hg17", "chr6",     "gi|51511722" ); 
    AddMapping( "hg17", "chr7",     "gi|51511723" ); 
    AddMapping( "hg17", "chr8",     "gi|51511724" ); 
    AddMapping( "hg17", "chr9",     "gi|51511725" ); 
    AddMapping( "hg17", "chr10",    "gi|51511726" ); 
    AddMapping( "hg17", "chr11",    "gi|51511727" ); 
    AddMapping( "hg17", "chr12",    "gi|51511728" ); 
    AddMapping( "hg17", "chr13",    "gi|51511729" ); 
    AddMapping( "hg17", "chr14",    "gi|51511730" ); 
    AddMapping( "hg17", "chr15",    "gi|51511731" ); 
    AddMapping( "hg17", "chr16",    "gi|51511732" ); 
    AddMapping( "hg17", "chr17",    "gi|51511734" ); 
    AddMapping( "hg17", "chr18",    "gi|51511735" ); 
    AddMapping( "hg17", "chr19",    "gi|42406306" ); 
    AddMapping( "hg17", "chr20",    "gi|51511747" ); 
    AddMapping( "hg17", "chr21",    "gi|51511750" ); 
    AddMapping( "hg17", "chr22",    "gi|51511751" ); 
    AddMapping( "hg17", "chrX",     "gi|51511752" ); 
    AddMapping( "hg17", "chrY",     "gi|51511753" ); 
    
    
    //
    //  Human build 36
    //
    AddMapping( "hg18", "chr1",     "gi|89161185" ); 
    AddMapping( "hg18", "chr2",     "gi|89161199" ); 
    AddMapping( "hg18", "chr3",     "gi|89161205" ); 
    AddMapping( "hg18", "chr4",     "gi|89161207" ); 
    AddMapping( "hg18", "chr5",     "gi|51511721" ); 
    AddMapping( "hg18", "chr6",     "gi|89161210" ); 
    AddMapping( "hg18", "chr7",     "gi|89161213" ); 
    AddMapping( "hg18", "chr8",     "gi|51511724" ); 
    AddMapping( "hg18", "chr9",     "gi|89161216" ); 
    AddMapping( "hg18", "chr10",    "gi|89161187" ); 
    AddMapping( "hg18", "chr11",    "gi|51511727" ); 
    AddMapping( "hg18", "chr12",    "gi|89161190" ); 
    AddMapping( "hg18", "chr13",    "gi|51511729" ); 
    AddMapping( "hg18", "chr14",    "gi|51511730" ); 
    AddMapping( "hg18", "chr15",    "gi|51511731" ); 
    AddMapping( "hg18", "chr16",    "gi|51511732" ); 
    AddMapping( "hg18", "chr17",    "gi|51511734" ); 
    AddMapping( "hg18", "chr18",    "gi|51511735" ); 
    AddMapping( "hg18", "chr19",    "gi|42406306" ); 
    AddMapping( "hg18", "chr20",    "gi|51511747" ); 
    AddMapping( "hg18", "chr21",    "gi|51511750" ); 
    AddMapping( "hg18", "chr22",    "gi|89161203" ); 
    AddMapping( "hg18", "chrX",     "gi|89161218" ); 
    AddMapping( "hg18", "chrY",     "gi|89161220" ); 
};

//  ============================================================================
void
CIdMapperBuiltin::Setup(
    const CArgs& args )
//  ============================================================================
{
};

//  ============================================================================
void
CIdMapperBuiltin::AddMapping(
    const string& strBuild,
    const string& strChrom,
    const string& strTarget,
    unsigned int )
//  ============================================================================
{
    CRef<CSeq_id> target( new CSeq_id( strTarget ) );
    m_Map.AddMapping( UcscID::Label( strBuild, strChrom ), target );
};
    
//  ============================================================================
 
//  ============================================================================
CSeq_id_Handle
CIdMapperBuiltin::MapID(
    const string& key,
    unsigned int& uLength )
//  ============================================================================
{
    CSeq_id_Handle idh = m_Map.GetMapping( key, uLength );
    if ( idh ) {
        return idh;
    }
    return CIdMapper::MapID( key, uLength );
};

//  ============================================================================
void
CIdMapperBuiltin::Dump(
    CNcbiOstream& out,
    const string& strPrefix )
//  ============================================================================
{
    out << strPrefix << "[CIdMapperBuiltIn:" << endl;
    m_Map.Dump( out, strPrefix + "\t" );
    out << strPrefix << "]" << endl;
};

END_NCBI_SCOPE


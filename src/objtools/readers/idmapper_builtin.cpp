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

#include <objtools/readers/idmapper.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ============================================================================
void
CIdMapperBuiltin::InitializeCache()
//  ============================================================================
{
    if ( m_strContext == "rn4" ) {
    
        AddMapEntry( "chr1", 62750345 ); 
        AddMapEntry( "chr2", 62750359 ); 
        AddMapEntry( "chr3", 62750360 ); 
        AddMapEntry( "chr4", 62750804 ); 
        AddMapEntry( "chr5", 62750805 ); 
        AddMapEntry( "chr6", 62750806 ); 
        AddMapEntry( "chr7", 62750807 ); 
        AddMapEntry( "chr8", 62750808 ); 
        AddMapEntry( "chr9", 62750809 ); 
        AddMapEntry( "chr10", 62750810 ); 
        AddMapEntry( "chr11", 62750811 ); 
        AddMapEntry( "chr12", 62750812 ); 
        AddMapEntry( "chr13", 62750813 ); 
        AddMapEntry( "chr14", 62750814 ); 
        AddMapEntry( "chr15", 62750815 ); 
        AddMapEntry( "chr16", 62750816 ); 
        AddMapEntry( "chr17", 62750817 ); 
        AddMapEntry( "chr18", 62750818 ); 
        AddMapEntry( "chr19", 62750819 ); 
        AddMapEntry( "chr20", 62750820 ); 
        AddMapEntry( "chrX", 62750821 ); 
    }
    if ( m_strContext == "mm8" ) {
    
        AddMapEntry( "chr1", 94471495 ); 
        AddMapEntry( "chr2", 94471506 ); 
        AddMapEntry( "chr3", 94471518 ); 
        AddMapEntry( "chr4", 94471531 ); 
        AddMapEntry( "chr5", 94471532 ); 
        AddMapEntry( "chr6", 94471533 ); 
        AddMapEntry( "chr7", 94471604 ); 
        AddMapEntry( "chr8", 94471605 ); 
        AddMapEntry( "chr9", 94471614 ); 
        AddMapEntry( "chr10", 94471496 ); 
        AddMapEntry( "chr11", 94471497 ); 
        AddMapEntry( "chr12", 94471498 ); 
        AddMapEntry( "chr13", 94471499 ); 
        AddMapEntry( "chr14", 94471500 ); 
        AddMapEntry( "chr15", 94471501 ); 
        AddMapEntry( "chr16", 94471502 ); 
        AddMapEntry( "chr17", 94471503 ); 
        AddMapEntry( "chr18", 94471504 ); 
        AddMapEntry( "chr19", 94471505 ); 
        AddMapEntry( "chrX", 94471643 ); 
        AddMapEntry( "chrY", 94490724 ); 
    }
    if ( m_strContext == "mm9" ) {
    
        AddMapEntry( "chr1", 149288852 ); 
        AddMapEntry( "chr2", 149338249 ); 
        AddMapEntry( "chr3", 149352351 ); 
        AddMapEntry( "chr4", 149354223 ); 
        AddMapEntry( "chr5", 149354224 ); 
        AddMapEntry( "chr6", 149361431 ); 
        AddMapEntry( "chr7", 149361432 ); 
        AddMapEntry( "chr8", 149361523 ); 
        AddMapEntry( "chr9", 149361524 ); 
        AddMapEntry( "chr10", 149288869 ); 
        AddMapEntry( "chr11", 149288871 ); 
        AddMapEntry( "chr12", 149292731 ); 
        AddMapEntry( "chr13", 149292733 ); 
        AddMapEntry( "chr14", 149292735 ); 
        AddMapEntry( "chr15", 149301884 ); 
        AddMapEntry( "chr16", 149304713 ); 
        AddMapEntry( "chr17", 149313536 ); 
        AddMapEntry( "chr18", 149321426 ); 
        AddMapEntry( "chr19", 149323268 ); 
        AddMapEntry( "chrX", 149361525 ); 
        AddMapEntry( "chrY", 149361526 ); 
    }  
    if ( m_strContext == "hg17" ) {
    
        AddMapEntry( "chr1", 51511461 ); 
        AddMapEntry( "chr2", 51511462 ); 
        AddMapEntry( "chr3", 51511463 ); 
        AddMapEntry( "chr4", 51511464 ); 
        AddMapEntry( "chr5", 51511721 ); 
        AddMapEntry( "chr6", 51511722 ); 
        AddMapEntry( "chr7", 51511723 ); 
        AddMapEntry( "chr8", 51511724 ); 
        AddMapEntry( "chr9", 51511725 ); 
        AddMapEntry( "chr10", 51511726 ); 
        AddMapEntry( "chr11", 51511727 ); 
        AddMapEntry( "chr12", 51511728 ); 
        AddMapEntry( "chr13", 51511729 ); 
        AddMapEntry( "chr14", 51511730 ); 
        AddMapEntry( "chr15", 51511731 ); 
        AddMapEntry( "chr16", 51511732 ); 
        AddMapEntry( "chr17", 51511734 ); 
        AddMapEntry( "chr18", 51511735 ); 
        AddMapEntry( "chr19", 42406306 ); 
        AddMapEntry( "chr20", 51511747 ); 
        AddMapEntry( "chr21", 51511750 ); 
        AddMapEntry( "chr22", 51511751 ); 
        AddMapEntry( "chrX", 51511752 ); 
        AddMapEntry( "chrY", 51511753 ); 
    }
    if ( m_strContext == "hg18" ) {
    
        AddMapEntry( "chr1", 89161185 );
        AddMapEntry( "chr2", 89161199 );
        AddMapEntry( "chr3", 89161205 );
        AddMapEntry( "chr4", 89161207 );
        AddMapEntry( "chr5", 51511721 );
        AddMapEntry( "chr6", 89161210 );
        AddMapEntry( "chr7", 89161213 );
        AddMapEntry( "chr8", 51511724 );
        AddMapEntry( "chr9", 89161216 );
        AddMapEntry( "chr10", 89161187 );
        AddMapEntry( "chr11", 51511727 );
        AddMapEntry( "chr12", 89161190 );
        AddMapEntry( "chr13", 51511729 );
        AddMapEntry( "chr14", 51511730 );
        AddMapEntry( "chr15", 51511731 );
        AddMapEntry( "chr16", 51511732 );
        AddMapEntry( "chr17", 51511734 );
        AddMapEntry( "chr18", 51511735 );
        AddMapEntry( "chr19", 42406306 );
        AddMapEntry( "chr20", 51511747 );
        AddMapEntry( "chr21", 51511750 );
        AddMapEntry( "chr22", 89161203 );
        AddMapEntry( "chrX", 89161218 );
        AddMapEntry( "chrY", 89161220 );
    }
};

//  ============================================================================
void
CIdMapperBuiltin::AddMapEntry(
    const string& strFrom,
    int iTo )
//  ============================================================================
{
    CSeq_id source( CSeq_id::e_Local, strFrom );
    CSeq_id target( CSeq_id::e_Gi, iTo );
    AddMapping( 
        CSeq_id_Handle::GetHandle( source ), 
        CSeq_id_Handle::GetHandle( target ) );
};

END_NCBI_SCOPE


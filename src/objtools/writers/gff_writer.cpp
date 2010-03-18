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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write gff file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objtools/readers/gff3_data.hpp>
#include <objtools/writers/gff_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CGffWriter::CGffWriter(
    CNcbiOstream& ostr,
    TFlags uFlags ) :
//  ----------------------------------------------------------------------------
    m_Os( ostr ),
    m_uFlags( uFlags )
{
};

//  ----------------------------------------------------------------------------
CGffWriter::~CGffWriter()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGffWriter::WriteAnnot( 
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    x_WriteHeader();

    if ( annot.IsFtable() ) {
        return x_WriteAnnotFTable( annot );
    }
    if ( annot.IsAlign() ) {
        return x_WriteAnnotAlign( annot );
    }
    cerr << "Unexpected!" << endl;
    return false;
}

//  ----------------------------------------------------------------------------
bool CGffWriter::x_WriteAnnotFTable( 
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    if ( ! annot.IsFtable() ) {
        return false;
    }

    const list< CRef< CSeq_feat > >& table = annot.GetData().GetFtable();
    list< CRef< CSeq_feat > >::const_iterator it = table.begin();
    CGff3RecordSet recordSet;
    while ( it != table.end() ) {
        x_AssignObject( annot, **it, recordSet );
        it++;
    }
    x_WriteRecords( recordSet );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriter::x_WriteAnnotAlign( 
    const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
//    cerr << "To be done." << endl;
    return false;
}

//  ----------------------------------------------------------------------------
bool CGffWriter::x_AssignObject(
   const CSeq_annot& annot,
   const CSeq_feat& feat,        
   CGff3RecordSet& set )
//  ----------------------------------------------------------------------------
{
    CGff3Record* pRecord = new CGff3Record;
    if ( ! pRecord->AssignFromAsn( annot, feat ) ) {
        return false;
    }
    if ( pRecord->Type() == "mRNA" ) {
        set.AddRecord( pRecord );
        // create one GFF3 exon feature for each constituent interval of the 
        // mRNA
        const CSeq_loc& loc = feat.GetLocation();
        if ( loc.IsPacked_int() && loc.GetPacked_int().CanGet() ) {
            const list< CRef< CSeq_interval > >& sublocs = loc.GetPacked_int().Get();
            list< CRef< CSeq_interval > >::const_iterator it;
            for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
                const CSeq_interval& subint = **it;
                CGff3Record* pExon = new CGff3Record;
                pExon->MakeExon( *pRecord, subint );
                set.AddRecord( pExon );
            }
        }
        return true;
    }

    if ( pRecord->Type() == "exon" ) {
        // only add exon feature if one has not been created before. If one
        // has been created before, migrate the attributes.
        CGff3RecordSet::TIt it;
        for ( it = set.begin(); it != set.end(); ++it ) {
            if ( (*it)->Type() != pRecord->Type() ) {
                continue;
            }
            if ( (*it)->SeqStart() != pRecord->SeqStart() ) {
                continue;
            }
            if ( (*it)->SeqStop() != pRecord->SeqStop() ) {
                continue;
            }
            break;
        }
        if ( it == set.end() ) {
            set.AddRecord( pRecord );
        }
        else {
            (*it)->MergeRecord( *pRecord );
            delete pRecord;
        }
        return true;
    }

    // default behavior:
    set.AddRecord( pRecord );
    return true;
}
    
//  ----------------------------------------------------------------------------
bool CGffWriter::x_WriteRecords(
    const CGff3RecordSet& set )
//  ----------------------------------------------------------------------------
{
    for ( CGff3RecordSet::TCit cit = set.begin(); cit != set.end(); ++cit ) {
        if ( ! x_WriteRecord( **cit ) ) {
            return false;
        }
    }
    return true;
}    
    
//  ----------------------------------------------------------------------------
bool CGffWriter::x_WriteRecord( 
    const CGff3Record& record )
//  ----------------------------------------------------------------------------
{
    m_Os << x_GffId( record ) << '\t';
    m_Os << x_GffSource( record ) << '\t';
    m_Os << x_GffType( record ) << '\t';
    m_Os << x_GffSeqStart( record ) << '\t';
    m_Os << x_GffSeqStop( record ) << '\t';
    m_Os << x_GffScore( record ) << '\t';
    m_Os << x_GffStrand( record ) << '\t';
    m_Os << x_GffPhase( record ) << '\t';
    m_Os << x_GffAttributes( record ) << endl;
    return true;
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffId(
    const CGff3Record& record ) const
//  ----------------------------------------------------------------------------
{
    return record.Id();
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffSource(
    const CGff3Record& record ) const
//  ----------------------------------------------------------------------------
{
    return record.Source();
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffType(
    const CGff3Record& record ) const
//  ----------------------------------------------------------------------------
{
    string strGffType;
    if ( record.GetAttribute( "gff_type", strGffType ) ) {
        return strGffType;
    }
    return record.Type();
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffSeqStart(
    const CGff3Record& record ) const
//  ----------------------------------------------------------------------------
{
    return NStr::UIntToString( record.SeqStart() + 1 );
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffSeqStop(
    const CGff3Record& record ) const
//  ----------------------------------------------------------------------------
{
    return NStr::UIntToString( record.SeqStop() + 1 );
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffScore(
    const CGff3Record& record ) const
//  ----------------------------------------------------------------------------
{
    if ( ! record.IsSetScore() ) {
        return ".";
    }
    char pcBuffer[ 16 ];
    ::sprintf( pcBuffer, "%6.6f", record.Score() );
    return string( pcBuffer );
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffStrand(
    const CGff3Record& record ) const
//  ----------------------------------------------------------------------------
{
    if ( ! record.IsSetStrand() ) {
        return ".";
    }
    switch ( record.Strand() ) {
    default:
        return ".";
    case eNa_strand_plus:
        return "+";
    case eNa_strand_minus:
        return "-";
    }
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffPhase(
    const CGff3Record& record ) const
//  ----------------------------------------------------------------------------
{
    if ( ! record.IsSetPhase() ) {
        return ".";
    }
    switch ( record.Phase() ) {
    default:
        return "0";
    case CCdregion::eFrame_two:
        return "1";
    case CCdregion::eFrame_three:
        return "2";
    }
}

//  ----------------------------------------------------------------------------
string CGffWriter::x_GffAttributes(
    const CGff3Record& record ) const
//  ----------------------------------------------------------------------------
{
    string strAttributes;
    CGff3Record::TAttributes attrs = record.Attributes();
    CGff3Record::TAttrCit cit;

    for ( cit = attrs.begin(); cit != attrs.end(); ++cit ) {
        string strKey = cit->first;
        if ( NStr::StartsWith( strKey, "gff_" ) ) {
            continue;
        }
        if ( ! strAttributes.empty() && ! (m_uFlags & fSoQuirks) ) {
            strAttributes += ";";
        }
        if ( m_uFlags & fSoQuirks ) {
            if ( 0 == NStr::CompareNocase( strKey, "Parent" ) ) {
                strKey = "PARENT";
            }
        }

        strAttributes += strKey;
        strAttributes += "=";
        strAttributes += cit->second;
        if ( m_uFlags & fSoQuirks ) {
            strAttributes += "; ";
        }
    }
    return strAttributes;
}

//  ----------------------------------------------------------------------------
bool CGffWriter::x_WriteHeader()
//  ----------------------------------------------------------------------------
{
    m_Os << "##gff-version 3" << endl;
    return true;
}

END_NCBI_SCOPE

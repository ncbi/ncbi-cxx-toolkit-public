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
 *   VCF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>              
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/stream_utils.hpp>

#include <util/static_map.hpp>
#include <util/line_reader.hpp>

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

// Objects includes
#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp> 
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/vcf_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
class CVcfData
//  ============================================================================
{
public:
    CVcfData() { m_pdQual = 0; };
    ~CVcfData() { delete m_pdQual; };

    bool ParseData(
        const string& );

    string m_strChrom;
    int m_iPos;
    vector<string> m_Ids;
    string m_strRef;
    vector<string> m_Alt;
    double* m_pdQual;
    string m_strFilter;
    map<string,string> m_Info;
    vector<string> m_FormatKeys;
    vector< vector<string> > m_FormatValues;
};

//  ----------------------------------------------------------------------------
bool
CVcfData::ParseData(
    const string& line )
//  ----------------------------------------------------------------------------
{
    vector<string> columns;
    NStr::Tokenize( line, " \t", columns, NStr::eMergeDelims );
    if ( columns.size() < 8 ) {
        return false;
    }
    try {
        m_strChrom = columns[0];
        m_iPos = NStr::StringToInt( columns[1] );
        NStr::Tokenize( columns[2], ";", m_Ids, NStr::eNoMergeDelims );
        if ( (m_Ids.size() == 1)  &&  (m_Ids[0] == ".") ) {
            m_Ids.clear();
        }
        m_strRef = columns[3];
        NStr::Tokenize( columns[4], ",", m_Alt, NStr::eNoMergeDelims );
        if ( columns[5] != "." ) {
            m_pdQual = new double( NStr::StringToDouble( columns[5] ) );
        }
        m_strFilter = columns[6];

        vector<string> infos;
        if ( columns[7] != "." ) {
            NStr::Tokenize( columns[7], ";", infos, NStr::eMergeDelims );
            for ( vector<string>::iterator it = infos.begin(); 
                it != infos.end(); ++it ) 
            {
                string key, value;
                NStr::SplitInTwo( *it, "=", key, value );
                m_Info[key] = value;
            }
        }
        if ( columns.size() > 8 ) {
            NStr::Tokenize( columns[8], ":", m_FormatKeys, NStr::eMergeDelims );
            for ( size_t u=9; u < columns.size(); ++u ) {
                vector<string> values;
                NStr::Tokenize( columns[u], ":", values, NStr::eMergeDelims );
                m_FormatValues.push_back( values );
            }
        }
    }
    catch ( ... ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
CVcfReader::CVcfReader(
    int flags )
//  ----------------------------------------------------------------------------
{
    m_iFlags = flags;
}


//  ----------------------------------------------------------------------------
CVcfReader::~CVcfReader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------                
CRef< CSeq_annot >
CVcfReader::ReadSeqAnnot(
    ILineReader& lr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{
    CRef< CSeq_annot > annot( new CSeq_annot );
    CRef< CAnnot_descr > desc( new CAnnot_descr );
    annot->SetDesc( *desc );
    annot->SetData().SetFtable();

    while ( ! lr.AtEOF() ) {
        string line = *(++lr);
        if ( x_ProcessMetaLine( line, annot ) ) {
            continue;
        }
        if ( x_ProcessHeaderLine( line, annot ) ) {
            continue;
        }
        if ( x_ProcessDataLine( line, annot ) ) {
            continue;
        }
        // still here? not good!
        cerr << "Unexpected line: " << line << endl;
    }
    return annot;
}

//  --------------------------------------------------------------------------- 
void
CVcfReader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    CNcbiIstream& istr,
    IErrorContainer* pErrorContainer )
//  ---------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    ReadSeqAnnots( annots, lr, pErrorContainer );
}
 
//  ---------------------------------------------------------------------------                       
void
CVcfReader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    ILineReader& lr,
    IErrorContainer* pErrorContainer )
//  ----------------------------------------------------------------------------
{
    while ( ! lr.AtEOF() ) {
        CRef<CSeq_annot> pAnnot = ReadSeqAnnot( lr, pErrorContainer );
        if ( pAnnot ) {
            annots.push_back( pAnnot );
        }
    }
}
                        
//  ----------------------------------------------------------------------------                
CRef< CSerialObject >
CVcfReader::ReadObject(
    ILineReader& lr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{ 
    CRef<CSerialObject> object( 
        ReadSeqAnnot( lr, pErrorContainer ).ReleaseOrNull() );    
    return object;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::x_ProcessMetaLine(
    const string& line,
    CRef<CSeq_annot> pAnnot )
//  ----------------------------------------------------------------------------
{
    static unsigned int uMetaCount = 0;

    if ( ! NStr::StartsWith( line, "##" ) ) {
        return false;
    }
    if ( ! m_Meta ) {
        m_Meta.Reset( new CAnnotdesc );
        m_Meta->SetUser().SetType().SetStr( "vcf-meta-info" );
    }

    m_Meta->SetUser().AddField( 
        NStr::UIntToString( ++uMetaCount ), line.substr( 2 ) );
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::x_ProcessHeaderLine(
    const string& line,
    CRef<CSeq_annot> pAnnot )
//  ----------------------------------------------------------------------------
{
    if ( NStr::StartsWith( line, "##" ) ) {
        return false;
    }
    if ( ! NStr::StartsWith( line, "#" ) ) {
        return false;
    }
    if ( m_Meta ) {
        pAnnot->SetDesc().Set().push_back( m_Meta );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::x_ProcessDataLine(
    const string& line,
    CRef<CSeq_annot> pAnnot )
//  ----------------------------------------------------------------------------
{
    if ( NStr::StartsWith( line, "#" ) ) {
        return false;
    }
    CVcfData data;
    if ( ! data.ParseData( line ) ) {
        return false;
    }
    CRef<CSeq_feat> pFeat( new CSeq_feat );
    pFeat->SetData().SetVariation().SetData().SetSet().SetType(
        CVariation_ref::C_Data::C_Set::eData_set_type_alleles );
    CSeq_feat::TExt& ext = pFeat->SetExt();
    ext.SetType().SetStr( "VcfAttributes" );

    if ( ! x_AssignFeatureLocation( data, pFeat ) ) {
        return false;
    }
    if ( ! x_AssignVariationIds( data, pFeat ) ) {
        return false;
    }
    if ( ! x_AssignVariationAlleles( data, pFeat ) ) {
        return false;
    }

    if ( ! x_ProcessScore( data, pFeat ) ) {
        return false;
    }
    if ( ! x_ProcessFilter( data, pFeat ) ) {
        return false;
    }
    if ( ! x_ProcessInfo( data, pFeat ) ) {
        return false;
    }

    if ( pFeat->GetExt().GetData().empty() ) {
        pFeat->ResetExt();
    }
    pAnnot->SetData().SetFtable().push_back( pFeat );
    return true;
}

//  ---------------------------------------------------------------------------
bool
CVcfReader::x_AssignFeatureLocation(
    const CVcfData& data,
    CRef<CSeq_feat> pFeature )
//  ---------------------------------------------------------------------------
{
    CRef< CSeq_id > pId( new CSeq_id( CSeq_id::e_Local, data.m_strChrom ) );

    pFeature->SetLocation().SetInt().SetId( *pId );
    pFeature->SetLocation().SetInt().SetFrom( data.m_iPos - 1 );
    pFeature->SetLocation().SetInt().SetTo( 
        data.m_iPos + data.m_strRef.length() - 1 );
    return true;
}

//  ----------------------------------------------------------------------------
bool 
CVcfReader::x_ProcessScore(
    const CVcfData& data,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    CSeq_feat::TExt& ext = pFeature->SetExt();
    if ( data.m_pdQual ) {
        ext.AddField( "score", *data.m_pdQual );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool 
CVcfReader::x_ProcessFilter(
    const CVcfData& data,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    CSeq_feat::TExt& ext = pFeature->SetExt();
    ext.AddField( "filter", data.m_strFilter );
    return true;
}

//  ----------------------------------------------------------------------------
bool 
CVcfReader::x_ProcessInfo(
    const CVcfData& data,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    CSeq_feat::TExt& ext = pFeature->SetExt();
    if ( ! data.m_Info.empty() ) {
        vector<string> infos;
        for ( map<string,string>::const_iterator cit = data.m_Info.begin();
            cit != data.m_Info.end(); cit++ )
        {
            string key = cit->first;
            string value = cit->second;
            if ( value.empty() ) {
                infos.push_back( key );
            }
            else {
                infos.push_back( key + "=" + value );
            }
        }
        ext.AddField( "info", NStr::Join( infos, ";" ) );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::x_AssignVariationIds(
    const CVcfData& data,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    if ( data.m_Ids.empty() ) {
        return true;
    }
    CVariation_ref& variation = pFeature->SetData().SetVariation();
    if ( data.m_Info.find( "DB" ) != data.m_Info.end() ) {
        variation.SetId().SetDb( "dbVar" );
    }
    else if ( data.m_Info.find( "H2" ) != data.m_Info.end() ) {
        variation.SetId().SetDb( "HapMap2" );
    }
    else {
        variation.SetId().SetDb( "local" );
    }
    variation.SetId().SetTag().SetStr( data.m_Ids[0] );

    for ( size_t i=1; i < data.m_Ids.size(); ++i ) {
        if ( data.m_Info.find( "DB" ) != data.m_Info.end()  
            &&  data.m_Info.find( "H2" ) != data.m_Info.end() ) 
        {
            variation.SetId().SetDb( "HapMap2" );
        }
        else {
            variation.SetId().SetDb( "local" );
        }      
        variation.SetId().SetTag().SetStr( data.m_Ids[i] );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::x_AssignVariationAlleles(
    const CVcfData& data,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    CVariation_ref::TData::TSet::TVariations& alleles = 
        pFeature->SetData().SetVariation().SetData().SetSet().SetVariations();

    vector<string> reference;
    reference.push_back( data.m_strRef );
    CRef<CVariation_ref> pReference( new CVariation_ref );
    pReference->SetSNV( reference, CVariation_ref::eSeqType_na );
    pReference->SetData().SetInstance().SetObservation( 
        CVariation_inst::eObservation_reference );
    alleles.push_back( pReference );

    for ( vector<string>::const_iterator cit = data.m_Alt.begin(); 
        cit != data.m_Alt.end(); ++cit )
    {
        vector<string> alternative;
        alternative.push_back( *cit );
        CRef<CVariation_ref> pAllele( new CVariation_ref );
        pAllele->SetSNV( alternative, CVariation_ref::eSeqType_na );
        pAllele->SetData().SetInstance().SetObservation( 
            CVariation_inst::eObservation_variant );
        alleles.push_back( pAllele );
    }
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE

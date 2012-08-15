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
#include <objects/seqfeat/VariantProperties.hpp>

#include <objtools/readers/read_util.hpp>
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
    typedef map<string,vector<string> > INFOS;
    typedef map<string, vector<string> > GTDATA;

    CVcfData() { m_pdQual = 0; };
    ~CVcfData() { delete m_pdQual; };

//    bool ParseData(
//        const string& );

    string m_strChrom;
    int m_iPos;
    vector<string> m_Ids;
    string m_strRef;
    vector<string> m_Alt;
    double* m_pdQual;
    string m_strFilter;
    INFOS m_Info;
    vector<string> m_FormatKeys;
//    vector< vector<string> > m_GenotypeData;
    GTDATA m_GenotypeData;
};

//  ----------------------------------------------------------------------------
ESpecType SpecType( 
    const string& spectype )
//  ----------------------------------------------------------------------------
{
    static map<string, ESpecType> typemap;
    if ( typemap.empty() ) {
        typemap["Integer"] = eType_Integer;
        typemap["Float"] = eType_Float;
        typemap["Flag"] = eType_Flag;
        typemap["Character"] = eType_Character;
        typemap["String"] = eType_String;
    }
    try {
        return typemap[spectype];
    }
    catch( ... ) {
        throw "Unexpected --- ##: bad type specifier!";
        return eType_String;
    }
};

//  ----------------------------------------------------------------------------
ESpecNumber SpecNumber(
    const string& specnumber )
//  ----------------------------------------------------------------------------
{
    if ( specnumber == "A" ) {
        return eNumber_CountAlleles;
    }
    if ( specnumber == "G" ) {
        return eNumber_CountGenotypes;
    }
    if ( specnumber == "." ) {
        return eNumber_CountUnknown;
    }
    try {
        return ESpecNumber( NStr::StringToInt( specnumber ) );
    }
    catch( ... ) {
        throw "Unexpected --- ##: bad number specifier!";
        return ESpecNumber( 0 );
    }    
};

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
    m_Meta.Reset( new CAnnotdesc );
    m_Meta->SetUser().SetType().SetStr( "vcf-meta-info" );

    while ( ! lr.AtEOF() ) {
        string line = *(++lr);
        NStr::TruncateSpacesInPlace( line );
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
    if ( ! NStr::StartsWith( line, "##" ) ) {
        return false;
    }
    m_MetaDirectives.push_back(line.substr(2));

    if ( x_ProcessMetaLineInfo( line, pAnnot ) ) {
        return true;
    }
    if ( x_ProcessMetaLineFilter( line, pAnnot ) ) {
        return true;
    }
    if ( x_ProcessMetaLineFormat( line, pAnnot ) ) {
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::x_ProcessMetaLineInfo(
    const string& line,
    CRef<CSeq_annot> pAnnot )
//  ----------------------------------------------------------------------------
{
    const string prefix = "##INFO=<";
    const string postfix = ">";

    if ( ! NStr::StartsWith( line, prefix ) || ! NStr::EndsWith( line, postfix ) ) {
        return false;
    }
    
    try {
        vector<string> fields;
        string key, id, numcount, type, description;
        string info = line.substr( 
            prefix.length(), line.length() - prefix.length() - postfix.length() );
        NStr::Tokenize( info, ",", fields );
        NStr::SplitInTwo( fields[0], "=", key, id );
        if ( key != "ID" ) {
            throw "Unexpected --- ##INFO: bad ID key!";
        }
        NStr::SplitInTwo( fields[1], "=", key, numcount );
        if ( key != "Number" ) {
            throw "Unexpected --- ##INFO: bad number key!";
        }
        NStr::SplitInTwo( fields[2], "=", key, type );
        if ( key != "Type" ) {
            throw "Unexpected --- ##INFO: bad type key!";
        }
        NStr::SplitInTwo( fields[3], "=", key, description );
        if ( key != "Description" ) {
            throw "Unexpected --- ##INFO: bad description key!";
        }
        m_InfoSpecs[id] = CVcfInfoSpec( id, numcount, type, description );        
    }
    catch ( ... ) {
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::x_ProcessMetaLineFilter(
    const string& line,
    CRef<CSeq_annot> pAnnot )
//  ----------------------------------------------------------------------------
{
    const string prefix = "##FILTER=<";
    const string postfix = ">";

    if ( ! NStr::StartsWith( line, prefix ) || ! NStr::EndsWith( line, postfix ) ) {
        return false;
    }
    
    try {
        vector<string> fields;
        string key, id, description;
        string info = line.substr( 
            prefix.length(), line.length() - prefix.length() - postfix.length() );
        NStr::Tokenize( info, ",", fields );
        NStr::SplitInTwo( fields[0], "=", key, id );
        if ( key != "ID" ) {
            throw "Unexpected --- ##FILTER: bad ID key!";
        }
        NStr::SplitInTwo( fields[1], "=", key, description );
        if ( key != "Description" ) {
            throw "Unexpected --- ##FILTER: bad description key!";
        }
        m_FilterSpecs[id] = CVcfFilterSpec( id, description );        
    }
    catch ( ... ) {
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::x_ProcessMetaLineFormat(
    const string& line,
    CRef<CSeq_annot> pAnnot )
//  ----------------------------------------------------------------------------
{
    const string prefix = "##FORMAT=<";
    const string postfix = ">";

    if ( ! NStr::StartsWith( line, prefix ) || ! NStr::EndsWith( line, postfix ) ) {
        return false;
    }
    
    try {
        vector<string> fields;
        string key, id, numcount, type, description;
        string info = line.substr( 
            prefix.length(), line.length() - prefix.length() - postfix.length() );
        NStr::Tokenize( info, ",", fields );
        NStr::SplitInTwo( fields[0], "=", key, id );
        if ( key != "ID" ) {
            throw "Unexpected --- ##FORMAT: bad ID key!";
        }
        NStr::SplitInTwo( fields[1], "=", key, numcount );
        if ( key != "Number" ) {
            throw "Unexpected --- ##FORMAT: bad number key!";
        }
        NStr::SplitInTwo( fields[2], "=", key, type );
        if ( key != "Type" ) {
            throw "Unexpected --- ##FORMAT: bad type key!";
        }
        NStr::SplitInTwo( fields[3], "=", key, description );
        if ( key != "Description" ) {
            throw "Unexpected --- ##FORMAT: bad description key!";
        }
        m_FormatSpecs[id] = CVcfFormatSpec( id, numcount, type, description );        
    }
    catch ( ... ) {
        return true;
    }
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

    m_Meta->SetUser().AddField("meta-information", m_MetaDirectives);

    //
    //  Per spec:
    //  The header line provides the column headers for the data records that follow.
    //  the first few are fixed and mandatory: CHROM .. FILTER.
    //  If genotype data is present this is followed by the FORMAT header.
    //  After that come the various headers for the genotype information, and these
    //  need to be preserved:
    //
    NStr::Tokenize(line, " \t", m_GenotypeHeaders, NStr::eMergeDelims);
    vector<string>::iterator pos_format = find(
        m_GenotypeHeaders.begin(), m_GenotypeHeaders.end(), "FORMAT");
    if ( pos_format == m_GenotypeHeaders.end() ) {
        m_GenotypeHeaders.clear();
    }
    else {
        m_GenotypeHeaders.erase( m_GenotypeHeaders.begin(), pos_format+1 );
        m_Meta->SetUser().AddField("genotype-headers", m_GenotypeHeaders);
    }
    
    //
    //  The header line signals the end of meta information, so migrate the
    //  accumulated meta information into the seq descriptor:
    //
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
    if ( ! x_ParseData( line, data ) ) {
        return false;
    }
    CRef<CSeq_feat> pFeat( new CSeq_feat );
    pFeat->SetData().SetVariation().SetData().SetSet().SetType(
        CVariation_ref::C_Data::C_Set::eData_set_type_alleles );
    pFeat->SetData().SetVariation().SetVariant_prop().SetVersion( 5 );
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
    if ( ! x_ProcessFormat( data, pFeat ) ) {
        return false;
    }

    if ( pFeat->GetExt().GetData().empty() ) {
        pFeat->ResetExt();
    }
    pAnnot->SetData().SetFtable().push_back( pFeat );
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::x_ParseData(
    const string& line,
    CVcfData& data )
//  ----------------------------------------------------------------------------
{
    vector<string> columns;
    NStr::Tokenize( line, "\t", columns, NStr::eMergeDelims );
    if ( columns.size() < 8 ) {
        return false;
    }
    try {
        data.m_strChrom = columns[0];
        data.m_iPos = NStr::StringToInt( columns[1] );
        NStr::Tokenize( columns[2], ";", data.m_Ids, NStr::eNoMergeDelims );
        if ( (data.m_Ids.size() == 1)  &&  (data.m_Ids[0] == ".") ) {
            data.m_Ids.clear();
        }
        data.m_strRef = columns[3];
        NStr::Tokenize( columns[4], ",", data.m_Alt, NStr::eNoMergeDelims );
        if ( columns[5] != "." ) {
            data.m_pdQual = new double( NStr::StringToDouble( columns[5] ) );
        }
        data.m_strFilter = columns[6];

        vector<string> infos;
        if ( columns[7] != "." ) {
            NStr::Tokenize( columns[7], ";", infos, NStr::eMergeDelims );
            for ( vector<string>::iterator it = infos.begin(); 
                it != infos.end(); ++it ) 
            {
                string key, value;
                NStr::SplitInTwo( *it, "=", key, value );
                data.m_Info[key] = vector<string>();
                NStr::Tokenize( value, ",", data.m_Info[key] );
            }
        }
        if ( columns.size() > 8 ) {
            NStr::Tokenize( columns[8], ":", data.m_FormatKeys, NStr::eMergeDelims );

            for ( size_t u=9; u < columns.size(); ++u ) {
                vector<string> values;
                NStr::Tokenize( columns[u], ":", values, NStr::eMergeDelims );
                data.m_GenotypeData[ m_GenotypeHeaders[u-9] ] = values;
            }
        }
    }
    catch ( ... ) {
        return false;
    }
    return true;
}

//  ---------------------------------------------------------------------------
bool
CVcfReader::x_AssignFeatureLocation(
    const CVcfData& data,
    CRef<CSeq_feat> pFeature )
//  ---------------------------------------------------------------------------
{
    CRef<CSeq_id> pId(CReadUtil::AsSeqId(data.m_strChrom, m_iFlags));

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
        for ( map<string,vector<string> >::const_iterator cit = data.m_Info.begin();
            cit != data.m_Info.end(); cit++ )
        {
            const string& key = cit->first;
            vector<string> value = cit->second;
            if ( value.empty() ) {
                infos.push_back( key );
            }
            else {
                string joined = NStr::Join( list<string>( value.begin(), value.end() ), ";" );
                infos.push_back( key + "=" + joined );
            }
        }
        ext.AddField( "info", NStr::Join( infos, ";" ) );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CVcfReader::x_ProcessFormat(
    const CVcfData& data,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    if (data.m_FormatKeys.empty()) {
        return true;
    }

    CSeq_feat::TExt& ext = pFeature->SetExt();
    ext.AddField("format", data.m_FormatKeys);

    CRef<CUser_field> pGenotypeData( new CUser_field );
    pGenotypeData->SetLabel().SetStr("genotype-data");

    for ( CVcfData::GTDATA::const_iterator cit = data.m_GenotypeData.begin();
            cit != data.m_GenotypeData.end(); ++cit) {

        CRef<CUser_field> pCol( new CUser_field );
        pCol->SetLabel().SetStr(cit->first);

        //for ( map<string,string>::const_iterator cc = cit->second.begin();
        //        cc != cit->second.end(); ++cc) {
        //    CRef<CUser_field> value( new CUser_field );
        //    value->SetLabel().SetStr(cc->first);
        //    value->SetData().SetStr(cc->second);
        //    col->SetData().SetFields().push_back(value);
        //}
        pCol->SetData().SetStrs() = cit->second;
        pGenotypeData->SetData().SetFields().push_back(pCol);
    }
    ext.SetData().push_back(pGenotypeData);
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
//    CVariation_ref::TVariant_prop& var_prop = variation.SetVariant_prop();
//    var_prop.SetVersion( 5 );
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
    pReference->SetVariant_prop().SetVersion( 5 );
    pReference->SetSNV( reference, CVariation_ref::eSeqType_na );
    pReference->SetData().SetInstance().SetObservation( 
        CVariation_inst::eObservation_reference );
    alleles.push_back( pReference );

    size_t altcount = 0;
    for ( vector<string>::const_iterator cit = data.m_Alt.begin(); 
        cit != data.m_Alt.end(); ++cit )
    {
        vector<string> alternative;
        alternative.push_back( *cit );
        CRef<CVariation_ref> pAllele( new CVariation_ref );
        pAllele->SetVariant_prop().SetVersion( 5 );
        pAllele->SetSNV( alternative, CVariation_ref::eSeqType_na );
        pAllele->SetData().SetInstance().SetObservation( 
            CVariation_inst::eObservation_variant );

        //  allele frequency:
        CVcfData::INFOS::const_iterator af = data.m_Info.find( "AF" );
        if ( af != data.m_Info.end() ) {
            const vector<string>& info = af->second;
            double freq = NStr::StringToDouble( info[altcount] );
            pAllele->SetVariant_prop().SetAllele_frequency( freq );
        }

        //  ancestral allele:
        CVcfData::INFOS::const_iterator aa = data.m_Info.find( "AA" );
        if ( aa != data.m_Info.end() ) {
            string ancestral = aa->second[0];
            if ( ancestral == *cit ) {
                pAllele->SetVariant_prop().SetIs_ancestral_allele( true );
            }
        }

        alleles.push_back( pAllele );
        ++altcount;
    }
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE

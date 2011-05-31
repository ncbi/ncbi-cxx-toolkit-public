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
    CVcfData() {};
    ~CVcfData() {};

    bool ParseData(
        const string& );

    string m_strChrom;
    int m_iPos;
    vector<string> m_Ids;
    string m_strRef;
    vector<string> m_Alt;
    double m_dQual;
    string m_strFilter;
    map<string,string> m_Info;
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
        m_dQual = NStr::StringToDouble( columns[5] );
        m_strFilter = columns[6];

        vector<string> infos;
        NStr::Tokenize( columns[7], ";", infos, NStr::eMergeDelims );
        for ( vector<string>::iterator it = infos.begin(); it != infos.end(); ++it ) {
            string key, value;
            NStr::SplitInTwo( *it, "=", key, value );
            m_Info[key] = value;
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
    pFeat->SetData().SetVariation().SetData().SetNote( "Place Holder" );
    if ( ! x_AssignFeatureLocation( data, pFeat ) ) {
        return false;
    }
    if ( ! x_AssignVariationIds( data, pFeat ) ) {
        return false;
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
CVcfReader::x_AssignVariationIds(
    const CVcfData& data,
    CRef<CSeq_feat> pFeature )
//  ----------------------------------------------------------------------------
{
    if ( data.m_Ids.empty() ) {
        return true;
    }
    CVariation_ref& variation = pFeature->SetData().SetVariation();
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE

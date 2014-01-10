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
 *   GFF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seq/sofa_type.hpp>
#include <objects/seq/sofa_map.hpp>

#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/writers/write_util.hpp>
#include <objtools/writers/gff3_writer.hpp>
#include <objtools/writers/gff3_write_data.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
const char* CGff3WriteRecordFeature::ATTR_SEPARATOR
//  ----------------------------------------------------------------------------
    = ";";

//  ----------------------------------------------------------------------------
const char* CGff3WriteRecordFeature::MULTIVALUE_SEPARATOR
//  ----------------------------------------------------------------------------
    = ",";

//  ----------------------------------------------------------------------------
string s_MakeGffDbtag( 
    const CSeq_id_Handle& idh,
    CScope& scope )
//  ----------------------------------------------------------------------------
{
    CSeq_id_Handle gi_idh = sequence::GetId( idh, scope, sequence::eGetId_ForceGi );
    if ( !gi_idh ) {
        return idh.AsString();
    }
    string strGffTag("GI:");
    gi_idh.GetSeqId()->GetLabel( &strGffTag, CSeq_id::eContent );
    return strGffTag;
}
        
//  ----------------------------------------------------------------------------
CConstRef<CUser_object> s_GetUserObjectByType(
    const CUser_object& uo,
    const string& strType )
//  ----------------------------------------------------------------------------
{
    if ( uo.IsSetType() && uo.GetType().IsStr() && 
            uo.GetType().GetStr() == strType ) {
        return CConstRef<CUser_object>( &uo );
    }
    const CUser_object::TData& fields = uo.GetData();
    for ( CUser_object::TData::const_iterator it = fields.begin(); 
            it != fields.end(); 
            ++it ) {
        const CUser_field& field = **it;
        if ( field.IsSetData() ) {
            const CUser_field::TData& data = field.GetData();
            if ( data.Which() == CUser_field::TData::e_Object ) {
                CConstRef<CUser_object> recur = s_GetUserObjectByType( 
                    data.GetObject(), strType );
                if ( recur ) {
                    return recur;
                }
            }
        }
    }
    return CConstRef<CUser_object>();
}
    
//  ----------------------------------------------------------------------------
CGff3WriteRecordFeature::CGff3WriteRecordFeature(
    CGffFeatureContext& fc,
    const string& id )
//  ----------------------------------------------------------------------------
    : CGffWriteRecordFeature(fc, id)
{
};

//  ----------------------------------------------------------------------------
CGff3WriteRecordFeature::CGff3WriteRecordFeature(
    const CGff3WriteRecordFeature& other )
//  ----------------------------------------------------------------------------
    : CGffWriteRecordFeature( other )
{
};

//  ----------------------------------------------------------------------------
CGff3WriteRecordFeature::~CGff3WriteRecordFeature()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::AssignFromAsnLinear(
    CMappedFeat mf,
    unsigned int flags )
//  ----------------------------------------------------------------------------
{
    return CGffWriteRecordFeature::AssignFromAsn(mf, flags);
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::AssignFromAsn(
    CMappedFeat mf,
    unsigned int flags )
//  ----------------------------------------------------------------------------
{
    m_pLoc.Reset( new CSeq_loc( CSeq_loc::e_Mix ) );
    m_pLoc->Add( mf.GetLocation() );
    CWriteUtil::ChangeToPackedInt(*m_pLoc);

    CBioseq_Handle bsh = m_fc.BioseqHandle();
    if (!CWriteUtil::IsSequenceCircular(bsh)) {
        return CGffWriteRecordFeature::AssignFromAsn(mf, flags);
    }

    //  intervals wrapping around the origin extend beyond the sequence length
    //  instead of breaking and restarting at the origin.
    //
    unsigned int len = bsh.GetInst().GetLength();
    list< CRef< CSeq_interval > >& sublocs = m_pLoc->SetPacked_int().Set();
    if (sublocs.size() < 2) {
        return CGffWriteRecordFeature::AssignFromAsn(mf, flags);
    }

    list< CRef< CSeq_interval > >::iterator it, it_ceil=sublocs.end(), 
        it_floor=sublocs.end();
    //fl new circular coordinate handling starts here >>>
    for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
        //fix intervals broken in two for crossing the origin to extend
        //  into virtual space instead
        CSeq_interval& subint = **it;
        if (subint.IsSetFrom()  &&  subint.GetFrom() == 0) {
            it_floor = it;
        }
        if (subint.IsSetTo()  &&  subint.GetTo() == len-1) {
            it_ceil = it;
        }
        if (it_floor != sublocs.end()  &&  it_ceil != sublocs.end()) {
            break;
        } 
    }
    if ( it_ceil != sublocs.end()  &&  it_floor != sublocs.end() ) {
        (*it_ceil)->SetTo( (*it_ceil)->GetTo() + (*it_floor)->GetTo() + 1 );
        sublocs.erase(it_floor);
    }

    return CGffWriteRecordFeature::AssignFromAsn(mf, flags);
};
    
//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignType(
    CMappedFeat mf,
    unsigned int flags )
//  ----------------------------------------------------------------------------
{
    if (flags & CGff3Writer::fExtraQuals  &&  mf.IsSetQual()) {
        const vector< CRef< CGb_qual > >& quals = mf.GetQual();
        vector< CRef< CGb_qual > >::const_iterator cit = quals.begin();
        for ( ; cit != quals.end(); cit++) {
            const CGb_qual& qual = **cit;
            if (qual.GetQual() == "gff_type") {
                m_strType = qual.GetVal();
                return true;
            }
        }
    }

    static CSafeStatic<CSofaMap> SOFAMAP;

    if ( ! mf.IsSetData() ) {
        m_strType = SOFAMAP->DefaultName();
    }
    m_strType = SOFAMAP->MappedName( mf.GetFeatType(), mf.GetFeatSubtype() );
    return true;
};

//  ----------------------------------------------------------------------------
bool sIsTransspliced(const CMappedFeat& mf)
//  ----------------------------------------------------------------------------
{
    return (mf.IsSetExcept_text()  &&  mf.GetExcept_text() == "trans-splicing");
}

//  ----------------------------------------------------------------------------
bool sGetTranssplicedInPoint(const CSeq_loc& loc, unsigned int& inPoint)
//  start determined by the minimum start of any sub interval
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSeq_interval> >::const_iterator CIT;

    if (!loc.IsPacked_int()) {
        return false;
    }
    const CPacked_seqint& packedInt = loc.GetPacked_int();
    inPoint = packedInt.GetStart(eExtreme_Biological);
    const list<CRef<CSeq_interval> >& intvs = packedInt.Get();
    for (CIT cit = intvs.begin(); cit != intvs.end(); cit++) {
        const CSeq_interval& intv = **cit;
        if (intv.GetFrom() < inPoint) {
            inPoint = intv.GetFrom();
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool sGetTranssplicedOutPoint(const CSeq_loc& loc, unsigned int& outPoint)
//  stop determined by the maximum stop of any sub interval
//  ----------------------------------------------------------------------------
{
    typedef list<CRef<CSeq_interval> >::const_iterator CIT;

    if (!loc.IsPacked_int()) {
        return false;
    }
    const CPacked_seqint& packedInt = loc.GetPacked_int();
    outPoint = packedInt.GetStop(eExtreme_Biological);
    const list<CRef<CSeq_interval> >& intvs = packedInt.Get();
    for (CIT cit = intvs.begin(); cit != intvs.end(); cit++) {
        const CSeq_interval& intv = **cit;
        if (intv.GetTo() > outPoint) {
            outPoint = intv.GetTo();
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool sGetTranssplicedLocation(
    const CSeq_loc& loc,
    unsigned int& inPoint,
    unsigned int& outPoint,
    ENa_strand& strand)
//  ----------------------------------------------------------------------------
{
    int strandBalance = 0;
    typedef list<CRef<CSeq_interval> >::const_iterator CIT;

    if (!loc.IsPacked_int()) {
        return false;
    }
    const CPacked_seqint& packedInt = loc.GetPacked_int();
    inPoint = packedInt.GetStart(eExtreme_Biological);
    outPoint = packedInt.GetStop(eExtreme_Biological);
    strand = eNa_strand_plus;

    const list<CRef<CSeq_interval> >& intvs = packedInt.Get();
    for (CIT cit = intvs.begin(); cit != intvs.end(); cit++) {
        const CSeq_interval& intv = **cit;
        if (intv.GetFrom() < inPoint) {
            inPoint = intv.GetFrom();
        }
        if (intv.GetTo() > outPoint) {
            outPoint = intv.GetTo();
        }
        if (intv.GetStrand() == objects::eNa_strand_minus) {
            --strandBalance;
        }
        else if (intv.GetStrand() == objects::eNa_strand_plus) {
            ++strandBalance;
        }
    }
    if (strandBalance < 0) {
        strand = eNa_strand_minus;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignStart(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( m_pLoc ) {
        if (sIsTransspliced(mapped_feat)) {
            
            if (!sGetTranssplicedInPoint(*m_pLoc, m_uSeqStart)) {
                return false;
            }
        }
        else {
            m_uSeqStart = m_pLoc->GetStart(eExtreme_Positional);
            if (m_pLoc->IsPartialStart(eExtreme_Biological)) {
                string min = NStr::IntToString(m_uSeqStart + 1);
                SetAttribute("start_range", string(".,") + min);
            }
        }
    }
    CBioseq_Handle bsh = m_fc.BioseqHandle();
    if (!CWriteUtil::IsSequenceCircular(bsh)) {
        return true;
    }

    unsigned int bstart = m_pLoc->GetStart( eExtreme_Biological );
    unsigned int bstop = m_pLoc->GetStop( eExtreme_Biological );

    ENa_strand strand = m_pLoc->GetStrand();
    if (strand == eNa_strand_minus) {
        if (m_uSeqStart < bstop) {
            m_uSeqStart += bsh.GetInst().GetLength();
        }
        return true;
    }
    //everything else considered eNa_strand_plus
    if (m_uSeqStart < bstart) {
        m_uSeqStart += bsh.GetInst().GetLength();
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignStop(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( m_pLoc ) {
        if (sIsTransspliced(mapped_feat)) {
            if (!sGetTranssplicedOutPoint(*m_pLoc, m_uSeqStop)) {
                return false;
            }
        }
        else {
            m_uSeqStop = m_pLoc->GetStop(eExtreme_Positional);
            if (m_pLoc->IsPartialStop(eExtreme_Biological)) {
                string max = NStr::IntToString(m_uSeqStop + 1);
                SetAttribute("end_range", max + string(",."));
            }
        }
    }
    CBioseq_Handle bsh = m_fc.BioseqHandle();
    if (!CWriteUtil::IsSequenceCircular(bsh)) {
        return true;
    }

    unsigned int bstart = m_pLoc->GetStart( eExtreme_Biological );
    unsigned int bstop = m_pLoc->GetStop( eExtreme_Biological );

    ENa_strand strand = m_pLoc->GetStrand();
    if (strand == eNa_strand_minus) {
        if (m_uSeqStop < bstop) {
            m_uSeqStop += bsh.GetInst().GetLength();
        }
        return true;
    }
    //everything else considered eNa_strand_plus
    if (m_uSeqStop < bstart) {
        m_uSeqStop += bsh.GetInst().GetLength();
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesFromAsnCore(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesFromAsnExtended(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    // collection of feature attribute that we want to propagate to every feature
    //  type under the sun
    //
    if ( ! x_AssignAttributeGbKey( mapped_feat ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
string CGff3WriteRecordFeature::StrAttributes() const
//  ----------------------------------------------------------------------------
{
    string strAttributes;
    strAttributes.reserve(256);
    CGffWriteRecord::TAttributes attrs;
    attrs.insert(Attributes().begin(), Attributes().end());
    CGffWriteRecord::TAttrIt it;

    x_StrAttributesAppendValueGff3("ID", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Name", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Alias", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Parent", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Target", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Gap", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Derives_from", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Note", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Dbxref", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("Ontology_term", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("start_range", attrs, strAttributes);
    x_StrAttributesAppendValueGff3("end_range", attrs, strAttributes);

    while (!attrs.empty()) {
        x_StrAttributesAppendValueGff3(attrs.begin()->first, attrs, strAttributes);
    }
    if (strAttributes.empty()) {
        strAttributes = ".";
    }
    return strAttributes;
}

//  ----------------------------------------------------------------------------
void CGff3WriteRecordFeature::x_StrAttributesAppendValueGff3(
    const string& strKey,
    map<string, vector<string> >& attrs,
    string& strAttributes ) const
//  ----------------------------------------------------------------------------
{
    x_StrAttributesAppendValue( strKey, ATTR_SEPARATOR,
        MULTIVALUE_SEPARATOR, attrs, strAttributes );
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributes(
    CMappedFeat mf,
    unsigned int flags )
//  ----------------------------------------------------------------------------
{
    if ( ! x_AssignAttributesFromAsnCore( mf ) ) {
        return false;
    }
    if ( ! x_AssignAttributesFromAsnExtended( mf ) ) {
        return false;
    }

    if ( !x_AssignAttributesMiscFeature( mf ) ) {
        return false;
    }
    
    switch( mf.GetData().GetSubtype() ) {
        default:
            break;
        case CSeqFeatData::eSubtype_gene:
            if ( !x_AssignAttributesGene( mf ) ) {
                return false;
            }
            break;
        case CSeqFeatData::eSubtype_mRNA:
            if ( !x_AssignAttributesMrna( mf ) ) {
                return false;
            }
            break;
        case CSeqFeatData::eSubtype_tRNA:
            if ( !x_AssignAttributesTrna( mf ) ) {
                return false;
            }
            break;
        case CSeqFeatData::eSubtype_cdregion:
            if ( !x_AssignAttributesCds( mf ) ) {
                return false;
            }
            break;
        case CSeqFeatData::eSubtype_ncRNA:
            if ( !x_AssignAttributesNcrna( mf ) ) {
                return false;
            }
            break;
    }

    //  any extra junk in the feature attributes. Such extra junk could originate
    //  from a GFF3 round trip starting with GFF3 that uses non-standard attribute
    //  keys:
    if (flags & CGff3Writer::fExtraQuals) {
        if (!x_AssignAttributesExtraQuals( mf ) ) {
            return false;
        }
    }
    
    //  deriviate attributes --- depend on other attributes. Hence need to be
    //  done last: 
    if ( !x_AssignAttributeName( mf ) ) {
        return false;
    }

    return true; 
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesExtraQuals(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetQual()) {
        return true;
    }
    const vector< CRef< CGb_qual > >& quals = mf.GetQual();
    vector< CRef< CGb_qual > >::const_iterator cit = quals.begin();
    for ( ; cit != quals.end(); cit++) {
        const CGb_qual& qual = **cit;
        if (!qual.IsSetQual() || !qual.IsSetVal()) {
            continue;
        }
        string key = qual.GetQual();
        if (NStr::StartsWith(key, "gff_")) {
            continue;
        }
        if (key == "exon_number"  &&  mf.GetData().IsGene()) {
            continue;
        }
        TAttrCit fit = this->m_Attributes.find(key);
        if (fit == m_Attributes.end()) {
            SetAttribute(key, qual.GetVal());
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesGene(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    return (
        x_AssignAttributeGene( mapped_feat )  &&
        x_AssignAttributeLocusTag( mapped_feat )  &&
        x_AssignAttributeGeneSynonym( mapped_feat )  &&
        x_AssignAttributeOldLocusTag( mapped_feat )  &&
        x_AssignAttributePartial( mapped_feat )  &&
        x_AssignAttributeGeneDesc( mapped_feat )  &&
        x_AssignAttributeMapLoc( mapped_feat ) );
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesMrna(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    return (
        x_AssignAttributePartial( mapped_feat )  &&
        x_AssignAttributeGene( mapped_feat ) );
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesTrna(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    return ( true );
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesNcrna(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    return (
        x_AssignAttributeNcrnaClass( mapped_feat) );
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesCds(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    return (
        x_AssignAttributePartial( mapped_feat )  &&
        x_AssignAttributeProteinId( mapped_feat )  &&
        x_AssignAttributeTranslationTable( mapped_feat )  &&
        x_AssignAttributeCodeBreak( mapped_feat ) );
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesMiscFeature(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    return (
        x_AssignAttributeProduct( mapped_feat )   &&
        x_AssignAttributeException( mapped_feat )  &&
        x_AssignAttributeExonNumber( mapped_feat )  &&
        x_AssignAttributePseudo( mapped_feat )  &&
        x_AssignAttributeDbXref( mapped_feat )  &&
        x_AssignAttributeGene( mapped_feat )  &&
        x_AssignAttributeNote( mapped_feat )  &&
        x_AssignAttributeOldLocusTag( mapped_feat )  &&
        x_AssignAttributeIsOrdered( mapped_feat )  &&
        x_AssignAttributeTranscriptId( mapped_feat ) );
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeGene(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    string strGene;
    if ( mf.GetData().Which() == CSeq_feat::TData::e_Gene ) {
        const CGene_ref& gene_ref = mf.GetData().GetGene();
        CWriteUtil::GetGeneRefGene(gene_ref, strGene);
    }

    if ( strGene.empty() && mf.IsSetXref() ) {
        const vector<CRef<CSeqFeatXref> > xrefs = mf.GetXref();
        for ( vector<CRef<CSeqFeatXref> >::const_iterator it = xrefs.begin();
            it != xrefs.end();
            ++it ) {
            const CSeqFeatXref& xref = **it;
            if ( xref.CanGetData() && xref.GetData().IsGene() ) {
                CWriteUtil::GetGeneRefGene(xref.GetData().GetGene(), strGene);
                break;
            }
        }
    }

    if ( strGene.empty() ) {
        //CMappedFeat gene = feature::GetBestGeneForFeat(mf, &m_fc.FeatTree());
        CMappedFeat gene = m_fc.FindBestGeneParent(mf);
        if (gene.IsSetData()  &&  gene.GetData().IsGene()) {
            CWriteUtil::GetGeneRefGene(gene.GetData().GetGene(), strGene);
        }
    }

    if (!strGene.empty()) {
        SetAttribute("gene", strGene);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeNote(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (!mf.IsSetComment()  ||  mf.GetComment().empty()) {
        return true;
    }
    SetAttribute("Note", mf.GetComment());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributePartial(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CSeqFeatData::ESubtype subtype = mf.GetData().GetSubtype();
    if (mf.IsSetPartial()) {
        if (mf.GetPartial() == true) {
            SetAttribute("partial", "true");
            return true;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributePseudo(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( ! mapped_feat.IsSetPseudo() ) {
        return true;
    }
    if ( mapped_feat.GetPseudo() == true ) {
        SetAttribute("pseudo", "true");
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeDbXref(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CSeqFeatData::E_Choice choice = mf.GetData().Which();

    if ( mf.IsSetDbxref() ) {
        const CSeq_feat::TDbxref& dbxrefs = mf.GetDbxref();
        for ( size_t i=0; i < dbxrefs.size(); ++i ) {
            string tag;
            if (CWriteUtil::GetDbTag(*dbxrefs[i], tag)) {
                SetAttribute("Dbxref", tag);
            }
        }
    }

    switch (choice) {
        default: {
            CMappedFeat parent;
            try {
                parent = m_fc.FeatTree().GetParent( mf );
            }
            catch(...) {
            }
            if ( parent  &&  parent.IsSetDbxref()) {
                const CSeq_feat::TDbxref& more_dbxrefs = parent.GetDbxref();
                for ( size_t i=0; i < more_dbxrefs.size(); ++i ) {
                    string tag;
                    if (CWriteUtil::GetDbTag(*more_dbxrefs[i], tag)) {
                        SetAttribute("Dbxref", tag);
                    }
                }
            }
            break;
        }

        case CSeq_feat::TData::e_Rna:
        case CSeq_feat::TData::e_Cdregion: {
            if ( mf.IsSetProduct() ) {
                CSeq_id_Handle idh = sequence::GetId( 
                    mf.GetProductId(), mf.GetScope(), sequence::eGetId_ForceAcc);
                if (!idh) {
                    idh = sequence::GetId(
                        mf.GetProductId(), mf.GetScope(), sequence::eGetId_ForceGi);
                }
                if (idh) {
                    string str;
                    idh.GetSeqId()->GetLabel(&str, CSeq_id::eContent);
                    if (isupper(str[0])) { //accession
                        if (NPOS != str.find('_')) { //nucleotide
                            str = string("Genbank:") + str;
                        }
                        else { //protein
                            str = string("NCBI_GP:") + str;
                        }
                    }
                    else { //GI
                        str = string("NCBI_gi:") + str;
                    }
                    SetAttribute("Dbxref", str);
                }
            }
            CMappedFeat gene_feat = m_fc.FeatTree().GetParent( mf, CSeqFeatData::e_Gene );
            if ( gene_feat  &&  mf.IsSetXref()) {
                const CSeq_feat::TXref& xref = mf.GetXref();
                for (CSeq_feat::TXref::const_iterator cit = xref.begin(); cit != xref.end(); ++cit) {
                    if ( (*cit)->IsSetData()  &&  (*cit)->GetData().IsGene()) {
                        const CSeqFeatData::TGene& gene = (*cit)->GetData().GetGene();
                        if (gene.IsSuppressed()) {
                            gene_feat = CMappedFeat();
                            break;
                        }
                    }
                }
            }

            if ( gene_feat  &&  gene_feat.IsSetDbxref() ) {
                const CSeq_feat::TDbxref& dbxrefs = gene_feat.GetDbxref();
                for ( size_t i=0; i < dbxrefs.size(); ++i ) {
                    string tag;
                    if (CWriteUtil::GetDbTag(*dbxrefs[i], tag)) {
                        SetAttribute("Dbxref", tag);
                    }
                }
            }
            break;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeGeneSynonym(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mapped_feat.GetData().GetGene();
    if (!gene_ref.IsSetSyn()) {
        return true;
    }

    const list<string>& syns = gene_ref.GetSyn();
    list<string>::const_iterator it = syns.begin();
    if (!gene_ref.IsSetLocus() && !gene_ref.IsSetLocus_tag()) {
        ++it;
    }    
    if (it == syns.end()) {
        return true;
    }
    while ( it != syns.end() ) {
        SetAttribute("gene_synonym", *(it++));
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeGeneDesc(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mapped_feat.GetData().GetGene();
    if ( ! gene_ref.IsSetDesc() ) {
        return true;
    }
    SetAttribute("description", gene_ref.GetDesc());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeMapLoc(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mapped_feat.GetData().GetGene();
    if ( ! gene_ref.IsSetMaploc() ) {
        return true;
    }
    SetAttribute("map", gene_ref.GetMaploc());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeLocusTag(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mapped_feat.GetData().GetGene();
    if ( ! gene_ref.IsSetLocus_tag() ) {
        return true;
    }
    SetAttribute("locus_tag", gene_ref.GetLocus_tag());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeName(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    vector<string> value;   
    switch ( mf.GetFeatSubtype() ) {

        default:
            break;

        case CSeqFeatData::eSubtype_gene:
            if (GetAttribute("gene", value)) {
                SetAttribute("Name", value.front());
                return true;
            }
            if (GetAttribute("locus_tag", value)) {
                SetAttribute("Name", value.front());
                return true;
            }
            return true;

        case CSeqFeatData::eSubtype_cdregion:
            if (GetAttribute("protein_id", value)) {
                SetAttribute("Name", value.front());
                return true;
            }
            return true;
    }

    if (GetAttribute("transcript_id", value)) {
        SetAttribute("Name", value.front());
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeCodonStart(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CCdregion& cdr = mapped_feat.GetData().GetCdregion();
    if ( ! cdr.IsSetFrame() ) {
        return true;
    }
    string strFrame;
    switch( cdr.GetFrame() ) {
    default:
        break;

    case CCdregion::eFrame_one:
        strFrame = "1";
        break;

    case CCdregion::eFrame_two:
        strFrame = "2";
        break;

    case CCdregion::eFrame_three:
        strFrame = "3";
        break;
    }
    if ( ! strFrame.empty() ) {
        SetAttribute("codon_start", strFrame);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeProduct(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    CSeqFeatData::ESubtype subtype = mf.GetFeatSubtype();
    if (subtype == CSeqFeatData::eSubtype_cdregion) {

        // Possibility 1:
        // Product name comes from a prot-ref which stored in the seqfeat's 
        // xrefs:
        const CProt_ref* pProtRef = mf.GetProtXref();
        if ( pProtRef && pProtRef->IsSetName() ) {
            const list<string>& names = pProtRef->GetName();
            SetAttribute("product", names.front());
            return true;
        }

        // Possibility 2:
        // Product name is from the prot-ref refered to by the seqfeat's 
        // data.product:
        if (mf.IsSetProduct()) {
            const CSeq_id* pId = mf.GetProduct().GetId();
            if (pId) {
                CBioseq_Handle bsh = mf.GetScope().GetBioseqHandle(*pId); 
                if (bsh) {
                    CFeat_CI it(bsh, SAnnotSelector(CSeqFeatData::eSubtype_prot));
                    if (it  &&  it->IsSetData() 
                            &&  it->GetData().GetProt().IsSetName()
                            &&  !it->GetData().GetProt().GetName().empty()) {
                        SetAttribute("product",
                            it->GetData().GetProt().GetName().front());
                        return true;
                    }
                }
            }
            
            string product;
            if (CWriteUtil::GetBestId(mf.GetProductId(), mf.GetScope(), product)) {
                SetAttribute("product", product);
                return true;
            }
        }
    }

    CSeqFeatData::E_Choice type = mf.GetFeatType();
    if (type == CSeqFeatData::e_Rna) {
        const CRNA_ref& rna = mf.GetData().GetRna();

        if (subtype == CSeqFeatData::eSubtype_tRNA) {
            if (rna.IsSetExt()  &&  rna.GetExt().IsTRNA()) {
                const CTrna_ext& trna = rna.GetExt().GetTRNA();
                string anticodon;
                if (CWriteUtil::GetTrnaAntiCodon(trna, anticodon)) {
                    SetAttribute("anticodon", anticodon);
                }
                string codons;
                if (CWriteUtil::GetTrnaCodons(trna, codons)) {
                    SetAttribute("codons", codons);
                }
                string aa;
                if (CWriteUtil::GetTrnaProductName(trna, aa)) {
                    SetAttribute("product", aa);
                    return true;
                }
            }
        }

        if (rna.IsSetExt()  &&  rna.GetExt().IsName()) {
            SetAttribute("product", rna.GetExt().GetName());
            return true;
        }

        if (rna.IsSetExt()  &&  rna.GetExt().IsGen()  &&  
                rna.GetExt().GetGen().IsSetProduct() ) {
            SetAttribute("product", rna.GetExt().GetGen().GetProduct());
            return true;
        }
    }

    // finally, look for gb_qual
    if (mf.IsSetQual()) {
        const CSeq_feat::TQual& quals = mf.GetQual();
        for ( CSeq_feat::TQual::const_iterator cit = quals.begin(); 
                cit != quals.end(); ++cit) {
            if ((*cit)->IsSetQual()  &&  (*cit)->IsSetVal()  &&  
                    (*cit)->GetQual() == "product") {
                SetAttribute("product", (*cit)->GetVal());
                return true;
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeEvidence(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const string strExperimentDefault(
        "experimental evidence, no additional details recorded" );
    const string strInferenceDefault(
        "non-experimental evidence, no additional details recorded" );

    bool bExperiment = false;
    bool bInference = false;
    const CSeq_feat::TQual& quals = mapped_feat.GetQual();
    for ( CSeq_feat::TQual::const_iterator it = quals.begin(); 
            ( it != quals.end() ) && ( !bExperiment && !bInference ); 
            ++it ) {
        if ( ! (*it)->CanGetQual() ) {
            continue;
        }
        string strKey = (*it)->GetQual();
        if ( strKey == "experiment" ) {
           SetAttribute("experiment", (*it)->GetVal());
            bExperiment = true;
        }
        if ( strKey == "inference" ) {
            SetAttribute("inference", (*it)->GetVal());
            bInference = true;
        }
    }

    // "exp_ev" only enters if neither "experiment" nor "inference" qualifiers
    //  present
    if ( !bExperiment && !bInference ) {
        if ( mapped_feat.IsSetExp_ev() ) {
            if ( mapped_feat.GetExp_ev() == CSeq_feat::eExp_ev_not_experimental ) {
                SetAttribute("inference", strInferenceDefault);
            }
            else if ( mapped_feat.GetExp_ev() == CSeq_feat::eExp_ev_experimental ) {
                SetAttribute("experiment", strExperimentDefault);
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeModelEvidence(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( mapped_feat.IsSetExt() ) {
        CConstRef<CUser_object> model_evidence = s_GetUserObjectByType( 
            mapped_feat.GetExt(), "ModelEvidence" );
        if ( model_evidence ) {
            string strNote;
            if ( model_evidence->HasField( "Method" ) ) {
                strNote += "Derived by automated computational analysis";
                strNote += " using gene prediction method: ";
                strNote += model_evidence->GetField( "Method" ).GetData().GetStr();
                strNote += ".";
            }
            if ( model_evidence->HasField( "Counts" ) ) {
                const CUser_field::TData::TFields& fields =
                    model_evidence->GetField( "Counts" ).GetData().GetFields();
                unsigned int uCountMrna = 0;
                unsigned int uCountEst = 0;
                unsigned int uCountProtein = 0;
                for ( CUser_field::TData::TFields::const_iterator cit = fields.begin();
                    cit != fields.end();
                    ++cit ) {
                    string strLabel = (*cit)->GetLabel().GetStr();
                    if ( strLabel == "mRNA" ) {
                        uCountMrna = (*cit)->GetData().GetInt();
                        continue;
                    }
                    if ( strLabel == "EST" ) {
                        uCountEst = (*cit)->GetData().GetInt();
                        continue;
                    }
                    if ( strLabel == "Protein" ) {
                        uCountProtein = (*cit)->GetData().GetInt();
                        continue;
                    }
                }
                if ( uCountMrna || uCountEst || uCountProtein ) {
                    string strSupport = " Supporting evidence includes similarity to:";
                    string strPrefix = " ";
                    if ( uCountMrna ) {
                        strSupport += strPrefix;
                        strSupport += NStr::UIntToString( uCountMrna );
                        strSupport += ( uCountMrna > 1 ? " mRNAs" : " mRNA" );
                        strPrefix = "%2C ";
                    }
                    if ( uCountEst ) {
                        strSupport += strPrefix;
                        strSupport += NStr::UIntToString( uCountEst );
                        strSupport += ( uCountEst > 1 ? " ESTs" : " EST" );
                        strPrefix = "%2C ";
                    }
                    if ( uCountProtein ) {
                        strSupport += strPrefix;
                        strSupport += NStr::UIntToString( uCountProtein );
                        strSupport += ( uCountProtein > 1 ? " Proteins" : " Protein" );
                    }
                    strNote += strSupport;
                }
            }
            if ( ! strNote.empty() ) {
                SetAttribute("model_evidence", strNote);
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeGbKey(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    SetAttribute("gbkey", mapped_feat.GetData().GetKey());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeTranscriptId(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( mf.GetFeatType() != CSeqFeatData::e_Rna ) {
        return true;
    }
    const CSeq_feat::TQual& quals = mf.GetQual();
    for ( CSeq_feat::TQual::const_iterator cit = quals.begin(); 
      cit != quals.end(); ++cit ) {
        if ( (*cit)->GetQual() == "transcript_id" ) {
            SetAttribute("transcript_id", (*cit)->GetVal());
            return true;
        }
    }

    if ( mf.IsSetProduct() ) {
        string transcript_id;
        if (CWriteUtil::GetBestId(mf.GetProductId(), mf.GetScope(), transcript_id)) {
            SetAttribute("transcript_id", transcript_id);
            return true;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeProteinId(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( ! mf.IsSetProduct() ) {
        return true;
    }
    string protein_id;
    if (CWriteUtil::GetBestId(mf.GetProductId(), mf.GetScope(), protein_id)) {
        SetAttribute("protein_id", protein_id);
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeException(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( mf.IsSetExcept_text() ) {
        SetAttribute("exception", mf.GetExcept_text());
        return true;
    }
    if ( mf.IsSetExcept() ) {
        // what should I do?
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeNcrnaClass(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( !mf.IsSetData()  ||  
            (mf.GetData().GetSubtype() != CSeqFeatData::eSubtype_ncRNA) ) {
        return true;
    }
    const CSeqFeatData::TRna& rna = mf.GetData().GetRna();
    if ( !rna.IsSetExt() ) {
        return true;
    }
    const CRNA_ref::TExt& ext = rna.GetExt();
    if ( !ext.IsGen()  ||  !ext.GetGen().IsSetClass()) {
        return true;
    }
    SetAttribute("ncrna_class", ext.GetGen().GetClass());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeTranslationTable(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( !mf.IsSetData()  ||  
            mf.GetFeatSubtype() != CSeqFeatData::eSubtype_cdregion ) {
        return true;
    }
    const CSeqFeatData::TCdregion& cds = mf.GetData().GetCdregion();
    if ( !cds.IsSetCode() ) {
        return true;
    }
    int id = cds.GetCode().GetId();
    if ( id != 1  &&  id != 255 ) {
        SetAttribute("transl_table", NStr::IntToString(id));
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeCodeBreak(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( !mf.IsSetData()  ||  
            mf.GetFeatSubtype() != CSeqFeatData::eSubtype_cdregion ) {
        return true;
    }
    const CSeqFeatData::TCdregion& cds = mf.GetData().GetCdregion();
    if ( !cds.IsSetCode_break() ) {
        return true;
    }
    const list<CRef<CCode_break> >& code_breaks = cds.GetCode_break();
    list<CRef<CCode_break> >::const_iterator it = code_breaks.begin();
    for (; it != code_breaks.end(); ++it) {
        string cbString;
        if (CWriteUtil::GetCodeBreak(**it, cbString)) {
            SetAttribute("transl_except", cbString);
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeOldLocusTag(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( !mf.IsSetQual() ) {
        return true;
    }
    string old_locus_tags;
    vector<CRef<CGb_qual> > quals = mf.GetQual();
    for ( vector<CRef<CGb_qual> >::const_iterator it = quals.begin(); 
            it != quals.end(); ++it ) {
        if ( (**it).IsSetQual()  &&  (**it).IsSetVal() ) {
            string qual = (**it).GetQual();
            if ( qual != "old_locus_tag" ) {
                continue;
            }
            if ( !old_locus_tags.empty() ) {
                old_locus_tags += ",";
            }
            old_locus_tags += (**it).GetVal();
        }
    }
    if ( !old_locus_tags.empty() ) {
        SetAttribute("old_locus_tag", old_locus_tags);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeExonNumber(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (mf.IsSetQual()) {
        const CSeq_feat::TQual& quals = mf.GetQual();
        for ( CSeq_feat::TQual::const_iterator cit = quals.begin(); 
            cit != quals.end(); 
            ++cit ) {
            const CGb_qual& qual = **cit;
            if (qual.IsSetQual()  &&  qual.GetQual() == "number") {
                SetAttribute("exon_number", qual.GetVal());
                return true;
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeIsOrdered(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (CWriteUtil::IsLocationOrdered(mf.GetLocation())) {
        SetAttribute("is_ordered", "true");
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::AssignParent(
    const CGff3WriteRecordFeature& parent )
//  ----------------------------------------------------------------------------
{
    vector<string> parentId;
    if ( !parent.GetAttribute("ID", parentId) ) {
        cerr << "Fix me: Parent record without GFF3 ID tag!" << endl;
        return false;
    }
    DropAttribute("Parent");
    for (vector<string>::iterator it = parentId.begin(); it != parentId.end();
            ++it) {
        SetAttribute("Parent", *it);
    }
    return true;
}

//  ----------------------------------------------------------------------------
void CGff3WriteRecordFeature::ForceAttributeID(
    const string& strId )
//  ----------------------------------------------------------------------------
{
    DropAttribute("ID");
    SetAttribute("ID", strId);
}  

END_objects_SCOPE
END_NCBI_SCOPE

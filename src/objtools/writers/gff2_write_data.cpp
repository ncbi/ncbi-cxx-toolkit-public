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
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>

#include <objtools/writers/gff2_write_data.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
static const string s_GetSubtypeString( const COrgMod::TSubtype& subtype )
//  ----------------------------------------------------------------------------
{
    switch ( subtype ) {
        case COrgMod::eSubtype_strain:           return "strain";
        case COrgMod::eSubtype_substrain:        return "substrain";
        case COrgMod::eSubtype_type:             return "type";
        case COrgMod::eSubtype_subtype:          return "subtype";
        case COrgMod::eSubtype_variety:          return "variety";
        case COrgMod::eSubtype_serotype:         return "serotype";
        case COrgMod::eSubtype_serogroup:        return "serogroup";
        case COrgMod::eSubtype_serovar:          return "serovar";
        case COrgMod::eSubtype_cultivar:         return "cultivar";
        case COrgMod::eSubtype_pathovar:         return "pathovar";
        case COrgMod::eSubtype_chemovar:         return "chemovar";
        case COrgMod::eSubtype_biovar:           return "biovar";
        case COrgMod::eSubtype_biotype:          return "biotype";
        case COrgMod::eSubtype_group:            return "group";
        case COrgMod::eSubtype_subgroup:         return "subgroup";
        case COrgMod::eSubtype_isolate:          return "isolate";
        case COrgMod::eSubtype_common:           return "common";
        case COrgMod::eSubtype_acronym:          return "acronym";
        case COrgMod::eSubtype_dosage:           return "dosage";
        case COrgMod::eSubtype_nat_host:         return "nat_host";
        case COrgMod::eSubtype_sub_species:      return "sub_species";
        case COrgMod::eSubtype_specimen_voucher: return "specimen_voucher";
        case COrgMod::eSubtype_authority:        return "authority";
        case COrgMod::eSubtype_forma:            return "forma";
        case COrgMod::eSubtype_forma_specialis:  return "dosage";
        case COrgMod::eSubtype_ecotype:          return "ecotype";
        case COrgMod::eSubtype_synonym:          return "synonym";
        case COrgMod::eSubtype_anamorph:         return "anamorph";
        case COrgMod::eSubtype_teleomorph:       return "teleomorph";
        case COrgMod::eSubtype_breed:            return "breed";
        case COrgMod::eSubtype_gb_acronym:       return "gb_acronym";
        case COrgMod::eSubtype_gb_anamorph:      return "gb_anamorph";
        case COrgMod::eSubtype_gb_synonym:       return "gb_synonym";
        case COrgMod::eSubtype_old_lineage:      return "old_lineage";
        case COrgMod::eSubtype_old_name:         return "old_name";
        case COrgMod::eSubtype_culture_collection: return "culture_collection";
        case COrgMod::eSubtype_bio_material:     return "bio_material";
        case COrgMod::eSubtype_other:            return "note";
        default:                                 return "";
    }
    return "";
}

//  ----------------------------------------------------------------------------
static const string s_GetSubsourceString( const CSubSource::TSubtype& subtype )
//  ----------------------------------------------------------------------------
{
    switch ( subtype ) {
        case CSubSource::eSubtype_chromosome: return "chromosome";
        case CSubSource::eSubtype_map: return "map";
        case CSubSource::eSubtype_clone: return "clone";
        case CSubSource::eSubtype_subclone: return "subclone";
        case CSubSource::eSubtype_haplogroup: return "haplogroup";
        case CSubSource::eSubtype_haplotype: return "haplotype";
        case CSubSource::eSubtype_genotype: return "genotype";
        case CSubSource::eSubtype_sex: return "sex";
        case CSubSource::eSubtype_cell_line: return "cell_line";
        case CSubSource::eSubtype_cell_type: return "cell_type";
        case CSubSource::eSubtype_tissue_type: return "tissue_type";
        case CSubSource::eSubtype_clone_lib: return "clone_lib";
        case CSubSource::eSubtype_dev_stage: return "dev_stage";
        case CSubSource::eSubtype_frequency: return "frequency";
        case CSubSource::eSubtype_germline: return "germline";
        case CSubSource::eSubtype_rearranged: return "rearranged";
        case CSubSource::eSubtype_lab_host: return "lab_host";
        case CSubSource::eSubtype_pop_variant: return "pop_variant";
        case CSubSource::eSubtype_tissue_lib: return "tissue_lib";
        case CSubSource::eSubtype_plasmid_name: return "plasmid_name";
        case CSubSource::eSubtype_transposon_name: return "transposon_name";
        case CSubSource::eSubtype_insertion_seq_name: return "insertion_seq_name";
        case CSubSource::eSubtype_plastid_name: return "plastid_name";
        case CSubSource::eSubtype_country: return "country";
        case CSubSource::eSubtype_segment: return "segment";
        case CSubSource::eSubtype_endogenous_virus_name: return "endogenous_virus_name";
        case CSubSource::eSubtype_transgenic: return "transgenic";
        case CSubSource::eSubtype_environmental_sample: return "environmental_sample";
        case CSubSource::eSubtype_isolation_source: return "isolation_source";
        case CSubSource::eSubtype_other: return "note";
        default: return "";
    }
    return "";
}

//  ----------------------------------------------------------------------------
static const string s_GetBiomolString( const CMolInfo::TBiomol& biomol )
//  ----------------------------------------------------------------------------
{
    switch( biomol ) {
    default:
        return "";
    case CMolInfo::eBiomol_unknown:
        return "unassigned DNA";
    case CMolInfo::eBiomol_genomic:
        return "genomic DNA";
    case CMolInfo::eBiomol_pre_RNA:
        return "transcribed RNA";
    case CMolInfo::eBiomol_mRNA:
        return "mRNA";
    case CMolInfo::eBiomol_rRNA:
        return "rRNA";
    case CMolInfo::eBiomol_tRNA:
        return "tRNA";
    case CMolInfo::eBiomol_snRNA:
        return "transcribed RNA";
    case CMolInfo::eBiomol_scRNA:
        return "transcribed RNA";
    case CMolInfo::eBiomol_other_genetic:
        return "other DNA";
    case CMolInfo::eBiomol_genomic_mRNA:
        return "unassigned DNA";
    case CMolInfo::eBiomol_cRNA:
        return "viral cRNA";
    case CMolInfo::eBiomol_snoRNA:
        return "transcribed RNA";
    case CMolInfo::eBiomol_transcribed_RNA:
        return "transcribed RNA";
    case CMolInfo::eBiomol_ncRNA:
        return "transcribed RNA";
    case CMolInfo::eBiomol_tmRNA:
        return "unassigned RNA";
    case CMolInfo::eBiomol_other:
        return "other DNA";
    }
    return "";
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrId() const
//  ----------------------------------------------------------------------------
{
    return m_strId;
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrSource() const
//  ----------------------------------------------------------------------------
{
    return m_strSource;
}

//  ----------------------------------------------------------------------------
CGffWriteRecord::CGffWriteRecord(
    const string& id ):

    m_strId( "" ),
    m_uSeqStart( 0 ),
    m_uSeqStop( 0 ),
    m_strSource( "." ),
    m_strType( "." ),
    m_pdScore( 0 ),
    m_peStrand( 0 ),
    m_puPhase( 0 )
//  ----------------------------------------------------------------------------
{
    if (!id.empty()) {
        m_Attributes["ID"] = id;
    }
};

//  ----------------------------------------------------------------------------
CGffWriteRecord::CGffWriteRecord(
    const CGffWriteRecord& other ):
    m_strId( other.m_strId ),
    m_uSeqStart( other.m_uSeqStart ),
    m_uSeqStop( other.m_uSeqStop ),
    m_strSource( other.m_strSource ),
    m_strType( other.m_strType ),
    m_pdScore( 0 ),
    m_peStrand( 0 ),
    m_puPhase( 0 )
//  ----------------------------------------------------------------------------
{
    if ( other.m_pdScore ) {
        m_pdScore = new double( *(other.m_pdScore) );
    }
    if ( other.m_peStrand ) {
        m_peStrand = new ENa_strand( *(other.m_peStrand) );
    }
    if ( other.m_puPhase ) {
        m_puPhase = new unsigned int( *(other.m_puPhase) );
    }

    this->m_Attributes.insert( 
        other.m_Attributes.begin(), other.m_Attributes.end() );
};

//  ----------------------------------------------------------------------------
CGffWriteRecord::~CGffWriteRecord()
//  ----------------------------------------------------------------------------
{
    delete m_pdScore;
    delete m_peStrand;
    delete m_puPhase; 
};

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::GetAttribute(
    const string& strKey,
    string& strValue ) const
//  ----------------------------------------------------------------------------
{
    TAttrCit it = m_Attributes.find( strKey );
    if ( it == m_Attributes.end() ) {
        return false;
    }
    strValue = it->second;
    return true;
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrType() const
//  ----------------------------------------------------------------------------
{
    string strGffType;
    if ( GetAttribute( "gff_type", strGffType ) ) {
        return strGffType;
    }
    return m_strType;
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrSeqStart() const
//  ----------------------------------------------------------------------------
{
    return NStr::UIntToString( m_uSeqStart + 1 );;
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrSeqStop() const
//  ----------------------------------------------------------------------------
{
    return NStr::UIntToString( m_uSeqStop + 1 );
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrScore() const
//  ----------------------------------------------------------------------------
{
    if ( ! m_pdScore ) {
        return ".";
    }
    return NStr::DoubleToString( *m_pdScore );
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrStrand() const
//  ----------------------------------------------------------------------------
{
    if ( ! m_peStrand ) {
        return "+";
    }
    switch ( *m_peStrand ) {
    default:
        return "+";
    case eNa_strand_minus:
        return "-";
    }
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrPhase() const
//  ----------------------------------------------------------------------------
{
    if ( ! m_puPhase ) {
        return ".";
    }
    return NStr::UIntToString( *m_puPhase );
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::StrAttributes() const
//  ----------------------------------------------------------------------------
{
    string strAttributes;
	strAttributes.reserve(256);
    CGffWriteRecord::TAttributes attrs;
    attrs.insert( Attributes().begin(), Attributes().end() );
    CGffWriteRecord::TAttrIt it;

    for ( it = attrs.begin(); it != attrs.end(); ++it ) {
        string strKey = it->first;

        if ( ! strAttributes.empty() ) {
            strAttributes += "; ";
        }
        strAttributes += strKey;
        strAttributes += "=";
//        strAttributes += " ";
		
		bool quote = x_NeedsQuoting(it->second);
		if ( quote )
			strAttributes += '\"';		
		strAttributes += it->second;
		if ( quote )
			strAttributes += '\"';
    }
    if ( strAttributes.empty() ) {
        strAttributes = ".";
    }
    return strAttributes;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::x_NeedsQuoting(
    const string& str )
//  ----------------------------------------------------------------------------
{
    if( str.empty() )
		return true;

	for ( size_t u=0; u < str.length(); ++u ) {
        if ( str[u] == '\"' )
			return false;
		if ( str[u] == ' ' || str[u] == ';' || str[u] == ':' || str[u] == '=' ) {
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
void CGffWriteRecord::x_PriorityProcess(
    const string& strKey,
    map<string, string >& attrs,
    string& strAttributes ) const
//  ----------------------------------------------------------------------------
{
    string strKeyMod( strKey );

    map< string, string >::iterator it = attrs.find( strKeyMod );
    if ( it == attrs.end() ) {
        return;
    }

    // Some of the attributes are multivalue translating into multiple gff attributes
    //  all carrying the same key. These are special cased here:
    //
    if ( strKey == "Dbxref" ) {
        vector<string> tags;
        NStr::Tokenize( it->second, ";", tags );
        for ( vector<string>::iterator pTag = tags.begin(); pTag != tags.end(); pTag++ ) {
            if ( ! strAttributes.empty() ) {
                strAttributes += "; ";
            }
            strAttributes += strKeyMod;
            strAttributes += "=";
            string strValue = *pTag;
            strValue = x_Encode(strValue);
            if (x_NeedsQuoting(*pTag)) {
                strValue = (string("\"") + strValue + string("\""));
            }
            strAttributes += strValue;
        }
		attrs.erase(it);
        return;
    }

    // General case: Single value, make straight forward gff attribute:
    //
    if ( ! strAttributes.empty() ) {
        strAttributes += "; ";
    }

    strAttributes += strKeyMod;
    strAttributes += "=";
   	bool quote = x_NeedsQuoting(it->second);
	if ( quote )
		strAttributes += '\"';		
	strAttributes += x_Encode(it->second);
	attrs.erase(it);
	if ( quote )
		strAttributes += '\"';
}

//  ----------------------------------------------------------------------------
string CGffWriteRecord::x_Encode(
    const string& strRaw )
//  ----------------------------------------------------------------------------
{
    static const char s_Table[256][4] = {
        "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
        "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
        "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
        "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
        " ",   "!",   "%22", "%23", "$",   "%25", "%26", "%27",
        "%28", "%29", "%2A", "%2B", "%2C", "-",   ".",   "%2F",
        "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
        "8",   "9",   ":",   "%3B", "%3C", "%3D", "%3E", "%3F",
        "@",   "A",   "B",   "C",   "D",   "E",   "F",   "G",
        "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
        "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
        "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "^",   "_",
        "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
        "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
        "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
        "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F",
        "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
        "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
        "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
        "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
        "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
        "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
        "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
        "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
        "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
        "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
        "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
        "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
        "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
        "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
        "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
        "%F8", "%F9", "%FA", "%FB", "|", "%FD", "%FE", "%FF"
    };
    string strEncoded;
    for ( size_t i = 0;  i < strRaw.size();  ++i ) {
        strEncoded += s_Table[static_cast<unsigned char>( strRaw[i] )];
    }
    return strEncoded;
}
    
//  ----------------------------------------------------------------------------
bool CGffWriteRecord::CorrectLocation(
    const CSeq_interval& interval ) 
//  ----------------------------------------------------------------------------
{
    if ( interval.CanGetFrom() ) {
        m_uSeqStart = interval.GetFrom();
    }
    if ( interval.CanGetTo() ) {
        m_uSeqStop = interval.GetTo();
    }
    if ( interval.IsSetStrand() ) {
        if ( 0 == m_peStrand ) {
            m_peStrand = new ENa_strand( interval.GetStrand() );
        }
        else {
            *m_peStrand = interval.GetStrand();
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::CorrectPhase(
    int iPhase )
//  ----------------------------------------------------------------------------
{
    if ( 0 == m_puPhase ) {
        return false;
    }
    *m_puPhase = (3+iPhase)%3;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecord::AssignSequenceNumber(
    unsigned int uSequenceNumber,
    const string& strPrefix ) 
//  ----------------------------------------------------------------------------
{
    TAttrIt it = m_Attributes.find( "ID" );
    if ( it != m_Attributes.end() ) {
        it->second += string( "|" ) + strPrefix + NStr::UIntToString( uSequenceNumber );
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::AssignFromAsn(
    CMappedFeat mapped_feature )
//  ----------------------------------------------------------------------------
{
    if ( ! x_AssignType( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignSeqId( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignSource( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignStart( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignStop( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignScore( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignStrand( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignPhase( mapped_feature ) ) {
        return false;
    }
    if ( ! x_AssignAttributes( mapped_feature ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::AssignSource(
    CBioseq_Handle bsh,
    const CSeqdesc& desc )
//  ----------------------------------------------------------------------------
{
    if ( ! desc.IsSource() ) {
        return false;
    }
    const CBioSource& bs = desc.GetSource();

    // id
    CConstRef<CSeq_id> pId = bsh.GetNonLocalIdOrNull();
    if ( pId ) {
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle( *pId ); 
        CSeq_id_Handle best_idh = 
            sequence::GetId(idh, bsh.GetScope(), sequence::eGetId_Best); 
        if ( !best_idh ) {
            best_idh = idh;
        }
        best_idh.GetSeqId()->GetLabel(&m_strId, CSeq_id::eContent);
    }
    if ( m_strId.empty() ) {
        m_strId = "<unknown>";
    }

    // type
    m_strType = "region";

    // source
    const CSeq_id* pBigId = sequence::GetId( bsh, sequence::eGetId_Best).GetSeqId();
    switch ( pBigId->Which() ) {
        default:
            m_strSource = CSeq_id::SelectionName( pBigId->Which() );
            NStr::ToUpper( m_strSource );
            break;
        case CSeq_id::e_Local:
            m_strSource = "Local";
            break;
        case CSeq_id::e_Gibbsq:
        case CSeq_id::e_Gibbmt:
        case CSeq_id::e_Giim:
        case CSeq_id::e_Gi:
            m_strSource = "GenInfo";
            break;
        case CSeq_id::e_Genbank:
            m_strSource = "Genbank";
            break;
        case CSeq_id::e_Swissprot:
            m_strSource = "SwissProt";
            break;
        case CSeq_id::e_Patent:
            m_strSource = "Patent";
            break;
        case CSeq_id::e_Other:
            m_strSource = "RefSeq";
            break;
        case CSeq_id::e_General:
            m_strSource = pBigId->GetGeneral().GetDb();
            break;
    }

    // start
    m_uSeqStart = 0;

    // stop
    m_uSeqStop = bsh.GetBioseqLength() - 1;

    //score

    // strand
    if ( bsh.CanGetInst_Strand() ) {
        m_peStrand = new ENa_strand( eNa_strand_plus );
    }

    // phase

    //  attributes:
    m_Attributes["gbkey"] = "Source";


    CSeqdesc_CI md( bsh.GetParentEntry(), CSeqdesc::e_Molinfo, 0 );
    if ( md ) {
        const CMolInfo& molinfo = (*md).GetMolinfo();
        if ( molinfo.IsSetBiomol() ) {
            m_Attributes["mol_type"] = s_GetBiomolString( molinfo.GetBiomol() );
        }
    }

    if ( bs.IsSetTaxname() ) {
        m_Attributes["organism"] = bs.GetTaxname();
    }
    if ( bs.IsSetOrg() ) {
        const CBioSource::TOrg& org = bs.GetOrg();
        if ( org.IsSetDb() ) {
            const vector< CRef< CDbtag > >& tags = org.GetDb();
            string strAttr, strDb, strTag;
            for ( vector< CRef< CDbtag > >::const_iterator it = tags.begin(); 
                    it != tags.end(); ++it ) {
                if ( ! strAttr.empty() ) {
                    strAttr += ';';
                }
                if ((*it)->IsSetDb()) {
                    strAttr += ((*it)->GetDb() + ":");
                }
                if ((*it)->IsSetTag()) {
                    const CDbtag::TTag& tag = (*it)->GetTag();
                    strAttr +=
                        (tag.IsStr()) ? tag.GetStr() : NStr::UIntToString(tag.GetId());
                }
            }
            m_Attributes["Dbxref"] = strAttr;
        }
        if ( org.IsSetOrgname() && org.GetOrgname().IsSetMod() ) {
            const list<CRef<COrgMod> >& orgmods = org.GetOrgname().GetMod();
            for ( list<CRef<COrgMod> >::const_iterator it = orgmods.begin();
                    it != orgmods.end(); ++it ) {
                const COrgMod& mod = **it;
                if ( !mod.IsSetSubtype() || !mod.IsSetSubname() ) {
                    continue;
                }
                string key = s_GetSubtypeString( mod.GetSubtype() );
                if ( !key.empty() ) {
                    m_Attributes[ key ] = mod.GetSubname();
                }
                cerr << "";
            }
        }
    }
    if ( bs.IsSetSubtype() ) {
        const list<CRef<CSubSource> >& subsources = bs.GetSubtype();
        for ( list<CRef<CSubSource> >::const_iterator it = subsources.begin();
                it != subsources.end(); ++it ) {
            const CSubSource& subsource = **it;
            if ( !subsource.IsSetSubtype() || !subsource.IsSetName() ) {
                continue;
            }

            CSubSource::TSubtype subtype = subsource.GetSubtype();
            string key = s_GetSubsourceString( subsource.GetSubtype() );
            if ( key.empty() ) {
                continue;
            }
            switch ( subtype ) {
                default: {
                    if ( !key.empty() ) {
                        m_Attributes[ key ] = subsource.GetName();
                    }
                    continue;
                }
            case CSubSource::eSubtype_environmental_sample: {
                    m_Attributes[key] = "true";
                    continue;
                }
            }
        }
    }

    if ( bsh.IsSetInst_Topology() && 
            bsh.GetInst_Topology() == CSeq_inst::eTopology_circular ) {
       m_Attributes["Is_circular"] = "true";
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignSeqId(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mf.GetOriginalFeature();
    CSeq_annot_Handle ah = mf.GetAnnot();
    CConstRef<CSeq_annot> pA = ah.GetCompleteSeq_annot();
    if ( pA->IsSetDesc() ) {
    }

    if ( feature.CanGetLocation() ) {
    	const CSeq_loc& loc = feature.GetLocation();
	    CConstRef<CSeq_id> id(loc.GetId());
		if (id) {
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle( *id ); 
            CSeq_id_Handle best_idh = 
                sequence::GetId(idh, mf.GetScope(), sequence::eGetId_Best); 
            if ( !best_idh ) {
                best_idh = idh;
            }
            best_idh.GetSeqId()->GetLabel(&m_strId, CSeq_id::eContent);
		}
    }
    if (m_strId.empty() ) {
        m_strId = "<unknown>";
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignType(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();

    m_strType = "region";

    if ( feature.CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = feature.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "standard_name" ) {
                    m_strType = (*it)->GetVal();
                    return true;
                }
            }
            ++it;
        }
    }

    if ( ! feature.CanGetData() ) {
        return true;
    }

    switch ( feature.GetData().GetSubtype() ) {
    default:
        break;

    case CSeq_feat::TData::eSubtype_cdregion:
        m_strType = "CDS";
        break;

    case CSeq_feat::TData::eSubtype_exon:
        m_strType = "exon";
        break;

    case CSeq_feat::TData::eSubtype_misc_RNA:
        m_strType = "transcript";
        break;

    case CSeq_feat::TData::eSubtype_gene:
        m_strType = "gene";
        break;

    case CSeq_feat::TData::eSubtype_mRNA:
        m_strType = "mRNA";
        break;

    case CSeq_feat::TData::eSubtype_scRNA:
        m_strType = "scRNA";
        break;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignStart(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();

    if ( feature.CanGetLocation() ) {
        const CSeq_loc& location = feature.GetLocation();
        unsigned int uStart = location.GetStart( eExtreme_Positional );
        m_uSeqStart = uStart;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignStop(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();

    if ( feature.CanGetLocation() ) {
        const CSeq_loc& location = feature.GetLocation();
        unsigned int uEnd = location.GetStop( eExtreme_Positional );
        m_uSeqStop = uEnd;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignSource(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();

    m_strSource = ".";

    if ( feature.CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = feature.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "gff_source" ) {
                    m_strSource = (*it)->GetVal();
                    return true;
                }
            }
            ++it;
        }
    }
    const CSeq_id* pBigId = sequence::GetId( mapped_feat.GetScope().GetBioseqHandle(
            mapped_feat.GetLocationId()), sequence::eGetId_Best).GetSeqId();

    switch ( pBigId->Which() ) {
        default:
            m_strSource = CSeq_id::SelectionName( pBigId->Which() );
            NStr::ToUpper( m_strSource );
            return true;
        case CSeq_id::e_Local:
            m_strSource = "Local";
            return true;
        case CSeq_id::e_Gibbsq:
        case CSeq_id::e_Gibbmt:
        case CSeq_id::e_Giim:
        case CSeq_id::e_Gi:
            m_strSource = "GenInfo";
            return true;
        case CSeq_id::e_Genbank:
            m_strSource = "Genbank";
            return true;
        case CSeq_id::e_Swissprot:
            m_strSource = "SwissProt";
            return true;
        case CSeq_id::e_Patent:
            m_strSource = "Patent";
            return true;
        case CSeq_id::e_Other:
            m_strSource = "RefSeq";
            return true;
        case CSeq_id::e_General:
            m_strSource = pBigId->GetGeneral().GetDb();
            return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignScore(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();

    if ( feature.CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = feature.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "gff_score" ) {
                    m_pdScore = new double( NStr::StringToDouble( 
                        (*it)->GetVal() ) );
                    return true;
                }
            }
            ++it;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignStrand(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feat.GetOriginalFeature();

    if ( feature.CanGetLocation() ) {
        m_peStrand = new ENa_strand( feature.GetLocation().GetStrand() );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignPhase(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( mf.GetFeatSubtype() == CSeq_feat::TData::eSubtype_cdregion ) {
        m_puPhase = new unsigned int( 0 ); // will be corrected by external code
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriteRecordFeature::x_AssignAttributes(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    cerr << "FIXME: CGffWriteRecord::x_AssignAttributes" << endl;
    return true;
}

//  ----------------------------------------------------------------------------
string CGffWriteRecordFeature::x_FeatIdString(
    const CFeat_id& id )
//  ----------------------------------------------------------------------------
{
    switch ( id.Which() ) {
    default:
        break;

    case CFeat_id::e_Local: {
        const CFeat_id::TLocal& local = id.GetLocal();
        if ( local.IsId() ) {
            return NStr::IntToString( local.GetId() );
        }
        if ( local.IsStr() ) {
            return local.GetStr();
        }
        break;
        }
    }
    return "FEATID";
}

END_objects_SCOPE
END_NCBI_SCOPE

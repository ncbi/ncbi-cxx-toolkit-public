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
#include <objects/seq/so_map.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc_mix_.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Variation_ref.hpp>

#include <objtools/readers/read_util.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/gff2_data.hpp>
#include <objtools/readers/gff2_reader.hpp>
#include <objtools/readers/gff3_reader.hpp>

#include <objtools/readers/reader_message.hpp>
#include <util/compile_time.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


    CRef<CSeq_loc>
    s_GetCodeBreakSeqLocMix(const string& locString, const CSeq_id& id, ENa_strand strand)
{
    CRef<CSeq_loc> pLoc;
    list<string>   tokens;
    NStr::Split(locString, ",", tokens);

    if (tokens.empty()) {
        return pLoc;
    }

    pLoc      = Ref(new CSeq_loc());
    auto& mix = pLoc->SetMix();

    if (strand == eNa_strand_minus) {
        tokens.reverse(); // different ordering of seq-loc-mix components on minus strand
    }

    for (const auto& token : tokens) {
        auto divPos = token.find("..");
        if (divPos == string::npos) {
            auto point = NStr::StringToInt(token) - 1;
            mix.AddInterval(id, point, point, strand);
        } else {
            auto from = NStr::StringToInt(token.substr(0, divPos)) - 1;
            auto to   = NStr::StringToInt(token.substr(divPos + 2)) - 1;
            mix.AddInterval(id, from, to, strand);
        }
    }

    return pLoc;
}

//  ----------------------------------------------------------------------------
CRef<CCode_break> s_StringToCodeBreak(
    const string& str,
    CSeq_id& id,
    CGff2Record::TReaderFlags flags)
//  ----------------------------------------------------------------------------
{
    const string cdstr_start = "(pos:";
    const string cdstr_div = ",aa:";
    const string cdstr_end = ")";

    CRef<CCode_break> pCodeBreak;
    if (!NStr::StartsWith(str, cdstr_start)  ||  !NStr::EndsWith(str, cdstr_end)) {
        return pCodeBreak;
    }
    size_t pos_start = cdstr_start.length();
    size_t pos_stop = str.find(cdstr_div);
    string posstr = str.substr(pos_start, pos_stop-pos_start);
    string aaa = str.substr(pos_stop+cdstr_div.length());
    aaa = aaa.substr(0, aaa.length()-cdstr_end.length());

    const string posstr_compl = "complement(";
    ENa_strand strand = eNa_strand_plus;
    if (NStr::StartsWith(posstr, posstr_compl)) {
        posstr = posstr.substr(posstr_compl.length());
        posstr = posstr.substr(0, posstr.length()-1);
        strand = eNa_strand_minus;
    }


    if (NStr::StartsWith(posstr, "join(")) {
        posstr = posstr.substr(5); // strip "join("
        _ASSERT(posstr.back() == ')');
        posstr     = posstr.substr(0, posstr.length() - 1);
        auto pLoc  = s_GetCodeBreakSeqLocMix(posstr, id, strand);
        pCodeBreak = Ref(new CCode_break());
        pCodeBreak->SetLoc(*pLoc);
    } else {
        const string posstr_div = "..";
        size_t       pos_div    = posstr.find(posstr_div);
        if (pos_div == string::npos) {
            return pCodeBreak;
        }

        const auto from = NStr::StringToInt(posstr.substr(0, pos_div)) - 1;
        const auto to   = NStr::StringToInt(posstr.substr(pos_div + posstr_div.length())) - 1;

        pCodeBreak.Reset(new CCode_break);
        pCodeBreak->SetLoc().SetInt().SetId(id);
        pCodeBreak->SetLoc().SetInt().SetFrom(from);
        pCodeBreak->SetLoc().SetInt().SetTo(to);
        pCodeBreak->SetLoc().SetInt().SetStrand(strand);
    }

    MAKE_CONST_MAP(AminoAcidMap, ct::tagStrNocase, char,
    {
        {"Ala", 'A'},
        {"Asx", 'B'},
        {"Cys", 'C'},
        {"Asp", 'D'},
        {"Glu", 'E'},
        {"Phe", 'F'},
        {"Gly", 'G'},
        {"His", 'H'},
        {"Ile", 'I'},
        {"Xle", 'J'},  
        {"Lys", 'K'},
        {"Leu", 'L'},
        {"Met", 'M'},
        {"Asn", 'N'},
        {"Pyl", 'O'},
        {"Pro", 'P'},
        {"Gln", 'Q'},
        {"Arg", 'R'},
        {"Ser", 'S'},
        {"Thr", 'T'},
        {"Val", 'V'},
        {"Trp", 'W'},
        {"Sec", 'U'}, 
        {"Xxx", 'X'},
        {"Tyr", 'Y'},
        {"Glx", 'Z'},
        {"TERM", '*'}, 
        {"OTHER", 'X'}
    });

//    int aacode = 'U'; //for now

    auto it = AminoAcidMap.find(aaa);
    char aacode = (it != AminoAcidMap.end()) ? it->second : 'X';

    pCodeBreak->SetAa().SetNcbieaa(aacode);
    return pCodeBreak;
}

//  ----------------------------------------------------------------------------
CBioSource::EGenome s_StringToGenome(
    const string& genome,
    CGff2Record::TReaderFlags flags)
//  ----------------------------------------------------------------------------
{
    typedef map<string, CBioSource::EGenome> GENOME_MAP;
    static CSafeStatic<GENOME_MAP> s_GenomeMap;
    GENOME_MAP& sGenomeMap = *s_GenomeMap;
    if (sGenomeMap.empty()) {
        sGenomeMap["apicoplast"] = CBioSource::eGenome_apicoplast;
        sGenomeMap["chloroplast"] = CBioSource::eGenome_chloroplast;
        sGenomeMap["chromatophore"] = CBioSource::eGenome_chromatophore;
        sGenomeMap["chromoplast"] = CBioSource::eGenome_chromoplast;
        sGenomeMap["chromosome"] = CBioSource::eGenome_chromosome;
        sGenomeMap["cyanelle"] = CBioSource::eGenome_cyanelle;
        sGenomeMap["endogenous_virus"] = CBioSource::eGenome_endogenous_virus;
        sGenomeMap["extrachrom"] = CBioSource::eGenome_extrachrom;
        sGenomeMap["genomic"] = CBioSource::eGenome_genomic;
        sGenomeMap["hydrogenosome"] = CBioSource::eGenome_hydrogenosome;
        sGenomeMap["insertion_seq"] = CBioSource::eGenome_insertion_seq;
        sGenomeMap["kinetoplast"] = CBioSource::eGenome_kinetoplast;
        sGenomeMap["leucoplast"] = CBioSource::eGenome_leucoplast;
        sGenomeMap["macronuclear"] = CBioSource::eGenome_macronuclear;
        sGenomeMap["mitochondrion"] = CBioSource::eGenome_mitochondrion;
        sGenomeMap["nucleomorph"] = CBioSource::eGenome_nucleomorph;
        sGenomeMap["plasmid"] = CBioSource::eGenome_plasmid;
        sGenomeMap["plastid"] = CBioSource::eGenome_plastid;
        sGenomeMap["proplastid"] = CBioSource::eGenome_proplastid;
        sGenomeMap["proviral"] = CBioSource::eGenome_proviral;
        sGenomeMap["transposon"] = CBioSource::eGenome_transposon;
        sGenomeMap["virion"] = CBioSource::eGenome_virion;
    }
    GENOME_MAP::const_iterator cit = sGenomeMap.find(genome);
    if (cit != sGenomeMap.end()) {
        return cit->second;
    }
    return CBioSource::eGenome_unknown;
}

//  -----------------------------------------------------------------------------
void CGff2Record::TokenizeGFF(
    vector<CTempStringEx>& columns,
    const CTempStringEx& in_line)
//  -----------------------------------------------------------------------------
{
    columns.clear();
    columns.reserve(9);
    size_t index;
    // first try to split just using tabs
    NStr::Split(in_line, "\t", columns, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    if (columns.size() == 9)
        return;
    columns.clear();

    // better to be thread-safe static
    const CTempString space_tab_delim("\t ");
    const CTempString digits("0123456789");

    size_t current = 0;
    while (current != CTempStringEx::npos && columns.size()<8 &&
            (index = in_line.find_first_of(space_tab_delim, current)) != CTempStringEx::npos) {
        CTempStringEx next = in_line.substr(current, index-current);
        current = in_line.find_first_not_of(space_tab_delim, index);
        if (columns.size() == 5) {
            // reminder:
            // columns [3] and [4] are positions and must always be numeric
            // column [5] is a score (floating point or "." if absent)
            bool isNumericCol3 = columns[3].find_first_not_of(digits) == CTempStringEx::npos;
            bool isNumericCol4 = columns[4].find_first_not_of(digits) == CTempStringEx::npos;
            bool isNumericNext = next.find_first_not_of(digits) == CTempStringEx::npos;

            if (!isNumericCol3  &&  isNumericCol4  &&  isNumericNext) {
                // merge col2 with col3 and shift subsequent columns
                // operations with iterators are legal since all belong to the same memory space
                size_t sizeof_col1 = (columns[2].begin() + columns[2].size() - columns[1].begin());
                size_t startof_col1 = columns[1].begin() - in_line.begin();
                columns[1] = in_line.substr(startof_col1, sizeof_col1);
                columns[2] = columns[3];
                columns[3] = columns[4];
                columns[4] = next;
                continue;
            }
        }
        columns.push_back(next);
    }
    if (current != CTempStringEx::npos)
        columns.push_back(in_line.substr(current));
}
//  ----------------------------------------------------------------------------
bool CGff2Record::AssignFromGff(
    const string& strRawInput )
//  ----------------------------------------------------------------------------
{
    vector< CTempStringEx > columns;

    TokenizeGFF(columns, strRawInput);
    if ( columns.size() < 9 ) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Bad data line: not enough columns",
            ILineError::eProblem_FeatureBadStartAndOrStop) );
        pErr->Throw();
    }
    //  to do: more sanity checks

    columns[0].Copy(mSeqId, 0, CTempString::npos);
    columns[1].Copy(m_strSource, 0, CTempString::npos);
    columns[2].Copy(m_strType, 0, CTempString::npos);
    m_strNormalizedType = m_strType;
    NStr::ToLower(m_strNormalizedType);

    try {
        m_uSeqStart = NStr::StringToUInt( columns[3] ) - 1;
        m_uSeqStop = NStr::StringToUInt( columns[4] ) - 1;
    }
    catch (const CException&) {
        string message = "Bad data line: Both \"start\" and \"stop\" must be positive integers.";
        CReaderMessage error(
            eDiag_Error,
            0,
            message);
        throw error;
    }
    if (m_uSeqStop < m_uSeqStart) {
        string message = "Bad data line: location start is greater than location stop (start="
            + string(columns[3]) + ", stop=" + string(columns[4]) + ").";
        CReaderMessage error(
            eDiag_Error,
            0,
            message);
        throw error;
    }

    if ( columns[5] != "." && columns[5] != "NA" ) {
        m_pdScore = new double( NStr::StringToDouble( columns[5], NStr::fAllowLeadingSpaces ) );
    }

    enum ENa_strand strand;
    switch (columns[6][0]) {
    default:
        strand = objects::eNa_strand_unknown;
        break;
    case '+':
        strand = objects::eNa_strand_plus;
        break;
    case '-':
        strand = objects::eNa_strand_minus;
        break;
    case '.':
        strand = objects::eNa_strand_both;
        break;
    }
    m_peStrand = new ENa_strand(strand);


    enum CCdregion::EFrame frame = CCdregion::eFrame_not_set;

    if ( columns[7] == "0" ) {
        frame = CCdregion::eFrame_one;
    }
    else
    if ( columns[7] == "1" ) {
        frame = CCdregion::eFrame_two;
    }
    else
    if ( columns[7] == "2" ) {
        frame = CCdregion::eFrame_three;
    }

    if (frame != CCdregion::eFrame_not_set)
        m_pePhase = new TFrame(frame);

    columns[8].Copy(m_strAttributes, 0, CTempString::npos);

    return xAssignAttributesFromGff(m_strType, columns[8]);
}

//  ----------------------------------------------------------------------------
bool CGff2Record::GetAttribute(
    const string& strKey,
    string& strValue ) const
//  ----------------------------------------------------------------------------
{
    TAttrCit it = m_Attributes.find( strKey );
    if ( it == m_Attributes.end() ) {
        strValue.clear();
        return false;
    }
    strValue = it->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Record::GetAttribute(
    const string& strKey,
    list<string>& values ) const
//  ----------------------------------------------------------------------------
{
    values.clear();
    TAttrCit it = m_Attributes.find(strKey);
    if (it == m_Attributes.end()) {
        return false;
    }
    NStr::Split(it->second, ",", values, 0);
    return !values.empty();
}

//  ----------------------------------------------------------------------------
string CGff2Record::xNormalizedAttributeKey(
    const CTempString& strRawKey )
//  ----------------------------------------------------------------------------
{
    return NStr::TruncateSpaces_Unsafe(strRawKey);
}

//  ----------------------------------------------------------------------------
string CGff2Record::xNormalizedAttributeValue(
    const CTempString& strRawValue )
//  ----------------------------------------------------------------------------
{
    CTempString strValue = NStr::TruncateSpaces_Unsafe(strRawValue);
    if ( NStr::StartsWith( strValue, "\"" ) ) {
        strValue = strValue.substr( 1, string::npos );
    }
    if ( NStr::EndsWith( strValue, "\"" ) ) {
        strValue = strValue.substr( 0, strValue.length() - 1 );
    }
    return NStr::URLDecode(strValue, NStr::eUrlDec_Percent);
}


//  ----------------------------------------------------------------------------
CTempString x_GetNextAttribute(CTempString& input)
{
    CTempString result;
    bool inQuotes = false;
    size_t i = 0;
    for (; i < input.length(); i++)
    {
        if (inQuotes) {
            if (input[i] == '\"') {
                inQuotes = false;
            }
        }
        else { // not in quotes
            if (input[i] == ';') {
                result = NStr::TruncateSpaces_Unsafe(input.substr(0, i));
                if (!result.empty())
                {
                    input = input.substr(i+1);
                    return result;
                }
            }
            else {
                if (input[i] == '\"') {
                    inQuotes = true;
                }
            }
        }
    }
    result = NStr::TruncateSpaces_Unsafe(input);
    input.clear();
    return result;
}

bool x_GetNextAttribute(CTempString& input, CTempString& key, CTempString& value)
{
    size_t semicolon = CTempString::npos;
    size_t space = CTempString::npos;
    size_t equal = CTempString::npos;

    key.clear();
    value.clear();

    bool inQuotes = false;
    size_t i = 0;
    while (input[i] == ';') {
        ++i;
    }
    input = input.substr(i);
    for (i=0; i < input.length(); i++)
    {
        if (inQuotes) {
            if (input[i] == '\"') {
                inQuotes = false;
            }
        }
        else { // not in quotes
            switch (input[i])
            {
            case ';':
                semicolon = i;
                break;
            case ' ':
                if (space == CTempString::npos && equal == CTempString::npos)
                    space = i;
                continue;
            case '"':
                inQuotes = true;
                continue;
            case '=':
                if (equal == CTempString::npos)
                    equal = i;
                continue;
            default:
                continue;
            }
            break;
        }
    }

    if (semicolon == CTempString::npos)
        semicolon = input.length();

    if (equal == CTempString::npos)
        equal = min(space, semicolon);

    key = NStr::TruncateSpaces_Unsafe(input.substr(0, equal));
    value = NStr::TruncateSpaces_Unsafe(input.substr(equal + 1, semicolon - equal - 1));

    input = NStr::TruncateSpaces_Unsafe(input.substr(semicolon+1));

    return !key.empty();
}

bool CGff2Record::xAssignAttributesFromGff(
    const string& strType,
    const string& strRawAttributes )
//  ----------------------------------------------------------------------------
{
    m_Attributes.clear();
    CTempString input(strRawAttributes);
    CTempString next;
    CTempString strKey;
    CTempString strValue;

    while (!input.empty() && x_GetNextAttribute(input, strKey, strValue))
    {
        m_Attributes[strKey] = strValue;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Record::xSplitGffAttributes(
    const string& strRawAttributes,
    vector< string >& attributes) const
//  ----------------------------------------------------------------------------
{
    string strCurrAttrib;
    bool inQuotes = false;

    ITERATE (string, iterChar, strRawAttributes) {
        if (inQuotes) {
            if (*iterChar == '\"') {
                inQuotes = false;
            }
            strCurrAttrib += *iterChar;
        } else { // not in quotes
            if (*iterChar == ';') {
                NStr::TruncateSpacesInPlace( strCurrAttrib );
                if(!strCurrAttrib.empty())
                    attributes.push_back(strCurrAttrib);
                strCurrAttrib.clear();
            } else {
                if(*iterChar == '\"') {
                    inQuotes = true;
                }
                strCurrAttrib += *iterChar;
            }
        }
    }

    NStr::TruncateSpacesInPlace( strCurrAttrib );
    if (!strCurrAttrib.empty())
        attributes.push_back(strCurrAttrib);

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Record::InitializeFeature(
    TReaderFlags flags,
    CRef<CSeq_feat> pFeature,
    SeqIdResolver seqidresolve ) const
    //  ----------------------------------------------------------------------------
{
    if (!CGffBaseColumns::InitializeFeature(flags, pFeature, seqidresolve)) {
        return false;
    }
    return xMigrateAttributes(flags, pFeature);
}

//  ----------------------------------------------------------------------------
bool CGff2Record::UpdateFeature(
    TReaderFlags flags,
    CRef<CSeq_feat> pFeature,
    SeqIdResolver seqidresolve ) const
    //  ----------------------------------------------------------------------------
{
    auto subtype = pFeature->GetData().GetSubtype();
    auto recType = NormalizedType();
    CRef<CSeq_loc> pAddLoc = GetSeqLoc(flags, seqidresolve);

    pFeature->SetLocation().SetMix().AddSeqLoc(*pAddLoc);
    if (!xUpdateFeatureData(flags, pFeature)) {
        return false;
    }
    if (subtype == CSeqFeatData::eSubtype_cdregion  &&  recType == "cds") {
        string cdsId;
        GetAttribute("ID", cdsId);
        if (!cdsId.empty()) {
            pFeature->AddOrReplaceQualifier("ID", cdsId);
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Record::xUpdateFeatureData(
    TReaderFlags flags,
    CRef<CSeq_feat> pFeature,
    SeqIdResolver seqidresolve) const
    //  ----------------------------------------------------------------------------
{
    const CSeq_loc& target = pFeature->GetLocation();
    CSeqFeatData::ESubtype subtype = pFeature->GetData().GetSubtype();
    CRef<CSeq_loc> pAddLoc = GetSeqLoc(flags, seqidresolve);

    switch(subtype) {
        default: {
            return true;
        }
        case CSeqFeatData::eSubtype_cdregion: {
            // if incoming piece is new feature start then it will provide the
            //  frame
            //
            if (!pAddLoc->GetInt().CanGetStrand()) {
                return true;
            }
            if (pAddLoc->GetInt().GetStrand() == eNa_strand_plus) {
                size_t curStart = target.GetStart(eExtreme_Positional);
                size_t newStart = pAddLoc->GetStart(eExtreme_Positional);
                if (curStart == newStart) {
                    pFeature->SetData().SetCdregion().SetFrame(Phase());
                }
                return true;
            }
            if (pAddLoc->GetInt().GetStrand() == eNa_strand_minus) {
                size_t curStop = target.GetStop(eExtreme_Positional);
                size_t newStop = pAddLoc->GetStop(eExtreme_Positional);
                if (curStop == newStop) {
                    pFeature->SetData().SetCdregion().SetFrame(Phase());
                }
                return true;
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Record::xMigrateAttributes(
    TReaderFlags flags,
    CRef<CSeq_feat> pFeature ) const
//  ----------------------------------------------------------------------------
{
    TAttributes attrs_left(m_Attributes.begin(), m_Attributes.end());
    TAttrIt it;

    it = attrs_left.find("Note");
    if (it != attrs_left.end()) {
        pFeature->SetComment(xNormalizedAttributeValue(it->second));
        attrs_left.erase(it);
    }

    it = attrs_left.find("Dbxref");
    if (it != attrs_left.end()) {
        vector<string> dbxrefs;
        NStr::Split(it->second, ",", dbxrefs, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
        for (vector<string>::iterator it1 = dbxrefs.begin(); it1 != dbxrefs.end();
                ++it1 ) {
            string dbtag = xNormalizedAttributeValue(*it1);
            pFeature->SetDbxref().push_back(CGff2Reader::x_ParseDbtag(dbtag));
        }
        attrs_left.erase(it);
    }

    it = attrs_left.find("Is_circular");
    if (it != attrs_left.end()) {
        if (pFeature->GetData().IsBiosrc()) {
            CRef<CSubSource> pSubSource(new CSubSource);
            pSubSource->SetSubtype(CSubSource::eSubtype_other);
            pSubSource->SetName("is_circular");
            pFeature->SetData().SetBiosrc().SetSubtype().push_back(pSubSource);
        }
    }

    it = attrs_left.find("Parent");
    if (it != attrs_left.end()) {
        if (Type() != "CDS") {
            xMigrateAttributeSingle(
                attrs_left, "Parent", pFeature, "Parent", flags);
        }
        else attrs_left.erase(it);
    }

    it = attrs_left.find("Name");
    if (it != attrs_left.end()) {
        auto soType = NormalizedType();
        string gbKey;
        GetAttribute("gbkey", gbKey);
        if (soType == "cds"  ||  soType == "mrna"  ||  soType == "biological_region"  ||
                (soType == "region"  &&  gbKey != "Src")) {
            attrs_left.erase(it);
        }
    }

    it = attrs_left.find("codon_start");
    if (it != attrs_left.end()) {
        if (pFeature->GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion) {
            int codon_start = NStr::StringToInt(it->second);
            switch(codon_start) {
                default:
                    break;
                case 1:
                    pFeature->SetData().SetCdregion().SetFrame(CCdregion::eFrame_one);
                    break;
                case 2:
                    pFeature->SetData().SetCdregion().SetFrame(CCdregion::eFrame_two);
                    break;
                case 3:
                    pFeature->SetData().SetCdregion().SetFrame(CCdregion::eFrame_three);
                    break;
            }
            attrs_left.erase(it);
        }
    }

    it = attrs_left.find("description");
    if (it != attrs_left.end()) {
        if (pFeature->GetData().IsGene()) {
            string description = xNormalizedAttributeValue(it->second);
            pFeature->SetData().SetGene().SetDesc(description);
            attrs_left.erase(it);
        }
    }

    it = attrs_left.find("exception");
    if (it != attrs_left.end()) {
        pFeature->SetExcept(true);
        pFeature->SetExcept_text(xNormalizedAttributeValue(it->second));
        attrs_left.erase(it);
    }

    it = attrs_left.find("exon_number");
    if (it != attrs_left.end()) {
        CRef<CGb_qual> pQual( new CGb_qual);
        pQual->SetQual("number");
        pQual->SetVal(it->second);
        pFeature->SetQual().push_back(pQual);
        attrs_left.erase(it);
    }

    it = attrs_left.find("experiment");
    if (it != attrs_left.end()) {
        const string strExperimentDefault(
            "experimental evidence, no additional details recorded" );
        string value = xNormalizedAttributeValue(it->second);
        if (value == strExperimentDefault) {
            pFeature->SetExp_ev(CSeq_feat::eExp_ev_experimental);
        }
        else {
            CRef<CGb_qual> pQual(new CGb_qual);
            pQual->SetQual("experiment");
            pQual->SetVal(value);
            pFeature->SetQual().push_back(pQual);
        }
        attrs_left.erase(it);
    }

    it = attrs_left.find("gbkey");
    if (it != attrs_left.end()) {
        attrs_left.erase(it); //ignore
    }

    it = attrs_left.find("gene");
    if (it != attrs_left.end()) {
        if (pFeature->GetData().IsGene()) {
            list<string> geneValues;
            NStr::Split(it->second, ",", geneValues, 0);
            string value;
            list<string>::const_iterator cit = geneValues.begin();
            if (cit != geneValues.end()) {
                value = xNormalizedAttributeValue(*cit);
                pFeature->SetData().SetGene().SetLocus(value);
                cit++;
                while (cit != geneValues.end()) {
                    value = xNormalizedAttributeValue(*cit);
                    pFeature->SetData().SetGene().SetSyn().push_back(value);
                    cit++;
                }
            }
            attrs_left.erase(it);
        }
    }

    it = attrs_left.find("genome");
    if (it != attrs_left.end()) {
        if (pFeature->GetData().IsBiosrc()) {
            pFeature->SetData().SetBiosrc().SetGenome(
                s_StringToGenome(it->second, flags));
            attrs_left.erase(it);
        }
    }

    it = attrs_left.find("gene_synonym");
    if (it != attrs_left.end()) {
        if (pFeature->GetData().IsGene()) {
            vector<string> synonyms;
            NStr::Split(it->second, ",", synonyms, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
            for (vector<string>::iterator it1 = synonyms.begin(); it1 != synonyms.end();
                    ++it1 ) {
                string synonym = xNormalizedAttributeValue(*it1);
                pFeature->SetData().SetGene().SetSyn().push_back(synonym);
            }
        }
        attrs_left.erase(it);
    }

    it = attrs_left.find("inference");
    if (it != attrs_left.end()) {
        auto inferenceVal = it->second;
        const string strInferenceDefault(
            "non-experimental evidence, no additional details recorded" );
       auto value = xNormalizedAttributeValue(inferenceVal);
        if (value == strInferenceDefault) {
            pFeature->SetExp_ev(CSeq_feat::eExp_ev_not_experimental);
        }
        else {
            vector<string> inferenceVals;
            NStr::Split(inferenceVal, ",", inferenceVals);
            for (auto val: inferenceVals) {
                CRef<CGb_qual> pQual(new CGb_qual);
                pQual->SetQual("inference");
                pQual->SetVal(xNormalizedAttributeValue(val));
                pFeature->SetQual().push_back(pQual);
            }
        }
        attrs_left.erase(it);
    }

    it = attrs_left.find("locus_tag");
    if (it != attrs_left.end()) {
        if (pFeature->GetData().IsGene()) {
            string tag = xNormalizedAttributeValue(it->second);
            pFeature->SetData().SetGene().SetLocus_tag(tag);
        }
        attrs_left.erase(it);
    }

    it = attrs_left.find("map");
    if (it != attrs_left.end()) {
        if (pFeature->GetData().IsGene()) {
            pFeature->SetData().SetGene().SetMaploc(
                xNormalizedAttributeValue(it->second));
        }
    }

    it = attrs_left.find("ncrna_class");
    if (it != attrs_left.end()) {
        if (pFeature->GetData().GetSubtype() == CSeqFeatData::eSubtype_ncRNA) {
            pFeature->SetData().SetRna().SetExt().SetGen().SetClass(
                xNormalizedAttributeValue(it->second));
        }
        attrs_left.erase(it);
    }

    it = attrs_left.find("partial");
    if (it != attrs_left.end()) {
        if (!(flags & CGff2Reader::fGenbankMode)) {
            pFeature->AddQualifier("partial", it->second);
        }
        attrs_left.erase(it);
    }

    it = attrs_left.find("pseudo");
    if (it != attrs_left.end()) {
        pFeature->SetPseudo(true);
        attrs_left.erase(it);
    }

    it = attrs_left.find("regulatory_class");
    if (it != attrs_left.end()) {
        if (pFeature->GetData().IsImp()  &&  (pFeature->GetData().GetImp().GetKey()  == "regulatory")) {
            //pFeature->SetData().SetImp().SetKey(it->second);
            pFeature->RemoveQualifier("regulatory_class");
            pFeature->AddQualifier("regulatory_class", it->second);
            attrs_left.erase(it);
        }
        //pFeature->RemoveQualifier("regulatory_class");
    }

    it = attrs_left.find("rpt_type");
    if (it != attrs_left.end()) {
        map<string, string> satellites = {
            {"microsatellite", "microsatellite"},
            {"minisatellite", "minisatellite"},
            {"satellite_DNA", "satellite"},
            {"satellite", "satellite"},
        };
        pFeature->RemoveQualifier("rpt_type");
        pFeature->RemoveQualifier("satellite");
        auto rpt_type = it->second;
        auto satelliteIt = satellites.find(rpt_type);
        if (satelliteIt != satellites.end()) {
            pFeature->AddQualifier("satellite", satelliteIt->second);
        }
        else {
            pFeature->AddQualifier("rpt_type", rpt_type);
        }
        attrs_left.erase(it);
    }

    it = attrs_left.find("satellite");
    if (it != attrs_left.end()) {
        if (pFeature->GetData().IsImp()  &&  pFeature->GetData().GetImp().GetKey() == "repeat_region") {
            attrs_left.erase(it);
        }
    }

    it = attrs_left.find("transl_except");
    if (it != attrs_left.end()) {
        if (pFeature->GetData().IsCdregion()) {
            vector<string> codebreaks;
            NStr::Split(it->second, ",", codebreaks, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
            for (vector<string>::iterator it1 = codebreaks.begin();
                    it1 != codebreaks.end(); ++it1 ) {
                string breakData = xNormalizedAttributeValue(*it1);
                CRef<CSeq_id> pBreakId = GetSeqId(flags);
                CRef<CCode_break> pCodeBreak;
                try {
                    pCodeBreak = s_StringToCodeBreak(breakData, *pBreakId, flags);
                } catch (...) {
                    string         message = "Unable to parse code-break : \"" + it->second + "\"";
                    CReaderMessage error(eDiag_Error, 0, message);
                    throw error;
                }
                if (pCodeBreak) {
                    pFeature->SetData().SetCdregion().SetCode_break().push_back(
                        pCodeBreak);
                }
            }
            attrs_left.erase(it);
        }
    }

    it = attrs_left.find("transl_table");
    if (it != attrs_left.end()) {
        if (pFeature->GetData().IsCdregion()) {
            CRef<CGenetic_code::C_E> pCe(new CGenetic_code::C_E) ;
            pCe->SetId(NStr::StringToInt(it->second));
            pFeature->SetData().SetCdregion().SetCode().Set().push_back(pCe);
            attrs_left.erase(it);
        }
    }

    if (!xMigrateAttributesGo(flags, pFeature, attrs_left)) {
        return false;
    }

    if (pFeature->GetData().IsBiosrc()) {
        if (!xMigrateAttributesSubSource(flags, pFeature, attrs_left)) {
            return false;
        }
        if (!xMigrateAttributesOrgName(flags, pFeature, attrs_left)) {
            return false;
        }
    }

    //
    //  Turn whatever is left into a gbqual:
    //
    while (!attrs_left.empty()) {
        const string& key = attrs_left.begin()->first;
        if (!xMigrateAttributeDefault(attrs_left, key, pFeature, key, flags)) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Record::xMigrateAttributeSingle(
    TAttributes& attributes,
    const string& attrKey,
    CRef<CSeq_feat> pFeature,
    const string& qualKey,
    TReaderFlags flags)
    //  -----------------------------------------------------------------------------
{
    //retrieve GFF3 attribute as a single value,
    //  turn unescaped value into gbqual of the same key.

    TAttributes::iterator it = attributes.find(attrKey);
    if (it == attributes.end()) {
        return true;
    }
    string value = xNormalizedAttributeValue(it->second);
    pFeature->AddQualifier(qualKey, value);
    attributes.erase(it);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Record::xMigrateAttributeDefault(
    TAttributes& attributes,
    const string& attrKey,
    CRef<CSeq_feat> pFeature,
    const string& qualKey,
    TReaderFlags flags)
    //  -----------------------------------------------------------------------------
{
    //split GFF3 multi-value into individual values, create a gbqual of the
    // same key for each of the individual values.
    //
    TAttributes::iterator it = attributes.find(attrKey);
    if (it == attributes.end()) {
        return true;
    }
    list<CTempStringEx> values;
    NStr::Split(it->second, ",", values, 0);
    for (list<CTempStringEx>::const_iterator cit = values.begin(); cit != values.end();
            cit++) {
        if (cit->empty()) {
            continue;
        }
        string value = xNormalizedAttributeValue(*cit);
        pFeature->AddQualifier(qualKey, value);
    }
    attributes.erase(it);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Record::xMigrateAttributesOrgName(
    TReaderFlags flags,
    CRef<CSeq_feat> pFeature,
    TAttributes& attrs_left) const
//  ----------------------------------------------------------------------------
{
    typedef map<string, COrgMod::ESubtype> ORGMOD_MAP;
    static CSafeStatic<ORGMOD_MAP> s_OrgModMap;
    ORGMOD_MAP& sOrgModMap = *s_OrgModMap;
    if (sOrgModMap.empty()) {
        sOrgModMap["strain"] = COrgMod::eSubtype_strain;
        sOrgModMap["substrain"] = COrgMod::eSubtype_substrain;
        sOrgModMap["type"] = COrgMod::eSubtype_type;
        sOrgModMap["subtype"] = COrgMod::eSubtype_subtype;
        sOrgModMap["variety"] = COrgMod::eSubtype_variety;
        sOrgModMap["serotype"] = COrgMod::eSubtype_serotype;
        sOrgModMap["serogroup"] = COrgMod::eSubtype_serogroup;
        sOrgModMap["serovar"] = COrgMod::eSubtype_serovar;
        sOrgModMap["cultivar"] = COrgMod::eSubtype_cultivar;
        sOrgModMap["pathovar"] = COrgMod::eSubtype_pathovar;
        sOrgModMap["chemovar"] = COrgMod::eSubtype_chemovar;
        sOrgModMap["biovar"] = COrgMod::eSubtype_biovar;
        sOrgModMap["biotype"] = COrgMod::eSubtype_biotype;
        sOrgModMap["group"] = COrgMod::eSubtype_group;
        sOrgModMap["subgroup"] = COrgMod::eSubtype_subgroup;
        sOrgModMap["isolate"] = COrgMod::eSubtype_isolate;
        sOrgModMap["common"] = COrgMod::eSubtype_common;
        sOrgModMap["acronym"] = COrgMod::eSubtype_acronym;
        sOrgModMap["dosage"] = COrgMod::eSubtype_dosage;
        sOrgModMap["nat_host"] = COrgMod::eSubtype_nat_host;
        sOrgModMap["sub_species"] = COrgMod::eSubtype_sub_species;
        sOrgModMap["specimen_voucher"] = COrgMod::eSubtype_specimen_voucher;
        sOrgModMap["authority"] = COrgMod::eSubtype_authority;
        sOrgModMap["forma"] = COrgMod::eSubtype_forma;
        sOrgModMap["dosage"] = COrgMod::eSubtype_forma_specialis;
        sOrgModMap["ecotype"] = COrgMod::eSubtype_ecotype;
        sOrgModMap["synonym"] = COrgMod::eSubtype_synonym;
        sOrgModMap["anamorph"] = COrgMod::eSubtype_anamorph;
        sOrgModMap["teleomorph"] = COrgMod::eSubtype_teleomorph;
        sOrgModMap["breed"] = COrgMod::eSubtype_breed;
        sOrgModMap["gb_acronym"] = COrgMod::eSubtype_gb_acronym;
        sOrgModMap["gb_anamorph"] = COrgMod::eSubtype_gb_anamorph;
        sOrgModMap["gb_synonym"] = COrgMod::eSubtype_gb_synonym;
        sOrgModMap["old_lineage"] = COrgMod::eSubtype_old_lineage;
        sOrgModMap["old_name"] = COrgMod::eSubtype_old_name;
        sOrgModMap["culture_collection"] = COrgMod::eSubtype_culture_collection;
        sOrgModMap["bio_material"] = COrgMod::eSubtype_bio_material;
        sOrgModMap["note"] = COrgMod::eSubtype_other;
    }
    list<CRef<COrgMod> >& orgMod =
        pFeature->SetData().SetBiosrc().SetOrg().SetOrgname().SetMod();
    for ( ORGMOD_MAP::const_iterator sit = sOrgModMap.begin();
            sit != sOrgModMap.end(); ++sit) {
        TAttributes::iterator ait = attrs_left.find(sit->first);
        if (ait == attrs_left.end()) {
            continue;
        }
        CRef<COrgMod> pOrgMod(new COrgMod);
        pOrgMod->SetSubtype(sit->second);
        pOrgMod->SetSubname(ait->second);
        orgMod.push_back(pOrgMod);
        attrs_left.erase(ait);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Record::xMigrateAttributesGo(
    TReaderFlags flags,
    CRef<CSeq_feat> pFeature,
    TAttributes& attrs) const
//  ----------------------------------------------------------------------------
{
    for (auto it = attrs.begin(); it != attrs.end(); /**/) {
        if (NStr::StartsWith(it->first, "go_")) {
            try {
                CReadUtil::AddGeneOntologyTerm(*pFeature, it->first, it->second);
            }
            catch(ILineError&) {
            }
            it = attrs.erase(it);
        }
        else {
            it++;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Record::xMigrateAttributesSubSource(
    TReaderFlags flags,
    CRef<CSeq_feat> pFeature,
    TAttributes& attrs_left) const
//  ----------------------------------------------------------------------------
{
    typedef map<string, CSubSource::ESubtype> SUBSOURCE_MAP;
    static CSafeStatic<SUBSOURCE_MAP> s_SubSourceMap;
    SUBSOURCE_MAP& sSubSourceMap = *s_SubSourceMap;
    if (sSubSourceMap.empty()) {
        sSubSourceMap["chromosome"] = CSubSource::eSubtype_chromosome;
        sSubSourceMap["map"] = CSubSource::eSubtype_map;
        sSubSourceMap["clone"] = CSubSource::eSubtype_clone;
        sSubSourceMap["subclone"] = CSubSource::eSubtype_subclone;
        sSubSourceMap["haplotype"] = CSubSource::eSubtype_haplotype;
        sSubSourceMap["genotype"] = CSubSource::eSubtype_genotype;
        sSubSourceMap["sex"] = CSubSource::eSubtype_sex;
        sSubSourceMap["cell_line"] = CSubSource::eSubtype_cell_line;
        sSubSourceMap["cell_type"] = CSubSource::eSubtype_cell_type;
        sSubSourceMap["tissue_type"] = CSubSource::eSubtype_tissue_type;
        sSubSourceMap["clone_lib"] = CSubSource::eSubtype_clone_lib;
        sSubSourceMap["dev_stage"] = CSubSource::eSubtype_dev_stage;
        sSubSourceMap["frequency"] = CSubSource::eSubtype_frequency;
        sSubSourceMap["germline"] = CSubSource::eSubtype_germline;
        sSubSourceMap["rearranged"] = CSubSource::eSubtype_rearranged;
        sSubSourceMap["lab_host"] = CSubSource::eSubtype_lab_host;
        sSubSourceMap["pop_variant"] = CSubSource::eSubtype_pop_variant;
        sSubSourceMap["tissue_lib"] = CSubSource::eSubtype_tissue_lib;
        sSubSourceMap["plasmid_name"] = CSubSource::eSubtype_plasmid_name;
        sSubSourceMap["transposon_name"] = CSubSource::eSubtype_transposon_name;
        sSubSourceMap["insertion_seq_name"] = CSubSource::eSubtype_insertion_seq_name;
        sSubSourceMap["plastid_name"] = CSubSource::eSubtype_plastid_name;
        sSubSourceMap["country"] = CSubSource::eSubtype_country;
        sSubSourceMap["segment"] = CSubSource::eSubtype_segment;
        sSubSourceMap["endogenous_virus_name"] = CSubSource::eSubtype_endogenous_virus_name;
        sSubSourceMap["transgenic"] = CSubSource::eSubtype_transgenic;
        sSubSourceMap["environmental_sample"] = CSubSource::eSubtype_environmental_sample;
        sSubSourceMap["isolation_source"] = CSubSource::eSubtype_isolation_source;
        sSubSourceMap["lat_lon"] = CSubSource::eSubtype_lat_lon;
        sSubSourceMap["altitude"] = CSubSource::eSubtype_altitude;
        sSubSourceMap["collection_date"] = CSubSource::eSubtype_collection_date;
        sSubSourceMap["collected_by"] = CSubSource::eSubtype_collected_by;
        sSubSourceMap["identified_by"] = CSubSource::eSubtype_identified_by;
        sSubSourceMap["fwd_primer_seq"] = CSubSource::eSubtype_fwd_primer_seq;
        sSubSourceMap["fwd_primer_name"] = CSubSource::eSubtype_fwd_primer_name;
        sSubSourceMap["rev_primer_seq"] = CSubSource::eSubtype_rev_primer_seq;
        sSubSourceMap["rev_primer_name"] = CSubSource::eSubtype_rev_primer_name;
        sSubSourceMap["metagenomic"] = CSubSource::eSubtype_metagenomic;
        sSubSourceMap["mating_type"] = CSubSource::eSubtype_mating_type;
        sSubSourceMap["linkage_group"] = CSubSource::eSubtype_linkage_group;
        sSubSourceMap["haplogroup"] = CSubSource::eSubtype_haplogroup;
        sSubSourceMap["whole_replicon"] = CSubSource::eSubtype_whole_replicon;
        sSubSourceMap["phenotype"] = CSubSource::eSubtype_phenotype;
        sSubSourceMap["note"] = CSubSource::eSubtype_other;
    }

    list<CRef<CSubSource> >& subType =
        pFeature->SetData().SetBiosrc().SetSubtype();
    for ( SUBSOURCE_MAP::const_iterator sit = sSubSourceMap.begin();
            sit != sSubSourceMap.end(); ++sit) {
        TAttributes::iterator ait = attrs_left.find(sit->first);
        if (ait == attrs_left.end()) {
            continue;
        }
        CRef<CSubSource> pSubSource(new CSubSource);
        pSubSource->SetSubtype(sit->second);
        pSubSource->SetName(xNormalizedAttributeValue(ait->second));
        subType.push_back(pSubSource);
        attrs_left.erase(ait);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff2Record::xInitFeatureData(
    TReaderFlags flags,
    CRef<CSeq_feat> pFeature ) const
//  ----------------------------------------------------------------------------
{
    auto recognizedType = NormalizedType();

    if (recognizedType == "region"  ||  recognizedType == "biological_region") {
        string gbkey;
        if (GetAttribute("gbkey", gbkey)) {
            if (gbkey == "Src") {
                pFeature->SetData().SetBiosrc();
                return true;
            }
         }
        // regardless of gbkey (rw-1062)
        string name;
        GetAttribute("Name", name);
        pFeature->SetData().SetRegion(name);
        return true;
    }

    if (recognizedType == "start_codon"  || recognizedType == "stop_codon") {
        recognizedType = "cds";
    }

    bool invalidFeaturesToRegion = !(flags & CGff2Reader::fGenbankMode);
    if (!CSoMap::SoTypeToFeature(
        Type(), *pFeature, invalidFeaturesToRegion)) {
        string message = "Bad data line: Invalid feature type \"" + Type() + "\"";
        CReaderMessage error(
            eDiag_Error,
            0,
            message);
        throw error;
    }
    return CGffBaseColumns::xInitFeatureData(flags, pFeature);
}

//  ============================================================================
bool
CGff2Record::IsMultiParent() const
//  =============================================================================
{
    list<string> parentIds;
    if (!GetAttribute("Parent", parentIds)) {
        return false;
    }
    return (parentIds.size() > 1);
}

END_objects_SCOPE
END_NCBI_SCOPE

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
 * Author:  Victor Sapojnikov, Michael Kornbluh
 *
 * File Description:
 *     Generic fast AGP stream reader    (CAgpReader),
 *     and even more generic line parser (CAgpRow).
 */

#include <ncbi_pch.hpp>
#include <objtools/readers/agp_util.hpp>

#ifndef i2s
#define i2s(x) NStr::NumericToString(x)
#define s2i(x) NStr::StringToNonNegativeInt(x)
#endif

BEGIN_NCBI_SCOPE

//// class CAgpErr

// When updating s_msg, also update the enum that indexes into this
const CAgpErr::TStr CAgpErr::s_msg[]= {
    kEmptyCStr,

    // Content Errors (codes 1..20)
    "expecting 9 tab-separated columns", // 8 or
    "column X is empty",
    "empty line",
    "invalid value for X",
    "invalid linkage",

    "X must be a positive integer not exceeding 2e+9",
    "object_end is less than object_beg",
    "component_end is less than component_beg",
    "object range length not equal to the gap length",
    "object range length not equal to component range length",

    "duplicate object ",
    "first line of an object must have object_beg=1",
    "first line of an object must have part_number=1",
    "part number (column 4) != previous part number + 1",
    "'na' or ? (formerly 0) component orientation may only be used in a singleton scaffold",

    "object_beg != previous object_end + 1",
    "no valid AGP lines",
    "consequtive gaps lines with the same type and linkage",
    "in \"Scaffold from component\" file, invalid scaffold-breaking gap",
    "in \"Chromosome from scaffold\" file, invalid \"within-scaffold\" gap",

    "scaffold X was not defined in any of \"Scaffold from component\" files",
    "scaffold X is not used in any of \"Chromosome from scaffold\" files",
    //"expecting X gaps per chromosome", // => expecting {2 telomere,1 centromere,not more than 1 short_arm)..., found 3
    "same gap_length=X in all gap lines, and component_type='N' ('U' is required for gaps of unknown size)",
    kEmptyCStr,
    kEmptyCStr,

    kEmptyCStr,
    kEmptyCStr,
    kEmptyCStr,
    kEmptyCStr,
    kEmptyCStr, // E_Last

    // Content Warnings
    "gap at the end of object (OK if X is the circular chromosome/plasmid)",
    "gap at the beginning of object ",
    "two consequtive gap lines (e.g. a gap at the end of "
        "a scaffold, two non scaffold-breaking gaps, ...)",
    "no components in object",
    "the span overlaps a previous span for this component",

    "component span appears out of order",
    "duplicate component with non-draft type",
    "line with component_type X appears to be a gap line and not a component line",
    "line with component_type X appears to be a component line and not a gap line",
    "extra tab or space at the end of line",

    "gap line missing column 9 (null)",
    "missing line separator at the end of file",
    "extra text in the column 9 of the gap line",
    "object names appear sorted, but not in a numerical order",
    "component_id looks like a WGS accession, component_type is not W",

    "component_id looks like a non-WGS accession, yet component_type is W",
    // ? "component_id looks like a protein accession"
    "object name (column 1) is the same as component_id (column 6)",
    // from Paul Kitts:
    // Size for a gap of unknown length is not 100 bases. The International Sequence
    // Database Collaboration uses a length of 100 bases for all gaps of unknown length.
    "gap length (column 6) is not 100 for a gap of unknown size (an INSDC standard)",
    "component lines with the same component_id found on different scaffolds",
    /*
    "component X is not used in full in a single-component scaffold",
    "only a part of component X is included in a singleton scaffold",
    "not the whole length of the component is included in a singleton scaffold",
    "singleton scaffold includes only X bases of Y in the component Z",
    "only X out of Y bases of component Z are used in the singleton scaffold",
    "singleton scaffold includes only a part of the component",
    "singleton scaffold include the whole component",
    "singleton scaffold includes only part of the component"
    */
    "in unplaced singleton scaffold, component is not used in full", // (X out of Y bp)

    "in unplaced singleton scaffold, component orientation is not \"+\"",
    "gap shorter than 10 bp",
    "space in object name ",
    "comments only allowed at the beginning of the file in AGP 2.0",
    "orientation '0' is deprecated in AGP 2.0;  use '?' instead",

    "linkage (column 9) should be 'na' for a gap with linkage 'no' (AGP 2.0)",
    "old gap type; not used in AGP 2.0",
    "assuming AGP version X",
    "in \"Chromosome from scaffold\" file, scaffold is not used in full",
    "missing linkage evidence (column 9) (AGP 2.0)",  // W_Last


    "AGP version comment is invalid, expecting ##agp-version 1.1 or ##agp-version 2.0",
    "ignoring AGP version comment - version already set to X",
    "linkage evidence term X appears more than once",
    "component_id X is not an HTG accession, but is used with component_type A, D, or F",
    "all objects are singletons with component_beg=1",

    kEmptyCStr,
    kEmptyCStr,
    kEmptyCStr,
    kEmptyCStr,
    kEmptyCStr,

    // GenBank-related errors
    "invalid component_id",
    "component_id not in GenBank",
    "component_id X is ambiguous without an explicit version",
    "component_end greater than sequence length",
    "sequence data is invalid or unavailable",

    "taxonomic data is not available",
    "object X not found in FASTA file(s)",
    "final object_end (column 3) not equal to object length in FASTA file(s)",
    "run(s) of Ns within the component span",
    kEmptyCStr  // G_Last
};

CAgpErr::CAgpErr()
    : m_apply_to(0)
{
}

const char* CAgpErr::GetMsg(int code)
{
    if(code>0 && code<G_Last) return s_msg[code];
    return NcbiEmptyCStr;
}

string CAgpErr::FormatMessage(const string& msg, const string& details)
{
    // string msg = GetMsg(code);
    if( details.size()==0 ) return msg;

    SIZE_TYPE pos = NStr::Find( string(" ") + msg + " ", " X " );
    if(pos!=NPOS) {
        // Substitute "X" with the real value (e.g. a column name or value)
        return msg.substr(0, pos) + details + msg.substr(pos+1);
    }
    else if(details.size()>2 && details[0]=='X' && details[1]==' ' && msg=="no valid AGP lines"){
      // Allow totally custom fatal error messages (such as bad text encoding)
      return details.substr(2);
    }
    else{
        return msg + details;
    }
}

string CAgpErr::GetErrorMessage(int mask)
{
    if(mask== fAtPrevLine) // messages to print after the prev line
        return m_messages_prev_line;
    if(mask & fAtPrevLine) // all messages to print in one go, simplistically
        return m_messages_prev_line+m_messages;
    return m_messages;     // messages to print after the current line
}

int CAgpErr::AppliesTo(int mask)
{
    return m_apply_to & mask;
}

// For the sake of speed, we do not care about warnings
// (unless they follow a previous error message).
void CAgpErr::Msg(int code, const string& details, int appliesTo)
{
     // Append warnings to the previously reported errors.
    // To collect all warnings, override Msg() in the derived class.
    if(code<E_Last || m_apply_to) {
        m_apply_to |= appliesTo;

        string& m( appliesTo==fAtPrevLine ? m_messages_prev_line : m_messages );
        m += "\t";
        m += ErrorWarningOrNote(code);
        m += ": " ;
        m += FormatMessage(GetMsg(code), details);
        m += "\n";
    }
}

void CAgpErr::Clear()
{
    m_messages="";
    m_messages_prev_line="";
    m_apply_to=0;
}


//// class CAgpRow
// if you update CAgpRow::gap_types, make sure you update CAgpRow::EGap
const CAgpRow::TStr CAgpRow::gap_types[CAgpRow::eGapCount] = {
    "clone",
    "fragment",
    "repeat",
    "scaffold",

    "contig",
    "centromere",
    "short_arm",
    "heterochromatin",
    "telomere"
};

CSafeStatic<CAgpRow::TMapStrEGap> CAgpRow::gap_type_codes( 
    & CAgpRow::gap_type_codes_creator, NULL );

// static
CAgpRow::TMapStrEGap * CAgpRow::gap_type_codes_creator(void)
{
    TMapStrEGap* p = new TMapStrEGap();
    for(int i=0; i<eGapCount; i++) {
        (*p)[ (string)gap_types[i] ] = (EGap)i;
    }
    return p;
}

CAgpRow::CAgpRow(EAgpVersion agp_version, CAgpReader* reader) :
    m_agp_version(agp_version), m_reader(reader),
        m_AgpErr( new CAgpErr )
{
}

CAgpRow::CAgpRow(CAgpErr* arg, EAgpVersion agp_version, CAgpReader* reader) :
    m_agp_version(agp_version), m_reader(reader),
        m_AgpErr(arg)
{
}

CAgpRow::CAgpRow(const CAgpRow & src)
{
    // leverage the assignment operator so we don't
    // have to keep this function up to date
    *this = src;
}

CAgpRow::~CAgpRow()
{
}

int CAgpRow::FromString(const string& line)
{
    // Comments
    cols.clear();
    pcomment = NStr::Find(line, "#");

    bool tabsStripped=false;
    bool extraTabOrSpace=false;
    if( pcomment != NPOS  ) {
        // Strip whitespace before "#"
        while( pcomment>0 && (line[pcomment-1]==' ' || line[pcomment-1]=='\t') ) {
            if( line[pcomment-1]=='\t' ) tabsStripped=true;
            pcomment--;
        }
        if(pcomment==0) return -1; // A comment line; to be skipped.
        NStr::Tokenize(line.substr(0, pcomment), "\t", cols);
    }
    else {
      int pos=line.size();
      if(pos == 0) {
          m_AgpErr->Msg(CAgpErr::E_EmptyLine);
          return CAgpErr::E_EmptyLine;
      }

      if(line[pos-1]==' ') {
        do {
          pos--;
        } while(pos>0 && line[pos-1]==' ');
        NStr::Tokenize(line.substr(0, pos), "\t", cols);
        m_AgpErr->Msg(CAgpErr::W_ExtraTab);
        extraTabOrSpace=true;
        pcomment=pos;
      }
      else NStr::Tokenize(line, "\t", cols);
    }



    // Column count
    if( cols.size()==10 && cols[9]=="") {
        if(!extraTabOrSpace) m_AgpErr->Msg(CAgpErr::W_ExtraTab);
    }
    else if( cols.size() < 8 || cols.size() > 9 ) {
        // skip this entire line, report an error
        m_AgpErr->Msg(CAgpErr::E_ColumnCount,
            string(", found ") + NStr::IntToString((unsigned)(cols.size())) );
        return CAgpErr::E_ColumnCount;
    }

    // No spaces allowed (except in comments, or inside the object name)
    // JIRA: GCOL-1236
    //   agp_validate generates a warning inside OnObjectChange(), once per each object name
    SIZE_TYPE p_space=line.find(' ', cols[0].size()+1);
    if( (NPOS != p_space && p_space<pcomment) || line[0]==' ' || line[cols[0].size()-1]==' ' ) {
        m_AgpErr->Msg( CAgpErr::E_ColumnCount, ", found space characters" );
        return CAgpErr::E_ColumnCount;
    }

    // Empty columns
    for(int i=0; i<8; i++) {
        if(cols[i].size()==0) {
            m_AgpErr->Msg(CAgpErr::E_EmptyColumn, NStr::IntToString(i+1) );
            return CAgpErr::E_EmptyColumn;
        }
    }

    // object_beg, object_end, part_number
    object_beg = s2i( GetObjectBeg() );
    if(object_beg<=0) m_AgpErr->Msg(CAgpErr::E_MustBePositive, "object_beg (column 2)");
    object_end = s2i( GetObjectEnd() );
    if(object_end<=0) {
        m_AgpErr->Msg(CAgpErr::E_MustBePositive, "object_end (column 3)");
    }
    part_number = s2i( GetPartNumber() );
    if(part_number<=0) {
        m_AgpErr->Msg(CAgpErr::E_MustBePositive, "part_number (column 4)");
        // may return this code later; (try to) parse other columns first
        // return CAgpErr::E_MustBePositive;
    }
    if(object_beg<=0 || object_end<=0) return CAgpErr::E_MustBePositive;
    if(object_end < object_beg) {
        m_AgpErr->Msg(CAgpErr::E_ObjEndLtBeg);
        return CAgpErr::E_ObjEndLtBeg;
    }
    int object_range_len = object_end - object_beg + 1;

    // component_type, type-specific columns
    if(GetComponentType().size()!=1) {
        m_AgpErr->Msg(CAgpErr::E_InvalidValue, "component_type (column 5)");
        return CAgpErr::E_InvalidValue;
    }
    component_type=GetComponentType()[0];
    switch(component_type) {
        case 'A':
        case 'D':
        case 'F':
        case 'G':
        case 'P':
        case 'O':
        case 'W':
        {
            is_gap=false;
            if(cols.size()==8) {
                if(tabsStripped) {
                    m_AgpErr->Msg(CAgpErr::E_EmptyColumn, "9");
                    return CAgpErr::E_EmptyColumn;
                }
                else {
                    m_AgpErr->Msg(CAgpErr::E_ColumnCount, ", found 8" );
                    return CAgpErr::E_ColumnCount;
                }
            }

            int code=ParseComponentCols();
            if(code==0) {
                int component_range_len=component_end-component_beg+1;
                if(component_range_len != object_range_len) {
                    m_AgpErr->Msg( CAgpErr::E_ObjRangeNeComp, string(": ") +
                        NStr::IntToString(object_range_len   ) + " != " +
                        NStr::IntToString(component_range_len)
                    );
                    return CAgpErr::E_ObjRangeNeComp;
                }
                if(part_number<=0) return CAgpErr::E_MustBePositive;
                return 0;  // successfully parsed
            }
            else {
                if(ParseGapCols(false)==0) {
                    m_AgpErr->Msg( CAgpErr::W_LooksLikeGap, GetComponentType() );
                }
                return code;
            }
        }

        case 'N':
        case 'U':
        {
            is_gap=true;
            if(cols.size()==8 && tabsStripped==false) {
                /* We do not want to prevent checks all other checks...
                if(m_agp_version == eAgpVersion_2_0) {
                    m_AgpErr->Msg( CAgpErr::E_ColumnCount, ", found 8");
                    return CAgpErr::E_ColumnCount;
                }
                else

                not important enough:
                m_AgpErr->Msg( CAgpErr::W_GapLineMissingCol9);
                */
            }
            if( m_agp_version == eAgpVersion_2_0 && cols.size()==8 ) {
                // just to make sure no out-of-bounds array accesses
                cols.push_back(NcbiEmptyString);
            }
            if(cols.size()==9 && cols[8].size()>0 &&
                m_agp_version == eAgpVersion_1_1)
            {
                m_AgpErr->Msg(CAgpErr::W_GapLineIgnoredCol9);
            }

            int code=ParseGapCols();
            if(code==0) {
                if(gap_length != object_range_len) {
                    m_AgpErr->Msg( CAgpErr::E_ObjRangeNeGap, string(": ") +
                        NStr::IntToString(object_range_len   ) + " != " +
                        NStr::IntToString(gap_length)
                    );
                    return CAgpErr::E_ObjRangeNeGap;
                }
                return 0; // successfully parsed
            }
            else {
                if(ParseComponentCols(false)==0) {
                    m_AgpErr->Msg( CAgpErr::W_LooksLikeComp, GetComponentType() );
                }
                return code;
            }

        }
        default :
            m_AgpErr->Msg(CAgpErr::E_InvalidValue, "component_type (column 5)");
            return CAgpErr::E_InvalidValue;
    }
}

int CAgpRow::ParseComponentCols(bool log_errors)
{
    // component_beg, component_end
    component_beg = s2i( GetComponentBeg() );
    if(component_beg<=0 && log_errors) {
        m_AgpErr->Msg(CAgpErr::E_MustBePositive, "component_beg (column 7)" );
    }
    component_end = s2i( GetComponentEnd() );
    if(component_end<=0 && log_errors) {
        m_AgpErr->Msg(CAgpErr::E_MustBePositive, "component_end (column 8)" );
    }
    if(component_beg<=0 || component_end<=0) return CAgpErr::E_MustBePositive;

    if( component_end < component_beg ) {
        if(log_errors) {
            m_AgpErr->Msg(CAgpErr::E_CompEndLtBeg);
        }
        return CAgpErr::E_CompEndLtBeg;
    }

    // orientation
    if(GetOrientation()=="na") {
        orientation = eOrientationIrrelevant;
        return 0;
    }
    if(GetOrientation().size()==1) {
        const char orientation_char = GetOrientation()[0];
        switch( orientation_char )
        {
            case '+':
                orientation = eOrientationPlus;
                return 0;
            case '-':
                orientation = eOrientationMinus;
                return 0;
            case '0':
                if( m_agp_version == eAgpVersion_2_0 ) {
                    m_AgpErr->Msg(CAgpErr::W_OrientationZeroDeprecated);
                }
                orientation = eOrientationUnknown;
                return 0;
            case '?':
                if( m_agp_version == eAgpVersion_1_1 ) {
                    if(log_errors) m_AgpErr->Msg(CAgpErr::E_InvalidValue, "orientation (column 9)");
                    return CAgpErr::E_InvalidValue;
                }
                orientation = eOrientationUnknown;
                return 0;
        }
    }
    if(log_errors) {
        m_AgpErr->Msg(CAgpErr::E_InvalidValue,"orientation (column 9)");
    }
    return CAgpErr::E_InvalidValue;
}

int CAgpRow::str_to_le(const string& str)
{
    if( str == "paired-ends"   ) return fLinkageEvidence_paired_ends;
    if( str == "align_genus"   ) return fLinkageEvidence_align_genus;
    if( str == "align_xgenus"  ) return fLinkageEvidence_align_xgenus;
    if( str == "align_trnscpt" ) return fLinkageEvidence_align_trnscpt;
    if( str == "within_clone"  ) return fLinkageEvidence_within_clone;
    if( str == "clone_contig"  ) return fLinkageEvidence_clone_contig;
    if( str == "map"           ) return fLinkageEvidence_map;
    if( str == "strobe"        ) return fLinkageEvidence_strobe;
    if( str == "unspecified"   ) return fLinkageEvidence_unspecified;
    if( str == "pcr"           ) return fLinkageEvidence_pcr;
    //if( str == "na"            ) return fLinkageEvidence_na;
    return fLinkageEvidence_INVALID;
}

int CAgpRow::ParseGapCols(bool log_errors)
{
    linkage_evidences.clear();
    linkage_evidence_flags = 0;

    gap_length = s2i( GetGapLength() );
    if(gap_length<=0) {
        if(log_errors) m_AgpErr->Msg(CAgpErr::E_MustBePositive, "gap_length (column 6)" );
        return CAgpErr::E_MustBePositive;
    }
    if(component_type=='U' && gap_length!=100) {
        m_AgpErr->Msg(CAgpErr::W_GapSizeNot100);
    }

    map<string, EGap>::const_iterator it = gap_type_codes->find( GetGapType() );
    if(it==gap_type_codes->end()) {
        if(log_errors) m_AgpErr->Msg(CAgpErr::E_InvalidValue, "gap_type (column 7)");
        return CAgpErr::E_InvalidValue;
    }
    gap_type=it->second;

    if(GetLinkage()=="yes") {
        linkage=true;
    }
    else if(GetLinkage()=="no") {
        linkage=false;
    }
    else {
        if(log_errors) m_AgpErr->Msg(CAgpErr::E_InvalidValue, "linkage (column 8)");
        return CAgpErr::E_InvalidValue;
    }

    if(linkage) {
        if( gap_type != eGapClone &&
            gap_type != eGapRepeat &&
            gap_type != eGapFragment &&
            gap_type != eGapScaffold )
        {
            if(log_errors) m_AgpErr->Msg(CAgpErr::E_InvalidLinkage, " \"yes\" for gap_type "+GetGapType() );
            return CAgpErr::E_InvalidLinkage;
        }
    }
    if( log_errors && m_agp_version==eAgpVersion_auto ) {
        string msg;
        if( GetLinkageEvidence().size()==0 ) {
            m_agp_version = eAgpVersion_1_1;
            msg = "1.1 since linkage evidence (column 9) is empty";
        }
        else {
            m_agp_version = eAgpVersion_2_0;
            msg = "2.0 since linkage evidence (column 9) is NOT empty";
        }
        if(m_reader) m_reader->SetVersion(m_agp_version);
        m_AgpErr->Msg(CAgpErr::W_AssumingVersion, msg );
    }

    // check gap_type, but only after we know linkage
    if( m_agp_version == eAgpVersion_2_0 ) {
        // gap-types not in AGP 2.0
        if( gap_type == eGapClone || gap_type == eGapFragment ) {
            // if(log_errors)
            m_AgpErr->Msg(CAgpErr::W_OldGapType, ". Recommended replacement: " + SubstOldGap(false) );
        }
        if(!linkage && gap_type==eGapScaffold)
        {
            if(log_errors) m_AgpErr->Msg(CAgpErr::E_InvalidLinkage, " \"no\" for gap_type "+GetGapType() );
            return CAgpErr::E_InvalidLinkage;
        }

    }
    if(m_agp_version == eAgpVersion_1_1){
        // gap-type not in AGP 1.1
        if( gap_type == eGapScaffold ) {
            if(log_errors) m_AgpErr->Msg(CAgpErr::E_InvalidValue, "gap_type (column 7)");
            return CAgpErr::E_InvalidValue;
        }
    }

    // linkage_evidence
    if( m_agp_version == eAgpVersion_2_0 ) {
        if( GetLinkageEvidence().size()==0 ) {
            if(log_errors) m_AgpErr->Msg(CAgpErr::W_MissingLinkage);
        }
        if( GapEndsScaffold() ) {
            if(GetLinkageEvidence() != "na") {
                if(log_errors) m_AgpErr->Msg(CAgpErr::W_NaLinkageExpected);
                linkage_evidence_flags=fLinkageEvidence_INVALID;
            }
            else {
                linkage_evidence_flags=fLinkageEvidence_na;
            }
        }
        else {
            if(GetLinkageEvidence() == "na") {
                linkage_evidence_flags = fLinkageEvidence_INVALID;
                if(log_errors) m_AgpErr->Msg(CAgpErr::E_InvalidValue,
                    "linkage_evidence (column 9): 'na' can only be used for gaps with linkage 'no'");
                return CAgpErr::E_InvalidValue;
            }
            else {
                vector<string> raw_linkage_evidences;
                NStr::Tokenize(GetLinkageEvidence(), ";", raw_linkage_evidences);
                bool has_unspecified=false;
                ITERATE( vector<string>, evid_iter, raw_linkage_evidences ) {
                    int le_flag = str_to_le(*evid_iter);
                    if( le_flag<0 ) {
                        linkage_evidences.clear();
                        linkage_evidence_flags = fLinkageEvidence_INVALID;
                        if(log_errors) m_AgpErr->Msg(CAgpErr::E_InvalidValue, "linkage_evidence (column 9): " + *evid_iter);
                        return CAgpErr::E_InvalidValue;
                    }
                    if( le_flag==fLinkageEvidence_unspecified ) has_unspecified=true;
                    else {
                        linkage_evidences.push_back((ELinkageEvidence)le_flag);
                        if( linkage_evidence_flags&le_flag ) {
                            if(log_errors) m_AgpErr->Msg(CAgpErr::W_DuplicateEvidence, *evid_iter);
                        }
                        linkage_evidence_flags |= le_flag;
                    }
                }
                if(has_unspecified && raw_linkage_evidences.size()>1) {
                    linkage_evidences.clear();
                    linkage_evidence_flags = fLinkageEvidence_INVALID;
                    if(log_errors) m_AgpErr->Msg(CAgpErr::E_InvalidValue,
                        "linkage_evidence (column 9) -- \"unspecified\" cannot be combined with other terms");
                    return CAgpErr::E_InvalidValue;
                }
            }
        }
    }

    return 0;
}

string CAgpRow::ToString(bool reorder_linkage_evidences)
{
    string res=
        GetObject() + "\t" +
        NStr::IntToString(object_beg ) + "\t" +
        NStr::IntToString(object_end ) + "\t" +
        NStr::IntToString(part_number) + "\t";

    res+=component_type;
    res+='\t';

    if(is_gap) {
        res +=
            NStr::IntToString(gap_length) + "\t" +
            gap_types[gap_type] + "\t" +
            (linkage?"yes":"no") + "\t";
        if(eAgpVersion_1_1!=m_agp_version) {
            res += reorder_linkage_evidences ? LinkageEvidenceFlagsToString(): LinkageEvidencesToString();
        }
    }
    else{
        res +=
            GetComponentId  () + "\t" +
            NStr::IntToString(component_beg) + "\t" +
            NStr::IntToString(component_end) + "\t" +
            OrientationToString(orientation);
    }

    return res;
}

string CAgpRow::GetErrorMessage()
{
    return m_AgpErr->GetErrorMessage();
}

void CAgpRow::SetErrorHandler(CAgpErr* arg)
{
    m_AgpErr=arg;
}

bool CAgpRow::CheckComponentEnd( const string& comp_id, int comp_end, int comp_len,
  CAgpErr& agp_err)
{
    if( comp_end > comp_len) {
        string details=": ";
        details += NStr::IntToString(comp_end);
        details += " > ";
        details += comp_id;
        details += " length = ";
        details += NStr::IntToString(comp_len);
        details += " bp";

        agp_err.Msg(CAgpErr::G_CompEndGtLength, details);
        return false;
    }
    return true;
}

const char* CAgpRow::le_str(CAgpRow::ELinkageEvidence le)
{
    switch( le ) {
        case fLinkageEvidence_paired_ends  : return "paired-ends";
        case fLinkageEvidence_align_genus  : return "align_genus";
        case fLinkageEvidence_align_xgenus : return "align_xgenus";
        case fLinkageEvidence_align_trnscpt: return "align_trnscpt";
        case fLinkageEvidence_within_clone : return "within_clone";
        case fLinkageEvidence_clone_contig : return "clone_contig";
        case fLinkageEvidence_map          : return "map";
        case fLinkageEvidence_strobe       : return "strobe";
        case fLinkageEvidence_unspecified  : return "unspecified";
        case fLinkageEvidence_pcr          : return "pcr";
        case fLinkageEvidence_na           : return "na";
        case fLinkageEvidence_INVALID      : return "INVALID_LINKAGE_EVIDENCE";
        default:;
    }
    //return "ERROR:UNKNOWN_LINKAGE_EVIDENCE_TYPE:" +  NStr::IntToString( le );
    return NcbiEmptyCStr;
}

string CAgpRow::LinkageEvidenceFlagsToString(int le)
{
    string res = le_str( (ELinkageEvidence)le );
    if(res.size()) return res;
    for(unsigned mask=1; mask<=fLinkageEvidence_HIGHEST_BIT_MASK; mask <<= 1 ) {
        if(le&mask) {
            if(res.size()) res += ";";
            res += le_str( (ELinkageEvidence)mask );
        }
    }
    return res;
}

string CAgpRow::LinkageEvidencesToString(void)
{
    string result;

    ITERATE( TLinkageEvidenceVec, evid_iter, linkage_evidences ) {
        if( ! result.empty() ) {
            result += ';';
        }
        const char* le = le_str( *evid_iter );
        if(*le!='\0') result += le;
        else result += "ERROR:UNKNOWN_LINKAGE_EVIDENCE_TYPE:" + NStr::IntToString( (int)*evid_iter );
    }

    if(result.size()) return result;
    return linkage ? "unspecified" : "na";
}

string CAgpRow::OrientationToString( EOrientation orientation )
{
    switch( orientation ) {
        case eOrientationPlus:
            return "+";
        case eOrientationMinus:
            return "-";
        case eOrientationUnknown:
            return ( m_agp_version == eAgpVersion_1_1 ? "0" : "?" );
        case eOrientationIrrelevant:
            return "na";
        default:
            return "ERROR:UNKNOWN_ORIENTATION:" +
                NStr::IntToString( (int)orientation );
    }
}

string CAgpRow::SubstOldGap(bool do_subst)
{
    ELinkageEvidence le=fLinkageEvidence_unspecified;
    if( gap_type == eGapFragment ) {
        le = linkage ? fLinkageEvidence_paired_ends : fLinkageEvidence_within_clone;
    }
    else if( gap_type == eGapClone ) {
        if(linkage) {
            le =  fLinkageEvidence_clone_contig;
        }
        else {
            if(do_subst) gap_type = eGapContig;
            return "gap type=contig, linkage=no, linkage evidence=na";
        }

    }
    else return NcbiEmptyString; // no conversion

    if(do_subst) {
        gap_type = eGapScaffold;
        linkage = true;
        if(linkage_evidence_flags==0 && le!=fLinkageEvidence_unspecified) {
            linkage_evidence_flags = le;
            linkage_evidences.clear(); linkage_evidences.push_back(le);
        }
    }
    return string("gap type=scaffold, linkage=yes, linkage evidence=")+le_str(le)+" or unspecified";
}

void CAgpRow::SetVersion(EAgpVersion ver)
{
    m_agp_version=ver;
}

//// class CAgpReader
CAgpReader::CAgpReader(EAgpVersion agp_version) :
    m_agp_version(agp_version)
{
    m_AgpErr=new CAgpErr;
    Init();
}

CAgpReader::CAgpReader(CAgpErr* arg,
                       EAgpVersion agp_version ) :
m_agp_version(agp_version)
{
    if( arg ) {
        m_AgpErr=arg;
    } else {
        m_AgpErr=new CAgpErr;
    }
    Init();
}

void CAgpReader::Init()
{
    m_prev_row= CAgpRow::New(m_AgpErr, m_agp_version, this);
    m_this_row= CAgpRow::New(m_AgpErr, m_agp_version, this);
    m_at_beg=true;
    m_prev_line_num=-1;
}

CAgpReader::~CAgpReader()
{
}

bool CAgpReader::ProcessThisRow()
{
    CRef<CAgpRow> this_row = m_this_row;
    CRef<CAgpRow> prev_row = m_prev_row;

    m_new_obj=prev_row->GetObject() != this_row->GetObject();
    if(m_new_obj) {
        if(!m_prev_line_skipped) {
            if(this_row->object_beg !=1) m_AgpErr->Msg(m_error_code=CAgpErr::E_ObjMustBegin1, CAgpErr::fAtThisLine);
            if(this_row->part_number!=1) m_AgpErr->Msg(m_error_code=CAgpErr::E_PartNumberNot1, CAgpErr::fAtThisLine);
            if(prev_row->is_gap && !prev_row->GapValidAtObjectEnd() && !m_at_beg) {
                m_AgpErr->Msg(CAgpErr::W_GapObjEnd, prev_row->GetObject(), CAgpErr::fAtPrevLine);
            }
        }
        if(!( prev_row->is_gap && prev_row->GapEndsScaffold() )) {
            OnScaffoldEnd();
        }
        OnObjectChange();
    }
    else {
        if(!m_prev_line_skipped) {
            if(this_row->part_number != prev_row->part_number+1) {
                m_AgpErr->Msg(m_error_code=CAgpErr::E_PartNumberNotPlus1, CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine);
            }
            if(this_row->object_beg != prev_row->object_end+1) {
                m_AgpErr->Msg(m_error_code=CAgpErr::E_ObjBegNePrevEndPlus1, CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine);
            }
        }
    }

    if(this_row->is_gap) {
        if(!m_prev_line_skipped) {
            if( m_new_obj ) {
	        if( !this_row->GapValidAtObjectEnd() ) {
                    m_AgpErr->Msg(CAgpErr::W_GapObjBegin, this_row->GetObject()); // , CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine
                }
            }
            else if(prev_row->is_gap && !m_at_beg) {
                if( prev_row->gap_type == this_row->gap_type &&
                    prev_row->linkage  == this_row->linkage
                  )  m_AgpErr->Msg( CAgpErr::E_SameConseqGaps, CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine);
                else m_AgpErr->Msg( CAgpErr::W_ConseqGaps    , CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine);
            }
        }
        if(!m_new_obj) {
            if( this_row->GapEndsScaffold() && !(
                prev_row->is_gap && prev_row->GapEndsScaffold()
            )) OnScaffoldEnd();
        }
        //OnGap();
    }
    //else { OnComponent(); }
    OnGapOrComponent();
    m_at_beg=false;

    if(m_error_code>0){
        if( !OnError() ) return false; // return m_error_code; - abort ReadStream()
        m_AgpErr->Clear();
    }

    // swap this_row and prev_row
    m_this_row=prev_row;
    m_prev_row=this_row;
    m_prev_line_num=m_line_num;
    m_prev_line_skipped=m_line_skipped;
    return true;
}

void CAgpReader::SetVersion(EAgpVersion ver)
{
    // to do (?) : check that previous version is the same or eAgpVersion_auto
    m_agp_version = ver;
    m_this_row->SetVersion(ver);
    m_prev_row->SetVersion(ver);
}


int CAgpReader::ReadStream(CNcbiIstream& is, EFinalize eFinalize)
{
    m_at_end=false;
    m_content_line_seen=false;
    if(m_at_beg) {
        //// The first line
        m_line_num=0;
        m_prev_line_skipped=false;

        // A fictitous empty row that ends with a scaffold-breaking gap.
        // Used to:
        // - prevent the two-row checks;
        // - prevent OnScaffoldEnd();
        // - trigger OnObjectChange().
        m_prev_row->cols.clear();
        m_prev_row->cols.push_back(NcbiEmptyString); // Empty object name
        m_prev_row->is_gap=true;
        m_prev_row->gap_type=CAgpRow::eGapContig; // eGapCentromere
        m_prev_row->linkage=false;
    }

    while( NcbiGetline(is, m_line, "\r\n") ) {
        m_line_num++;

        if(m_at_beg && m_line.size()>=2) {
          if( ((Uint1)(m_line[0])==(Uint1)0xFF && (Uint1)(m_line[1])==(Uint1)0xFE) ||
              ((Uint1)(m_line[0])==(Uint1)0xFE && (Uint1)(m_line[1])==(Uint1)0xFF)
          ) {
            m_AgpErr->Msg(m_error_code=CAgpErr::E_NoValidLines, "X UTF-16 not supported, text needs to be in ASCII encoding.", CAgpErr::fAtNone);
            return CAgpErr::E_NoValidLines;
          }
        }

        // processes pragma comments on the line, if any
        x_CheckPragmaComment();

        m_error_code = m_this_row->FromString(m_line);
        if( m_error_code != -1 ) {
            m_content_line_seen = true;
        }

        m_line_skipped=false;
        if(m_error_code==0) {
            if( !ProcessThisRow() ) return m_error_code;
            if( m_error_code < 0 ) break; // A simulated EOF midstream
        }
        else if(m_error_code==-1) {
            if( m_agp_version == eAgpVersion_2_0 && m_content_line_seen ) {
                m_AgpErr->Msg(CAgpErr::W_CommentsAfterStart);
            }
            OnComment();
            if( m_error_code < -1 ) break; // A simulated EOF midstream
        }
        else {
            m_line_skipped=true;
            if( !OnError() ) return m_error_code;
            m_AgpErr->Clear();
            // for OnObjectChange(), keep the line before previous as if it is the previous
            m_prev_line_skipped=m_line_skipped;
        }

        if(is.eof() && !m_at_beg) {
            m_AgpErr->Msg(CAgpErr::W_NoEolAtEof);
        }
    }
    if(m_at_beg) {
        m_AgpErr->Msg(m_error_code=CAgpErr::E_NoValidLines, CAgpErr::fAtNone);
        return CAgpErr::E_NoValidLines;
    }

    return (eFinalize == eFinalize_Yes) ? Finalize() : 0;
}

// By default, called at the end of ReadStream
// Only needs to be called manually after reading all input lines
// via ReadStream(stream, false).
int CAgpReader::Finalize()
{
    m_at_end=true;
    m_error_code=0;
    if(!m_at_beg) {
        m_new_obj=true; // The only meaning here: scaffold ended because object ended

        CRef<CAgpRow> prev_row = m_prev_row;
        if( !m_prev_line_skipped ) {
            if(prev_row->is_gap && !prev_row->GapValidAtObjectEnd()) {
                m_AgpErr->Msg(CAgpErr::W_GapObjEnd, prev_row->GetObject(), CAgpErr::fAtPrevLine);
            }
        }

        if(!( prev_row->is_gap && prev_row->GapEndsScaffold() )) {
            OnScaffoldEnd();
        }
        OnObjectChange();
    }

    // In preparation for the next file
    //m_prev_line_skipped=false;
    m_at_beg=true;

    return m_error_code;
}

void CAgpReader::SetErrorHandler(CAgpErr* arg)
{
    m_AgpErr=arg;
    m_this_row->SetErrorHandler(arg);
    m_prev_row->SetErrorHandler(arg);
}

string CAgpReader::GetErrorMessage(const string& filename)
{
    string msg;
    if( m_AgpErr->AppliesTo(CAgpErr::fAtPrevLine) && m_prev_line_num>0 ) {
        if(filename.size()){
            msg+=filename;
            msg+=":";
        }
        msg+= NStr::IntToString(m_prev_line_num);
        msg+=":";

        msg+=m_prev_row->ToString();
        msg+="\n";

        msg+=m_AgpErr->GetErrorMessage(CAgpErr::fAtPrevLine);
    }
    if( m_AgpErr->AppliesTo(CAgpErr::fAtThisLine) ) {
        if(filename.size()){
            msg+=filename;
            msg+=":";
        }
        msg+= NStr::IntToString(m_line_num);
        msg+=":";

        msg+=m_line;
        msg+="\n";
    }

    // Messages printed at the end  apply to:
    // current line, 2 lines, no lines.
    return msg + m_AgpErr->GetErrorMessage(CAgpErr::fAtThisLine|CAgpErr::fAtNone);
}

void CAgpReader::x_CheckPragmaComment(void)
{
    static const char* kAgpVersionCommentStart = "##agp-version";
    if( NStr::StartsWith(m_line, kAgpVersionCommentStart) ) {
        // skip whitespace before and after version number
        const SIZE_TYPE versionStartPos = m_line.find_first_not_of(
            " \t\v\f",
            strlen(kAgpVersionCommentStart) );
        const SIZE_TYPE versionEndPos = m_line.find_last_not_of(
            " \t\v\f" );
        string version;
        if( versionStartPos != NPOS && versionEndPos != NPOS ) {
            version = m_line.substr( versionStartPos,
                (versionEndPos - versionStartPos) + 1 );
        }
        if( m_agp_version == eAgpVersion_auto ) {
            if( version == "1.1" ) {
                m_agp_version = eAgpVersion_1_1;
                m_prev_row->SetVersion( m_agp_version );
                m_this_row->SetVersion( m_agp_version );
            } else if( version == "2.0" ) {
                m_agp_version = eAgpVersion_2_0;
                m_prev_row->SetVersion( m_agp_version );
                m_this_row->SetVersion( m_agp_version );
            } else {
                // unknown AGP version
                // cannot use fAtThisLine: it prints the next component or gap line, not the comment line
                m_AgpErr->Msg(CAgpErr::W_AGPVersionCommentInvalid, CAgpErr::fAtNone);
            }
        } else {
            // extra AGP version
            // cannot use fAtThisLine: it prints the next component or gap line, not the comment line
            m_AgpErr->Msg(CAgpErr::W_AGPVersionCommentUnnecessary, m_agp_version == eAgpVersion_1_1 ? "1.1" : "2.0", CAgpErr::fAtNone );
        }
    }
}

//// class CAgpErrEx - static members and functions

bool CAgpErrEx::MustSkip(int code)
{
    return m_MustSkip[code];
}

void CAgpErrEx::PrintAllMessages(CNcbiOstream& out)
{
    out << "### Errors within a single line. Lines with such errors are skipped, ###\n";
    out << "### i.e. not used for: further checks, object/component/gap counts.  ###\n";
    for(int i=E_First; i<=E_LastToSkipLine; i++) {
        out << GetPrintableCode(i) << "\t" << GetMsg(i);
        if(i==E_EmptyColumn) {
            out << " (X: 1..9)";
        }
        else if(i==E_InvalidValue) {
            out << " (X: component_type, gap_type, linkage, orientation)";
        }
        else if(i==E_MustBePositive) {
            out << " (X: object_beg, object_end, part_num, gap_length, component_beg, component_end)";
        }
        out << "\n";
    }

    out << "### Errors that may involve several lines ###\n";
    for(int i=E_LastToSkipLine+1; i<E_Last; i++) {
        out << GetPrintableCode(i) << "\t" << GetMsg(i);
        out << "\n";
    }

    out << "### Warnings ###\n";
    for(int i=W_First; i<W_Last; i++) {
        out << GetPrintableCode(i) << "\t" << GetMsg(i);
        if(i==W_GapLineMissingCol9) {
            out << " (no longer reported)";
            //out << " (only the total count is printed unless you specify: -only " << GetPrintableCode(i) << ")";
        }
        out << "\n";
    }

    out << "### Errors for GenBank-based (-alt) and other component checks (-g, FASTA files) ###\n";
    for(int i=G_First; i<G_Last; i++) {
        out << GetPrintableCode(i) << "\t" << GetMsg(i);
        out << "\n";
    }
    out <<
        "#\tErrors reported once at the end of validation:\n"
        "#\tunable to determine a Taxid for the AGP (less than 80% of components have one common taxid)\n"
        "#\tcomponents with incorrect taxids\n";
}

string CAgpErrEx::GetPrintableCode(int code)
{
    string res =
        (code<E_Last) ? "e" :
        (code<W_Last) ? "w" :
        (code<G_Last) ? "g" : "x";
    if(code<10) res += "0";
    res += NStr::IntToString(code);
    return res;
}

void CAgpErrEx::PrintLine(CNcbiOstream& ostr,
    const string& filename, int linenum, const string& content)
{
    string line=content.size()<200 ? content : content.substr(0,160)+"...";

    // Mark the first space that is not inside a EOL comment
    SIZE_TYPE posComment = NStr::Find(line, "#");
    SIZE_TYPE posSpace   = NStr::Find(line, " ", 0, posComment);
    if(posSpace!=NPOS) {
        SIZE_TYPE posTab     = NStr::Find(line, "\t", 0, posComment);
        if(posTab!=NPOS && posTab>posSpace+1 && posSpace!=0 ) {
            // GCOL-1236: allow spaces in object names, emit a WARNING instead of an ERROR
            // => if there is ANOTHER space not inside the object name, then mark that another space
            posTab = NStr::Find(line, " ", posTab+1, posComment);
            if(posTab!=NPOS) posSpace = posTab;
        }
        posSpace++;
        line = line.substr(0, posSpace) + "<<<SPACE!" + line.substr(posSpace);
    }

    if(filename.size()) ostr << filename << ":";
    ostr<< linenum  << ":" << line << "\n";
}

void CAgpErrEx::PrintLineXml(CNcbiOstream& ostr,
    const string& filename, int linenum, const string& content,
    bool two_lines_involved)
{
    string attr="num=\""+i2s(linenum)+"\"";
    if(filename.size()) attr+=" filename=\"" + NStr::XmlEncode(filename) + "\"";
    if(two_lines_involved) attr+=" two_lines=\"true\"";

    string xml_content = NStr::XmlEncode(content);
    if( xml_content.find("&#x0;")!=NPOS ) NStr::ReplaceInPlace(xml_content, "&#x0;", "?");
    ostr << " <line " << attr << ">" << xml_content << "</line>\n";

}

void CAgpErrEx::PrintMessage(CNcbiOstream& ostr, int code,
        const string& details)
{
    ostr<< "\t" << ErrorWarningOrNote(code)
        << (code <=E_LastToSkipLine ? ", line skipped" : "")
        << ": " << FormatMessage( GetMsg(code), details ) << "\n";
}

void CAgpErrEx::PrintMessageXml(CNcbiOstream& ostr, int code, const string& details, int appliesTo)
{
    ostr<< " <message severity=\"" << ErrorWarningOrNote(code) << "\"";
    if(code <=E_LastToSkipLine) ostr << " line_skipped=\"1\"";
    ostr<<">\n";

    ostr << " <code>"     << GetPrintableCode(code) << "</code>\n";
    if(appliesTo & CAgpErr::fAtPpLine  ) ostr << " <line_num>" << m_line_num_pp    << "</line_num>\n";
    if(appliesTo & CAgpErr::fAtPrevLine) ostr << " <line_num>" << m_line_num_prev  << "</line_num>\n";
    if(appliesTo & CAgpErr::fAtThisLine) ostr << " <line_num>current</line_num>\n";
    ostr << " <text>" << NStr::XmlEncode( FormatMessage( GetMsg(code), details ) ) << "</text>\n";

    ostr << "</message>\n";
}


//// class CAgpErrEx - constructor
CAgpErrEx::CAgpErrEx(CNcbiOstream* out, bool use_xml, EOwnership eOwnsOut) : 
    m_use_xml(use_xml), 
    m_messages( new CNcbiOstrstream() ),
    m_out(out)
{
    // if we own m_out, we set a special autoptr so it's automatically destroyed
    if( eOwnsOut == eTakeOwnership ) {
        m_out_destroyer.reset( out );
    }

    m_MaxRepeat = 0; // no limit
    m_MaxRepeatTopped = false;
    m_msg_skipped=0;
    m_lines_skipped=0;
    m_line_num=1;
    m_filenum_pp=-1; m_filenum_prev=-1;

    m_line_num_pp=0; m_line_num_prev=0;
    m_pp_printed=false; m_prev_printed=false;

    m_two_lines_involved=false;

    memset(m_MustSkip , 0, sizeof(m_MustSkip ));
    ResetTotals();

    // errors that are "silenced" by default (only the count is printed)
    m_MustSkip[W_GapLineMissingCol9]=true;
    if(!use_xml) // perhaps, we should have a separate parameter for hiding these...
    {
        m_MustSkip[W_ExtraTab          ]=true;
        m_MustSkip[W_CompIsWgsTypeIsNot]=true;
        m_MustSkip[W_CompIsNotWgsTypeIs]=true;
        m_MustSkip[W_CompIsNotHtgTypeIs]=true;
        m_MustSkip[W_ShortGap          ]=true;
    }

    // A "random check" to make sure enum and msg[] are not out of skew.
    //cerr << sizeof(msg)/sizeof(msg[0]) << "\n";
    //cerr << G_Last+1 << "\n";
    NCBI_ASSERT( sizeof(s_msg)/sizeof(s_msg[0])==G_Last+1,
        "s_msg[] size != G_Last+1");
        //(string("s_msg[] size ")+NStr::IntToString(sizeof(s_msg)/sizeof(s_msg[0])) +
        //" != G_Last+1 "+NStr::IntToString(G_Last+1)).c_str() );
    NCBI_ASSERT( string(GetMsg(E_Last))=="",
        "CAgpErrEx -- GetMsg(E_Last) not empty" );
    NCBI_ASSERT( string(GetMsg( (E_Last-1) ))!="",
        "CAgpErrEx -- GetMsg(E_Last-1) is empty" );
    NCBI_ASSERT( string(GetMsg(W_Last))=="",
        "CAgpErrEx -- GetMsg(W_Last) not empty" );
    NCBI_ASSERT( string(GetMsg( (W_Last-1) ))!="",
        "CAgpErrEx -- GetMsg(W_Last-1) is empty" );
    NCBI_ASSERT( string(GetMsg(G_Last))=="",
        "CAgpErrEx -- GetMsg(G_Last) not empty" );
    NCBI_ASSERT( string(GetMsg( (G_Last-1) ))!="",
        "CAgpErrEx -- GetMsg(G_Last-1) is empty" );
}


//// class CAgpErrEx - non-static functions
void CAgpErrEx::ResetTotals()
{
    memset(m_MsgCount, 0, sizeof(m_MsgCount));
}

void CAgpErrEx::Msg(int code, const string& details, int appliesTo)
{
    // Suppress some messages while still counting them
    m_MsgCount[code]++;
    if( m_MustSkip[code]) {
        m_msg_skipped++;
        return;
    }
    if( m_MaxRepeat>0 && m_MsgCount[code] > m_MaxRepeat) {
        m_MaxRepeatTopped=true;
        m_msg_skipped++;
        return;
    }

    if(appliesTo & CAgpErr::fAtPpLine) {
        // Print the line before previous if it was not printed
        if( !m_pp_printed && m_line_pp.size() ) {
            if(m_use_xml) {
                PrintLineXml(*m_out,
                    m_filenum_pp>=0 ? m_InputFiles[m_filenum_pp] : NcbiEmptyString,
                    m_line_num_pp, m_line_pp, m_two_lines_involved);
            }
            else {
                *m_out << "\n";
                PrintLine(*m_out,
                    m_filenum_pp>=0 ? m_InputFiles[m_filenum_pp] : NcbiEmptyString,
                    m_line_num_pp, m_line_pp);
            }
        }
        m_pp_printed=true;
    }
    if( (appliesTo&CAgpErr::fAtPpLine) && (appliesTo&CAgpErr::fAtPrevLine) ) m_two_lines_involved=true;
    if(appliesTo & CAgpErr::fAtPrevLine) {
        // Print the previous line if it was not printed
        if( !m_prev_printed && m_line_prev.size() ) {
            if(m_use_xml) {
                PrintLineXml(*m_out,
                    m_filenum_prev>=0 ? m_InputFiles[m_filenum_prev] : NcbiEmptyString,
                    m_line_num_prev, m_line_prev, m_two_lines_involved);
            }
            else {
                if( !m_two_lines_involved ) *m_out << "\n";
                PrintLine(*m_out,
                    m_filenum_prev>=0 ? m_InputFiles[m_filenum_prev] : NcbiEmptyString,
                    m_line_num_prev, m_line_prev);
            }
        }
        m_prev_printed=true;
    }
    if(appliesTo & CAgpErr::fAtThisLine) {
        // Accumulate messages
        if(m_use_xml) {
            PrintMessageXml(*m_messages, code, details, appliesTo);
        }
        else {
            PrintMessage(*m_messages, code, details);
        }
    }
    else {
        // Print it now (useful for appliesTo==CAgpErr::fAtPrevLine)
        if(m_use_xml) {
            PrintMessageXml(*m_out, code, details, appliesTo);
        }
        else {
            // E_NoValidLines
            if(appliesTo==fAtNone && m_InputFiles.size() ) *m_out << m_InputFiles.back() << ":\n";
            PrintMessage(*m_out, code, details);
        }
    }

    if( (appliesTo&CAgpErr::fAtPrevLine) && (appliesTo&CAgpErr::fAtThisLine) ) m_two_lines_involved=true;
}

void CAgpErrEx::LineDone(const string& s, int line_num, bool invalid_line)
{
    if( m_messages->pcount() ) {
        if(m_use_xml) {
            PrintLineXml(*m_out, m_filename, line_num, s, m_two_lines_involved);
        }
        else {
            if( !m_two_lines_involved ) *m_out << "\n";
            PrintLine(*m_out, m_filename, line_num, s);
        }

        if(m_use_xml) {
            string m;
            NStr::Replace((string)CNcbiOstrstreamToString(*m_messages),
              "<line_num>current</line_num>",
              "<line_num>"+i2s(line_num)+"</line_num>", m);
            *m_out << m;
        }
        else {
            *m_out << (string)CNcbiOstrstreamToString(*m_messages);
        }
        m_messages.reset( new CNcbiOstrstream );

        m_pp_printed=m_prev_printed; m_prev_printed=true;
    }
    else {
        m_pp_printed=m_prev_printed; m_prev_printed=false;
    }

    m_line_num_pp = m_line_num_prev; m_line_num_prev = line_num;
    m_line_pp     = m_line_prev    ; m_line_prev     = s;
    m_filenum_pp  = m_filenum_prev ; m_filenum_prev  = m_InputFiles.size()-1;

    if(invalid_line) {
        m_lines_skipped++;
    }

    m_two_lines_involved=false;
}

void CAgpErrEx::StartFile(const string& s)
{
    // might need to set it here in case some file is empty and LineDone() is never called
    m_filenum_pp=m_filenum_prev; m_filenum_prev=m_InputFiles.size()-1;
    m_filename=s;
    m_InputFiles.push_back(s);
}

// Initialize m_MustSkip[]
// Return values:
//   ""                          no matches found for str
//   string beginning with "  "  one or more messages that matched
string CAgpErrEx::SkipMsg(const string& str, bool skip_other)
{
    string res = skip_other ? "Printing" : "Skipping";
    const static char* skipErr  = "Skipping errors, printing warnings.";
    const static char* skipWarn = "Skipping warnings, printing errors.";

    // Keywords: all warn* err* alt
    int i_from=CODE_Last;
    int i_to  =0;
    if(str=="all") {
        i_from=0; i_to=CODE_Last;
        // "-only all" does not make a lot of sense,
        // but we can support it anyway.
        res+=" all errors and warnings.";
    }
    else if(str=="alt") {
        i_from=G_First; i_to=G_Last;
        // "-only all" does not make a lot of sense,
        // but we can support it anyway.
        res+=" Accession/Length/Taxid errors.";
    }
    else if (str.substr(0,4)=="warn" && str.size()<=8 ) { // warn ings
        i_from=W_First; i_to=W_Last;
        res = skip_other ? skipErr : skipWarn;
    }
    else if (str.substr(0,4)=="err" && str.size()<=6 ) { // err ors
        i_from=E_First; i_to=E_Last;
        res = skip_other ? skipWarn : skipErr;
    }
    if(i_from<i_to) {
        for( int i=i_from; i<i_to; i++ ) m_MustSkip[i] = !skip_other;
        return res;
    }

    // Error or warning codes, substrings of the messages.
    res="";
    for( int i=E_First; i<CODE_Last; i++ ) {
        bool matchesCode = ( str==GetPrintableCode(i) );
        if( matchesCode || NStr::Find(GetMsg(i), str) != NPOS) {
            m_MustSkip[i] = !skip_other;
            res += "  ";
            res += GetPrintableCode(i);
            res += "  ";
            res += GetMsg(i);
            res += "\n";
            if(matchesCode) break;
        }
    }

    return res;
}

int CAgpErrEx::CountTotals(int from, int to)
{
    if(to==E_First) {
        //// One argument: count errors/warnings/genbank errors/given type
        if     (from==E_Last) { from=E_First; to=E_Last; }
        else if(from==W_Last) { from=W_First; to=W_Last; }
        else if(from==G_Last) { from=G_First; to=G_Last; }
        else if(from<CODE_Last)  return m_MsgCount[from];
        else return -1; // Invalid "from"
    }

    int count=0;
    for(int i=from; i<to; i++) {
        count += m_MsgCount[i];
    }
    return count;
}

void CAgpErrEx::PrintMessageCounts(CNcbiOstream& ostr, int from, int to, bool report_lines_skipped, TMapCcodeToString* hints)
{
    if(to==E_First) {
        //// One argument: count errors/warnings/genbank errors/given type
        if     (from==E_Last) { from=E_First; to=E_Last; }
        else if(from==W_Last) { from=W_First; to=W_Last; }
        else if(from==G_Last) { from=G_First; to=G_Last; }
        else if(from<CODE_Last)  { to=(from+1); }
        else {
            ostr << "Internal error in CAgpErrEx::PrintMessageCounts().";
        }
    }

    if(m_use_xml) {
        for(int i=from; i<to; i++) {
            if( m_MsgCount[i] ) {
                ostr << "<msg_summary>\n";
                ostr << " <code>" << GetPrintableCode(i)            << "</code>\n";
                ostr << " <text>" << NStr::XmlEncode(GetMsg(i))     << "</text>\n";
                ostr << " <cnt>"  << m_MsgCount[i]                  << "</cnt>\n";
                ostr << "</msg_summary>\n";
            }
        }
        // lines that we failed to parse because of syntax errors
        ostr << " <invalid_lines>"  << m_lines_skipped << "</invalid_lines>\n";
    }
    else {
        if(from<to) ostr<< setw(7) << "Count" << " Code  Description\n"; // code?
        for(int i=from; i<to; i++) {
            if( m_MsgCount[i] ) {
                ostr<< setw(7) << m_MsgCount[i] << "  "
                        << GetPrintableCode(i) << "  "
                        << GetMsg(i) << "\n";
            }
            // ouside of previous "if" because one hint may apply to one or more of several consequitive warnings
            // (such as W_CompIsWgsTypeIsNot and W_CompIsNotWgsTypeIs)
            if(hints && (*hints).find(i)!=(*hints).end() ) {
                ostr << "         " << (*hints)[i] << "\n";
            }
        }
        if(m_lines_skipped && report_lines_skipped) {
          ostr << "\nNOTE: " << m_lines_skipped <<
            " invalid lines were skipped (not subjected to all the checks, not included in most of the counts below).\n";
        }
    }
}

void CAgpErrEx::PrintTotalsXml(CNcbiOstream& ostr, int e_count, int w_count, int note_count, int skipped_count)
{
    ostr << " <notes>"    << note_count    << "</notes>\n";
    ostr << " <warnings>" << w_count       << "</warnings>\n";
    ostr << " <errors>"   << e_count       << "</errors>\n";
    ostr << " <skipped>"  << skipped_count << "</skipped>\n";
}

void CAgpErrEx::PrintTotals(CNcbiOstream& ostr, int e_count, int w_count, int note_count, int skipped_count)
{
    if     (e_count==0) ostr << "No errors, ";
    else if(e_count==1) ostr << "1 error, "  ;
    else                ostr << e_count << " errors, ";

    if     (w_count==0) ostr << "no warnings";
    else if(w_count==1) ostr << "1 warning";
    else                ostr << w_count << " warnings";

    if(note_count>0) {
        ostr << ", " << note_count << " note";
        if(note_count>1) ostr << "s";
    }

    if(skipped_count) {
        ostr << "; " << skipped_count << " not printed";
    }
}


END_NCBI_SCOPE


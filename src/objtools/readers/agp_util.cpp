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
 * Author:  Victor Sapojnikov
 *
 * File Description:
 *     Generic fast AGP stream reader    (CAgpReader),
 *     and even more generic line parser (CAgpRow).
 */

#include <ncbi_pch.hpp>
#include <objtools/readers/agp_util.hpp>

BEGIN_NCBI_SCOPE

//// class CAgpErr
const CAgpErr::TStr CAgpErr::s_msg[]= {
    kEmptyCStr,

    // Content Errors (codes 1..20)
    "expecting 9 tab-separated columns", // 8 or
    "column X is empty",
    "empty line",
    "invalid value for X",
    "invalid linkage \"yes\" for gap_type ",

    "X must be a positive integer",
    "object_end is less than object_beg",
    "component_end is less than component_beg",
    "object range length not equal to the gap length",
    "object range length not equal to component range length",

    "duplicate object ",
    "first line of an object must have object_beg=1",
    "first line of an object must have part_number=1",
    "part number (column 4) != previous part number + 1",
    "0 or na component orientation may only be used in a singleton scaffold",

    "object_beg != previous object_end + 1",
    "no valid AGP lines",
    kEmptyCStr, // E_Last
    kEmptyCStr,
    kEmptyCStr,

    // Content Warnings
    "gap at the end of object ",
    "gap at the beginning of object ",
    "two consequtive gap lines (e.g. a gap at the end of "
        "a scaffold, two non scaffold-breaking gaps, ...)",
    "no components in object",
    "the span overlaps a previous span for this component",

    "component span appears out of order",
    "duplicate component with non-draft type",
    "line with component_type X appears to be a gap line and not a component line",
    "line with component_type X appears to be a component line and not a gap line",
    "extra <TAB> character at the end of line",

    "gap line missing column 9 (null)",
    "missing line separator at the end of file",
    kEmptyCStr // W_Last
};

const char* CAgpErr::GetMsg(int code)
{
    return s_msg[code];
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
    // Append warnings to preexisting errors.
    // To collect all warnings, override Msg() in the derived class.
    if(code<E_Last || m_apply_to) {
        m_apply_to |= appliesTo;

        string& m( appliesTo==fAtPrevLine ? m_messages_prev_line : m_messages );
        m += ( code<E_Last ? "\tERROR: " : "\tWARNING: " );
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
const CAgpRow::TStr CAgpRow::gap_types[CAgpRow::eGapCount] = {
    "clone",
    "fragment",
    "repeat",

    "contig",
    "centromere",
    "short_arm",
    "heterochromatin",
    "telomere"
};

CAgpRow::TMapStrEGap* CAgpRow::gap_type_codes=NULL;
DEFINE_CLASS_STATIC_FAST_MUTEX(CAgpRow::init_mutex);

CAgpRow::CAgpRow()
{
    if(gap_type_codes==NULL) {
        // initialize this static map once
        StaticInit();
    }

    m_OwnAgpErr=true;
    m_AgpErr = new CAgpErr;
}

CAgpRow::CAgpRow(CAgpErr* arg)
{
    if(gap_type_codes==NULL) {
        // initialize this static map once
        StaticInit();
    }

    m_OwnAgpErr=false;
    m_AgpErr = arg;
}

CAgpRow::~CAgpRow()
{
    if(m_OwnAgpErr) delete m_AgpErr;
}

void CAgpRow::StaticInit()
{
    CFastMutexGuard guard(init_mutex);
    if(gap_type_codes==NULL) {
        // not initialized while we were waiting for the mutex
        TMapStrEGap* p = new TMapStrEGap();
        for(int i=0; i<eGapCount; i++) {
            (*p)[ (string)gap_types[i] ] = (EGap)i;
        }
        gap_type_codes = p;
    }
}

int CAgpRow::FromString(const string& line)
{
    // Comments
    cols.clear();
    pcomment = NStr::Find(line, "#");

    bool tabsStripped=false;
    if( pcomment != NPOS  ) {
        // Strip whitespace before "#"
        while( pcomment>0 && (line[pcomment-1]==' ' || line[pcomment-1]=='\t') ) {
            if( line[pcomment-1]=='\t' ) tabsStripped=true;
            pcomment--;
        }
        if(pcomment==0) return -1; // A comment line; to be skipped.
        //line.resize(pos);
        NStr::Tokenize(line.substr(0, pcomment), "\t", cols);
    }
    else {
        NStr::Tokenize(line, "\t", cols);
    }

    if(line.size() == 0) {
        m_AgpErr->Msg(CAgpErr::E_EmptyLine);
        return CAgpErr::E_EmptyLine;
    }

    // Column count
    if( cols.size()==10 && cols[9]=="") {
        m_AgpErr->Msg(CAgpErr::W_ExtraTab);
    }
    else if( cols.size() < 8 || cols.size() > 9 ) {
        // skip this entire line, report an error
        m_AgpErr->Msg(CAgpErr::E_ColumnCount,
            string(", found ") + NStr::IntToString(cols.size()) );
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
    object_beg = NStr::StringToNumeric( GetObjectBeg() );
    if(object_beg<=0) m_AgpErr->Msg(CAgpErr::E_MustBePositive, "object_beg (column 2)");
    object_end = NStr::StringToNumeric( GetObjectEnd() );
    if(object_end<=0) {
        m_AgpErr->Msg(CAgpErr::E_MustBePositive, "object_end (column 3)");
    }
    part_number = NStr::StringToNumeric( GetPartNumber() );
    if(part_number<=0) {
        m_AgpErr->Msg(CAgpErr::E_MustBePositive, "part_number (column 4)");
        return CAgpErr::E_MustBePositive;
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
                m_AgpErr->Msg(CAgpErr::W_GapLineMissingCol9);
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
    component_beg = NStr::StringToNumeric( GetComponentBeg() );
    if(component_beg<=0 && log_errors) {
        m_AgpErr->Msg(CAgpErr::E_MustBePositive, "component_beg (column 7)" );
    }
    component_end = NStr::StringToNumeric( GetComponentEnd() );
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
        orientation='n';
        return 0;
    }
    if(GetOrientation().size()==1) {
        orientation=GetOrientation()[0];
        switch( orientation )
        {
            case '+':
            case '-':
            case '0':
                return 0;
        }
    }
    if(log_errors) {
        m_AgpErr->Msg(CAgpErr::E_InvalidValue,"orientation (column 9)");
    }
    return CAgpErr::E_InvalidValue;
}

int CAgpRow::ParseGapCols(bool log_errors)
{
    gap_length = NStr::StringToNumeric( GetGapLength() );
    if(gap_length<=0) {
        if(log_errors) m_AgpErr->Msg(CAgpErr::E_MustBePositive, "gap_length (column 6)" );
        return CAgpErr::E_MustBePositive;
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
        if( gap_type!=eGapClone && gap_type!=eGapRepeat && gap_type!=eGapFragment ) {
            if(log_errors) m_AgpErr->Msg(CAgpErr::E_InvalidYes, GetGapType() );
            return CAgpErr::E_InvalidYes;
        }
    }

    return 0;
}

string CAgpRow::ToString()
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
    }
    else{
        res +=
            GetComponentId  () + "\t" +
            NStr::IntToString(component_beg) + "\t" +
            NStr::IntToString(component_end) + "\t";
        if(orientation=='n') res+="na";
        else res+=orientation;
    }

    return res;
}

string CAgpRow::GetErrorMessage()
{
    return m_AgpErr->GetErrorMessage();
}

void CAgpRow::SetErrorHandler(CAgpErr* arg)
{
    NCBI_ASSERT(!m_OwnAgpErr,
        "CAgpRow -- cannot redefine the default error handler. "
        "Use different constructor, e.g. CAgpRow(NULL)"
    );
    m_AgpErr=arg;
}

//// class CAgpReader
CAgpReader::CAgpReader()
{
    m_OwnAgpErr=true;
    m_AgpErr=new CAgpErr();
    Init();
}

CAgpReader::CAgpReader(CAgpErr* arg)
{
    m_OwnAgpErr=false;
    m_AgpErr=arg;
    Init();
}

void CAgpReader::Init()
{
    m_prev_row=new CAgpRow(m_AgpErr);
    m_this_row=new CAgpRow(m_AgpErr);
    m_at_beg=true;
    m_prev_line_num=-1;
}

CAgpReader::~CAgpReader()
{
    delete m_prev_row;
    delete m_this_row;
    if(m_OwnAgpErr) delete m_AgpErr;
}

bool CAgpReader::ProcessThisRow()
{
    CAgpRow* this_row=m_this_row;;
    CAgpRow* prev_row=m_prev_row;

    m_new_obj=prev_row->GetObject() != this_row->GetObject();
    if(m_new_obj) {
        if(!m_prev_line_skipped) {
            if(this_row->object_beg !=1) m_AgpErr->Msg(m_error_code=CAgpErr::E_ObjMustBegin1, CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine);
            if(this_row->part_number!=1) m_AgpErr->Msg(m_error_code=CAgpErr::E_PartNumberNot1, CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine);
            if(prev_row->is_gap && !prev_row->GapValidAtObjectEnd()) {
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
            if( m_new_obj && !this_row->GapValidAtObjectEnd() ) {
                m_AgpErr->Msg(CAgpErr::W_GapObjBegin, this_row->GetObject()); // , CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine
            }
            else if(prev_row->is_gap) {
                m_AgpErr->Msg(CAgpErr::W_ConseqGaps, CAgpErr::fAtThisLine|CAgpErr::fAtPrevLine);
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

int CAgpReader::ReadStream(CNcbiIstream& is, bool finalize)
{
    m_at_end=false;
    if(m_at_beg) {
        //// The first line
        m_line_num=0;
        m_prev_line_skipped=true;

        // A fictitous empty row that ends with scaffold-breaking gap.
        // Used to:
        // - prevent the two-row checks;
        // - prevent OnScaffoldEnd();
        // - trigger OnObjectChange().
        m_prev_row->cols.clear();
        m_prev_row->cols.push_back(NcbiEmptyString);
        m_prev_row->is_gap=true;
        m_prev_row->gap_type=CAgpRow::eGapContig; // eGapCentromere
        m_prev_row->linkage=false;
    }

    while( NcbiGetline(is, m_line, "\r\n") ) {
        m_line_num++;
        m_error_code=m_this_row->FromString(m_line);
        m_line_skipped=false;
        if(m_error_code==0) {
            if( !ProcessThisRow() ) return m_error_code;
            if( m_error_code < 0 ) break; // A simulated EOF midstream (very rare)
        }
        else if(m_error_code==-1) {
            OnComment();
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

    return finalize ? Finalize() : 0;
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

        CAgpRow* prev_row=m_prev_row;
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
    NCBI_ASSERT(!m_OwnAgpErr,
        "CAgpReader -- cannot redefine the default error handler. "
        "Use different constructor, e.g. CAgpReader(NULL)"
    );
    m_AgpErr=arg;
    m_this_row->SetErrorHandler(arg);
    m_prev_row->SetErrorHandler(arg);
}

string CAgpReader::GetErrorMessage(const string& filename)
{
    string msg;
    if( m_AgpErr->AppliesTo(CAgpErr::fAtPrevLine) ) {
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

END_NCBI_SCOPE


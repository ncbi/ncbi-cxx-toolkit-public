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
* Author:  Aaron Ucko
*
* File Description:
*   Simple program demonstrating the use of serializable objects (in this
*   case, biological sequences).  Does NOT use the object manager.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqtable/seqtable__.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objtools/readers/idmapper.hpp>
#include <corelib/ncbifile.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);

class CWigBufferedLineReader
{
public:
    /// read from the file, "-" (but not "./-") means standard input
    CWigBufferedLineReader(const string& filename);
    virtual ~CWigBufferedLineReader();
    
    bool                AtEOF(void) const;
    CWigBufferedLineReader& operator++(void);
    void                UngetLine(void);
    CTempStringEx       operator*(void) const;
    CT_POS_TYPE         GetPosition(void) const;
    unsigned int        GetLineNumber(void) const;

protected:

    void x_LoadLong(void);
    bool x_ReadBuffer(void);

private:
    AutoPtr<IReader> m_Reader;
    bool          m_Eof;
    bool          m_UngetLine;
    size_t        m_BufferSize;
    AutoArray<char> m_Buffer;
    char*         m_Pos;
    char*         m_End;
    CTempStringEx m_Line;
    string        m_String;
    CT_POS_TYPE   m_InputPos;
    unsigned int  m_LineNumber;
};


CWigBufferedLineReader::CWigBufferedLineReader(const string& filename)
    : m_Reader(CFileReader::New(filename)),
      m_Eof(false),
      m_UngetLine(false),
      m_BufferSize(32*1024),
      m_Buffer(new char[m_BufferSize]),
      m_Pos(m_Buffer.get()),
      m_End(m_Pos),
      m_InputPos(0),
      m_LineNumber(0)
{
    x_ReadBuffer();
}


CWigBufferedLineReader::~CWigBufferedLineReader()
{
}


bool CWigBufferedLineReader::AtEOF(void) const
{
    return m_Eof && !m_UngetLine;
}


void CWigBufferedLineReader::UngetLine(void)
{
    _ASSERT(!m_UngetLine);
    _ASSERT(m_Line.begin());
    --m_LineNumber;
    m_UngetLine = true;
}


CWigBufferedLineReader& CWigBufferedLineReader::operator++(void)
{
    ++m_LineNumber;
    if ( m_UngetLine ) {
        _ASSERT(m_Line.begin());
        m_UngetLine = false;
        return *this;
    }
    // check if we are at the buffer end
    char* start = m_Pos;
    char* end = m_End;
    for ( char* p = start; p < end; ++p ) {
        if ( *p == '\n' ) {
            *p = '\0';
            m_Line.assign(start, p - start, CTempStringEx::eHasZeroAtEnd);
            m_Pos = ++p;
            if ( p == end ) {
                m_String = m_Line;
                m_Line = m_String;
                x_ReadBuffer();
            }
            return *this;
        }
        else if ( *p == '\r' ) {
            *p = '\0';
            m_Line.assign(start, p - start, CTempStringEx::eHasZeroAtEnd);
            if ( ++p == end ) {
                m_String = m_Line;
                m_Line = m_String;
                if ( x_ReadBuffer() ) {
                    p = m_Pos;
                    if ( *p == '\n' ) {
                        m_Pos = p+1;
                    }
                }
                return *this;
            }
            if ( *p != '\n' ) {
                return *this;
            }
            m_Pos = ++p;
            if ( p == end ) {
                m_String = m_Line;
                m_Line = m_String;
                x_ReadBuffer();
            }
            return *this;
        }
    }
    x_LoadLong();
    return *this;
}


void CWigBufferedLineReader::x_LoadLong(void)
{
    char* start = m_Pos;
    char* end = m_End;
    m_String.assign(start, end);
    while ( x_ReadBuffer() ) {
        start = m_Pos;
        end = m_End;
        for ( char* p = start; p < end; ++p ) {
            char c = *p;
            if ( c == '\r' || c == '\n' ) {
                m_String.append(start, p - start);
                m_Line = m_String;
                if ( ++p == end ) {
                    m_String = m_Line;
                    m_Line = m_String;
                    if ( x_ReadBuffer() ) {
                        p = m_Pos;
                        end = m_End;
                        if ( p < end && c == '\r' && *p == '\n' ) {
                            ++p;
                            m_Pos = p;
                        }
                    }
                }
                else {
                    if ( c == '\r' && *p == '\n' ) {
                        if ( ++p == end ) {
                            x_ReadBuffer();
                            p = m_Pos;
                        }
                    }
                    m_Pos = p;
                }
                return;
            }
        }
        m_String.append(start, end - start);
    }
    m_Line = m_String;
    return;
}


bool CWigBufferedLineReader::x_ReadBuffer()
{
    _ASSERT(m_Reader);

    if ( m_Eof ) {
        return false;
    }

    m_InputPos += CT_OFF_TYPE(m_End - m_Buffer.get());
    m_Pos = m_End = m_Buffer.get();
    for (bool flag = true; flag; ) {
        size_t size;
        ERW_Result result =
            m_Reader->Read(m_Buffer.get(), m_BufferSize, &size);
        switch (result) {
        case eRW_NotImplemented:
        case eRW_Error:
            NCBI_THROW(CIOException, eRead, "Read error");
            /*NOTREACHED*/
            break;
        case eRW_Timeout:
            // keep spinning around
            break;
        case eRW_Eof:
            m_Eof = true;
            // fall through
        case eRW_Success:
            m_End = m_Pos + size;
            return (result == eRW_Success  ||  size > 0);
        default:
            _ASSERT(0);
        }
    } // for
    return false;
}


CTempStringEx CWigBufferedLineReader::operator*(void) const
{
    return m_Line;
}


CT_POS_TYPE CWigBufferedLineReader::GetPosition(void) const
{
    return m_InputPos + CT_OFF_TYPE(m_Pos - m_Buffer.get());
}


unsigned int CWigBufferedLineReader::GetLineNumber(void) const
{
    return m_LineNumber;
}


class CWig2tableApplication : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);

    CRef<CSeq_annot> MakeTableAnnot(void);
    void DumpTableAnnot(void);
    void ResetData(void);
    void SetChrom(CTempString chrom);

    double EstimateSize(size_t rows, bool fixed_span) const;

    void ReadBrowser(void);
    void ReadTrack(void);
    void ReadFixedStep(void);
    void ReadVariableStep(void);
    void ReadBedLine(CTempString chrom);

    AutoPtr<CWigBufferedLineReader> m_Input;
    AutoPtr<CObjectOStream> m_Output;

    size_t m_LineNumber;
    CTempStringEx m_FullLine;
    CTempStringEx m_CurLine;
    size_t m_LineBufferSize;
    char* m_LineBufferPtr;
    size_t m_LineBufferLen;
    bool m_UngetLine;
    AutoArray<char> m_LineBuffer;

    bool x_GetLine(void);
    void x_UngetLine(void);
    bool x_SkipWS(void);
    bool x_CommentLine(void) const;
    CTempString x_GetWord(void);
    CTempString x_GetParamName(void);
    CTempString x_GetParamValue(void);
    bool x_TryGetPos(TSeqPos& v);
    bool x_TryGetDoubleSimple(double& v);
    bool x_TryGetDouble(double& v);
    void x_GetPos(TSeqPos& v);
    void x_GetDouble(double& v);

    void x_Error(const char* msg);

    string m_ChromId;
    struct SValueInfo {
        TSeqPos m_Pos;
        TSeqPos m_Span;
        double m_Value;

        bool operator<(const SValueInfo& v) const {
            return m_Pos < v.m_Pos;
        }
    };

    void AddValue(const SValueInfo& value) {
        if ( !m_OmitZeros || value.m_Value != 0 ) {
            m_Values.push_back(value);
        }
    }

    string m_TrackName;
    string m_TrackTypeValue;
    enum ETrackType {
        eTrackType_invalid,
        eTrackType_wiggle_0,
        eTrackType_bedGraph
    };
    ETrackType m_TrackType;
    typedef map<string, string> TTrackParams;
    TTrackParams m_TrackParams;
    typedef vector<SValueInfo> TValues;
    TValues m_Values;

    bool m_OmitZeros;
    bool m_JoinSame;
    bool m_AsByte;
    AutoPtr<IIdMapper> m_IdMapper;
};


void CWig2tableApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Object serialization demo program: Seq-entry translator");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey
        ("in", "InputFile",
         "name of file to read from (standard input by default)",
         CArgDescriptions::eInputFile, "-");

    arg_desc->AddDefaultKey
        ("out", "OutputFile",
         "name of file to write to (standard output by default)",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey("outfmt", "OutputFormat", "format of output file",
                            CArgDescriptions::eString, "asn");
    arg_desc->SetConstraint
        ("outfmt", &(*new CArgAllow_Strings, "asn", "asnb", "xml"));

    arg_desc->AddOptionalKey
        ("mapfile",
         "MapFile",
         "IdMapper config filename",
         CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey
        ("genome", 
         "Genome",
         "UCSC build number",
         CArgDescriptions::eString,
         "");

    arg_desc->AddFlag("byte", "Convert data in byte range");
    arg_desc->AddFlag("omit_zeros", "Omit zero values");
    arg_desc->AddFlag("join_same", "Join equal sequential values");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


static ESerialDataFormat s_GetFormat(const string& name)
{
    if (name == "asn") {
        return eSerial_AsnText;
    } else if (name == "asnb") {
        return eSerial_AsnBinary;
    } else if (name == "xml") {
        return eSerial_Xml;
    } else {
        // Should be caught by argument processing, but as an illustration...
        THROW1_TRACE(runtime_error, "Bad serial format name " + name);
    }
}


double CWig2tableApplication::EstimateSize(size_t rows, bool fixed_span) const
{
    double ret = 0;
    ret += rows*4;
    if ( !fixed_span )
        ret += rows*4;
    if ( m_AsByte )
        ret += rows;
    else
        ret += 8*rows;
    return ret;
}


CRef<CSeq_annot> CWig2tableApplication::MakeTableAnnot(void)
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    if ( !m_TrackName.empty() ) {
        CRef<CAnnotdesc> desc(new CAnnotdesc);
        desc->SetName(m_TrackName);
        annot->SetDesc().Set().push_back(desc);
    }
    if ( !m_TrackParams.empty() ) {
        CRef<CAnnotdesc> desc(new CAnnotdesc);
        annot->SetDesc().Set().push_back(desc);
        CUser_object& user = desc->SetUser();
        user.SetType().SetStr("Track Data");
        ITERATE ( TTrackParams, it, m_TrackParams ) {
            CRef<CUser_field> field(new CUser_field);
            field->SetLabel().SetStr(it->first);
            field->SetData().SetStr(it->second);
            user.SetData().push_back(field);
        }
    }
    CSeq_table& table = annot->SetData().SetSeq_table();

    size_t size = m_Values.size();

    table.SetFeat_type(0);

    CRef<CSeq_id> chrom_id(new CSeq_id(CSeq_id::e_Local, m_ChromId));
    if ( m_IdMapper ) {
        m_IdMapper->MapObject(*chrom_id);
    }

    CRef<CSeq_loc> table_loc(new CSeq_loc);
    { // Seq-table location
        CRef<CSeqTable_column> col_id(new CSeqTable_column);
        table.SetColumns().push_back(col_id);
        col_id->SetHeader().SetField_name("Seq-table location");
        col_id->SetDefault().SetLoc(*table_loc);
    }

    { // Seq-id
        CRef<CSeqTable_column> col_id(new CSeqTable_column);
        table.SetColumns().push_back(col_id);
        col_id->SetHeader().SetField_id(CSeqTable_column_info::eField_id_location_id);
        col_id->SetDefault().SetId(*chrom_id);
    }
    
    // position
    CRef<CSeqTable_column> col_pos(new CSeqTable_column);
    table.SetColumns().push_back(col_pos);
    col_pos->SetHeader().SetField_id(CSeqTable_column_info::eField_id_location_from);
    CSeqTable_multi_data::TInt& pos = col_pos->SetData().SetInt();
    
    TSeqPos span = 1;
    double min = 0, max = 0, step = 1;
    bool fixed_span = true, sorted = true;
    { // analyze
        if ( size ) {
            span = m_Values[0].m_Span;
            min = max = m_Values[0].m_Value;
        }
        for ( size_t i = 1; i < size; ++i ) {
            if ( m_Values[i].m_Span != span ) {
                fixed_span = false;
            }
            if ( m_Values[i].m_Pos < m_Values[i-1].m_Pos ) {
                sorted = false;
            }
            double v = m_Values[i].m_Value;
            if ( v < min ) {
                min = v;
            }
            if ( v > max ) {
                max = v;
            }
        }
        if ( max > min ) {
            step = (max-min)/255;
        }
    }

    if ( !sorted ) {
        sort(m_Values.begin(), m_Values.end());
    }

    if ( m_Values.empty() ) {
        table_loc->SetEmpty(*chrom_id);
    }
    else {
        CSeq_interval& interval = table_loc->SetInt();
        interval.SetId(*chrom_id);
        interval.SetFrom(m_Values.front().m_Pos);
        interval.SetTo(m_Values.back().m_Pos + m_Values.back().m_Span - 1);
    }

    if ( m_JoinSame && size ) {
        TValues nv;
        nv.reserve(size);
        nv.push_back(m_Values[0]);
        for ( size_t i = 1; i < size; ++i ) {
            if ( m_Values[i].m_Pos == nv.back().m_Pos+nv.back().m_Span &&
                 m_Values[i].m_Value == nv.back().m_Value ) {
                nv.back().m_Span += m_Values[i].m_Span;
            }
            else {
                nv.push_back(m_Values[i]);
            }
        }
        if ( nv.size() != size ) {
            double s = EstimateSize(size, fixed_span);
            double ns = EstimateSize(nv.size(), false);
            if ( ns < s*.75 ) {
                m_Values.swap(nv);
                size = m_Values.size();
                LOG_POST("Joined size: "<<size);
                fixed_span = false;
            }
        }
    }

    table.SetNum_rows(size);
    pos.reserve(size);

    CSeqTable_multi_data::TInt* span_ptr = 0;
    { // span
        CRef<CSeqTable_column> col_span(new CSeqTable_column);
        table.SetColumns().push_back(col_span);
        col_span->SetHeader().SetField_name("span");
        if ( fixed_span ) {
            col_span->SetDefault().SetInt(span);
        }
        else {
            span_ptr = &col_span->SetData().SetInt();
            span_ptr->reserve(size);
        }
    }

    if ( m_AsByte ) { // values
        CRef<CSeqTable_column> col_min(new CSeqTable_column);
        table.SetColumns().push_back(col_min);
        col_min->SetHeader().SetField_name("value_min");
        col_min->SetDefault().SetReal(min);

        CRef<CSeqTable_column> col_step(new CSeqTable_column);
        table.SetColumns().push_back(col_step);
        col_step->SetHeader().SetField_name("value_step");
        col_step->SetDefault().SetReal(step);

        CRef<CSeqTable_column> col_val(new CSeqTable_column);
        table.SetColumns().push_back(col_val);
        col_val->SetHeader().SetField_name("values");
        
        double mul = 1/step;
        if ( 1 ) {
            AutoPtr< vector<char> > values(new vector<char>());
            values->reserve(size);
            ITERATE ( TValues, it, m_Values ) {
                pos.push_back(it->m_Pos);
                if ( span_ptr ) {
                    span_ptr->push_back(it->m_Span);
                }
                int val = int((it->m_Value-min)*mul+.5);
                values->push_back(val);
            }
            col_val->SetData().SetBytes().push_back(values.release());
        }
        else {
            CSeqTable_multi_data::TInt& values = col_val->SetData().SetInt();
            values.reserve(size);
            
            ITERATE ( TValues, it, m_Values ) {
                pos.push_back(it->m_Pos);
                if ( span_ptr ) {
                    span_ptr->push_back(it->m_Span);
                }
                int val = int((it->m_Value-min)*mul+.5);
                values.push_back(val);
            }
        }
    }
    else {
        CRef<CSeqTable_column> col_val(new CSeqTable_column);
        table.SetColumns().push_back(col_val);
        col_val->SetHeader().SetField_name("values");
        CSeqTable_multi_data::TReal& values = col_val->SetData().SetReal();
        values.reserve(size);
        
        ITERATE ( TValues, it, m_Values ) {
            pos.push_back(it->m_Pos);
            if ( span_ptr ) {
                span_ptr->push_back(it->m_Span);
            }
            values.push_back(it->m_Value);
        }
    }
    return annot;
}


void CWig2tableApplication::DumpTableAnnot(void)
{
    LOG_POST("DumpTableAnnot: "<<m_ChromId<<" "<<m_Values.size());
    CRef<CSeq_annot> annot = MakeTableAnnot();
    *m_Output << *annot;
}


void CWig2tableApplication::ResetData(void)
{
    m_ChromId.clear();
    m_Values.clear();
}


void CWig2tableApplication::x_Error(const char* msg)
{
    ERR_POST(Fatal<<GetArgs()["in"].AsString()<<":"<<m_LineNumber<<": "<<msg<<
             ": \""<<m_CurLine<<"\"");
}


bool CWig2tableApplication::x_SkipWS(void)
{
    const char* ptr = m_CurLine.data();
    size_t skip = 0;
    for ( size_t len = m_CurLine.size(); skip < len; ++skip ) {
        char c = ptr[skip];
        if ( c != ' ' && c != '\t' ) {
            break;
        }
    }
    m_CurLine = m_CurLine.substr(skip);
    return !m_CurLine.empty();
}


inline bool CWig2tableApplication::x_CommentLine(void) const
{
    char c = m_CurLine.data()[0];
    return c == '#' || c == '\0';
}


CTempString CWig2tableApplication::x_GetWord(void)
{
    const char* ptr = m_CurLine.data();
    size_t skip = 0;
    for ( size_t len = m_CurLine.size(); skip < len; ++skip ) {
        char c = ptr[skip];
        if ( c == ' ' || c == '\t' ) {
            break;
        }
    }
    if ( skip == 0 ) {
        x_Error("Identifier expected");
    }
    m_CurLine = m_CurLine.substr(skip);
    return CTempString(ptr, skip);
}


CTempString CWig2tableApplication::x_GetParamName(void)
{
    const char* ptr = m_CurLine.data();
    size_t skip = 0;
    for ( size_t len = m_CurLine.size(); skip < len; ++skip ) {
        char c = ptr[skip];
        if ( c == '=' ) {
            m_CurLine = m_CurLine.substr(skip+1);
            return CTempString(ptr, skip);
        }
        if ( c == ' ' || c == '\t' ) {
            break;
        }
    }
    x_Error("'=' expected");
    return CTempString();
}


CTempString CWig2tableApplication::x_GetParamValue(void)
{
    const char* ptr = m_CurLine.data();
    size_t len = m_CurLine.size();
    if ( len && *ptr == '"' ) {
        size_t pos = 1;
        for ( ; pos < len; ++pos ) {
            char c = ptr[pos];
            if ( c == '"' ) {
                m_CurLine = m_CurLine.substr(pos+1);
                return CTempString(ptr+1, pos-1);
            }
        }
        x_Error("Open quotes");
    }
    return x_GetWord();
}


void CWig2tableApplication::x_GetPos(TSeqPos& v)
{
    TSeqPos ret = 0;
    const char* ptr = m_CurLine.data();
    for ( size_t skip = 0; ; ++skip ) {
        char c = ptr[skip];
        if ( c >= '0' && c <= '9' ) {
            ret = ret*10 + (c-'0');
        }
        else if ( (c == ' ' || c == '\t' || c == '\0') && skip ) {
            m_CurLine = m_CurLine.substr(skip);
            v = ret;
            return;
        }
        else {
            x_Error("Integer value expected");
        }
    }
}


bool CWig2tableApplication::x_TryGetDoubleSimple(double& v)
{
    double ret = 0;
    const char* ptr = m_CurLine.data();
    size_t skip = 0;
    bool negate = false, digits = false;
    for ( ; ; ++skip ) {
        char c = ptr[skip];
        if ( !skip ) {
            if ( c == '-' ) {
                negate = true;
                continue;
            }
            if ( c == '+' ) {
                continue;
            }
        }
        if ( c >= '0' && c <= '9' ) {
            digits = true;
            ret = ret*10 + (c-'0');
        }
        else if ( c == '.' ) {
            ++skip;
            break;
        }
        else if ( c == '\0' ) {
            if ( !digits ) {
                return false;
            }
            m_CurLine.clear();
            v = ret;
            return true;
        }
        else {
            return false;
        }
    }
    double digit_mul = 1;
    for ( ; ; ++skip ) {
        char c = ptr[skip];
        if ( c >= '0' && c <= '9' ) {
            digits = true;
            digit_mul *= .1;
            ret += (c-'0')*digit_mul;
        }
        else if ( (c == ' ' || c == '\t' || c == '\0') && digits ) {
            m_CurLine.clear();
            v = ret;
            return true;
        }
        else {
            return false;
        }
    }
}


bool CWig2tableApplication::x_TryGetDouble(double& v)
{
    if ( x_TryGetDoubleSimple(v) ) {
        return true;
    }
    const char* ptr = m_CurLine.data();
    char* endptr = 0;
    v = strtod(ptr, &endptr);
    if ( endptr == ptr ) {
        return false;
    }
    if ( *endptr ) {
        x_Error("extra text in line");
    }
    m_CurLine.clear();
    return true;
}


inline bool CWig2tableApplication::x_TryGetPos(TSeqPos& v)
{
    char c = m_CurLine[0];
    if ( c < '0' || c > '9' ) {
        return false;
    }
    x_GetPos(v);
    return true;
}


inline void CWig2tableApplication::x_GetDouble(double& v)
{
    if ( !x_TryGetDouble(v) ) {
        x_Error("Floating point value expected");
    }
}


bool CWig2tableApplication::x_GetLine()
{
    if ( m_Input->AtEOF() ) {
        return false;
    }
    m_FullLine = m_CurLine = *++*m_Input;
    return true;
}


inline void CWig2tableApplication::x_UngetLine(void)
{
    m_Input->UngetLine();
}


void CWig2tableApplication::SetChrom(CTempString chrom)
{
    if ( chrom != m_ChromId ) {
        if ( !m_ChromId.empty() ) {
            DumpTableAnnot();
        }
        ResetData();
        m_ChromId = chrom;
    }
}


void CWig2tableApplication::ReadBrowser(void)
{
}


void CWig2tableApplication::ReadTrack(void)
{
    m_TrackName = "User Track";
    m_TrackTypeValue.clear();
    m_TrackType = eTrackType_invalid;
    m_TrackParams.clear();
    while ( x_SkipWS() ) {
        CTempString name = x_GetParamName();
        CTempString value = x_GetParamValue();
        if ( name == "type" ) {
            m_TrackTypeValue = value;
            if ( value == "wiggle_0" ) {
                m_TrackType = eTrackType_wiggle_0;
            }
            else if ( value == "bedGraph" ) {
                m_TrackType = eTrackType_bedGraph;
            }
            else {
                x_Error("Invalid track type");
            }
        }
        else if ( name == "name" ) {
            m_TrackName = value;
        }
        else {
            m_TrackParams[name] = value;
        }
    }
    if ( m_TrackType == eTrackType_invalid ) {
        x_Error("Unknown track type");
    }
}


void CWig2tableApplication::ReadFixedStep(void)
{
    if ( m_TrackType != eTrackType_wiggle_0 &&
         m_TrackType != eTrackType_invalid ) {
        x_Error("Track type=wiggle_0 is required");
    }

    size_t start = 0;
    size_t step = 0;
    size_t span = 1;
    while ( x_SkipWS() ) {
        CTempString name = x_GetParamName();
        CTempString value = x_GetParamValue();
        if ( name == "chrom" ) {
            SetChrom(value);
        }
        else if ( name == "start" ) {
            start = NStr::StringToUInt(value);
        }
        else if ( name == "step" ) {
            step = NStr::StringToUInt(value);
        }
        else if ( name == "span" ) {
            span = NStr::StringToUInt(value);
        }
        else {
            x_Error("Bad param name");
        }
    }
    if ( m_ChromId.empty() ) {
        x_Error("No chrom");
    }
    if ( start == 0 ) {
        x_Error("No start");
    }
    if ( step == 0 ) {
        x_Error("No step");
    }

    SValueInfo value;
    value.m_Pos = start-1;
    value.m_Span = span;
    while ( x_GetLine() ) {
        if ( !x_TryGetDouble(value.m_Value) ) {
            x_UngetLine();
            break;
        }
        AddValue(value);
        value.m_Pos += step;
    }
}


void CWig2tableApplication::ReadVariableStep(void)
{
    if ( m_TrackType != eTrackType_wiggle_0 &&
         m_TrackType != eTrackType_invalid ) {
        x_Error("Track type=wiggle_0 is required");
    }

    size_t span = 1;
    while ( x_SkipWS() ) {
        CTempString name = x_GetParamName();
        CTempString value = x_GetParamValue();
        if ( name == "chrom" ) {
            SetChrom(value);
        }
        else if ( name == "span" ) {
            span = NStr::StringToUInt(value);
        }
        else {
            x_Error("Bad param name");
        }
    }
    if ( m_ChromId.empty() ) {
        x_Error("No chrom");
    }
    SValueInfo value;
    value.m_Span = span;
    while ( x_GetLine() ) {
        if ( !x_TryGetPos(value.m_Pos) ) {
            x_UngetLine();
            break;
        }
        x_SkipWS();
        x_GetDouble(value.m_Value);
        value.m_Pos -= 1;
        AddValue(value);
    }
}


void CWig2tableApplication::ReadBedLine(CTempString chrom)
{
    if ( m_TrackType != eTrackType_bedGraph &&
         m_TrackType != eTrackType_invalid ) {
        x_Error("Track type=bedGraph is required");
    }
    SetChrom(chrom);
    SValueInfo value;
    x_SkipWS();
    x_GetPos(value.m_Pos);
    x_SkipWS();
    x_GetPos(value.m_Span);
    x_SkipWS();
    x_GetDouble(value.m_Value);
    value.m_Span -= value.m_Pos;
    AddValue(value);
}


int CWig2tableApplication::Run(void)
{
    // Get arguments
    CArgs args = GetArgs();

    if ( args["mapfile"] ) {
        m_IdMapper.reset(new CIdMapperConfig(args["mapfile"].AsInputFile(),
                                             args["genome"].AsString(),
                                             false));
    }
    else if ( !args["genome"].AsString().empty() ) {
        LOG_POST("Genome: "<<args["genome"].AsString());
        m_IdMapper.reset(new CIdMapperBuiltin(args["genome"].AsString(),
                                              false));
    }

    m_OmitZeros = args["omit_zeros"];
    m_JoinSame = args["join_same"];
    m_AsByte = args["byte"];

    // Read the entry
    m_Input.reset(new CWigBufferedLineReader(args["in"].AsString()));
    m_Output.reset
        (CObjectOStream::Open(s_GetFormat(args["outfmt"].AsString()),
                              args["out"].AsOutputFile()));

    while ( x_GetLine() ) {
        if ( x_CommentLine() ) {
            continue;
        }
        CTempString s = x_GetWord();
        if ( s == "browser" ) {
            ReadBrowser();
        }
        else if ( s == "track" ) {
            ReadTrack();
        }
        else if ( s == "fixedStep" ) {
            ReadFixedStep();
        }
        else if ( s == "variableStep" ) {
            ReadVariableStep();
        }
        else {
            ReadBedLine(s);
        }
    }
    m_Input.reset();

    SetChrom(""); // flush current graph
    m_Output.reset();

    // Exit successfully
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CWig2tableApplication().AppMain(argc, argv);
}

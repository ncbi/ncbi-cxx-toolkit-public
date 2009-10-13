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

#include <objects/seqtable/seqtable__.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objtools/readers/idmapper.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);

class CWig2tableApplication : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);

    CRef<CSeq_annot> MakeTableAnnot(void);
    void DumpTableAnnot(void);
    void ResetData(void);
    void SetChrom(const string& chrom);

    void SkipEOL(void);
    string GetLine(void);
    pair<string, string> GetParam(const string& s) const;
    double EstimateSize(size_t rows, bool fixed_span) const;

    void ReadBrowser(void);
    void ReadTrack(void);
    void ReadFixedStep(void);
    void ReadVariableStep(void);
    void ReadBedLine(const char* chrom);

    FILE* m_Input;
    AutoPtr<CObjectOStream> m_Output;

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


inline void CWig2tableApplication::SkipEOL(void)
{
    int c;
    while ( (c = fgetc(m_Input)) != EOF && c != '\n' ) {
        if ( !isspace(c) ) {
            ERR_POST(Fatal << "Extra text in line: "<<char(c));
        }
    }
}


string CWig2tableApplication::GetLine()
{
    string line;
    char buf[1024];
    while ( !feof(m_Input) ) {
        fgets(buf, sizeof(buf), m_Input);
        size_t len = strlen(buf);
        if ( len && buf[len-1] == '\n' ) {
            --len;
            if ( len && buf[len-1] == '\r' ) {
                --len;
            }
            buf[len] = '\0';
            line += buf;
            break;
        }
        line += buf;
    }
    return line;
}


pair<string, string> CWig2tableApplication::GetParam(const string& s) const
{
    pair<string, string> ret;
    size_t eq = s.find('=');
    if ( eq == NPOS ) {
        ERR_POST(Fatal<<"Bad param: "<<s);
    }
    ret.first = s.substr(0, eq);
    ret.second = s.substr(eq+1);
    size_t s2 = ret.second.size();
    if ( s2 >= 2 && ret.second[0] == '"' && ret.second[s2-1] == '"' ) {
        ret.second = ret.second.substr(1, s2-2);
    }
    return ret;
}


void CWig2tableApplication::SetChrom(const string& chrom)
{
    if ( chrom != m_ChromId ) {
        if ( !m_ChromId.empty() ) {
            DumpTableAnnot();
        }
        ResetData();
    }
    m_ChromId = chrom;
}


void CWig2tableApplication::ReadBrowser(void)
{
    GetLine();
}


void CWig2tableApplication::ReadTrack(void)
{
    GetLine();
}


void CWig2tableApplication::ReadFixedStep(void)
{
    vector<string> ss;
    string line = GetLine();
    NStr::Tokenize(line, " \t", ss);

    size_t start = 0;
    size_t step = 0;
    size_t span = 1;
    for ( size_t i = 1; i < ss.size(); ++i ) {
        pair<string, string> p = GetParam(ss[i]);
        if ( p.first == "chrom" ) {
            SetChrom(p.second);
        }
        else if ( p.first == "start" ) {
            start = NStr::StringToUInt(p.second);
        }
        else if ( p.first == "step" ) {
            step = NStr::StringToUInt(p.second);
        }
        else if ( p.first == "span" ) {
            span = NStr::StringToUInt(p.second);
        }
        else {
            ERR_POST(Fatal << "Bad param name: " << line);
        }
    }
    if ( m_ChromId.empty() ) {
        ERR_POST(Fatal << "No chrom: " << line);
    }
    if ( start == 0 ) {
        ERR_POST(Fatal << "No start: " << line);
    }
    if ( step == 0 ) {
        ERR_POST(Fatal << "No step: " << line);
    }

    SValueInfo value;
    value.m_Pos = start;
    value.m_Span = span;
    while ( fscanf(m_Input, "%lf", &value.m_Value) == 1 ) {
        SkipEOL();
        AddValue(value);
        value.m_Pos += step;
    }
}


void CWig2tableApplication::ReadVariableStep(void)
{
    vector<string> ss;
    string line = GetLine();
    NStr::Tokenize(line, " \t", ss);

    size_t span = 1;
    for ( size_t i = 1; i < ss.size(); ++i ) {
        pair<string, string> p = GetParam(ss[i]);
        if ( p.first == "chrom" ) {
            SetChrom(p.second);
        }
        else if ( p.first == "span" ) {
            span = NStr::StringToUInt(p.second);
        }
        else {
            ERR_POST(Fatal << "Bad param name: " << line);
        }
    }
    if ( m_ChromId.empty() ) {
        ERR_POST(Fatal << "No chrom: " << line);
    }
    SValueInfo value;
    value.m_Span = span;
    while ( fscanf(m_Input, "%u%lf", &value.m_Pos, &value.m_Value) == 2 ) {
        SkipEOL();
        AddValue(value);
    }
}


void CWig2tableApplication::ReadBedLine(const char* chrom)
{
    if ( m_ChromId != chrom ) {
        SetChrom(chrom);
    }
    SValueInfo value;
    if ( fscanf(m_Input, "%u%u%lf",
                &value.m_Pos, &value.m_Span, &value.m_Value) != 3 ) {
        ERR_POST(Fatal << "Failed to parse BED graph line");
    }
    SkipEOL();
    value.m_Span -= value.m_Pos;
    value.m_Pos += 1;
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
    m_Input = fopen(args["in"].AsString().c_str(), "rt");
    if ( !m_Input ) {
        ERR_POST(Fatal << "Cannot open file: "<<args["in"].AsString());
    }

    m_Output.reset
        (CObjectOStream::Open(s_GetFormat(args["outfmt"].AsString()),
                              args["out"].AsOutputFile()));

    const size_t BAD_ID_LEN = 1023;
    const char* BAD_ID_FMT = "%1023s"; // same number as in BAD_ID_LEN
    char buf[BAD_ID_LEN+1];
    while ( !feof(m_Input) && fscanf(m_Input, BAD_ID_FMT, buf) == 1 ) {
        if ( strlen(buf) >= BAD_ID_LEN ) {
            ERR_POST(Fatal << "Too long identifier: "<<buf);
        }
        if ( buf[0] == '#' ) { // comment line
            GetLine();
        }
        else if ( strcmp(buf, "browser") == 0 ) {
            ReadBrowser();
        }
        else if ( strcmp(buf, "track") == 0 ) {
            ReadTrack();
        }
        else if ( strcmp(buf, "fixedStep") == 0 ) {
            ReadFixedStep();
        }
        else if ( strcmp(buf, "variableStep") == 0 ) {
            ReadVariableStep();
        }
        else {
            ReadBedLine(buf);
        }
    }
    fclose(m_Input);
    m_Input = 0;

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

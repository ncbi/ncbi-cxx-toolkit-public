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
 * Author:  Justin Foley
 *
 * File Description:
 *   asnvalidate output formatters
 *
 */

#include <ncbi_pch.hpp>
#include <objects/valerr/ValidErrItem.hpp>
#include <serial/objostrxml.hpp>
#include "app_config.hpp"
#include "formatters.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

class CStandardFormat : public IFormatter {

public:
    CStandardFormat(CNcbiOstream& ostr) : IFormatter(ostr) {}
    virtual ~CStandardFormat(){}

    void operator()(const objects::CValidErrItem& item) override;
};


class CSpacedFormat : public IFormatter {
    
public:
    CSpacedFormat(CNcbiOstream& ostr) : IFormatter(ostr) {}
    virtual ~CSpacedFormat(){}

    void operator()(const objects::CValidErrItem& item) override;
};


class CTabbedFormat : public IFormatter {

public:
    CTabbedFormat(CNcbiOstream& ostr) : IFormatter(ostr) {}
    virtual ~CTabbedFormat(){}

    void operator()(const objects::CValidErrItem& item) override;
};


class CXmlFormat : public IFormatter, public CObjectOStreamXml {

public:
    CXmlFormat(EDiagSev lowCuttof, CNcbiOstream& ostr);
    virtual ~CXmlFormat(){}

    void Start() override;

    void operator()(const objects::CValidErrItem& item) override;
    
    void Finish() override;

private:
    EDiagSev m_LowCutoff;

};


static string s_GetSeverityLabel(EDiagSev sev, bool is_xml=false)
{
    static const string str_sev[] = {
        "NOTE", "WARNING", "ERROR", "REJECT", "FATAL", "MAX"
    };
    if (sev < 0 || sev > eDiagSevMax) {
        return "NONE";
    }
    if (sev == 0 && is_xml) {
        return "INFO";
    }
    return str_sev[sev];
}


void CStandardFormat::operator()(const CValidErrItem& item) 
{
    m_Ostr << s_GetSeverityLabel(item.GetSeverity())
           << ": valid [" << item.GetErrGroup() << "." << item.GetErrCode() << "] "
           << item.GetMsg();
    if (item.IsSetObjDesc()) {
        m_Ostr << " " << item.GetObjDesc();
    }
    m_Ostr << "\n";
}


void CSpacedFormat::operator()(const CValidErrItem& item) 
{
    string spacer = "                    ";
    string msg = item.GetAccnver() + spacer;
    msg = msg.substr(0, 15);
    msg += s_GetSeverityLabel(item.GetSeverity());
    msg += spacer;
    msg = msg.substr(0, 30);
    msg += item.GetErrGroup() + "_" + item.GetErrCode();
    m_Ostr << msg << "\n";
}


void CTabbedFormat::operator()(const CValidErrItem& item) 
{
    m_Ostr << item.GetAccnver() << "\t"
        << s_GetSeverityLabel(item.GetSeverity()) << "\t"
        << item.GetErrGroup() << "_" << item.GetErrCode() << "\n";
}


CXmlFormat::CXmlFormat(EDiagSev lowCutoff, CNcbiOstream& ostr) : 
    IFormatter(ostr), CObjectOStreamXml(ostr, eNoOwnership),
    m_LowCutoff(lowCutoff)
{
    SetEncoding(eEncoding_UTF8);
    SetReferenceDTD(false);
    SetEnforcedStdXml(true);
}


void CXmlFormat::Start() 
{
    WriteFileHeader(CValidErrItem::GetTypeInfo());
    SetUseIndentation(true); // Move to constructor?
    Flush();
    m_Ostr << "\n" << "<asnvalidate version=\"" << "3." << NCBI_SC_VERSION_PROXY << "."
        << NCBI_TEAMCITY_BUILD_NUMBER_PROXY << "\" severity_cutoff=\""
        << s_GetSeverityLabel(m_LowCutoff, true) << "\">" << "\n";
}


void CXmlFormat::operator()(const CValidErrItem& item)
{
    m_Output.PutString("  <message severity=\"");
    m_Output.PutString(s_GetSeverityLabel(item.GetSeverity(), true));
    if (item.IsSetAccnver()) {
        m_Output.PutString("\" seq-id=\"");
        WriteString(item.GetAccnver(), eStringTypeVisible);
    }

    if (item.IsSetFeatureId()) {
        m_Output.PutString("\" feat-id=\"");
        WriteString(item.GetFeatureId(), eStringTypeVisible);
    }

    if (item.IsSetLocation()) {
        m_Output.PutString("\" interval=\"");
        string loc = item.GetLocation();
        if (NStr::StartsWith(loc, "(") && NStr::EndsWith(loc, ")")) {
            loc = "[" + loc.substr(1, loc.length() - 2) + "]";
        }
        WriteString(loc, eStringTypeVisible);
    }

    m_Output.PutString("\" code=\"");
    WriteString(item.GetErrGroup(), eStringTypeVisible);
    m_Output.PutString("_");
    WriteString(item.GetErrCode(), eStringTypeVisible);
    m_Output.PutString("\">");

    WriteString(item.GetMsg(), eStringTypeVisible);

    m_Output.PutString("</message>");
    m_Output.PutEol();
}


void CXmlFormat::Finish() 
{
    FlushBuffer(); // Needed to enure that the body of the XML is written before the closing tag
    m_Ostr << "</asnvalidate>" << endl; // flush output stream
}


unique_ptr<IFormatter> g_CreateFormatter(const CAppConfig& config, CNcbiOstream& ostr) 
{
    switch (config.mVerbosity) {
    case CAppConfig::eVerbosity_Normal:
        return make_unique<CStandardFormat>(ostr);
    case CAppConfig::eVerbosity_Spaced:
        return make_unique<CSpacedFormat>(ostr);
    case CAppConfig::eVerbosity_Tabbed:
        return make_unique<CTabbedFormat>(ostr);
    case CAppConfig::eVerbosity_XML:
        return make_unique<CXmlFormat>(config.mLowCutoff, ostr);
    default:
        break;
    }
    return make_unique<CStandardFormat>(ostr);
}

END_NCBI_SCOPE

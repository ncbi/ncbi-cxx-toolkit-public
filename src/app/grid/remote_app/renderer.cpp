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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 *
 */

#include <ncbi_pch.hpp>

#include "renderer.hpp"

BEGIN_NCBI_SCOPE
///////////////////////////////////////////////////////
//
void CTextTagWriter::x_WriteTag(const string& name,
                                 const TAttributes* attr,
                                 const string& text,
                                 bool close_tag)
{
    m_Os << name;
    if (attr && !attr->empty() ) {
        m_Os << " {";
        ITERATE(TAttributes, it, *attr) {
            if (it != attr->begin())
                m_Os << ", ";
            m_Os << it->first << "=" << it->second;
        }
        m_Os << "}";
    }
    m_Os << " : " << text << NcbiEndl;
}

void CTextTagWriter::x_WriteCloseTag(const string& name)
{
}
void CTextTagWriter::x_WriteText(const string& text)
{
    m_Os << text << NcbiEndl;
}
void CTextTagWriter::x_WriteStream(CNcbiIstream& is)
{
    NcbiStreamCopy(m_Os, is);
    m_Os << NcbiEndl;
}

///////////////////////////////////////////////////////
//
CXmlTagWriter::CXmlTagWriter(CNcbiOstream& os, const string& indent)
    : m_Os(os), m_IndentCount(0), m_IndentString(indent)
{
    m_Os << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>" << NcbiEndl;
    WriteBeginTag("NSJobsInfo");
}
CXmlTagWriter::~CXmlTagWriter()
{
    try {
        WriteCloseTag("NSJobsInfo");
    } catch (...) {}
}
void CXmlTagWriter::x_WriteTag(const string& name,
                               const TAttributes* attr,
                               const string& text,
                               bool close_tag)
{
    x_WriteIndent();
    m_Os << "<" << name;
    if (attr) {
        ITERATE(TAttributes, it, *attr) {
            m_Os << " " << it->first << "=\"" << it->second << "\"";
        }
    }
    if (!text.empty()) {
        m_Os << ">" << text  << "</" << name << ">" << NcbiEndl;
    } else {
        if (close_tag)
            m_Os << "/>";
        else {
            m_Os << ">";
            ++m_IndentCount;
        }
        m_Os << NcbiEndl;
    }
}

void CXmlTagWriter::x_WriteCloseTag(const string& name)
{
    if (m_IndentCount > 0)
        --m_IndentCount;
    x_WriteIndent();
    m_Os << "</" << name << ">" << NcbiEndl;
}

void CXmlTagWriter::x_WriteText(const string& text)
{
    m_Os << text << NcbiEndl;
}
void CXmlTagWriter::x_WriteStream(CNcbiIstream& is)
{
    NcbiStreamCopy(m_Os, is);
    m_Os << NcbiEndl;
}

void  CXmlTagWriter::x_WriteIndent()
{
    for(int i = 0; i < m_IndentCount; ++i)
        m_Os << m_IndentString;
}
///////////////////////////////////////////////////////
//
CNSInfoRenderer::CNSInfoRenderer(ITagWriter& writer, CNSInfoCollector& collector)
    : m_Writer(writer), m_Collector(collector)
{
}
CNSInfoRenderer::~CNSInfoRenderer()
{
}

struct STagGuard
{
    STagGuard(ITagWriter& writer, const string& tag,
             const ITagWriter::TAttributes& attrs)
        : m_Writer(writer), m_Tag(tag)
    {
        m_Writer.WriteBeginTag(m_Tag, attrs);
    }
    STagGuard(ITagWriter& writer, const string& tag)
        : m_Writer(writer), m_Tag(tag)
    {
        m_Writer.WriteBeginTag(m_Tag);
    }
    ~STagGuard()
    {
        m_Writer.WriteCloseTag(m_Tag);
    }
    ITagWriter& m_Writer;
    string m_Tag;
};

class CNSJobListRenderAction : public  CNSInfoCollector::IAction<CNSJobInfo>
{
public:
    CNSJobListRenderAction(CNSInfoRenderer& renderer,
                           CNSInfoRenderer::TFlags flags =
                           CNSInfoRenderer::eMinimal)
        : m_Renderer(renderer), m_Flags(flags) {}

    virtual ~CNSJobListRenderAction() {};

    virtual void operator()(const CNSJobInfo& info)
    {
        m_Renderer.RenderJob(info, m_Flags);
    }
private:
    CNSInfoRenderer& m_Renderer;
    CNSInfoRenderer::TFlags m_Flags;
};


void CNSInfoRenderer::RenderJob(const string& job_id, TFlags flags)
{
    auto_ptr<CNSJobInfo> info(m_Collector.CreateJobInfo(job_id));
    RenderJob(*info, flags);
}

void CNSInfoRenderer::RenderJob(const CNSJobInfo& info, TFlags flags)
{
    CNetScheduleAPI::EJobStatus status = info.GetStatus();
    ITagWriter::TAttributes attrs;
    attrs.push_back(ITagWriter::TAttribute("Id", info.GetId()));
    if (flags & eStatus)
        attrs.push_back(ITagWriter::TAttribute
                        ("Status",
                         CNetScheduleAPI::StatusToString(status) ));

    if (flags & eRetCode && (status == CNetScheduleAPI::eDone ||
                             status == CNetScheduleAPI::eFailed) )
        attrs.push_back(ITagWriter::TAttribute
                        ("RetCode",
                         NStr::IntToString(info.GetRetCode()) ));


    if ( flags < eCmdLine) {
        m_Writer.WriteTag("Job", attrs);
        return;
    }

    STagGuard guard(m_Writer,"Job", attrs);
    if (flags & eCmdLine) {
        x_RenderString("CmdLine", info.GetCmdLine());
    }
    if (flags & eProgress) {
        x_RenderString("ProgressMsg", info.GetProgressMsg());
    }
    if (flags & eErrMsg && status == CNetScheduleAPI::eFailed) {
        x_RenderString("ErrMsg", info.GetErrMsg());
    }
    if (flags & eStdIn) {
        x_RenderStream("StdIn", info.GetStdIn());
    }
    if (flags & eStdOut) {
        x_RenderStream("StdOut", info.GetStdOut());
    }
    if (flags & eStdErr) {
        x_RenderStream("StdErr", info.GetStdErr());
    }

    if (flags & eRawInput) {
        x_RenderString("RawInput", info.GetRawInput());
    }
    if ((flags & eRawOutput) && (status == CNetScheduleAPI::eDone ||
                                 status == CNetScheduleAPI::eFailed ||
                                 status == CNetScheduleAPI::eCanceled)) {
        x_RenderString("RawOutput", info.GetRawOutput());
    }

}

void CNSInfoRenderer::RenderBlob(const string& blob_id)
{
    ITagWriter::TAttributes attrs;
    attrs.push_back(ITagWriter::TAttribute("Id", blob_id));
    size_t bsize = 0;
    auto_ptr<CNcbiIstream> is(m_Collector.GetBlobContent(blob_id, &bsize));
    if (!is->good())
        m_Writer.WriteTag("Blob", attrs);
    else {
        if (bsize < 50) {
            string buf(bsize,0);
            is->read(&*buf.begin(), bsize);
            m_Writer.WriteTag("Blob", attrs, buf);
        } else {
            STagGuard guard(m_Writer, "Blob", attrs);
            m_Writer.WriteStream(*is);
        }
    }
}

void CNSInfoRenderer::x_RenderString(const string& name, const string& value)
{
    if(value.size() < 50) {
        m_Writer.WriteTag(name, value);
    } else {
        STagGuard guard(m_Writer, name);
        m_Writer.WriteText(value);
    }
}
void CNSInfoRenderer::x_RenderStream(const string& name, CNcbiIstream& is)
{
    if(!is.good())
        m_Writer.WriteTag(name);
    else {
        STagGuard guard(m_Writer, name);
        m_Writer.WriteStream(is);
    }
}

void CNSInfoRenderer::RenderWNode(
    const CNetScheduleAdmin::SWorkerNodeInfo& info,
    TFlags flags)
{
    ITagWriter::TAttributes attrs;
    attrs.push_back(ITagWriter::TAttribute("Host", info.host));
    attrs.push_back(ITagWriter::TAttribute("Port",
        NStr::IntToString(info.port)));
    if (flags <= eMinimal) {
        m_Writer.WriteTag("WNode", attrs);
        return;
    }
    STagGuard guard(m_Writer,"WNode", attrs);
    if (flags <= eStandard) {
        x_RenderString("Node", info.node);
        x_RenderString("Session", info.session);
        x_RenderString("LastAccess", info.last_access.AsString("M/D/Y h:m:s"));
    }

}

class CWNodeListRenderAction :
    public CNSInfoCollector::IAction<CNetScheduleAdmin::SWorkerNodeInfo>
{
public:
    CWNodeListRenderAction(CNSInfoRenderer& renderer,
                           CNSInfoRenderer::TFlags flags =
                           CNSInfoRenderer::eMinimal)
        : m_Renderer(renderer), m_Flags(flags) {}

    virtual ~CWNodeListRenderAction() {};

    virtual void operator()(const CNetScheduleAdmin::SWorkerNodeInfo& info)
    {
        m_Renderer.RenderWNode(info, m_Flags);
    }
private:
    CNSInfoRenderer& m_Renderer;
    CNSInfoRenderer::TFlags m_Flags;
};

void CNSInfoRenderer::RenderWNodes(TFlags flags)
{
    STagGuard guard(m_Writer, "WNodes");
    CWNodeListRenderAction action(*this, flags);
    m_Collector.TraverseNodes(action);
}

END_NCBI_SCOPE

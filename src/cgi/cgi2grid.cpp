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
 * Author: Maxim Didenko
 *
 *
 */
#include <ncbi_pch.hpp>
#include <cgi/cgi2grid.hpp>
#include <cgi/cgi_serial.hpp>

BEGIN_NCBI_SCOPE

static const string s_Html_1 = 
                   "<html><head></head><body>\n"
                   "<form name=\"startform\" action=\"";
static const string s_Html_2 = 
                    "<INPUT TYPE=\"submit\" NAME=\"Submit\" VALUE=\"Submit\">\n";

static const string s_Html_3 = 
                    "</form>\n\n"
                    "<script language=\"JavaScript1.2\">\n"
                    "<!--\n"
                    "function onLoad() {\n"
                    "     document.startform.submit()\n"
                    "}\n"
                    "// For IE & NS6\n"
                    "if (!document.layers) onLoad();\n"
                     "//-->\n"
                     "</script>\n"
                     "</body></html>\n";

string s_GetCgiTunnel2GridUrl(const CCgiRequest&  cgi_request)
{
    string ret = "http://";
    string server(cgi_request.GetProperty(eCgi_ServerName));
    if (NStr::StartsWith(server, "www.ncbi",   NStr::eNocase) )
        ret += "www.ncbi.nlm.nih.gov";
    else if (NStr::StartsWith(server, "web.ncbi",   NStr::eNocase) )
        ret += "web.ncbi.nlm.nih.gov";
    else if (NStr::StartsWith(server, "wwwqa.ncbi",   NStr::eNocase) )
        ret += "wwwqa.ncbi.nlm.nih.gov";
    else if (NStr::StartsWith(server, "webqa.ncbi",   NStr::eNocase) )
        ret += "webqa.ncbi.nlm.nih.gov";
    else 
        ret += "web.ncbi.nlm.nih.gov";

    ret += "/Service/cgi_tunnel2grid/cgi_tunnel2grid.cgi";
    
    return ret;
}

template <typename TValue>
inline CNcbiOstream& s_WriteHiddenField(CNcbiOstream& os,
                                        const string& name, const TValue& value)
{
    os << "<INPUT TYPE=\"HIDDEN\" NAME=\"" << name 
       << "\" VALUE=\"" << value << +"\">\n";
    return os;
}

template <>
inline CNcbiOstream& s_WriteHiddenField<multimap<string, string> >(
                                     CNcbiOstream& os,
                                     const string& name, 
                                     const multimap<string, string>& value)
{
    os << "<INPUT TYPE=\"HIDDEN\" NAME=\"" << name 
       << "\" VALUE=\"" ;
    WriteMap(os, value);
    os << "\">\n";
    return os;
}

template <>
inline CNcbiOstream& s_WriteHiddenField<TCgiEntries>(
                                     CNcbiOstream& os,
                                     const string& name, 
                                     const TCgiEntries& value)
{
    os << "<INPUT TYPE=\"HIDDEN\" NAME=\"" << name 
       << "\" VALUE=\"" ;
    WriteMap(os, value);
    os << "\">\n";
    return os;
}

template <typename TAgs>
CNcbiOstream& s_ComposeHtmlPage(CNcbiOstream&       os,
                                const CCgiRequest& cgi_request,
                                const string&      project_name,
                                const string&      return_url,
                                const TAgs&        cgi_args)
{
    string url = s_GetCgiTunnel2GridUrl(cgi_request);
    os << s_Html_1 << url << "\">\n" << s_Html_2;
    s_WriteHiddenField(os, "ctg_project", project_name);
    s_WriteHiddenField(os, "ctg_input", cgi_args);
    s_WriteHiddenField(os, "ctg_error_url", return_url);
    os << s_Html_3;

    return os;
}


CNcbiOstream& CGI2GRID_ComposeHtmlPage(CNcbiOstream&       os,
                                       const CCgiRequest& cgi_request,
                                       const string&      project_name,
                                       const string&      return_url,
                                       const string&      cgi_args)
{
    s_ComposeHtmlPage(os, cgi_request, project_name, return_url, cgi_args);
    return os;
}

CNcbiOstream& CGI2GRID_ComposeHtmlPage(CNcbiOstream&                   os,
                                       const CCgiRequest&              cgi_request,
                                       const string&                   project_name,
                                       const string&                   return_url,
                                       const multimap<string, string>& cgi_args)
{
    s_ComposeHtmlPage(os, cgi_request, project_name, return_url, cgi_args);
    return os;
}

CNcbiOstream& CGI2GRID_ComposeHtmlPage(CNcbiOstream&      os,
                                       const CCgiRequest& cgi_request,
                                       const string&      project_name,
                                       const string&      return_url,
                                       const TCgiEntries& cgi_args)
{
    s_ComposeHtmlPage(os, cgi_request, project_name, return_url, cgi_args);
    return os;
}

END_NCBI_SCOPE

/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/04/28 20:18:57  ucko
 * Make s_GetCgiTunnel2GridUrl non-static for the sake of compilers
 * (WorkShop) that don't let templates access statics.
 *
 * Revision 1.1  2005/04/28 17:40:53  didenko
 * Added CGI2GRID_ComposeHtmlPage(...) fuctions
 *
 * ===========================================================================
 */

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
 * Author:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <connect/email_diag_handler.hpp>
#include <html/commentdiag.hpp>
#include <html/html.hpp>
#include <html/page.hpp>

#include <misc/grid_cgi/grid_cgiapp.hpp>

#include <vector>

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CGridCgiSampleApplication::
//

class CGridCgiSampleApplication : public  CGridCgiApplication
{
public:

    virtual void Init(void);

protected:

    // Render the job input paramers HTML page
    virtual void ShowParamsPage(CGridCgiContext& ctx) const ;

    // Collect parameters from the HTML page.
    virtual bool CollectParams(void);

    // Prepare the job's input data
    virtual void PrepareJobData(CNcbiOstream& os);

    // Show an information page
    virtual void OnJobSubmitted(CGridCgiContext& ctx);

    // Get the job's result.
    virtual void OnJobDone(CNcbiIstream& is, CGridCgiContext& ctx);
    
    // Report the job's failure.
    virtual void OnJobFailed(const string& msg, CGridCgiContext& ctx);

    // Report when the job is canceled
    virtual void OnJobCanceled(CGridCgiContext& ctx);

    // Report a running status and allow the user to 
    // cancel the job.
    virtual void OnStatusCheck(CGridCgiContext& ctx);

    // Return a job cancelation status.
    virtual bool JobStopRequested(void) const;

    // Get the HTML page title.
    virtual string GetPageTitle() const;
    
    // Get the HTML page template file.
    virtual string GetPageTemplate() const;

private:
    // This  function just demonstrate the use of cmd-line argument parsing
    // mechanism in CGI application -- for the processing of both cmd-line
    // arguments and HTTP entries
    void x_SetupArgs(void);

    // Just a helper function 
    static string VectorToString( const vector<double>& vec);
    vector<double> m_Doubles;
    
};


/////////////////////////////////////////////////////////////////////////////
//  CGridCgiSampleApplication::
//
string CGridCgiSampleApplication::GetPageTitle() const
{
    return "Grid Sample CGI";
}


string CGridCgiSampleApplication::GetPageTemplate() const
{
    return "grid_cgi_sample.html";
}


void CGridCgiSampleApplication::Init()
{
    // Standard CGI framework initialization
    CGridCgiApplication::Init();

    // Allows CGI client to put the diagnostics to:
    //   HTML body (as comments) -- using CGI arg "&diag-destination=comments"
    RegisterDiagFactory("comments", new CCommentDiagFactory);
    //   E-mail -- using CGI arg "&diag-destination=email:user@host"
    RegisterDiagFactory("email",    new CEmailDiagFactory);


    // Describe possible cmd-line and HTTP entries
    // (optional)
    x_SetupArgs();

}


void CGridCgiSampleApplication::ShowParamsPage(CGridCgiContext& ctx) const 
{
    CHTMLText* inp_text = new CHTMLText(
            "<p>Enter your Input doubles here:  "
            "<INPUT TYPE=\"text\" NAME=\"message\" VALUE=\"\"><p>"
            "<INPUT TYPE=\"submit\" NAME=\"SUBMIT\" VALUE=\"Submit\">"
            "<INPUT TYPE=\"reset\"  VALUE=\"Reset\">" );
    ctx.GetHTMLPage().AddTagMap("VIEW", inp_text);
}

bool CGridCgiSampleApplication::CollectParams()
{
    // You can catch CArgException& here to process argument errors,
    // or you can handle it in OnException()
    const CArgs& args = GetArgs();

    // "args" now contains both command line arguments and the arguments 
    // extracted from the HTTP request

    if ( args["message"] ) {
        // get the first "message" argument only...
        const string& m = args["message"].AsString();
        vector<string> sdoubles;
        NStr::Tokenize(m, " ", sdoubles);
        for (size_t i = 0; i < sdoubles.size(); ++i) {
            try {
                double d = NStr::StringToDouble(sdoubles[i],NStr::eCheck_Skip);
                m_Doubles.push_back(d);
            }
            catch(...) {}
        }
    } else {
        // no "message" argument is present
    }
    return m_Doubles.size() > 1;
}


void CGridCgiSampleApplication::PrepareJobData(CNcbiOstream& os)
{   
    // Send jobs input data
    os << m_Doubles.size() << ' ';
    for (size_t j = 0; j < m_Doubles.size(); ++j) {
        os << m_Doubles[j] << ' ';
    }
}

  
void CGridCgiSampleApplication::OnJobSubmitted(CGridCgiContext& ctx)
{   
    // Render a report page
    CHTMLText* inp_text = new CHTMLText(
               "<p/>Input Data : <@INPUT_DATA@><br/>"
               "<INPUT TYPE=\"submit\" NAME=\"Check Status\" VALUE=\"Check Status\">");
    ctx.GetHTMLPage().AddTagMap("VIEW", inp_text);
    CHTMLPlainText* idoubles = new CHTMLPlainText(VectorToString(m_Doubles));
    ctx.GetHTMLPage().AddTagMap("INPUT_DATA", idoubles);
}



void CGridCgiSampleApplication::OnJobDone(CNcbiIstream& is, 
                                          CGridCgiContext& ctx)
{
    int count;
                
    // Get the result
    m_Doubles.clear();
    is >> count;
    for (int i = 0; i < count; ++i) {
        if (!is.good()) {
            ERR_POST( "Input stream error. Index : " << i );
            break;
        }
        double d;
        is >> d;
        m_Doubles.push_back(d);
    }

    // Render the result page
    CHTMLText* inp_text = new CHTMLText(
            "<p/>Job is done.<br/>"
            "<p>Result received : <@OUTPUT_DATA@> <br/>"
            "<INPUT TYPE=\"submit\" NAME=\"Submit new Data\" "
                                   "VALUE=\"Submit new Data\">");
    ctx.GetHTMLPage().AddTagMap("VIEW", inp_text);
    CHTMLPlainText* idoubles = new CHTMLPlainText(VectorToString(m_Doubles));
    ctx.GetHTMLPage().AddTagMap("OUTPUT_DATA",idoubles);
}

void CGridCgiSampleApplication::OnJobFailed(const string& msg, 
                                            CGridCgiContext& ctx)
{
    // Render a error page
    CHTMLText* inp_text = new CHTMLText(
                     "<p/>Job failed.<br/>"
                     "Error Message : <@MSG@><br/>"
                     "<INPUT TYPE=\"submit\" NAME=\"Submit new Data\" "
                                            "VALUE=\"Submit new Data\">");
    ctx.GetHTMLPage().AddTagMap("VIEW", inp_text);
    CHTMLPlainText* err = new CHTMLPlainText(msg);
    ctx.GetHTMLPage().AddTagMap("MSG",err);
}

void CGridCgiSampleApplication::OnJobCanceled(CGridCgiContext& ctx)
{
    // Render a job cancelation page
    CHTMLText* inp_text = new CHTMLText(
               "<p/>Job is canceled.<br/>"
               "<INPUT TYPE=\"submit\" NAME=\"Submit new Data\" "
                                      "VALUE=\"Submit new Data\">");
    ctx.GetHTMLPage().AddTagMap("VIEW", inp_text);
}

void CGridCgiSampleApplication::OnStatusCheck(CGridCgiContext& ctx)
{
    // Render a status report page
    CHTMLText* inp_text = new CHTMLText(
                  "<p/>Job is still runnig.<br/>"
                  "<INPUT TYPE=\"submit\" NAME=\"Check Status\" VALUE=\"Check\">"
                  "<INPUT TYPE=\"submit\" NAME=\"Cancel\" "
                                         "VALUE=\"Cancel the job\">");
   ctx.GetHTMLPage().AddTagMap("VIEW", inp_text);
}

bool CGridCgiSampleApplication::JobStopRequested(void) const
{
    const CArgs& args = GetArgs();

    // Check if job cancelation has been requested.
    if ( args["Cancel"] )
        return true;
    return false;
}

void CGridCgiSampleApplication::x_SetupArgs()
{
    // Disregard the case of CGI arguments
    SetRequestFlags(CCgiRequest::fCaseInsensitiveArgs);

    // Create CGI argument descriptions class
    //  (For CGI applications only keys can be used)
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CGI sample application");
        
    // Describe possible cmd-line and HTTP entries
    // (optional)
    arg_desc->AddOptionalKey("message",
                             "message",
                             "Message passed to CGI application",
                             CArgDescriptions::eString,
                             CArgDescriptions::fAllowMultiple);

    arg_desc->AddOptionalKey("Cancel",
                             "Cancel",
                             "Cancel the job",
                             CArgDescriptions::eString);
    
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


string CGridCgiSampleApplication::VectorToString( const vector<double>& vec)
{
    string ret;
    if (vec.size() > 0) {
        for (size_t i = 0; i < vec.size(); ++i) {
            if (i != 0)
                ret += ", ";
            ret += NStr::DoubleToString(vec[i],3);
        }
    }
    else
        ret = "<EMPTY>";

    return ret;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN
//
int main(int argc, const char* argv[])
{
    int result = CGridCgiSampleApplication().AppMain(argc, argv, 0, eDS_Default);
    _TRACE("back to normal diags");
    return result;
}



/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/04/04 14:15:15  didenko
 * Cosmetcis
 *
 * Revision 1.4  2005/04/01 15:07:31  didenko
 * Divided OnJobSubmit methos onto two PrepareJobData and OnJobSubmitted
 *
 * Revision 1.3  2005/03/31 20:14:59  didenko
 * Added CGrigCgiContext
 *
 * Revision 1.2  2005/03/31 15:57:14  didenko
 * Code cleanup
 *
 * ===========================================================================
 */

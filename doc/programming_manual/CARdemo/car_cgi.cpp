<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<html>
  <head>
    <title>car_cgi.cpp</title>
  </head>

  <body BGCOLOR="#FFFFFF">
  <pre><font color = "red">// File name: car_cgi.cpp
// Description: Implement the CCarCgi class and function main
<font color = "green">
#include &lt;cgi/cgiapp.hpp>
#include &lt;cgi/cgictx.hpp>
#include &lt;html/html.hpp>
#include &lt;html/page.hpp>

#include "car.hpp"
<font color = blue>
USING_NCBI_SCOPE;
</font></font>

/////////////////////////////////////////////////////////////////////////////
//  CCarCgi::  declaration
</font>
class CCarCgi : public CCgiApplication
{
public:
    virtual int ProcessRequest(CCgiContext& ctx);

private:
    CCar* CreateCarByRequest(const CCgiContext& ctx);

    void PopulatePage(CHTMLPage& page, const CCar& car);

    static CNCBINode* ComposeSummary(const CCar& car);
    static CNCBINode* ComposeForm   (const CCar& car);
    static CNCBINode* ComposePrice  (const CCar& car);

    static const char sm_ColorTag[];
    static const char sm_FeatureTag[];
};

<font color = red>
/////////////////////////////////////////////////////////////////////////////
//  CCarCgi::  implementation
</font>

const char CCarCgi::sm_ColorTag[]   = "color";
const char CCarCgi::sm_FeatureTag[] = "feature";


int CCarCgi::ProcessRequest(CCgiContext& ctx)
{
    <font color = red>// Create new "car" object with the attributes retrieved
    // from the CGI request parameters</font>
    auto_ptr&lt;CCar> car;
    try {
        car.reset( CreateCarByRequest(ctx) );
    } catch (exception& e) {
        ERR_POST("Failed to create car: " << e.what());
        return 1;
    }
    <font color = red>
    // Create an HTML page (using the template file "car.html")</font>
    CRef&lt;CHTMLPage> page;
    try {
        page = new CHTMLPage("Car", "car.html");
    } catch (exception& e) {
        ERR_POST("Failed to create the Car HTML page: " << e.what());
        return 2;
    }
    <font color = red>
    // Register all substitutions for the template parameters &lt;@XXX@>
    // (fill them out using the "car" attributes)</font>
    try {
        PopulatePage(*page, *car);
    } catch (exception& e) {
        ERR_POST("Failed to populate the Car HTML page: " << e.what());
        return 3;
    }
    <font color = red>
    // Compose and flush the resultant HTML page</font>
    try {
        const CCgiResponse& response = ctx.GetResponse();
        response.WriteHeader();
        page->Print(response.out(), CNCBINode::eHTML);
        response.Flush();
    } catch (exception& e) {
        ERR_POST("Failed to compose and send the Car HTML page: " << e.what());
        return 4;
    }

    return 0;
}


CCar* CCarCgi::CreateCarByRequest(const CCgiContext& ctx)
{
    auto_ptr&lt;CCar> car(new CCar);
    <font color = red>
    // Get the list of CGI request name/value pairs</font>
    const TCgiEntries& entries = ctx.GetRequest().GetEntries();

    TCgiEntriesCI it;
    <font color = red>
    // load the car with selected features</font>
    pair&lt;TCgiEntriesCI,TCgiEntriesCI> feature_range =
        entries.equal_range(sm_FeatureTag);
    for (it = feature_range.first;  it != feature_range.second;  ++it) {
        car->AddFeature(it->second);
    }
    <font color = red>
    // color</font>
    if ((it = entries.find(sm_ColorTag)) != entries.end()) {
        car->SetColor(it->second);
    } else {
        car->SetColor(*CCarAttr::GetColors().begin());
    }

    return car.release();
}

<font color = red>
    /************ Create a form with the following structure:
      &lt;form>
        &lt;table>
          &lt;tr> 
            &lt;td> (Features) &lt;/td>
            &lt;td> (Colors)   &lt;/td>
            &lt;td> (Submit)   &lt;/td>
          &lt;/tr>
        &lt;/table>
      &lt;/form>
    ********************/
</font>
CNCBINode* CCarCgi::ComposeForm(const CCar& car)
{
    set&lt;string>::const_iterator it;

    CRef&lt;CHTML_table> Table = new CHTML_table();
    Table->SetCellSpacing(0)->SetCellPadding(4)
        ->SetBgColor("#CCCCCC")->SetAttribute("border", "0");
    
    CRef&lt;CHTMLNode> Row = new CHTML_tr();
    <font color = red>
    // features (check boxes)</font>
    CRef&lt;CHTMLNode> Features = new CHTML_td();
    Features->SetVAlign("top")->SetWidth("200");
    Features->AppendChild(new CHTMLText("Options: &lt;br>"));

    for (it = CCarAttr::GetFeatures().begin();
         it != CCarAttr::GetFeatures().end();  ++it) {
        Features->AppendChild
            (new CHTML_checkbox
             (sm_FeatureTag, *it, car.HasFeature(*it), *it));
        Features->AppendChild(new CHTML_br());
    }
    <font color = red>
    // colors (radio buttons)</font>
    CRef&lt;CHTMLNode> Colors = new CHTML_td();
    Colors->SetVAlign("top")->SetWidth("128");
    Colors->AppendChild(new CHTMLText("Color: &lt;br>"));

    for (it = CCarAttr::GetColors().begin();
         it != CCarAttr::GetColors().end();  ++it) {
            Colors->AppendChild
                (new CHTML_radio
                 (sm_ColorTag, *it, !NStr::Compare(*it, car.GetColor()), *it));
            Colors->AppendChild(new CHTML_br());
    }

    Row->AppendChild(&*Features);
    Row->AppendChild(&*Colors);
    Row->AppendChild
        ((new CHTML_td())->AppendChild
         (new CHTML_submit("submit", "submit")));
    Table->AppendChild(&*Row);
    <font color = red>
    // done</font>
    return (new CHTML_form("car.cgi", CHTML_form::eGet))->AppendChild(&*Table);
}


CNCBINode* CCarCgi::ComposeSummary(const CCar& car) 
{
    string summary = "You have ordered a " + car.GetColor() + " model";

    if ( car.GetFeatures().empty() ) {
        summary += " with no additional features.&lt;br>";
        return new CHTMLText(summary);
    }

    summary += " with the following features:&lt;br>";
    CRef&lt;CHTML_ol> ol = new CHTML_ol();

    for (set&lt;string>::const_iterator i = car.GetFeatures().begin();
         i != car.GetFeatures().end();  ++i) {
        ol->AppendItem(*i);
    }
    return (new CHTMLText(summary))->AppendChild((CNodeRef&)ol);
}


CNCBINode* CCarCgi::ComposePrice(const CCar& car)
{
    return
        new CHTMLText("Total price:  $" + NStr::UIntToString(car.GetPrice()));
}


void CCarCgi::PopulatePage(CHTMLPage& page, const CCar& car) 
{
    page.AddTagMap("FORM",     ComposeForm    (car));
    page.AddTagMap("SUMMARY",  ComposeSummary (car));
    page.AddTagMap("PRICE",    ComposePrice   (car));
}


<font color = red>
/////////////////////////////////////////////////////////////////////////////
//  MAIN
</font>

int main(int argc, char* argv[])
{
    SetDiagStream(&NcbiCerr);
    return CCarCgi().AppMain(argc, argv);
}

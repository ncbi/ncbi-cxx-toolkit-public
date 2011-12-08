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
 * Author:  Vladimir Ivanov
 *
 * File Description:  Sample of usage the HTML library templates.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <html/page.hpp>
#include <stdlib.h>

USING_NCBI_SCOPE;


//----------------------------------------------------------------------------
//
// The hook procedure to generate table rows.
//
//----------------------------------------------------------------------------
//

// Simple data source.
// You can use here any other: files, databases...

struct SPerson {
    const char*  name; 
    const char*  phone;
    const char*  email;
};
static const SPerson s_Persons[] = {
    { "Username 1", "111-111-1111", "name1@server" },
    { "Username 2", "222-222-2222", "name2@server" },
    { "Username 3", "333-333-3333", "name3@server" },
    { "Username 4", "444-444-4444", "name4@server" },
    { "Username 5", "555-555-5555", "name5@server" },
    { "Username 6", "666-666-6666", "name6@server" },
    { "Username 7", "777-777-7777", "name7@server" },
    { "Username 8", "888-888-8888", "name8@server" },
    { "Username 9", "999-999-9999", "name9@server" },
    { 0, 0, 0}
};

struct STableRowHook_Ctx
{
    STableRowHook_Ctx(const SPerson *const x_persons)
        : persons(x_persons), current(0) {}

    const SPerson *const persons;
    size_t               current;
};


// You can use hook functions with other parameters like next:
//
//   static CNCBINode* s_TableRowHook(void)
//   static CNCBINode* s_TableRowHook(const string& name)
//   static CNCBINode* s_TableRowHook(void* data)
//   static CNCBINode* s_TableRowHook(void* data, const string& name)
//   static CNCBINode* s_TableRowHook(CAnyNodeClass* node)
//   static CNCBINode* s_TableRowHook(CAnyNodeClass* node, const string& name)
//   static CNCBINode* s_TableRowHook(CAnyNodeClass* node, TAnyType data)
//   static CNCBINode* s_TableRowHook(CAnyNodeClass* node, TAnyType data,
//                                    const string& name)
//
// <node> - contains a pointer to node called the function.
// <data> - allows to pass into hook-function some data.
// <name> - contains a name of the tag for which the functons is called.
//          so the one mapper function can be set for few different tags.
//
// (see file nodemap.hpp, CreateTagMapper() for details)
//

static CNCBINode* s_TableRowHook(CHTMLPage*          page  /*never NULL*/,
                                 STableRowHook_Ctx*  ctx)
{
    SPerson person;
    person = ctx->persons[ctx->current];

    // Do we have something to print out?
    if ( !person.name ) {
        // No more data. Stop the table row dublication.
        return 0;
    }

    // Here we are constructing a node using an already existent
    // template. But you can construct it yourself, using a full set
    // of CNCBINode and its derived classes, and also an AppendChild()
    // method.
    
    CNCBINode* node = new CNCBINode();
    if ( !node ) {
        return 0;
    }
    // For example, we add a HTML comment before each table row.
    // We add CHTMLText("\n") nodes only for good look in the text mode,
    // they will be ignored by Internet browsers.
    node->AppendChild(new CHTMLText("\n"));
    node->AppendChild(new CHTMLComment("Table row #"
                                       + NStr::NumericToString(ctx->current+1)));
    node->AppendChild(new CHTMLText("\n"));
    node->AppendChild(new CHTMLTagNode("table_row_template"));

    // Define variables for the new row.
    page->AddTagMap("name",  new CHTMLText(person.name));
    page->AddTagMap("phone", new CHTMLText(person.phone));
    page->AddTagMap("email", new CHTMLText(person.email));

    // Decorate table rows.
    string css_class = "colored";
    if (ctx->current != 5) {
        css_class = ctx->current %2 ? "even" : "odd";
    }
    page->AddTagMap("class", new CHTMLText(css_class));

    // Increment the hook's context row counter.
    ctx->current++;

    // Instruct to call this hook again after printing already prepared data.
    // By default repetition is disabled.
    node->RepeatTag();

    // Return generated node.
    return node;
}


//----------------------------------------------------------------------------
//
// The class to provide random numbers
//
//----------------------------------------------------------------------------

class CNumAdderCtx
{
public:
    // Constructor.
    CNumAdderCtx(CHTMLPage* page)
        : m_Counter(0), m_Sum(0), m_Page(page)
        { srand((unsigned int)time(0)); }

    // Generate random number by module 10.
    int GetRandomNumber()
    {
        int n = rand() % 10;
        m_Counter++;
        m_Sum += n;
        return n;
    }
    
    // Return current counter value
    int GetCounter()
        { return m_Counter; };
    
    // Return accumulated sum.
    int GetSum()
        { return m_Sum; };

    // Return accumulated sum.
    CHTMLPage* GetPage()
        { return m_Page; }
private:
    int        m_Counter;   // Just a counter.
    int        m_Sum;       // Accumulated sum.
    CHTMLPage* m_Page;      // Reference to the main page.
};


//----------------------------------------------------------------------------
//
// Demo application class
//
//----------------------------------------------------------------------------

class CDemoApplication : public CNcbiApplication
{
public:
    virtual int Run (void);

    // Function to generate data for number addition example.
    //
    // You can use hook functions with other parameters like next:
    //
    //   CNCBINode* NumAdderHook(void)
    //   CNCBINode* NumAdderHook(const string& name)
    //   CNCBINode* NumAdderHook(TAnyType data)
    //   CNCBINode* NumAdderHook(TAnyType data, const string& name)
    //
    // (see file nodemap.hpp, CreateTagMapper() for details)
    //

    CNCBINode* NumAdderHook(CNumAdderCtx* ctx);
};


CNCBINode* CDemoApplication::NumAdderHook(CNumAdderCtx* ctx)
{
    // Generate random numbers until it sum is less 50.
    if ( ctx->GetSum() >= 50 ) {
        return 0;
    }

    // Generate node to represent a generated number.
    int n = ctx->GetRandomNumber();
    int i = ctx->GetCounter();
    CNCBINode* node = new CHTMLText(NStr::IntToString(i) +
                                    ". Add number: " +
                                    NStr::IntToString(n) + " <br>\n");
    // Get sum value.
    int sum = ctx->GetSum();

    // Add result if sum is above 50.
    if ( sum >= 50) {
        CNCBINode* sep = new CHTMLPlainText("-");
        sep->SetRepeatCount(25);
        node->AppendChild(sep);
        node->AppendChild(new CHTML_br());
        ctx->GetPage()->AddTagMap("num_sum",
                                  new CHTMLText(NStr::IntToString(sum)));
    }

    // Enable to call this hook again after printing already prepared data.
    // By default repetition is disabled.
    node->RepeatTag();

    // Return generated node.
    return node;
}


int CDemoApplication::Run(void)
{
    // Create main page, that is used to compose and write out the HTML code.
    CHTMLPage page("HTML library template demo page");
    
    // Set used template. The main template can be only one.
    // Each next call of the SetTemplate*() redefine previous template.

    page.SetTemplateFile("demo_html_template.html"); 
    // page.SetTemplateString(...);
    // page.SetTemplateStream(...);
    // page.SetTemplateBuffer(...);

    // Load template libraries. The number of loaded libraries is
    // limited only with amount of available memory.

    page.LoadTemplateLibFile("demo_html_template.inc");
    // page.LoadTemplateLibString(...);
    // page.LoadTemplateLibStream(...);
    // page.LoadTemplateLibBuffer(...);

    // Redefine some definition from already loaded template library.
    page.AddTagMap("TITLE",
        new CHTMLText("Sample of usage the HTML library templates"));

    // Create other necessary tags.
    page.AddTagMap("HEADLINE",
        new CHTMLText("Phone browser"));
    page.AddTagMap("DATE",
        new CHTMLText(CTime(CTime::eCurrent).AsString("M B Y, h:m")));

    // Setup static hook procedure to generate table rows
    STableRowHook_Ctx table_ctx(s_Persons);
    page.AddTagMap("table_row_hook", CreateTagMapper(s_TableRowHook,
                                                     &table_ctx));

    // We also can use any other tag mappers, such as a function with
    // tag name parameter, or some class method as shown below.
    CNumAdderCtx adder_ctx(&page);
    page.AddTagMap("num_add", CreateTagMapper(&CDemoApplication::NumAdderHook,
                                              &adder_ctx));

    // Print out the results.
    page.Print(cout);

    // Next line added to better output look in the text mode only
    cout << endl;

    // All done.
    return 0;
}


//----------------------------------------------------------------------------
//
// Main function
//
//----------------------------------------------------------------------------

static CDemoApplication theDemoApplication;

int main(int argc, const char* argv[])
{
    // Execute main application function.
    return theDemoApplication.AppMain(argc, argv, 0, eDS_Default, 0);
}


#include <html.hpp>
#include <page.hpp>

int main()
{
    string output("");
    
    CHTMLPage * page = new CHTMLPage;

    page->Create();

    page->Print(output);  // serialize it
    cout << output << endl;
    
    return 0;  
}

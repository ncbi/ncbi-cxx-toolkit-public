#include <corelib/ncbistd.hpp>
#include <parser.hpp>
#include <lexer.hpp>

#include <list>
#include <autoptr.hpp>
#include <module.hpp>

USING_NCBI_SCOPE;

int main()
{
    SetDiagStream(&NcbiCerr);
    ASNLexer lexer(NcbiCin);
    ASNParser parser(lexer);
    list<AutoPtr<ASNModule> > modules;
    try {
        parser.Modules(modules);
    }
    catch (runtime_error e) {
        NcbiCerr << e.what() << endl;
        NcbiCerr << "Current token: " << parser.Next() << " '" <<
            lexer.CurrentTokenText() << "'" << endl;
        NcbiCerr << "Parsing failed" << endl;
        return 1;
    }
    catch (...) {
        NcbiCerr << "Parsing failed" << endl;
        return 1;
    }
    NcbiCerr << "OK" << endl;
    for ( list<AutoPtr<ASNModule> >::const_iterator i = modules.begin();
          i != modules.end(); ++i ) {
        (*i)->Print(NcbiCout);
    }
}

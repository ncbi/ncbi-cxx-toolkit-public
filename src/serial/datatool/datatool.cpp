#include <corelib/ncbistd.hpp>
#include <parser.hpp>
#include <lexer.hpp>
#include <moduleset.hpp>
#include <module.hpp>
#include <fstream>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>

USING_NCBI_SCOPE;

enum EFileType {
    eNone,
    eASNText,
    eASNBinary,
    eXMLText
};

static void Help(void)
{
    NcbiCout << endl <<
        "DataTool 1.0 arguments:" << endl <<
        endl <<
        "  -m  ASN.1 Module File [File In]" << endl <<
        "  -f  ASN.1 Module File [File Out]  Optional" << endl <<
        "  -v  Print Value File [File In]  Optional" << endl <<
        "  -p  Print Value File [File Out]  Optional" << endl <<
        "  -d  Binary Value File (type required) [File In]  Optional" << endl <<
        "  -t  Binary Value Type [String]  Optional" << endl <<
        "  -e  Binary Value File [File Out]  Optional" << endl;
}

static void Error(const string& message)
{
    NcbiCerr << "datatool: Error: " << message << endl;
    exit(1);
}

static const char* StringArgument(const char* arg)
{
    if ( !arg || !arg[0] ) {
        Error("argument expected");
    }
    return arg;
}

static const char* FileInArgument(const char* arg)
{
    return StringArgument(arg);
}

static const char* FileOutArgument(const char* arg)
{
    return StringArgument(arg);
}

static void LoadDefinition(CModuleSet& types,
                           const string& fileName, EFileType fileType);
static void StoreDefinition(const CModuleSet& types,
                            const string& fileName, EFileType fileType);
static
pair<TObjectPtr, TTypeInfo> LoadValue(CModuleSet& types, const string& typeName,
                                      const string& fileName, EFileType fileType);
static void StoreValue(const pair<TObjectPtr, TTypeInfo>& object,
                       const string& fileName, EFileType fileType);

int main(int argc, const char*argv[])
{
    string sourceInFile;
    EFileType sourceInType;

    string sourceOutFile;
    EFileType sourceOutType;

    string dataInFile;
    EFileType dataInType;
    string dataInName;

    string dataOutFile;
    EFileType dataOutType;

    if ( argc <= 1 ) {
        Help();
        return 0;
    }
    else {
        for ( int i = 1; i < argc; ++i ) {
            const char* arg = argv[i];
            if ( arg[0] != '-' ) {
                Error(string("Invalid argument ") + arg);
            }
            switch ( arg[1] ) {
            case 'm':
                sourceInType = eASNText;
                sourceInFile = FileInArgument(argv[++i]);
                break;
            case 'f':
                sourceOutType = eASNText;
                sourceOutFile = FileOutArgument(argv[++i]);
                break;
            case 'v':
                dataInType = eASNText;
                dataInFile = FileInArgument(argv[++i]);
                break;
            case 'p':
                dataOutType = eASNText;
                dataOutFile = FileOutArgument(argv[++i]);
                break;
            case 'd':
                dataInType = eASNBinary;
                dataInFile = FileInArgument(argv[++i]);
                break;
            case 'e':
                dataOutType = eASNBinary;
                dataOutFile = FileOutArgument(argv[++i]);
                break;
            case 't':
                dataInName = StringArgument(argv[++i]);
                break;
            default:
                Error(string("Invalid argument ") + arg);
            }
        }
    }

    ofstream diag("datatool.log");
    SetDiagStream(&diag);

    try {
        if ( sourceInFile.empty() )
            Error("Module file not specified");

        CModuleSet types;
        LoadDefinition(types, sourceInFile, sourceInType);
    
        if ( !sourceOutFile.empty() )
            StoreDefinition(types, sourceOutFile, sourceOutType);

        if ( dataInFile.empty() )
            return 0;

        pair<TObjectPtr, TTypeInfo> object =
            LoadValue(types, dataInName, dataInFile, dataInType);

        if ( !dataOutFile.empty() )
            StoreValue(object, dataOutFile, dataOutType);
    }
    catch (exception& e) {
        SetDiagStream(&NcbiCerr);
        NcbiCerr << "Error: " << e.what() << endl;
        return 1;
    }
    catch (...) {
        SetDiagStream(&NcbiCerr);
        NcbiCerr << "Error: unknown" << endl;
        return 1;
    }

    SetDiagStream(&NcbiCerr);
}


void LoadDefinition(CModuleSet& types,
                    const string& fileName, EFileType fileType)
{
    ifstream in(fileName.c_str());
    if ( !in )
        Error("cannot open file " + fileName);
    
    if ( fileType != eASNText )
        Error("data definition format not supported");

    ASNLexer lexer(in);
    ASNParser parser(lexer);
    try {
        parser.Modules(types);
    }
    catch (...) {
        NcbiCerr << "Current token: " << parser.Next() << " '" <<
            lexer.CurrentTokenText() << "'" << endl;
        NcbiCerr << "Parsing failed" << endl;
        throw;
    }
}

void StoreDefinition(const CModuleSet& types,
                     const string& fileName, EFileType fileType)
{
    ofstream out(fileName.c_str());
    if ( !out )
        Error("cannot open file " + fileName);

    if ( fileType != eASNText )
        Error("data definition format not supported");
    
    for ( CModuleSet::TModules::const_iterator i = types.modules.begin();
          i != types.modules.end(); ++i ) {
        (*i)->Print(out);
    }
}

pair<TObjectPtr, TTypeInfo> LoadValue(CModuleSet& types, const string& typeName,
                                      const string& fileName, EFileType fileType)
{
    ifstream in(fileName.c_str(),
                fileType == eASNBinary? ios::in | ios::binary: ios::in);
    if ( !in )
        Error("cannot open file " + fileName);
    
    auto_ptr<CObjectIStream> objIn;
    switch ( fileType ) {
    case eASNText:
        objIn.reset(new CObjectIStreamAsn(in));
        break;
    case eASNBinary:
        objIn.reset(new CObjectIStreamAsnBinary(in));
        break;
    default:
        Error("value format not supported");
    }
    objIn->SetTypeMapper(&types);
    string type = objIn->ReadTypeName();
    if ( type.empty() ) {
        if ( typeName.empty() )
            Error("ASN.1 value type must be specified (-t)");
        type = typeName;
    }
    TTypeInfo typeInfo = types.MapType(type);
    if ( typeInfo == 0 )
        Error("type not found: " + type);
    
    TObjectPtr object = 0;
    objIn->ReadExternalObject(&object, typeInfo);
    return make_pair(object, typeInfo);
}

void StoreValue(const pair<TObjectPtr, TTypeInfo>& object,
                const string& fileName, EFileType fileType)
{
    ofstream out(fileName.c_str(),
                 fileType == eASNBinary? ios::out | ios::binary: ios::out);
    if ( !out )
        Error("cannot open file " + fileName);
    
    auto_ptr<CObjectOStream> objOut;
    switch ( fileType ) {
    case eASNText:
        objOut.reset(new CObjectOStreamAsn(out));
        break;
    case eASNBinary:
        objOut.reset(new CObjectOStreamAsnBinary(out));
        break;
    default:
        Error("value format not supported");
    }
    objOut->Write(&object.first, object.second);
}

#include <corelib/ncbistd.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <memory>
#include <fstream>
#include "parser.hpp"
#include "lexer.hpp"
#include "moduleset.hpp"
#include "module.hpp"

USING_NCBI_SCOPE;

static void Error(const char* msg, const char* msg2 = "")
{
    NcbiCerr << msg << msg2 << endl;
    exit(1);
}

class SourceFile
{
public:
    SourceFile(const string& name, bool binary = false);
    ~SourceFile(void);

    operator CNcbiIstream&(void) const
        {
            return *m_StreamPtr;
        }

private:
    CNcbiIstream* m_StreamPtr;
    bool m_Open;
};

SourceFile::SourceFile(const string& name, bool binary)
{
    if ( name == "stdin" || name == "-" ) {
        m_StreamPtr = &NcbiCin;
        m_Open = false;
    }
    else {
        m_StreamPtr = new ifstream(name.c_str(),
                                   binary? ios::in | ios::binary: ios::in);
        if ( !*m_StreamPtr ) {
            delete m_StreamPtr;
            m_StreamPtr = 0;
            Error("cannot open file ", name.c_str());
        }
        m_Open = true;
    }
}

SourceFile::~SourceFile(void)
{
    if ( m_Open ) {
        delete m_StreamPtr;
    }
}

class DestinationFile
{
public:
    DestinationFile(const string& name, bool binary = false);
    ~DestinationFile(void);

    operator CNcbiOstream&(void) const
        {
            return *m_StreamPtr;
        }

private:
    CNcbiOstream* m_StreamPtr;
    bool m_Open;
};

DestinationFile::DestinationFile(const string& name, bool binary)
{
    if ( name == "stdout" || name == "-" ) {
        m_StreamPtr = &NcbiCout;
        m_Open = false;
    }
    else {
        m_StreamPtr = new ofstream(name.c_str(),
                                   binary? ios::in | ios::binary: ios::in);
        if ( !*m_StreamPtr ) {
            delete m_StreamPtr;
            m_StreamPtr = 0;
            Error("cannot open file ", name.c_str());
        }
        m_Open = true;
    }
}

DestinationFile::~DestinationFile(void)
{
    if ( m_Open ) {
        delete m_StreamPtr;
    }
}

enum EFileType {
    eNone,
    eASNText,
    eASNBinary,
    eXMLText
};

typedef pair<TObjectPtr, TTypeInfo> TObject;

static void Help(void)
{
    NcbiCout << endl <<
        "DataTool 1.0 arguments:" << endl <<
        endl <<
        "  -m  ASN.1 module file [File In]" << endl <<
        "  -f  ASN.1 module file [File Out]  Optional" << endl <<
        "  -mx XML module file [File In]" << endl <<
        "  -fx XML module file [File Out]  Optional" << endl <<
        "  -v  Read value in ASN.1 text format [File In]  Optional" << endl <<
        "  -p  Print value in ASN.1 text format [File Out]  Optional" << endl <<
        "  -vx Read value in XML format [File In]  Optional" << endl <<
        "  -px Print value in XML format [File Out]  Optional" << endl <<
        "  -d  Read value in ASN.1 binary format (type required) [File In]  Optional" << endl <<
        "  -t  Binary value type [String]  Optional" << endl <<
        "  -e  Write value in ASN.1 binary format [File Out]  Optional" << endl <<
        "  -o  C++ code definition file [File In] Optional" << endl <<
        "  -oh Directory for generated C++ headers [Directory] Optional" << endl <<
        "  -oc Directory for generated C++ code [Directory] Optional" << endl <<
        "  -of File for list of generated C++ files [File Out] Optional" << endl;
}

static const char* StringArgument(const char* arg)
{
    if ( !arg || !arg[0] ) {
        Error("argument expected");
    }
    return arg;
}

static const char* DirArgument(const char* arg)
{
    return StringArgument(arg);
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
static TObject LoadValue(CModuleSet& types, const string& typeName,
                         const string& fileName, EFileType fileType);
static void StoreValue(const TObject& object,
                       const string& fileName, EFileType fileType);
void GenerateCode(const CModuleSet& types, const string& fileName,
                  const string& sourcesListName,
                  const string& headersDir, const string& sourcesDir);

inline EFileType FileType(const char* arg, EFileType defType = eASNText)
{
    switch ( arg[2] ) {
    case 0:
        return defType;
    case 'x':
        return eXMLText;
    default:
        Error("Invalid argument: ", arg);
        return defType;
    }
}

int main(int argc, const char*argv[])
{
    SetDiagStream(&NcbiCerr);

    string sourceInFile;
    EFileType sourceInType = eNone;

    string sourceOutFile;
    EFileType sourceOutType = eNone;

    string dataInFile;
    EFileType dataInType = eNone;
    string dataInName;

    string dataOutFile;
    EFileType dataOutType = eNone;

    string codeInFile;
    string headersDir;
    string sourcesDir;
    string sourcesListFile;

    if ( argc <= 1 ) {
        Help();
        return 0;
    }
    else {
        for ( int i = 1; i < argc; ++i ) {
            const char* arg = argv[i];
            if ( arg[0] != '-' ) {
                Error("Invalid argument: ", arg);
            }
            switch ( arg[1] ) {
            case 'm':
                sourceInType = FileType(arg);
                sourceInFile = FileInArgument(argv[++i]);
                break;
            case 'f':
                sourceOutType = FileType(arg);
                sourceOutFile = FileOutArgument(argv[++i]);
                break;
            case 'v':
                dataInType = FileType(arg);
                dataInFile = FileInArgument(argv[++i]);
                break;
            case 'p':
                dataOutType = FileType(arg);
                dataOutFile = FileOutArgument(argv[++i]);
                break;
            case 'd':
                dataInType = FileType(arg, eASNBinary);
                dataInFile = FileInArgument(argv[++i]);
                break;
            case 'e':
                dataOutType = FileType(arg, eASNBinary);
                dataOutFile = FileOutArgument(argv[++i]);
                break;
            case 't':
                dataInName = StringArgument(argv[++i]);
                break;
            case 'o':
                switch ( arg[2] ) {
                case 0:
                    codeInFile = FileInArgument(argv[++i]);
                    break;
                case 'h':
                    headersDir = DirArgument(argv[++i]);
                    break;
                case 'c':
                    sourcesDir = DirArgument(argv[++i]);
                    break;
                case 'f':
                    sourcesListFile = FileOutArgument(argv[++i]);
                    break;
                default:
                    Error("Invalid argument: ", arg);
                }
                break;
            default:
                Error("Invalid argument: ", arg);
            }
        }
    }

    try {
        if ( sourceInFile.empty() )
            Error("Module file not specified");

        CModuleSet types;
        LoadDefinition(types, sourceInFile, sourceInType);
    
        if ( !sourceOutFile.empty() )
            StoreDefinition(types, sourceOutFile, sourceOutType);

        if ( !dataInFile.empty() ) {
            TObject object =
                LoadValue(types, dataInName, dataInFile, dataInType);
            
            if ( !dataOutFile.empty() )
                StoreValue(object, dataOutFile, dataOutType);
        }

        if ( !codeInFile.empty() )
            GenerateCode(types, codeInFile, sourcesListFile,
                         headersDir, sourcesDir);
    }
    catch (exception& e) {
        Error("Error: ", e.what());
        return 1;
    }
    catch (...) {
        Error("Error: unknown");
        return 1;
    }
	return 0;
}


void LoadDefinition(CModuleSet& types,
                    const string& fileName, EFileType fileType)
{
    if ( fileType != eASNText )
        Error("data definition format not supported");

    SourceFile in(fileName);
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
    if ( fileType != eASNText )
        Error("data definition format not supported");
    
    DestinationFile out(fileName);
    
    for ( CModuleSet::TModules::const_iterator i = types.modules.begin();
          i != types.modules.end(); ++i ) {
        (*i)->Print(out);
    }
}

TObject LoadValue(CModuleSet& types, const string& typeName,
                  const string& fileName, EFileType fileType)
{
    SourceFile in(fileName, fileType == eASNBinary);

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
        Error("type not found: ", type.c_str());
    
    TObjectPtr object = 0;
    objIn->ReadExternalObject(&object, typeInfo);
    return make_pair(object, typeInfo);
}

void StoreValue(const TObject& object,
                const string& fileName, EFileType fileType)
{
    DestinationFile out(fileName, fileType == eASNBinary);

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

#include <corelib/ncbistd.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <memory>
#include <set>
#include "parser.hpp"
#include "lexer.hpp"
#include "moduleset.hpp"
#include "module.hpp"
#include "type.hpp"

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

struct FileInfo {
    FileInfo(void)
        : type(eNone)
        { }
    FileInfo(const string& n, EFileType t)
        : name(n), type(t)
        { }

    operator bool(void) const
        { return !name.empty(); }
    string name;
    EFileType type;
};

typedef pair<TObjectPtr, TTypeInfo> TObject;

static void Help(void)
{
    NcbiCout << endl <<
        "DataTool 1.0 arguments:" << endl <<
        endl <<
        "  -m  ASN.1 module file [File In] Optional" << endl <<
        "  -mx XML module file [File In] Optional" << endl <<
        "  -M  external ASN.1 module files [File In] Optional" << endl <<
        "  -Mx external XML module files [File In] Optional" << endl <<
        "  -f  ASN.1 module file [File Out]  Optional" << endl <<
        "  -fx XML module file [File Out]  Optional" << endl <<
        "  -v  Read value in ASN.1 text format [File In]  Optional" << endl <<
        "  -p  Print value in ASN.1 text format [File Out]  Optional" << endl <<
        "  -vx Read value in XML format [File In]  Optional" << endl <<
        "  -px Print value in XML format [File Out]  Optional" << endl <<
        "  -d  Read value in ASN.1 binary format (type required) [File In]  Optional" << endl <<
        "  -t  Binary value type [String]  Optional" << endl <<
        "  -e  Write value in ASN.1 binary format [File Out]  Optional" << endl <<
        "  -o  generate C++ files for all types Optional" << endl <<
        "  -ot generate C++ files for listed types [Types] Optional" << endl <<
        "  -od C++ code definition file [File In] Optional" << endl <<
        "  -oh Directory for generated C++ headers [Directory] Optional" << endl <<
        "  -oc Directory for generated C++ code [Directory] Optional" << endl <<
        "  -of File for list of generated C++ files [File Out] Optional" << endl;
}

static
const char* StringArgument(const char* arg)
{
    if ( !arg || !arg[0] ) {
        Error("argument expected");
    }
    return arg;
}

inline
const char* DirArgument(const char* arg)
{
    return StringArgument(arg);
}

inline
const char* FileInArgument(const char* arg)
{
    return StringArgument(arg);
}

inline
const char* FileOutArgument(const char* arg)
{
    return StringArgument(arg);
}

static void LoadDefinition(CModuleSet& types, const FileInfo& file);
static void StoreDefinition(const CModuleSet& types, const FileInfo& file);
static TObject LoadValue(CModuleSet& types, const FileInfo& file,
                         const string& typeName);
static void StoreValue(const TObject& object, const FileInfo& file);
void GenerateCode(const CModuleSet& types, const set<string>& typeNames,
                  const string& fileName, const string& sourcesListName,
                  const string& headersDir, const string& sourcesDir);

inline
EFileType FileType(const char* arg, EFileType defType = eASNText)
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

static
void GetFileIn(FileInfo& info, const char* typeArg, const char* name,
               EFileType defType = eASNText)
{
    info.type = FileType(typeArg, defType);
    info.name = FileInArgument(name);
}

static
void GetFilesIn(list<FileInfo>& files, const char* typeArg, const char* namesIn)
{
    EFileType type = FileType(typeArg);
    string names = StringArgument(namesIn);
    SIZE_TYPE pos = 0;
    SIZE_TYPE next = names.find(',');
    while ( next != NPOS ) {
        files.push_back(FileInfo(names.substr(pos, next - pos), type));
        pos = next + 1;
        next = names.find(',', pos);
    }
    files.push_back(FileInfo(names.substr(pos), type));
}

static
void GetFileOut(FileInfo& info, const char* typeArg, const char* name,
               EFileType defType = eASNText)
{
    info.type = FileType(typeArg, defType);
    info.name = FileOutArgument(name);
}

static
void GetTypes(set<string>& typeSet, const char* typesIn)
{
    string types = StringArgument(typesIn);
    SIZE_TYPE pos = 0;
    SIZE_TYPE next = types.find(',');
    while ( next != NPOS ) {
        typeSet.insert(types.substr(pos, next - pos));
        pos = next + 1;
        next = types.find(',', pos);
    }
    typeSet.insert(types.substr(pos));
}

static
void GetTypes(set<string>& typeSet, const CModuleSet& moduleSet)
{
    for ( CModuleSet::TModules::const_iterator mi = moduleSet.modules.begin();
          mi != moduleSet.modules.end();
          ++mi ) {
        const ASNModule* module = mi->get();
        for ( ASNModule::TDefinitions::const_iterator ti =
                  module->definitions.begin();
              ti != module->definitions.end();
              ++ti ) {
            typeSet.insert(module->name + '.' + (*ti)->name);
        }
    }
}

int main(int argc, const char*argv[])
{
    SetDiagStream(&NcbiCerr);

    FileInfo moduleIn;
    list<FileInfo> importModules;
    FileInfo moduleOut;
    FileInfo dataIn;
    string dataInTypeName;
    FileInfo dataOut;

    bool generateAllTypes = false;
    set<string> generateTypes;
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
                GetFileIn(moduleIn, arg, argv[++i]);
                break;
            case 'M':
                GetFilesIn(importModules, arg, argv[++i]);
                break;
            case 'f':
                GetFileOut(moduleOut, arg, argv[++i]);
                break;
            case 'v':
                GetFileIn(dataIn, arg, argv[++i]);
                break;
            case 'p':
                GetFileOut(dataOut, arg, argv[++i]);
                break;
            case 'd':
                GetFileIn(dataIn, arg, argv[++i], eASNBinary);
                break;
            case 'e':
                GetFileOut(dataOut, arg, argv[++i], eASNBinary);
                break;
            case 't':
                dataInTypeName = StringArgument(argv[++i]);
                break;
            case 'o':
                switch ( arg[2] ) {
                case 'A':
                    generateAllTypes = true;
                    break;
                case 't':
                    GetTypes(generateTypes, argv[++i]);
                    break;
                case 'd':
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
        if ( !moduleIn )
            Error("Module file not specified");

        CModuleSet types;
        LoadDefinition(types, moduleIn);

        if ( moduleOut )
            StoreDefinition(types, moduleOut);

        if ( generateAllTypes ) {
            GetTypes(generateTypes, types);
        }

        for ( list<FileInfo>::const_iterator fi = importModules.begin();
              fi != importModules.end();
              ++fi ) {
            LoadDefinition(types, *fi);
        }
    
        if ( dataIn ) {
            TObject object = LoadValue(types, dataIn, dataInTypeName);
            
            if ( dataOut )
                StoreValue(object, dataOut);
        }

        if ( !generateTypes.empty() )
            GenerateCode(types, generateTypes, codeInFile,
                         sourcesListFile, headersDir, sourcesDir);
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


void LoadDefinition(CModuleSet& types, const FileInfo& file)
{
    if ( file.type != eASNText )
        Error("data definition format not supported");

    SourceFile in(file.name);
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

void StoreDefinition(const CModuleSet& types, const FileInfo& file)
{
    if ( file.type != eASNText )
        Error("data definition format not supported");
    
    DestinationFile out(file.name);
    
    for ( CModuleSet::TModules::const_iterator i = types.modules.begin();
          i != types.modules.end(); ++i ) {
        (*i)->Print(out);
    }
}

TObject LoadValue(CModuleSet& types, const FileInfo& file, const string& typeName)
{
    SourceFile in(file.name, file.type == eASNBinary);

    auto_ptr<CObjectIStream> objIn;
    switch ( file.type ) {
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

void StoreValue(const TObject& object, const FileInfo& file)
{
    DestinationFile out(file.name, file.type == eASNBinary);

    auto_ptr<CObjectOStream> objOut;
    switch ( file.type ) {
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

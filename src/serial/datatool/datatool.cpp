#include <corelib/ncbistd.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <memory>
#include "parser.hpp"
#include "lexer.hpp"
#include "moduleset.hpp"
//#include "module.hpp"
//#include "type.hpp"
#include "typecontext.hpp"
#include "generate.hpp"

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
        m_StreamPtr = new CNcbiIfstream(name.c_str(),
                                        binary?
                                            IOS_BASE::in | IOS_BASE::binary:
                                            IOS_BASE::in);
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
        m_StreamPtr = new CNcbiOfstream(name.c_str(),
                                        binary?
                                            IOS_BASE::out | IOS_BASE::binary:
                                            IOS_BASE::out);
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

typedef pair<AnyType, TTypeInfo> TObject;

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
        "  -oA generate C++ files for all types Optional" << endl <<
        "  -ot generate C++ files for listed types [Types] Optional" << endl <<
        "  -ox exclude listed types from generation [Types] Optional" << endl <<
        "  -oX exclude all other types from generation Optional" << endl <<
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

static void LoadDefinition(const FileInfo& file, CModuleSet& types);
static void StoreDefinition(const CModuleSet& types, const FileInfo& file);
static TObject LoadValue(CModuleSet& types, const FileInfo& file,
                         const string& typeName);
static void StoreValue(const TObject& object, const FileInfo& file);

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
    CCodeGenerator generator;

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
                    generator.GetTypes(generator.m_GenerateTypes,
                                       StringArgument(argv[++i]));
                    break;
                case 'x':
                    generator.GetTypes(generator.m_ExcludeTypes,
                                       StringArgument(argv[++i]));
                    break;
                case 'X':
                    generator.m_ExcludeAllTypes = true;
                    break;
                case 'd':
                    generator.LoadConfig(FileInArgument(argv[++i]));
                    break;
                case 'h':
                    generator.m_HeadersDir = DirArgument(argv[++i]);
                    break;
                case 'c':
                    generator.m_SourcesDir = DirArgument(argv[++i]);
                    break;
                case 'f':
                    generator.m_FileListFileName = FileOutArgument(argv[++i]);
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

        LoadDefinition(moduleIn, generator.m_Modules);
        if ( moduleOut )
            StoreDefinition(generator.m_Modules, moduleOut);

        generator.m_Modules.SetMainTypes();

        for ( list<FileInfo>::const_iterator fi = importModules.begin();
              fi != importModules.end();
              ++fi ) {
            LoadDefinition(*fi, generator.m_Modules);
        }
        if ( !generator.m_Modules.Check() )
            Error("Errors was found...");
    
        if ( generateAllTypes )
            generator.GetAllTypes();

        if ( dataIn ) {
            TObject object = LoadValue(generator.m_Modules,
                                       dataIn, dataInTypeName);
            
            if ( dataOut )
                StoreValue(object, dataOut);
        }

        if ( !generator.m_GenerateTypes.empty() )
            generator.GenerateCode();
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


void LoadDefinition(const FileInfo& file, CModuleSet& types)
{
    if ( file.type != eASNText )
        Error("data definition format not supported");

    SourceFile in(file.name);
    ASNLexer lexer(in);
    ASNParser parser(lexer);
    try {
        parser.Modules(CFilePosition(file.name), types, types.modules);
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
        i->second->Print(out);
    }
}

TObject LoadValue(CModuleSet& types, const FileInfo& file,
                  const string& defTypeName)
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
    //    objIn->SetTypeMapper(&types);
    string typeName = objIn->ReadTypeName();
    if ( typeName.empty() ) {
        if ( defTypeName.empty() )
            Error("ASN.1 value type must be specified (-t)");
        typeName = defTypeName;
    }
    TTypeInfo typeInfo =
        CTypeRef(new CAnyTypeSource(types.ResolveFull(typeName))).Get();
    AnyType value;
    objIn->ReadExternalObject(&value, typeInfo);
    return make_pair(value, typeInfo);
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

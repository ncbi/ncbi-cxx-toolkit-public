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
* Author: Eugene Vasilchenko
*
* File Description:
*   main datatool file: argument processing and task manager
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.26  2000/02/01 21:48:01  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.25  2000/01/06 16:13:43  vasilche
* Updated help messages.
*
* Revision 1.24  1999/12/30 21:33:39  vasilche
* Changed arguments - more structured.
* Added intelligence in detection of source directories.
*
* Revision 1.23  1999/12/28 18:55:58  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.22  1999/12/21 17:18:35  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.21  1999/12/20 21:00:18  vasilche
* Added generation of sources in different directories.
*
* Revision 1.20  1999/12/01 17:36:26  vasilche
* Fixed CHOICE processing.
*
* Revision 1.19  1999/11/15 20:01:34  vasilche
* Fixed GCC error
*
* Revision 1.18  1999/11/15 19:36:16  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <memory>
#include <serial/tool/fileutil.hpp>
#include <serial/tool/parser.hpp>
#include <serial/tool/lexer.hpp>
#include <serial/tool/moduleset.hpp>
#include <serial/tool/module.hpp>
#include <serial/tool/type.hpp>
#include <serial/tool/generate.hpp>

USING_NCBI_SCOPE;

typedef pair<AnyType, TTypeInfo> TObject;

static void Help(void)
{
    NcbiCout <<
        "\n"
        "DataTool 1.0 arguments:\n"
        "\n"
        "  -H           display this message (optional)\n"
        "  -oH          display code generation arguments (optional)\n"
        "  -m <file>    ASN.1 module file\n"
        "  -mx <file>   XML module file\n"
        "  -M <files>   external ASN.1 module files (optional)\n"
        "  -Mx <files>  external XML module files (optional)\n"
        "  -i           ignore unresolved symbols (optional)\n"
        "  -f <file>    write ASN.1 module file (optional)\n"
        "  -fx <file>   write XML module file (optional)\n"
        "  -v <file>    read value in ASN.1 text format (optional)\n"
        "  -p <file>    write value in ASN.1 text format (optional)\n"
        "  -vx <file>   read value in XML format (optional)\n"
        "  -px <file>   write value in XML format (optional)\n"
        "  -d <file>    read value in ASN.1 binary format (type required) (optianal)\n"
        "  -t <type>    binary value type (optional)\n"
        "  -e <file>    write value in ASN.1 binary format (optional)\n" <<
        NcbiFlush;
}

static void GenerateHelp(void)
{
    NcbiCout <<
        "\n"
        "DataTool 1.0 code generation arguments (all optional):\n"
        "\n"
        "  -oH          display this message\n"
        "  -oA          generate C++ files for all types\n"
        "  -ot <types>  generate C++ files for listed types\n"
        "  -ox <types>  exclude listed types from generation\n"
        "  -oX          turn off recursive type generation\n"
        "  -od <file>   C++ code definition file\n"
        "  -of <file>   write list of generated C++ files\n"
        "  -opm <dir>   directory for searching source modules\n"
        "  -oph <dir>   directory for generated *.hpp files\n"
        "  -opc <dir>   directory for generated *.cpp files\n"
        "  -or <prefix> add prefix to generated file names\n"
        "  -ors         add source file dir to generated file names\n"
        "  -orm         add module name to generated file names\n"
        "  -orA         combine all -or* prefixes\n"
        "  -oR <dir>    set -op* and -or* arguments for NCBI dir tree\n" <<
        NcbiFlush;
}

static
const char* StringArgument(const char* arg)
{
    if ( !arg || !arg[0] ) {
        ERR_POST(Fatal << "argument expected");
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

static void LoadDefinitions(CFileSet& fileSet,
                            const string& modulesDir,
                            const list<FileInfo>& file);
static void StoreDefinition(const CFileSet& types, const FileInfo& file);
static TObject LoadValue(CFileSet& types, const FileInfo& file,
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
        ERR_POST(Fatal << "Invalid argument: " << arg);
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

    string modulesDir;
    list<FileInfo> mainModules;
    list<FileInfo> importModules;
    FileInfo moduleOut;
    FileInfo dataIn;
    string dataInTypeName;
    FileInfo dataOut;
    bool ignoreErrors = false;
    list<string> additionalDefinitions;

    bool generateAllTypes = false;
    CCodeGenerator generator;

    if ( argc <= 1 ) {
        Help();
        return 1;
    }
    else {
        for ( int i = 1; i < argc; ++i ) {
            const char* arg = argv[i];
            if ( arg[0] != '-' ) {
                ERR_POST(Fatal << "Invalid argument: " << arg);
            }
            switch ( arg[1] ) {
            case 'H':
                Help();
                return 1;
            case 'm':
                GetFilesIn(mainModules, arg, argv[++i]);
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
            case 'i':
                ignoreErrors = true;
                break;
            case 'o':
                switch ( arg[2] ) {
                case 'H':
                    GenerateHelp();
                    return 1;
                case 'A':
                    generateAllTypes = true;
                    break;
                case 't':
                    generator.IncludeTypes(StringArgument(argv[++i]));
                    break;
                case 'x':
                    generator.ExcludeTypes(StringArgument(argv[++i]));
                    break;
                case 'X':
                    generator.ExcludeRecursion();
                    break;
                case 'd':
                    generator.LoadConfig(FileInArgument(argv[++i]));
                    break;
                case 'D':
                    additionalDefinitions.push_back(StringArgument(argv[++i]));
                    break;
                case 'f':
                    generator.SetFileListFileName(FileOutArgument(argv[++i]));
                    break;
                case 'p':
                    // set directories
                    switch ( arg[3] ) {
                    case 'h':
                        generator.SetHPPDir(DirArgument(argv[++i]));
                        break;
                    case 'c':
                        generator.SetCPPDir(DirArgument(argv[++i]));
                        break;
                    case 'm':
                        modulesDir = DirArgument(argv[++i]);
                        break;
                    default:
                        ERR_POST(Fatal << "Invalid argument: " << arg <<
                                 "\nRun datatool -oH for more info");
                        break;
                    }
                    break;
                case 'r':
                    // set relative path of classes
                    switch ( arg[3] ) {
                    case '\0':
                        generator.SetFileNamePrefix(StringArgument(argv[++i]));
                        break;
                    case 's':
                        generator.SetFileNamePrefixSource(eFileName_FromSourceFileName);
                        break;
                    case 'm':
                        generator.SetFileNamePrefixSource(eFileName_FromModuleName);
                        break;
                    case 'A':
                        generator.SetFileNamePrefixSource(eFileName_UseAllPrefixes);
                        break;
                    default:
                        ERR_POST(Fatal << "Invalid argument: " << arg <<
                                 "\nRun datatool -oH for more info");
                    }
                    break;
                case 'R':
                    ++i;
                    generator.SetHPPDir(Path(DirArgument(argv[i]),
                                             "include"));
                    generator.SetCPPDir(Path(DirArgument(argv[i]),
                                             "src"));
                    modulesDir = Path(DirArgument(argv[i]), "src");
                    generator.SetFileNamePrefixSource(eFileName_FromSourceFileName);
                    break;
                default:
                    ERR_POST(Fatal << "Invalid argument: " << arg <<
                             "\nRun datatool -oH for more info");
                }
                break;
            default:
                ERR_POST(Fatal << "Invalid argument: " << arg <<
                         "\nRun datatool -H for more info");
            }
        }
    }

    iterate ( list<string>, i, additionalDefinitions ) {
        generator.AddConfigLine(*i);
    }
    additionalDefinitions.clear();

    try {
        if ( mainModules.empty() )
            ERR_POST(Fatal << "Module file not specified");

        
        LoadDefinitions(generator.GetMainModules(),
                        modulesDir, mainModules);
        if ( moduleOut )
            StoreDefinition(generator.GetMainModules(), moduleOut);

        LoadDefinitions(generator.GetImportModules(),
                        modulesDir, importModules);
        if ( !generator.Check() ) {
            ERR_POST((ignoreErrors? Error: Fatal) << "Errors was found...");
        }
    
        if ( generateAllTypes )
            generator.IncludeAllMainTypes();

        if ( dataIn ) {
            TObject object = LoadValue(generator.GetMainModules(),
                                       dataIn, dataInTypeName);
            
            if ( dataOut )
                StoreValue(object, dataOut);
        }

        generator.GenerateCode();
    }
    catch (exception& e) {
        ERR_POST(Fatal << e.what());
        return 1;
    }
    catch (...) {
        ERR_POST(Fatal << "unknown");
        return 1;
    }
	return 0;
}


void LoadDefinitions(CFileSet& fileSet,
                     const string& modulesDir,
                     const list<FileInfo>& names)
{
    iterate ( list<FileInfo>, fi, names ) {
        const string& name = *fi;
        if ( fi->type != eASNText ) {
            ERR_POST("data definition format not supported: " << name);
            continue;
        }
        try {
			SourceFile fName(name, modulesDir);
            ASNLexer lexer(fName);
            ASNParser parser(lexer);
            fileSet.AddFile(parser.Modules(name));
        }
        catch (exception& exc) {
            ERR_POST("Parsing failed: " << exc.what());
        }
        catch (...) {
            ERR_POST("Parsing failed: " << name);
        }
    }
}

void StoreDefinition(const CFileSet& fileSet, const FileInfo& file)
{
    if ( file.type != eASNText )
        ERR_POST(Fatal << "data definition format not supported");
    
    DestinationFile out(file);
    fileSet.PrintASN(out);
}

TObject LoadValue(CFileSet& types, const FileInfo& file,
                  const string& defTypeName)
{
    SourceFile in(file, file.type == eASNBinary);

    auto_ptr<CObjectIStream> objIn;
    switch ( file.type ) {
    case eASNText:
        objIn.reset(new CObjectIStreamAsn(in));
        break;
    case eASNBinary:
        objIn.reset(new CObjectIStreamAsnBinary(in));
        break;
    default:
        ERR_POST(Fatal << "value format not supported");
    }
    //    objIn->SetTypeMapper(&types);
    string typeName = objIn->ReadTypeName();
    if ( typeName.empty() ) {
        if ( defTypeName.empty() )
            ERR_POST(Fatal << "ASN.1 value type must be specified (-t)");
        typeName = defTypeName;
    }
    TTypeInfo typeInfo =
        CTypeRef(new CAnyTypeSource(types.ResolveInAnyModule(typeName, true))).Get();
    AnyType value;
    objIn->ReadExternalObject(&value, typeInfo);
    return make_pair(value, typeInfo);
}

void StoreValue(const TObject& object, const FileInfo& file)
{
    DestinationFile out(file, file.type == eASNBinary);

    auto_ptr<CObjectOStream> objOut;
    switch ( file.type ) {
    case eASNText:
        objOut.reset(new CObjectOStreamAsn(out));
        break;
    case eASNBinary:
        objOut.reset(new CObjectOStreamAsnBinary(out));
        break;
    default:
        ERR_POST(Fatal << "value format not supported");
    }
    objOut->Write(&object.first, object.second);
}

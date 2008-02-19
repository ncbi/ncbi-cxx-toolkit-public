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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>

#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>

#include <memory>

#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/code.hpp>
#include <serial/datatool/lexer.hpp>
#include <serial/datatool/dtdlexer.hpp>
#include <serial/datatool/parser.hpp>
#include <serial/datatool/dtdparser.hpp>
#include <serial/datatool/xsdlexer.hpp>
#include <serial/datatool/xsdparser.hpp>
#include <serial/datatool/moduleset.hpp>
#include <serial/datatool/module.hpp>
#include <serial/datatool/type.hpp>
#include <serial/datatool/generate.hpp>
#include <serial/datatool/datatool.hpp>
#include <serial/datatool/filecode.hpp>
#include <serial/objistrxml.hpp>
#include <serial/objostrxml.hpp>
#include <serial/error_codes.hpp>

#include <common/test_assert.h>  /* This header must go last */


#define NCBI_USE_ERRCODE_X   Serial_DataTool


BEGIN_NCBI_SCOPE

int CDataTool::Run(void)
{
    if ( !ProcessModules() )
        return 1;
    if ( !ProcessData() )
        return 1;
    if ( !GenerateCode() )
        return 1;

    return 0;
}

CDataTool::CDataTool(void)
{
    SetVersion( CVersionInfo(1,9,0) );
}

void CDataTool::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);

    auto_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("datatool", "work with ASN.1/XML data");

    // module arguments
    d->AddKey("m", "moduleFile",
              "module file(s)",
              CArgDescriptions::eString,
              CArgDescriptions::fAllowMultiple);
    d->AddDefaultKey("M", "externalModuleFile",
                     "external module file(s)",
                     CArgDescriptions::eString, NcbiEmptyString,
                     CArgDescriptions::fAllowMultiple);
    d->AddFlag("i",
               "ignore unresolved symbols");
    d->AddOptionalKey("f", "moduleFile",
                      "write ASN.1 module file",
                      CArgDescriptions::eOutputFile);
    d->AddOptionalKey("fx", "dtdFile",
                      "write DTD file (\"-fx m\" writes modular DTD file)",
                      CArgDescriptions::eOutputFile);
    d->AddOptionalKey("fxs", "XMLSchemaFile",
                      "write XML Schema file (\"-fxs m\" writes modular Schema file)",
                      CArgDescriptions::eOutputFile);
    d->AddOptionalKey("fd", "SpecificationDump",
                      "write specification dump file (datatool internal format)",
                      CArgDescriptions::eOutputFile);
    d->AddOptionalKey("ms", "moduleSuffix",
                      "suffix of modular DTD or Schema file name",
                      CArgDescriptions::eString);

    // data arguments
    d->AddOptionalKey("v", "valueFile",
                      "read value in ASN.1 text format",
                      CArgDescriptions::eInputFile);
    d->AddOptionalKey("vx", "valueFile",
                      "read value in XML format",
                      CArgDescriptions::eInputFile);
    d->AddOptionalKey("d", "valueFile",
                      "read value in ASN.1 binary format (-t is required)",
                      CArgDescriptions::eInputFile);
    d->AddOptionalKey("t", "type",
                      "binary value type (see \"-d\" argument)",
                      CArgDescriptions::eString);
    d->AddOptionalKey("dn", "filename",
                      "DTD module name in XML header (no extension). "
                      "If empty, omit DOCTYPE line.",
                      CArgDescriptions::eString);
    d->AddFlag("F",
               "read value completely into memory");
    d->AddOptionalKey("p", "valueFile",
                      "write value in ASN.1 text format",
                      CArgDescriptions::eOutputFile);
    d->AddOptionalKey("px", "valueFile",
                      "write value in XML format",
                      CArgDescriptions::eOutputFile);
    d->AddOptionalKey("pj", "valueFile",
                      "write value in JSON format",
                      CArgDescriptions::eOutputFile);
    d->AddOptionalKey("xmlns", "namespaceName",
                      "XML namespace name",
                      CArgDescriptions::eString);
    d->AddOptionalKey("e", "valueFile",
                      "write value in ASN.1 binary format",
                      CArgDescriptions::eOutputFile);
    d->AddFlag("sxo",
               "no scope prefixes in XML output");
    d->AddFlag("sxi",
               "no scope prefixes in XML input");

    // code generation arguments
    d->AddOptionalKey("oex", "exportSpec",
                      "class export specifier for MSVC",
                      CArgDescriptions::eString);
    d->AddOptionalKey("od", "defFile",
                      "code definition file",
                      CArgDescriptions::eInputFile);
    d->AddFlag("odi",
               "silently ignore absent code definition file");
    d->AddFlag("odw",
               "issue a warning about absent code definition file");
    d->AddOptionalKey("of", "listFile",
                      "write list of generated C++ files",
                      CArgDescriptions::eOutputFile);
    d->AddOptionalKey("oc", "basename",
                      "write combining C++ files",
                      CArgDescriptions::eString);
    d->AddFlag("oA",
               "generate C++ files for all types");
    d->AddOptionalKey("ot", "types",
                      "generate C++ files for listed types",
                      CArgDescriptions::eString);
    d->AddOptionalKey("ox", "types",
                      "exclude listed types from generation",
                      CArgDescriptions::eString);
    d->AddFlag("oX",
               "turn off recursive type generation");

    d->AddOptionalKey("on", "namespace",
                      "default namespace", 
                      CArgDescriptions::eString);

    d->AddOptionalKey("opm", "directory",
                      "directory for searching source modules",
                      CArgDescriptions::eString);
    d->AddOptionalKey("oph", "directory",
                      "directory for generated *.hpp files",
                      CArgDescriptions::eString);
    d->AddOptionalKey("opc", "directory",
                      "directory for generated *.cpp files",
                      CArgDescriptions::eString);

    d->AddOptionalKey("or", "prefix",
                      "add prefix to generated file names",
                      CArgDescriptions::eString);
    d->AddFlag("orq",
               "use quoted syntax form for generated include files");
    d->AddFlag("ors",
               "add source file dir to generated file names");
    d->AddFlag("orm",
               "add module name to generated file names");
    d->AddFlag("orA",
               "combine all -or* prefixes");
    d->AddFlag("ocvs",
               "create \".cvsignore\" files");
    d->AddOptionalKey("oR", "rootDirectory",
                      "set \"-o*\" arguments for NCBI directory tree",
                      CArgDescriptions::eString);

    d->AddFlag("oDc",
               "turn on generation of DOXYGEN-style comments");
    d->AddOptionalKey("odx", "URL",
                      "URL of documentation root folder (for DOXYGEN)",
                      CArgDescriptions::eString);
    d->AddFlag("lax_syntax",
               "allow non-standard ASN.1 syntax accepted by asntool");
    d->AddOptionalKey("pch", "file",
                      "name of the precompiled header to include in all *.cpp files",
                      CArgDescriptions::eString);

    SetupArgDescriptions(d.release());
}

bool CDataTool::ProcessModules(void)
{
    const CArgs& args = GetArgs();

    list<string> modulesPath;

    if ( const CArgValue& oR = args["oR"] ) {
        // NCBI directory tree
        const string& rootDir = oR.AsString();
        generator.SetRootDir(rootDir);
        generator.SetHPPDir(Path(rootDir, "include"));
        string srcDir = Path(rootDir, "src");
        generator.SetCPPDir(srcDir);
        modulesPath.push_back(srcDir);
        generator.SetFileNamePrefixSource(eFileName_FromSourceFileName);
        generator.SetDefaultNamespace("NCBI_NS_NCBI::objects");
    }
    
    if ( const CArgValue& opm = args["opm"] ) {
        modulesPath.clear();
        NStr::Split(opm.AsString(), ",", modulesPath);
    }
    
    SourceFile::EType srctype =
        LoadDefinitions(generator.GetMainModules(),
                        modulesPath, args["m"].GetStringList(), false);
    
    if (srctype == SourceFile::eASN) {
        LoadDefinitions(generator.GetImportModules(),
                        modulesPath, args["M"].GetStringList(), true, srctype);
    }

    if ( args["sxo"] ) {
        CDataType::SetEnforcedStdXml(true);
    }

    if ( const CArgValue& f = args["f"] ) {
        generator.GetMainModules().PrintASN(f.AsOutputFile());
        f.CloseFile();
    }
    if ( const CArgValue& f = args["fd"] ) {
        generator.GetMainModules().PrintSpecDump(f.AsOutputFile());
        f.CloseFile();
    }

    if ( const CArgValue& fx = args["fx"] ) {
        if (srctype == SourceFile::eDTD || srctype == SourceFile::eXSD) {
            CDataType::SetEnforcedStdXml(true);
        }
        if ( fx.AsString() == "m" ) {
            if ( const CArgValue& ms = args["ms"] ) {
                CDataTypeModule::SetModuleFileSuffix(ms.AsString());
            }
            generator.ResolveImportRefs();
            CDataType::EnableDTDEntities(true);
            generator.GetMainModules().PrintDTDModular();
        } else {
            generator.GetMainModules().PrintDTD(fx.AsOutputFile());
            fx.CloseFile();
        }
    }

    if ( const CArgValue& ax = args["fxs"] ) {
        if (srctype == SourceFile::eDTD || srctype == SourceFile::eXSD) {
            CDataType::SetEnforcedStdXml(true);
        }
        if ( ax.AsString() == "m" ) {
            if ( const CArgValue& ms = args["ms"] ) {
                CDataTypeModule::SetModuleFileSuffix(ms.AsString());
            }
            generator.ResolveImportRefs();
            generator.GetMainModules().PrintXMLSchemaModular();
        } else {
            generator.GetMainModules().PrintXMLSchema(ax.AsOutputFile());
            ax.CloseFile();
        }
    }

    if ( !generator.Check() ) {
        if ( !args["i"] ) { // ignored
            ERR_POST_X(1, "some types are unknown");
            return false;
        }
        else {
            ERR_POST_X(2, Warning << "some types are unknown: ignoring");
        }
    }
    return true;
}

bool CDataTool::ProcessData(void)
{    
    const CArgs& args = GetArgs();
    bool stdXmlIn = false;
    if ( args["sxi"] ) {
        stdXmlIn = true;
    }
    bool stdXmlOut = false;
    if ( args["sxo"] ) {
        stdXmlOut = true;
    }

    // convert data
    ESerialDataFormat inFormat;
    string inFileName;
    const CArgValue& t = args["t"];
    
    if ( const CArgValue& v = args["v"] ) {
        inFormat = eSerial_AsnText;
        inFileName = v.AsString();
    }
    else if ( const CArgValue& vx = args["vx"] ) {
        inFormat = eSerial_Xml;
        inFileName = vx.AsString();
    }
    else if ( const CArgValue& d = args["d"] ) {
        if ( !t ) {
            ERR_POST_X(3, "ASN.1 value type must be specified (-t)");
            return false;
        }
        inFormat = eSerial_AsnBinary;
        inFileName = d.AsString();
    }
    else // no input data
        return true;

    auto_ptr<CObjectIStream>
        in(CObjectIStream::Open(inFormat, inFileName, eSerial_StdWhenAny));
    if (inFormat == eSerial_Xml) {
        CObjectIStreamXml *is = dynamic_cast<CObjectIStreamXml*>(in.get());
        if (stdXmlIn) {
            is->SetEnforcedStdXml(true);
        }
        is->SetDefaultStringEncoding(eEncoding_Unknown);
    }

    string typeName;
    if ( t ) {
        typeName = t.AsString();
        in->ReadFileHeader();
    }
    else {
        typeName = in->ReadFileHeader();
    }

    TTypeInfo typeInfo =
        generator.GetMainModules().ResolveInAnyModule(typeName, true)->
        GetTypeInfo().Get();
    
    // determine output data file
    ESerialDataFormat outFormat;
    string outFileName;
    bool use_nsName = false;
    string nsName;
    
    if ( const CArgValue& p = args["p"] ) {
        outFormat = eSerial_AsnText;
        outFileName = p.AsString();
    }
    else if ( const CArgValue& px = args["px"] ) {
        outFormat = eSerial_Xml;
        outFileName = px.AsString();
        if ( const CArgValue& px_ns = args["xmlns"] ) {
            use_nsName = true;
            nsName = px_ns.AsString();
        }
    }
    else if ( const CArgValue& pj = args["pj"] ) {
        outFormat = eSerial_Json;
        outFileName = pj.AsString();
    }
    else if ( const CArgValue& e = args["e"] ) {
        outFormat = eSerial_AsnBinary;
        outFileName = e.AsString();
    }
    else {
        // no input data
        outFormat = eSerial_None;
    }

    if ( args["F"] ) {
        // read fully in memory
        AnyType value;
        in->Read(&value, typeInfo, CObjectIStream::eNoFileHeader);
        if ( outFormat != eSerial_None ) {
            // store data
            auto_ptr<CObjectOStream>
                out(CObjectOStream::Open(outFormat, outFileName,
                                         eSerial_StdWhenAny));
            if ( outFormat == eSerial_Xml ) {
                CObjectOStreamXml *os = dynamic_cast<CObjectOStreamXml*>(out.get());
                if (stdXmlOut) {
                    os->SetEnforcedStdXml(true);
                }
                os->SetDefaultStringEncoding(eEncoding_Unknown);
                if (use_nsName) {
                    os->SetReferenceSchema(true);
                    if (!nsName.empty()) {
                        os->SetDefaultSchemaNamespace(nsName);
                    }
                }
                // Set DTD file name (default prefix is added in any case)
                if( const CArgValue& dn = args["dn"] ) {
                    const string& name = dn.AsString();
                    if ( name.empty() ) {
                        os->SetReferenceDTD(false);
                    }
                    else {
                        os->SetReferenceDTD(true);
                        os->SetDTDFileName(name);
                    }
                }
            }
            out->Write(&value, typeInfo);
        }
    }
    else {
        if ( outFormat != eSerial_None ) {
            // copy
            auto_ptr<CObjectOStream>
                out(CObjectOStream::Open(outFormat, outFileName,
                                         eSerial_StdWhenAny));
            if ( outFormat == eSerial_Xml ) {
                CObjectOStreamXml *os = dynamic_cast<CObjectOStreamXml*>(out.get());
                if (stdXmlOut) {
                    os->SetEnforcedStdXml(true);
                }
                os->SetDefaultStringEncoding(eEncoding_Unknown);
                if (use_nsName) {
                    os->SetReferenceSchema(true);
                    if (!nsName.empty()) {
                        os->SetDefaultSchemaNamespace(nsName);
                    }
                }
                // Set DTD file name (default prefix is added in any case)
                if( const CArgValue& dn = args["dn"] ) {
                    const string& name = dn.AsString();
                    if ( name.empty() ) {
                        os->SetReferenceDTD(false);
                    }
                    else {
                        os->SetReferenceDTD(true);
                        os->SetDTDFileName(name);
                    }
                }
            }
            CObjectStreamCopier copier(*in, *out);
            copier.Copy(typeInfo, CObjectStreamCopier::eNoFileHeader);
            // In case the input stream has more than one object of this type,
            // keep converting them
            for (bool go=true; go; ) {
                try {
                    copier.Copy(typeInfo);
                } catch (CEofException&) {
                    go = false;
                }
            }
        }
        else {
            // skip
            in->Skip(typeInfo, CObjectIStream::eNoFileHeader);
        }
    }
    return true;
}

bool CDataTool::GenerateCode(void)
{
    const CArgs& args = GetArgs();

    // load generator config
    if ( const CArgValue& od = args["od"] )
        generator.LoadConfig(od.AsString(), args["odi"], args["odw"]);
    //if ( const CArgValue& oD = args["oD"] )
    //    generator.AddConfigLine(oD.AsString());

    // set list of types for generation
    if ( args["oX"] )
        generator.ExcludeRecursion();
    if ( args["oA"] )
        generator.IncludeAllMainTypes();
    if ( const CArgValue& ot = args["ot"] )
        generator.IncludeTypes(ot.AsString());
    if ( const CArgValue& ox = args["ox"] )
        generator.ExcludeTypes(ox.AsString());

    if ( !generator.HaveGenerateTypes() )
        return true;

    // set the export specifier, if provided
    if ( const CArgValue& oex = args["oex"] ) {
        string ex;
        ex = generator.GetConfig().Get("-","_export");
        if (ex.empty()) {
            ex = oex.AsString();
        }
        CClassCode::SetExportSpecifier(ex);
    }
    // define the Doxygen group
    {
        if ( args["oDc"] ) {
            CClassCode::SetDoxygenComments(true);
            if ( const CArgValue& odx = args["odx"] ) {
                string root = odx.AsString();
                if (root.empty()) {
                    // default
                    root = "http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/lxr/source";
                }
                CClassCode::SetDocRootURL(root);
            }
            string group = generator.GetConfig().Get("-","_addtogroup_name");
            CClassCode::SetDoxygenGroup(group);
            group = generator.GetConfig().Get("-","_ingroup_name");
            generator.SetDoxygenIngroup(group);
            group = generator.GetConfig().Get("-","_addtogroup_description");
            generator.SetDoxygenGroupDescription(group);
        } else {
            CClassCode::SetDoxygenComments(false);
        }
    }

    // prepare generator
    
    // set namespace
    if ( const CArgValue& on = args["on"] )
        generator.SetDefaultNamespace(on.AsString());
    
    // set output files
    if ( const CArgValue& oc = args["oc"] ) {
        const string& fileName = oc.AsString();
        generator.SetCombiningFileName(fileName);
        generator.SetFileListFileName(fileName+".files");
    }
    if ( const CArgValue& of = args["of"] )
        generator.SetFileListFileName(of.AsString());
    
    // set directories
    if ( const CArgValue& oph = args["oph"] )
        generator.SetHPPDir(oph.AsString());
    if ( const CArgValue& opc = args["opc"] )
        generator.SetCPPDir(opc.AsString());
    
    // set file names prefixes
    if ( const CArgValue& orF = args["or"] )
        generator.SetFileNamePrefix(orF.AsString());
    if ( args["orq"] )
        generator.UseQuotedForm(true);
    if ( args["ocvs"] )
        generator.CreateCvsignore(true);
    if ( args["ors"] )
        generator.SetFileNamePrefixSource(eFileName_FromSourceFileName);
    if ( args["orm"] )
        generator.SetFileNamePrefixSource(eFileName_FromModuleName);
    if ( args["orA"] )
        generator.SetFileNamePrefixSource(eFileName_UseAllPrefixes);

    // precompiled header
    if ( const CArgValue& pch = args["pch"] )
        CFileCode::SetPchHeader(pch.AsString());
    
    // generate code
    generator.GenerateCode();
    return true;
}


SourceFile::EType CDataTool::LoadDefinitions(
    CFileSet& fileSet, const list<string>& modulesPath,
    const CArgValue::TStringArray& nameList,
    bool split_names, SourceFile::EType srctype)
{
    SourceFile::EType moduleType;
    list<string> names;

    ITERATE (CArgValue::TStringArray, n, nameList) {
        if (split_names) {
            list<string> t;
            NStr::Split(*n, " ", t);
            names.insert(names.end(), t.begin(), t.end());
        } else {
            names.push_back(*n);
        }
    }

    ITERATE ( list<string>, fi, names ) {
        const string& name = *fi;
        if ( !name.empty() ) {
            SourceFile fName(name, modulesPath);
            moduleType = fName.GetType();

// if first module has unknown type - assume ASN
            if (srctype == SourceFile::eUnknown) {
                if (moduleType == SourceFile::eUnknown) {
                    moduleType = SourceFile::eASN;
                }
                srctype = moduleType;
            }
// if second module has unknown type - assume same as the previous one
            else {
                if (moduleType == SourceFile::eUnknown) {
                    moduleType = srctype;
                }
// if modules have different types - exception
                else if (moduleType != srctype) {
                    NCBI_THROW(CDatatoolException,eWrongInput,
                               "Unable to process modules of different types"
                               " simultaneously: "+name);
                }
            }

            switch (moduleType) {
            default:
                NCBI_THROW(CDatatoolException,eWrongInput,"Unknown file type: "+name);

            case SourceFile::eASN:
                {
                    ASNLexer lexer(fName,name);
                    lexer.AllowIDsEndingWithMinus(GetArgs()["lax_syntax"]);
                    ASNParser parser(lexer);
                    fileSet.AddFile(parser.Modules(name));
                }
                break;
            case SourceFile::eDTD:
                {
                    DTDLexer lexer(fName,name);
                    DTDParser parser(lexer);
                    fileSet.AddFile(parser.Modules(name));
                    CDataType::SetXmlSourceSpec(true);
                }
                break;
            case SourceFile::eXSD:
                {
                    XSDLexer lexer(fName,name);
                    XSDParser parser(lexer);
                    fileSet.AddFile(parser.Modules(name));
                    CDataType::SetXmlSourceSpec(true);
                }
                break;
            }
        }
    }
    return srctype;
}

END_NCBI_SCOPE


int main(int argc, const char* argv[])
{
    USING_NCBI_SCOPE;
    CException::EnableBackgroundReporting(false);
    return CDataTool().AppMain(argc, argv, 0, eDS_Default, 0, "datatool");
}

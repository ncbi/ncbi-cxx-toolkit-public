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
 * Authors:  Anton Lavrentiev, Vladimir Ivanov
 *
 * File Description:
 *    MSVC 6.0 project file converter. Expand a signle configuration project
 *    file to multi-configuration project file.
 *
 */

/*  !!! Warning !!!

    Seems that PCRE has a bug (or may be this is a feature?).
    It doesn't correct handle regular expressions with brackets "[]"
    if a PCRE_MULTILINE / PCRE_DOTALL flag is specified
    (may be and some other).
    It is necessary to investigate and try a new PCRE version.
*/


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <util/regexp.hpp>


USING_NCBI_SCOPE;


/// End-of-line symbol.
const char* kEOL = "\n";

/// String to replace matched data in templates.
const char* kTemplate = "@@@";

/// Single/Multiline PCRE compilation flags.
const CRegexp::TCompile kSL = CRegexp::fCompile_dotall |
                              CRegexp::fCompile_ungreedy;
const CRegexp::TCompile kML = CRegexp::fCompile_newline | 
                              CRegexp::fCompile_ungreedy;
const CRegexp::TCompile kDF = CRegexp::fCompile_default;


/////////////////////////////////////////////////////////////////////////////
//  
//  Configurations
//

/// Configurations.
static const char* kConfigName[] = { "Release", "Debug"  };

/// Suffixes. Empty suffix ("") should be last.
static const char* kSuffixName[] = { "DLL", "MT",  ""    };  
static const char* kSuffixLib[]  = { "/MD", "/MT", "/ML" };


/////////////////////////////////////////////////////////////////////////////
//
//  Main application class
//

class CMainApplication : public CNcbiApplication
{
public:
    /// Configuration enumerators.
    /// Lesser value have more priority.
    enum EConfig {
        eRelease = 0,
        eDebug,
        eConfigMax
    };
    enum ESuffix {
        eDLL = 0,
        eMT,
        eST,
        eSuffixMax
    };

    /// Define a replacement command. 
    struct SReplacement {
        const char*  from;  ///< Change from value.
        const char*  to;    ///< Change to value.
        size_t       count; ///< Maximum count of changes (0 - infinite).
    };

public:
    /// Constructor.
    CMainApplication(void);

    /// Overrided methods.
    virtual void Init(void);
    virtual int  Run (void);

private:
    /// Parse program arguments.
    void ParseArguments(void);

    /// Process all project files in the specified directory.
    void ProcessDir(const string& dir_name);

    /// Process one project file only.
    void ProcessFile(const string& file_name);

    /// Replace configuration settings accordingly required configuration.
    /// Return name of the configuration with or without used GUI.
    string Configure(
        const string& cfg_template,
        string&       cfg_str,
        EConfig       config,
        ESuffix       suffix
    );

private:
    /// Parameters.
    bool    m_Cfg[eConfigMax][eSuffixMax]; //< Configurations
    string  m_Path;                        //< Directory or file name
    bool    m_IsRecursive;                 //< Process m_Path recursively
    bool    m_IsDistribution;              //< Distribution mode
    bool    m_CreateBackup;                //< Create backup files
};


/// Iterators by possible configurations.
#define ITERATE_CONFIG(i) \
    for (int i = 0; i < CMainApplication::eConfigMax; i++)

#define ITERATE_SUFFIX(i) \
    for (int i = 0; i < CMainApplication::eSuffixMax; i++)

/// Compose configuration name.
#define CONFIG_NAME(config, suffix) \
    ((string)kConfigName[config] + kSuffixName[suffix]);



CMainApplication::CMainApplication(void)
    : m_Path(kEmptyStr), m_IsRecursive(false), m_IsDistribution(false)
{
    memset(m_Cfg, 0, sizeof(m_Cfg));
}


void CMainApplication::Init(void)
{
    // Set error posting and tracing on maximum.
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Describe the expected command-line arguments.
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddPositional
        ("path",
         "Directory or project file name to convert",
         CArgDescriptions::eString);
    arg_desc->AddOptionalPositional
        ("config_type",
         "Type of configuration to produce (default is \"all\")",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("config_type", &(*new CArgAllow_Strings, "all", "release", "debug"));
    arg_desc->AddOptionalPositional
        ("config_spec",
         "Set of build configurations (default is all)",
         CArgDescriptions::eInteger);
    arg_desc->SetConstraint
        ("config_spec", new CArgAllow_Integers(2, 4));
    arg_desc->AddFlag
        ("r", "Process all project files in the <path> recursively " \
         "(only if <path> is a directory)");
    arg_desc->AddFlag
        ("d", "Distributive mode;  remove include/library paths, " \
         "which refers to DIZZY");
    arg_desc->AddFlag
        ("b", "Create backup file");

/*
    arg_desc->AddFlag
        ("dll", "generate DLL configuration");
    arg_desc->AddFlag
        ("st", "generate single-threaded configuration");
    arg_desc->AddFlag
        ("mt", "generate multi-threaded configuration");
*/

    // Specify USAGE context.
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
       "Automatic multi config for Microsoft Visual C++ Project File V 6.0");

    // Setup arg.descriptions for this application.
    SetupArgDescriptions(arg_desc.release());
}


void CMainApplication::ParseArguments(void)
{
    CArgs args = GetArgs();

    // Path
    m_Path = args["path"].AsString();

    // Flags
    m_IsRecursive    = args["r"];
    m_IsDistribution = args["d"];
    m_CreateBackup   = args["b"];

    // Configurations
    string cfg = args["config_type"] ? args["config_type"].AsString() : "all";
    bool cfg_enable[eConfigMax];
    memset(cfg_enable, 0, sizeof(cfg_enable));
    if (cfg == "all") {
        cfg_enable[eRelease] = true;
        cfg_enable[eDebug]   = true;
    }
    else if (cfg == "debug") {
        cfg_enable[eDebug] = true;
    }
    else if (cfg == "release") {
        cfg_enable[eRelease] = true;
    }

    // Special parameter
    int spec = args["config_spec"] ? args["config_spec"].AsInteger() : 0;

    ITERATE_CONFIG(i) {
        if ( cfg_enable[i] ) {
            switch ( spec ) {
            case 2:
                // DLL
                m_Cfg[i][eDLL] = true;
                break;
            case 3:
                // MT
                m_Cfg[i][eMT]  = true;
                break;
            case 4:
                // DLL, ST
                m_Cfg[i][eDLL] = true;
                m_Cfg[i][eST]  = true;
                break;
            default:
                // DLL, MT, ST
                m_Cfg[i][eDLL] = true;
                m_Cfg[i][eMT]  = true;
                m_Cfg[i][eST]  = true;
            }
        }
    }
}


void CMainApplication::ProcessDir(const string& dir_name)
{
    CDir dir(dir_name);
    CDir::TEntries contents = dir.GetEntries("*");
    ITERATE(CDir::TEntries, i, contents) {
        string name  = (*i)->GetName();
        if ( name == "."  ||  name == ".."  ||  
             name == string(1,CDir::GetPathSeparator()) ) {
            continue;
        }
        string path = (*i)->GetPath();
        if ( (*i)->IsFile()  &&  NStr::EndsWith(name, ".dsp") ) {
            ProcessFile(path);
        } 
        else if ( (*i)->IsDir() ) {
            if ( m_IsRecursive ) {
                ProcessDir(path);
            }
        }
    }
}


void CMainApplication::ProcessFile(const string& file_name)
{
    try 
    {
        string content;  // File content
        string str;      // Temporary string

        // Read all file into memory.

        CNcbiIfstream is(file_name.c_str(), IOS_BASE::in | IOS_BASE::binary);
        if ( !is.is_open() ) {
            throw (string)"cannot be opened";
        }
        while ( is ) {
            char buf[16384];
            is.read(buf, sizeof(buf)-1);
            size_t count = is.gcount();
            if ( count ) {
                buf[count] = '\0';
                content.append(buf);
            }
        }
        is.close();


        // DOS-2-UNIX conversion.
        // (shouldn't be here, but just in case for sanity).

        content = NStr::Replace(content, "\r\n", kEOL);
        content = NStr::Replace(content, "\r",   kEOL);
        CRegexpUtil re(content);


        // Check signature.

        if ( !re.Exists(
            "^# Microsoft Developer Studio Project File - Name=.*\n" \
            "# Microsoft Developer Studio Generated Build File, " \
            "Format Version 6.00\n" \
            "# \\*\\* DO NOT EDIT \\*\\*\n\n" \
            "# TARGTYPE .*\n",  kSL) ) {
            throw (string)"doesn't look like MSVC++ 6.0 Project File";
        }


        // Check multiple configurations.

        if ( re.Exists("\n!IF .*\n!ELSEIF .*\n!ENDIF.*# Begin Target", kSL) ) {
            throw (string)"contains more than one configuration";
        }


        // Check for per-config dependencies, warn if any and disable it.

        if ( re.Exists("^# PROP +AllowPerConfigDependencies +1", kML) ) {
            if ( re.Exists("\n# Begin Target.*\n!IF ", kSL) ) {
                ERR_POST(Warning <<
                         "File contains per-configuration dependencies,\n" \
                         "which may or may not be handled correctly by "   \
                         "this program.");
            }
        }
        re.Replace("^(# PROP +AllowPerConfigDependencies +)1", "{$1}0", kML);


        // Extract beginning of the file.
        // Get template to summarize configurations in the header.

        str = re.Extract("^.*\n!MESSAGE *\n\n", kSL);
        CRegexpUtil re_header(str);
        str = re.Extract("^!MESSAGE +\".*\".*$", kML);
        CRegexpUtil re_header_cfg_template(str);
        re_header_cfg_template.Replace("(!MESSAGE +\".* - \\w+ ).*(\".*)$",
                                       string("$1") + kTemplate + "$2", kSL);
        string cfg_name = re_header_cfg_template.Extract("\".* - .*\"", kML);


        // Extract middle part.

        string middle = 
            re.Extract("\n(# Begin Project.*)\n*(!IF|# PROP BASE) +",
                       kSL, kDF, 1)
            + kEOL + kEOL;


        // Extract configuration-dependent part. Make template from it.

        str = re.Extract("\n(!IF .*\n!E.*)\n.*# Begin Target", kSL, kDF, 1);
        CRegexpUtil re_cfg_template(str);
        if ( str.empty() ) {
            // Extract a configuration-dependent part.
            str = re.Extract("\n(# PROP BASE .*\n)# Begin Target", kSL, kDF,1);
            // Get all together.
            str = "!IF  \"$(CFG)\" == " + cfg_name + "\n\n" + str +"\n!ENDIF ";
            re_cfg_template.Reset(str);
        } else {
            re_cfg_template.Replace("^!ELSEIF .*$", "!ENDIF", kML);
            re_cfg_template.Replace("^(!IF .*\".* - \\w+ ).*(\".*)$",
                                    string("$1") + kTemplate + "$2", kML);
        }

        ITERATE_CONFIG(i) {
            ITERATE_SUFFIX(j) {
                string cfg = CONFIG_NAME(i,j);
                re_cfg_template.SetRange("^# PROP ");
                re_cfg_template.ReplaceRange(cfg, kTemplate, kDF, kDF,
                                             CRegexpUtil::eOutside);
                re_cfg_template.SetRange("^# PROP .*\"[^ ]*" + cfg +
                                         "[^ ]*\"");
                re_cfg_template.ReplaceRange(cfg, kTemplate, kDF, kDF,
                                             CRegexpUtil::eInside);
            }
        }


        // Extract tail of the file.

        string tail;
        size_t pos = content.find("\n# Begin Target");
        if (pos != NPOS) {
            tail = content.substr(pos);

            size_t src_start = 0;
            size_t src_end   = 0;

            // Process all custom builds.
            for (;;) {
                // Find each source file description.
                src_start = tail.find("\n# Begin Source File", src_end);
                if (src_start == NPOS) {
                    break;
                }
                src_end = tail.find("\n# End Source File", src_start);
                str = tail.substr(src_start, src_end - src_start);

                // Check for custom build.
                CRegexpUtil re_cb(str);
                re_cb.Replace("\n!ELSEIF .*\n!ENDIF", "\n!ENDIF", kSL);

                string str_cb_template = 
                    re_cb.Extract("!IF .*\n# Begin Custom Build.*\n!ENDIF",
                                  kSL);
                if ( str_cb_template.empty() ) {
                    str_cb_template = re_cb.Extract("\nSOURCE=.*\n(.*# Begin Custom Build.*)$", kSL, kDF, 1);
                    if ( !str_cb_template.empty() ) {
                        str_cb_template = "!IF  \"$(CFG)\" == " +
                            cfg_name + "\n\n" + str_cb_template + "\n\n!ENDIF";
                        re_cb.Replace("(\nSOURCE=.*\n).*$", "$1\n@CB", kSL);
                    }
                } else {
                    re_cb.Replace("(.*\n)!IF .*\n!ENDIF(.*)$", "$1@CB$2", kSL);
                }

                // Is custom build block found?
                if ( !str_cb_template.empty() ) {
                    // Replace config name wherever appropriate.
                    ITERATE_CONFIG(i) {
                        ITERATE_SUFFIX(j) {
                            string cfg = CONFIG_NAME(i,j);
                            str_cb_template = NStr::Replace(str_cb_template,
                                                            cfg, kTemplate);
                        }
                    }
                    // Accumulate parts to replace. 
                    string cb;
                    ITERATE_CONFIG(i) {
                        ITERATE_SUFFIX(j) {
                            // Is configuration [i,j] enabled?
                            if ( m_Cfg[i][j] ) {
                                string cfg = CONFIG_NAME(i,j);
                                cb += NStr::Replace(str_cb_template,
                                                    kTemplate, cfg);
                            }
                        }
                    }
                    // Replace ENDIF..IF with ELSEIF.
                    cb = NStr::Replace(cb, "!ENDIF!IF", "!ELSEIF") + kEOL;

                    // Put custom build back.
                    re_cb.Replace("@CB", cb, kSL);
                    tail.replace(src_start, src_end - src_start, re_cb);
                    pos += re_cb.GetResult().length();
                }
            }
        }
        CRegexpUtil re_tail(tail);


        // Get template to summarize configurations in the tail.

        str = re_tail.Extract("^# +Name +\".*\"", kML);
        CRegexpUtil re_tail_cfg_template(str);
        re_tail_cfg_template.Replace("(.*\".* - \\w+ ).*(\".*)$",
                                     string("$1") + kTemplate + "$2", kSL);


        // Make required replacements and configs.

        int    default_found = false;
        string cfg_header;
        string cfg_tail;
        string eol = "";
        string cfgs;

        ITERATE_CONFIG(i) {
            ITERATE_SUFFIX(j) {
                // Is configuration $i$j enabled?
                if ( m_Cfg[i][j] ) {
                    string cfg = CONFIG_NAME(i,j);

                    // Accumulate configurations to replace. 
                    cfg_header += eol + NStr::Replace(re_header_cfg_template,
                                                      kTemplate, cfg);
                    cfg_tail   += eol + NStr::Replace(re_tail_cfg_template,
                                                      kTemplate, cfg);

                    // Is a first configuration (namely default) ?
                    // It must be a configuration with maximum priority.
                    if ( !default_found ) {
                        // Replace all conf. names in the header with
                        // the name of default configuration.
                        re_header.Replace("(CFG=[\"]*.* - \\w+ ).*(\"|\n)",
                                          "$1" + cfg + "$2", kSL);
                        default_found = true;
                        eol = kEOL;
                    }
                    // Configure $i$j.
                    cfgs += Configure(re_cfg_template, str,
                                      EConfig(i), ESuffix(j)) + " ";
                    middle += str;
                }
            }
        }
        // Replace ENDIF..IF with ELSEIF.
        CRegexpUtil re_middle(middle);
        re_middle.Replace("!ENDIF\\s*!IF", "!ELSEIF", kSL);


        // Summarize configurations in the header and
        // tail parts of the project.

        re_header.Replace("^!MESSAGE +\".*\".*$", cfg_header, kML);
        re_tail.Replace  ("^# Name +\".*\".*$",   cfg_tail,   kML);


        // Glue all parts together and make UNIX-2-DOS conversion.

        str = (string)re_header + (string)re_middle + kEOL + (string)re_tail;
        str = NStr::Replace(str, kEOL, "\r\n");


        // Write content into new file and than rename it.

        string file_name_new = file_name + ".new";
        CNcbiOfstream os(file_name_new.c_str(),
                         IOS_BASE::out | IOS_BASE::binary);
        if ( !os.is_open() ) {
            throw (string)"cannot create output file " + file_name_new;
        }
        os << str;
        os.flush();
        if ( !os ) {
            throw (string)"cannot write to file " + file_name_new;
        }
        os.close();

        // Print names of created configurations
        LOG_POST(file_name << ":  " << cfgs);

        // Replace original project file (backup kept in .bak).
        CTime ftime;
        CFile(file_name).GetTime(&ftime);
        if ( m_CreateBackup ) {
            string file_backup =  file_name + ".bak";
            CFile(file_backup).Remove();
            CFile(file_name).Rename(file_backup);
        } else {
            CFile(file_name).Remove();
        }
        if ( !CFile(file_name_new).Rename(file_name) ) {
            throw (string)"cannot rename file";
        }
        CFile(file_name).SetTime(&ftime);
    }
    catch (string& e) {
        ERR_POST(file_name << ": " << e << ".");
    }
    catch (CException& e) {
        NCBI_REPORT_EXCEPTION(file_name + ": ", e);
    }
    catch (...) {
        ERR_POST(file_name << ": unknown error");
    }
}


string CMainApplication::Configure(const string& cfg_template,
                                   string& cfg_str,
                                   EConfig config, ESuffix suffix)
{
    // Configuration name.
    string cfg = CONFIG_NAME(config, suffix);


    // Replace templated chunks with configuration name.

    cfg_str = NStr::Replace(cfg_template, kTemplate, cfg);


    // Replace debugging macro.

    if (config == eDebug) {
        cfg_str = NStr::Replace(cfg_str, "NDEBUG", "_DEBUG");
    } else {
        cfg_str = NStr::Replace(cfg_str, "_DEBUG", "NDEBUG");
    }

    // Check used GUI.
    
    CRegexpUtil re(cfg_str);
    string gui;
    string wxdll;
    bool   wxdll_making = false;

    if (re.Exists("^# ADD .*CPP .*__WX(DEBUG|MSW)__", kML)  ||
        re.Exists("^# ADD .*CPP .*WX.{2,3}INGDLL", kML)) {
        // The project use wxWindows (or is a part of wxWindows).
        gui = "wxwin";
        // Flag proper macros for DLL mode.
        if ( suffix == eDLL ) {
            if ( re.Exists("^# ADD .*CPP .*/D *[\"]{0,1}WXMAKINGDLL=*[0-9]*[\"]{0,1}", kML) ) {
                wxdll = "/D \"WXMAKINGDLL=1\"";
                wxdll_making = true;
            } else {
                wxdll = "/D \"WXUSINGDLL=1\"";
            }
        }
        // Remove some wxWindows macros, which are surely
        // configuration-dependent.
        re.SetRange("^# ADD .*CPP ");
        re.ReplaceRange("  */D  *[\"]{0,1}__WXDEBUG__=*[0-9]*[\"]{0,1}",
                        kEmptyStr);
        re.ReplaceRange("  */D  *[\"]{0,1}WX.{2,3}INGDLL=*[0-9]*[\"]{0,1}",
                        kEmptyStr);

    } else if (re.Exists("^# ADD .*LINK32 .*fltk[a-z]*[.]lib", kML)) {
        // The project is a FLTK-dependent project.
        gui = "fltk";
    }

    // Either replace with hooks, or just remove the compiler switches,
    // which may be configuration-dependent or inconsistent.

    const SReplacement ksOpt[] = {
        { "/Gm"           , ""   , 0 },
        { "/GZ"           , ""   , 0 },
        { "/G[0-9]"       , ""   , 0 },
        { "/FR"           , ""   , 0 },
        { "/Fr"           , ""   , 0 },
        { "/c"            , " @c", 0 },
        { "/ZI"           , " @Z", 0 },
        { "/Zi"           , " @Z", 0 },
        { "/Z7"           , " @Z", 0 },
        { "/O[0-9A-Za-z]*", " @O", 0 },
        { "/D +\"{0,1}DEBUG=*[0-9]*\"{0,1}", " @D", 0 }
    };
    re.SetRange("^# ADD .*CPP ");
    for (size_t i = 0; i < sizeof(ksOpt)/sizeof(ksOpt[0]); i++) {
        re.ReplaceRange(string(" +") + ksOpt[i].from, ksOpt[i].to,
                        kDF, kDF, CRegexpUtil::eInside, (int)ksOpt[i].count);
    }

    // Configuration-dependent changes: replace hooks and more compiler
    // options where appropriate.

    const SReplacement ksOptDebug[] = {
        { "@c"       , " /GZ /c"         , 0 },
        { "@O"       , " /Od"            , 1 },
        { "@Z"       , " /Z7"            , 1 },
        { "@D"       , " /D \"DEBUG=1\"" , 1 },
        { "(/W[^ ]*)", " $1 /Gm"         , 1 },
        { "@O"       , ""                , 0 },
        { "@Z"       , ""                , 0 },
        { "@D"       , ""                , 0 }
    };
    const SReplacement ksOptRelease[] = {
        { "@c"       , " /c"             , 0 },
        { "@O"       , " /O2"            , 1 },
        { "@O"       , ""                , 0 },
        { "@Z"       , ""                , 0 },
        { "@D"       , ""                , 0 }
    };
  
    const SReplacement* subst;
    size_t              subst_size;

    if (config == eDebug) {
        re.SetRange("^# PROP ");
        re.ReplaceRange("  *Use_Debug_Libraries  *0"," Use_Debug_Libraries 1");
        re.SetRange("^# ADD .*LINK32 ");
        re.ReplaceRange("  */pdb:[^ ]*", kEmptyStr);
        re.ReplaceRange("/mach", "/pdb:none /debug /mach");
        subst = &ksOptDebug[0];
        subst_size = sizeof(ksOptDebug)/sizeof(subst[0]);
    } else {
        re.SetRange("^# PROP ");
        re.ReplaceRange("  *Use_Debug_Libraries  *1"," Use_Debug_Libraries 0");
        re.SetRange("^# ADD .*LINK32 ");
        re.ReplaceRange("  */pdbtype[^ ]*", kEmptyStr);
        subst = &ksOptRelease[0];
        subst_size = sizeof(ksOptRelease)/sizeof(subst[0]);
    }
    re.SetRange("^# ADD .*CPP ");
    for (unsigned int i = 0; i < subst_size; i++) {
        re.ReplaceRange(string(" +") + subst[i].from, subst[i].to, 
                        kDF, kDF, CRegexpUtil::eInside, subst[i].count);
    }
    re.Replace("^(# ADD .*LINK32.*) */debug(.*)", "$1$2", kML);


    // Now replace the code generation switch.

    re.SetRange("^# ADD .*CPP ");
    ITERATE_SUFFIX(i) {
        re.ReplaceRange(string(" +") + kSuffixLib[i] + "[d]{0,1}", " @C");
    }
    string lib_switch = kSuffixLib[suffix];
    if (config == eDebug) {
        lib_switch += "d";
    }

    string ksLib[][2] = {
        { "@C"      , lib_switch              },
        { "/nologo" , "/nologo " + lib_switch },
        { " CPP"    , " CPP" + lib_switch     }
    };
    re.SetRange("^# ADD +CPP ");
    for (size_t i = 0; i < sizeof(ksLib)/sizeof(ksLib[0]); i++) {
        if ( re.ReplaceRange(ksLib[i][0], ksLib[i][1], 
                             kDF, kDF, CRegexpUtil::eInside, 1) ) {
            break;
        }
    }
    re.SetRange("^# ADD +BASE +CPP ");
    for (size_t i = 0; i < sizeof(ksLib)/sizeof(ksLib[0]); i++) {
        if ( re.ReplaceRange(ksLib[i][0], ksLib[i][1],
                             kDF, kDF, CRegexpUtil::eInside, 1) ) {
            break;
        }
    }

    re.Replace("^(# ADD .*CPP +.*) *@C(.*)", "$1$2", kML);
    re.Replace("^(# ADD .*LINK32 +.*) */incremental:(yes|no)(.*)", "$1$3",kML);


    // Make sure that incremental linking is on
    // except for wxWindows DLLs in the release mode (very slow).

    string incr_link;

    if ( !re.Exists("^# ADD .*LINK32 .*/dll", kML) ) {
        incr_link = (config == eDebug) ? "yes" : "no";
    } else if ( !gui.empty()  &&  config == eRelease  &&  wxdll_making ) {
        incr_link = "no";
    }
    if ( !incr_link.empty() ) {
        re.Replace("^(# ADD .*LINK32 .*/nologo)(.*)",
                   string("$1 /incremental:") + incr_link + "$2", kML);
    }


    // When requested, remove include/library paths, which refer to DIZZY.

    if ( m_IsDistribution ) {
        re.SetRange("^# ADD .*(CPP|RSC) ");
        re.ReplaceRange(" +/[Ii] +[\"]{0,1}\\\\\\\\[Dd][Ii][Zz][Zz][Yy]\\\\[^ ]*",
                        kEmptyStr);
        re.SetRange("^# ADD .*LINK32 ");
        re.ReplaceRange(" +/libpath:[\"]{0,1}\\\\\\\\[Dd][Ii][Zz][Zz][Yy]\\\\[^ ]*",
                        kEmptyStr);
    }


    // wxWindows-specific changes from now on.

    if ( gui == "wxwin" ) {
        // # Define __WXDEBUG__ for debug configs.
        string wxdebug; 
        if ( config == eDebug ) {
            wxdebug = "/D \"__WXDEBUG__=1\"";
        }
        // wxWindows defines is to be added to compiler options.
        if ( !wxdebug.empty()  ||  !wxdll.empty() ) {
            string wx = NStr::TruncateSpaces(wxdebug + " " + wxdll);
            string ksWx[][2] = {
                { "/Y" , wx + " /Y" },
                { "/D" , wx + " /D" },
                { "/c" , wx + " /Y" },
                { " *$", "$0 "+ wx  }
            };
            // Insert it once in each line
            re.SetRange("^# ADD +CPP ");
            for (size_t i = 0; i < sizeof(ksWx)/sizeof(ksWx[0]); i++) {
                if ( re.ReplaceRange(ksWx[i][0], ksWx[i][1],
                                     kDF, kDF, CRegexpUtil::eInside, 1) ) {
                    break;
                }
            }
            re.SetRange("^# ADD +BASE +CPP ");
            for (size_t i = 0; i < sizeof(ksWx)/sizeof(ksWx[0]); i++) {
                if ( re.ReplaceRange(ksWx[i][0], ksWx[i][1],
                                     kDF, kDF, CRegexpUtil::eInside, 1) ) {
                    break;
                }
            }
        }
        // Enforce /subsystem:windows.
        re.SetRange("^# ADD .*LINK32 ");
        re.ReplaceRange("/subsystem:[A-Za-z]*", "@s");

        string ksWxss[][2] = {
            { "@s"     , ""         },
            { "/nologo", "/nologo " },
            { " *$"    , "$0 "      }
        };
        re.SetRange("^# ADD +LINK32 ");
        for (size_t i = 0; i < sizeof(ksWxss)/sizeof(ksWxss[0]); i++) {
            if ( re.ReplaceRange(ksWxss[i][0],
                                 ksWxss[i][1] + "/subsystem:windows",
                                 kDF, kDF, CRegexpUtil::eInside, 1) ) {
                break;
            }
        }
        re.SetRange("^# ADD .*LINK32 ");
        re.ReplaceRange(" *@s", kEmptyStr);

        // Take care of libraries: remove all wxWindows ones.
        string ksWxLib[][2] = {
            { "jpegd{0,1}[.]lib" , ""     },
            { "pngd{0,1}[.]lib"  , ""     },
            { "tiffd{0,1}[.]lib" , ""     },
            { "zpmd{0,1}[.]lib"  , ""     },
            { "zlibd{0,1}[.]lib" , ""     },
            { "wx[dl]{0,1}[.]lib", " @wx" }
        };
        re.SetRange("^# ADD .*LINK32 .*wx[dl]{0,1}[.]lib");
        for (size_t i = 0; i < sizeof(ksWxLib)/sizeof(ksWxLib[0]); i++) {
            re.ReplaceRange("  *" + ksWxLib[i][0], ksWxLib[i][1]);
        }

        // Insert them back but with correct names (which we use).
        // Note that in DLL mode only one (import) library has to be included.
        // The note above was true formely; now images libs are all static.

        re.SetRange("^# ADD .*LINK32 .*@wx ");
        re.ReplaceRange("@wx", "jpeg.lib png.lib tiff.lib zlib.lib wx.lib",
                         kDF, kDF, CRegexpUtil::eInside, 1); 
        re.ReplaceRange("@wx", kEmptyStr);
    }


    // FLTK specific changes from now on

    if ( gui == "fltk" ) {
        // Enforce /subsystem:windows.
        re.SetRange("^# ADD .*LINK32 ");
        re.ReplaceRange("/subsystem:[A-Za-z]*", "@s");

        string ksWxss[][2] = {
            { "@s"     , ""         },
            { "/nologo", "/nologo " },
            { " *$"    , "$0 "      }
        };
        re.SetRange("^# ADD LINK32 ");
        for (size_t i = 0; i < sizeof(ksWxss)/sizeof(ksWxss[0]); i++) {
            if ( re.ReplaceRange(ksWxss[i][0],
                                 ksWxss[i][1] + "/subsystem:windows", 
                                 kDF, kDF, CRegexpUtil::eInside, 1) ) {
                break;
            }
        }
        re.SetRange("^# ADD .*LINK32 ");
        re.ReplaceRange(" *@s", kEmptyStr);

        // Take care of libraries: remove all FLTK ones
        re.SetRange("^# ADD .*LINK32 * fltk[a-z]*[.]lib");
        re.ReplaceRange("  *fltk[a-z]*[.]lib", " @fltk");

        // Insert them back but with correct names (which we use).
        // Note that in DLL mode only one (import) library has to be included.

        string lib = "fltkdll.lib";
        if ( suffix != eDLL ) {
            lib = "fltkforms.lib fltkimages.lib fltkgl.lib " + lib;
        }
        re.SetRange("^# ADD .*LINK32 .*@fltk");
        re.ReplaceRange("@fltk", "fltkdll.lib", kDF, kDF,
                        CRegexpUtil::eInside, 1); 
        re.ReplaceRange("@fltk", kEmptyStr);
    }

    // Get result;
    cfg_str = re;


    // Return name of configuration, which is created.
   
    if ( !gui.empty() ) {
        cfg += " (" + gui + ")";
    }
    return cfg;

}

 
int CMainApplication::Run(void)
{
    // Get and check arguments
    ParseArguments();

    // Run conversion
    CDirEntry entry(m_Path);
    if ( entry.IsFile() ) {
        ProcessFile(m_Path);
    }
    else if ( entry.IsDir() ) {
        LOG_POST("Begin converting directory \"" << m_Path << "\".");
        ProcessDir(m_Path);
    }
    else {
        ERR_POST(Fatal << "Path \"" + m_Path + "\" must exist.");
    }
    LOG_POST("Finished.");
    return 0;
}

  
/////////////////////////////////////////////////////////////////////////////
//
//  Main
//

static CMainApplication theApp;

int main(int argc, const char* argv[])
{
    // Execute main application function
    return theApp.AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.14  2005/04/12 11:40:50  ivanov
 * Restore only file modification time
 *
 * Revision 1.13  2004/11/22 17:20:50  ivanov
 * Use fCompile_* and fMatch_* flags.
 *
 * Revision 1.12  2004/11/22 16:49:21  ivanov
 * Use CRegexp:: eCompile_* and eMatch_* flags instead of PCRE_*.
 *
 * Revision 1.11  2004/10/01 16:17:04  ivanov
 * ProcessFile():: Fixed buffer overrun
 *
 * Revision 1.10  2004/05/17 21:08:36  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.9  2003/12/02 18:01:19  ivanov
 * Fixed message for the previous commit
 *
 * Revision 1.8  2003/12/02 17:59:13  ivanov
 * Remove ,bak file only if new one is created
 *
 * Revision 1.7  2003/11/28 16:53:23  ivanov
 * Keep modification time for conveted file
 *
 * Revision 1.6  2003/11/10 17:29:05  ivanov
 * Added option "-b" -- create backup files for projects (by default is off).
 * wxWindows: make sure that incremental linking is on except for DLLs
 * in the release mode (very slow).
 *
 * Revision 1.5  2003/11/10 14:59:51  ivanov
 * Fixed array size determination after previous fix.
 * Use caseless check for include/library paths, which refer to DIZZY.
 *
 * Revision 1.4  2003/11/07 17:14:56  ivanov
 * Use array of SReplacement instead of two-dim arrays
 *
 * Revision 1.3  2003/11/07 13:42:27  ivanov
 * Fixed lines wrapped at 79th columns. Get rid of compilation warnings on UNIX.
 *
 * Revision 1.2  2003/11/06 17:08:32  ivanov
 * Remove ".bak" file before renaming
 *
 * Revision 1.1  2003/11/06 16:17:58  ivanov
 * Initial revision. Based on one2all.sh script.
 *
 * ===========================================================================
 */

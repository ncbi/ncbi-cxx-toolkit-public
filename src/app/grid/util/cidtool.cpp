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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description: Entry point and command line parsing
 *                   of the cidtool application.
 *
 */

#include <ncbi_pch.hpp>

#include "cidtool.hpp"

#include <connect/services/clparser.hpp>
#include <connect/services/grid_app_version_info.hpp>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbi_config.hpp>


USING_NCBI_SCOPE;

CComponentIDToolApp::CComponentIDToolApp(int argc, const char* argv[]) :
    m_ArgC(argc),
    m_ArgV(argv)
{
}

#ifdef _DEBUG
#define OPT_DEF(opt_type, opt_id) CCommandLineParser::opt_type, opt_id
#else
#define OPT_DEF(opt_type, opt_id) CCommandLineParser::opt_type
#endif

struct SOptionDefinition {
    CCommandLineParser::EOptionType type;
#ifdef _DEBUG
    int opt_id;
#endif
    const char* name_variants;
    const char* description;
    int required_options[2];
} static const s_OptionDefinitions[eNumberOfOptions] = {

    {OPT_DEF(ePositionalArgument, eCID), "CID", NULL, {-1}},

    {OPT_DEF(eOptionWithParameter, eInputFile),
        INPUT_FILE_OPTION, "Read input from the specified file.", {-1}},

    {OPT_DEF(eOptionWithParameter, eOutputFile),
        "o|" OUTPUT_FILE_OPTION, "Save output to the specified file.", {-1}},
};

struct SCommandDefinition {
    int (CComponentIDToolApp::*cmd_proc)();
    const char* name_variants;
    const char* synopsis;
    const char* usage;
    int options[eNumberOfOptions + 1];
} static const s_CommandDefinitions[] = {

    {&CComponentIDToolApp::Cmd_Dump,
        "dump", "Dump the contents of a CompoundID.",
        "Print the contents of the CompoundID specified on the "
        "command line to the standard output stream (or a file).",
        {eCID, eOutputFile, -1}},

    {&CComponentIDToolApp::Cmd_Make,
        "make", "Generate a CompoundID.",
        "Read CompoundID dump from the standard input stream "
        "(or the specified file) and print the CompoundID that "
        "corresponds it.",
        {eInputFile, -1}},
};

#define TOTAL_NUMBER_OF_COMMANDS int(sizeof(s_CommandDefinitions) / \
    sizeof(*s_CommandDefinitions))

int CComponentIDToolApp::Run()
{
    const SCommandDefinition* cmd_def;
    const int* cmd_opt;

    {
        CCommandLineParser clparser(GRID_APP_NAME, GRID_APP_VERSION_INFO,
            "Generate and decode identifiers in CompoundID format.");

        const SOptionDefinition* opt_def = s_OptionDefinitions;
        int opt_id = 0;
        do {
#ifdef _DEBUG
            _ASSERT(opt_def->opt_id == opt_id &&
                "EOption order must match positions in s_OptionDefinitions.");
#endif
            clparser.AddOption(opt_def->type, opt_id,
                opt_def->name_variants, opt_def->description ?
                    opt_def->description : kEmptyStr);
            ++opt_def;
        } while (++opt_id < eNumberOfOptions);

        cmd_def = s_CommandDefinitions;
        for (int i = 0; i < TOTAL_NUMBER_OF_COMMANDS; ++i, ++cmd_def) {
            clparser.AddCommand(i, cmd_def->name_variants,
                cmd_def->synopsis, cmd_def->usage);
            for (cmd_opt = cmd_def->options; *cmd_opt >= 0; ++cmd_opt)
                clparser.AddAssociation(i, *cmd_opt);
        }

        try {
            int cmd_id = clparser.Parse(m_ArgC, m_ArgV);
            if (cmd_id < 0)
                return 0;

            cmd_def = s_CommandDefinitions + cmd_id;
        }
        catch (exception& e) {
            NcbiCerr << e.what();
            return 1;
        }

        for (cmd_opt = cmd_def->options; *cmd_opt >= 0; ++cmd_opt)
            MarkOptionAsAccepted(*cmd_opt);

        const char* opt_value;

        while (clparser.NextOption(&opt_id, &opt_value)) {
            MarkOptionAsExplicitlySet(opt_id);
            switch (EOption(opt_id)) {
            case eCID:
                m_Opts.cid = opt_value;
                break;
            case eInputFile:
                if ((m_Opts.input_stream = fopen(opt_value, "rb")) == NULL) {
                    fprintf(stderr, "%s: %s\n", opt_value, strerror(errno));
                    return 2;
                }
                break;
            case eOutputFile:
                if ((m_Opts.output_stream = fopen(opt_value, "wb")) == NULL) {
                    fprintf(stderr, "%s: %s\n", opt_value, strerror(errno));
                    return 2;
                }
                break;
            default: // Just to silence the compiler.
                break;
            }
        }

        opt_id = eNumberOfOptions - 1;
        do
            if (IsOptionSet(opt_id))
                for (const int* required_opt =
                        s_OptionDefinitions[opt_id].required_options;
                                *required_opt != -1; ++required_opt)
                    if (!IsOptionSet(*required_opt)) {
                        fprintf(stderr, GRID_APP_NAME
                                ": option '--%s' requires option '--%s'.\n",
                                s_OptionDefinitions[opt_id].name_variants,
                                s_OptionDefinitions[
                                        *required_opt].name_variants);
                        return 2;
                    }
        while (--opt_id >= 0);

        if (IsOptionAcceptedButNotSet(eInputFile)) {
            m_Opts.input_stream = stdin;
        }
        if (IsOptionAcceptedButNotSet(eOutputFile)) {
            m_Opts.output_stream = stdout;
        }
    }

    try {
        return (this->*cmd_def->cmd_proc)();
    }
    catch (CConfigException& e) {
        fprintf(stderr, "%s\n", e.GetMsg().c_str());
        return 2;
    }
    catch (CArgException& e) {
        fprintf(stderr, GRID_APP_NAME " %s: %s\n",
                cmd_def->name_variants, e.GetMsg().c_str());
        return 2;
    }
    catch (CException& e) {
        fprintf(stderr, "%s\n", e.what());
        return 3;
    }
    catch (string& s) {
        fprintf(stderr, "%s\n", s.c_str());
        return 4;
    }
}

CComponentIDToolApp::~CComponentIDToolApp()
{
    if (IsOptionSet(eInputFile) && m_Opts.input_stream != NULL)
        fclose(m_Opts.input_stream);
    if (IsOptionSet(eOutputFile) && m_Opts.output_stream != NULL)
        fclose(m_Opts.output_stream);
}

int CComponentIDToolApp::Cmd_Dump()
{
    CCompoundID cid(m_CompoundIDPool.FromString(m_Opts.cid));

    fprintf(m_Opts.output_stream, "%s\n", cid.Dump().c_str());

    return 0;
}

int CComponentIDToolApp::Cmd_Make()
{
    string dump;

    char buffer[1024];
    size_t bytes_read;

    do {
        bytes_read = fread(buffer, 1, sizeof(buffer), m_Opts.input_stream);
        dump.append(buffer, bytes_read);
    } while (bytes_read == sizeof(buffer));

    CCompoundID cid(m_CompoundIDPool.FromDump(dump));

    puts(cid.ToString().c_str());

    return 0;
}

int main(int argc, const char* argv[])
{
    return CComponentIDToolApp(argc, argv).AppMain(1, argv);
}

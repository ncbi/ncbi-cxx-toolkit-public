#ifndef CONNECT_SERVICES__CLI__HPP
#define CONNECT_SERVICES__CLI__HPP

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
 * Authors:  Dmitry Kazimirov
 *
 */

/// @file cli.hpp
/// Command line handlers.
///


#include "netcomponent.hpp"


BEGIN_NCBI_SCOPE

struct SCommandLineParserImpl;

class NCBI_XCONNECT_EXPORT CCommandLineParser
{
    NCBI_NET_COMPONENT(CommandLineParser);

    CCommandLineParser(
        const string& program_name,
        const string& program_version,
        const string& program_description);

    void SetHelpTextMargins(
        int help_text_width,
        int cmd_descr_indent,
        int opt_descr_indent);

    enum EOptionType {
        eSwitch,
        eOptionWithParameter,
        ePositionalArgument,
        eOptionalPositional,
        eZeroOrMorePositional,
        eOneOrMorePositional
    };

    void AddOption(
        EOptionType type,
        int opt_id,
        const string& name_variants,
        const string& description);

    void AddCommandCategory(
        int cat_id,
        const string& title);

    void AddCommand(
        int cmd_id,
        const string& name_variants,
        const string& synopsis,
        const string& usage,
        int cat_id = -1);

    void AddAssociation(int cmd_id, int opt_id);

    /// @return One of the command identifiers or -1
    ///         if a help command was requested and
    ///         has been already processed.
    int Parse(int argc, const char* const *argv);

    const string& GetProgramName() const;

    bool NextOption(int* opt_id, const char** opt_value);
};

/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE

#endif // CONNECT_SERVICES__CLI__HPP

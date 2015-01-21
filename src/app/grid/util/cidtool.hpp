#ifndef JSONTOOL__HPP
#define JSONTOOL__HPP

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

/// @file cidtool.hpp
/// Declarations of command line interface arguments and handlers.
///

#include <corelib/ncbiapp.hpp>

#include <connect/services/compound_id.hpp>

#define GRID_APP_NAME "cidtool"

#define INPUT_FILE_OPTION "input-file"
#define OUTPUT_FILE_OPTION "output-file"

BEGIN_NCBI_SCOPE

enum EOption {
    eCID,
    eInputFile,
    eOutputFile,
    eNumberOfOptions
};

#define OPTION_ACCEPTED 1
#define OPTION_SET 2
#define OPTION_EXPLICITLY_SET 4
#define OPTION_N(number) (1 << number)

class CComponentIDToolApp : public CNcbiApplication
{
public:
    CComponentIDToolApp(int argc, const char* argv[]);

    virtual int Run();

    virtual ~CComponentIDToolApp();

private:
    int m_ArgC;
    const char** m_ArgV;

    struct SOptions {
        string cid;
        FILE* input_stream;
        FILE* output_stream;

        char option_flags[eNumberOfOptions];

        SOptions() : input_stream(NULL), output_stream(NULL)
        {
            memset(option_flags, 0, sizeof(option_flags));
        }
    } m_Opts;

private:
    void MarkOptionAsAccepted(int option)
    {
        m_Opts.option_flags[option] |= OPTION_ACCEPTED;
    }

    bool IsOptionAccepted(EOption option) const
    {
        return (m_Opts.option_flags[option] & OPTION_ACCEPTED) != 0;
    }

    int IsOptionAccepted(EOption option, int mask) const
    {
        return (m_Opts.option_flags[option] & OPTION_ACCEPTED) ? mask : 0;
    }

    bool IsOptionAcceptedButNotSet(EOption option) const
    {
        return m_Opts.option_flags[option] == OPTION_ACCEPTED;
    }

    bool IsOptionAcceptedAndSetImplicitly(EOption option) const
    {
        return m_Opts.option_flags[option] == (OPTION_ACCEPTED | OPTION_SET);
    }

    void MarkOptionAsSet(int option)
    {
        m_Opts.option_flags[option] |= OPTION_SET;
    }

    bool IsOptionSet(int option) const
    {
        return (m_Opts.option_flags[option] & OPTION_SET) != 0;
    }

    int IsOptionSet(int option, int mask) const
    {
        return (m_Opts.option_flags[option] & OPTION_SET) ? mask : 0;
    }

    void MarkOptionAsExplicitlySet(int option)
    {
        m_Opts.option_flags[option] |= OPTION_SET | OPTION_EXPLICITLY_SET;
    }

    bool IsOptionExplicitlySet(int option) const
    {
        return (m_Opts.option_flags[option] & OPTION_EXPLICITLY_SET) != 0;
    }

    int IsOptionExplicitlySet(int option, int mask) const
    {
        return (m_Opts.option_flags[option] & OPTION_EXPLICITLY_SET) ? mask : 0;
    }

public:
    int Cmd_Dump();
    int Cmd_Make();

private:
    CCompoundIDPool m_CompoundIDPool;
};

END_NCBI_SCOPE

#endif // JSONTOOL__HPP

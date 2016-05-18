#ifndef GPIPE_COMMON___GP_APP__HPP
#define GPIPE_COMMON___GP_APP__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbiapp.hpp>
#include <serial/serial.hpp>

BEGIN_NCBI_SCOPE

class CArgs;
class CArgDescriptions;

BEGIN_SCOPE(objects)
    class CObjectManager;
END_SCOPE(objects)

enum class EBioseqSourceType {
    eSeqIdList, //A list of seq_id
    eFasta,     //Fasta file
    eCSra,      //cSra file
    eAsnBinary,
    eAsnText
};

class CGPipeAppUtil
{
public:

    /// Start and stop logging requests
    static void DiagRequestStart();
    static void DiagRequestStop(int status);

    /// Provide standard logging of arguments
    static void PrintArgs(const CArgs& args,
                          CDiagContext_Extra& extra);

    /// Add a standard set of arguments used to configure the object manager
    static void AddArgumentDescriptions(CArgDescriptions& arg_desc);

    /// Set up the standard object manager data loaders according to the
    /// arguments provided above
    static void SetupObjectManager(const CArgs& args,
                                   objects::CObjectManager& obj_mgr,
                                   bool overrideIdServerOnReplicationSuspended);
    static void SetupObjectManager(const CArgs& args,
                                   objects::CObjectManager& obj_mgr); // uses -override-idload command-line flag
    /// VDB is after ID
    static void SetupObjectManager2(const CArgs& args,
                                   objects::CObjectManager& obj_mgr,
                                   bool overrideIdServerOnReplicationSuspended = false);

    //initialize object manager using SetupObjectManager, and create CScope with default loaders
    static CRef<objects::CScope> GetDefaultScope(const CArgs& args);

    /// Add an argument "-it" that indicates text input format
    static void AddSerialInputFormatArgumentDescription(CArgDescriptions& a_arg_desc);

    /// Parse the command line arguments to establish serial input format
    static ESerialDataFormat GetSerialInputFormat(const CArgs& args);

    /// Add an argument "-ot" that indicates text output format
    static void AddSerialOutputFormatArgumentDescription(CArgDescriptions& a_arg_desc);

    /// Parse the command line arguments to establish serial output format
    static ESerialDataFormat GetSerialOutputFormat(const CArgs& args);


    /// Add a standard set of arguments used to configure serial formats of
    /// input and output streams
    static void AddSerialFormatArgumentDescriptions(CArgDescriptions& a_arg_desc);

    /// Parse the command line arguments to establish serial formats of
    /// input and output streams
    static void GetSerialFormat(const CArgs& a_args,
                                ESerialDataFormat& a_input_format,
                                ESerialDataFormat& a_output_format);

    /// Add a standard set of arguments used to configure input sequence format for IBioseqSource
    static void AddIBioseqSourceFormatArgumentDescriptions(CArgDescriptions& a_arg_desc, const string& arg_name = "ifmt");

    /// Parse the command line arguments to establish IBioseqSource input format
    static EBioseqSourceType GetIBioseqSourceInputFormat(const CArgs& a_args, const string& arg_name = "ifmt");


    static bool IsIdReplSuspended();
};


class CDiagRequestGuard
{
public:
    CDiagRequestGuard()
        : m_Status(500)
    {
        CGPipeAppUtil::DiagRequestStart();
    }

    ~CDiagRequestGuard()
    {
        CGPipeAppUtil::DiagRequestStop(m_Status);
    }

    void SetStatus(int status)
    {
        m_Status = status;
    }

private:
    int m_Status;
};


END_NCBI_SCOPE

#endif  // GPIPE_COMMON___GP_APP__HPP

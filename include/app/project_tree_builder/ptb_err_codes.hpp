#ifndef APP_PROJECT_TREE_BUILDER___PTB_ERROR_CODES___HPP
#define APP_PROJECT_TREE_BUILDER___PTB_ERROR_CODES___HPP

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

BEGIN_NCBI_SCOPE


enum EProjectTreeBuilderErrCode
{
    ePTB_NoError = -1,
    ePTB_Unknown = 0,

    ePTB_ConfigurationError,
    ePTB_FileExcluded,
    ePTB_FileModified,
    ePTB_FileNotFound,
    ePTB_InvalidMakefile,
    ePTB_MacroInvalid,
    ePTB_MacroUndefined,
    ePTB_MissingDependency,
    ePTB_MissingMakefile,
    ePTB_PathNotFound,
    ePTB_ProjectExcluded,
    ePTB_ProjectNotFound,
};


/////////////////////////////////////////////////////////////////////////////
///
/// MDiagFile --
///
/// Manipulator to set File for CNcbiDiag

class MDiagFile 
{
public:
    MDiagFile(const string& file)
        : m_File(file)
    {
    }

    friend const CNcbiDiag& operator<< (const CNcbiDiag& diag,
                                        const MDiagFile& file)
    {
        //return diag;
        return diag.SetFile(file.m_File.c_str()).SetLine(0);
    }

private:
    string m_File;
};





#define PTB_ERROR(file, msg) \
    LOG_POST(Error << MDiagFile(file) << ": " << msg)

#define PTB_ERROR_EX(file, err_code, msg) \
    LOG_POST(Error << MDiagFile(file) << ErrCode(err_code) << msg)

#define PTB_WARNING(file, msg) \
    LOG_POST(Warning << MDiagFile(file) << ": " << msg)

#define PTB_WARNING_EX(file, err_code, msg) \
    LOG_POST(Warning << MDiagFile(file) << ErrCode(err_code) << msg)

#define PTB_INFO(msg) \
    LOG_POST(Info << MDiagFile(kEmptyStr) << ErrCode(ePTB_NoError) << msg)

#define PTB_INFO_EX(file, err_code, msg) \
    LOG_POST(Info << MDiagFile(file) << ErrCode(err_code) << msg)

#define PTB_TRACE(msg) \
    _TRACE(ErrCode(ePTB_NoError) << msg)

#define PTB_TRACE_EX(file, err_code, msg) \
    _TRACE(MDiagFile(file) << ErrCode(err_code) << msg)



END_NCBI_SCOPE

#endif  // APP_PROJECT_TREE_BUILDER___PTB_ERROR_CODES___HPP

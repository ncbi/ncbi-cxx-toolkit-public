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
 * Author: Christiam Camacho
 *
 */

/** @file mask_cmdline_args.hpp
 * Contains the command line options common to filtering algorithms.
 * Definitions can be found in mask_writer.cpp
 */

#ifndef __MASK_CMDLINE_ARGS__HPP__
#define __MASK_CMDLINE_ARGS__HPP__

#include <ncbiconf.h>
#include <corelib/ncbistd.hpp>
#include <corelib/mswin_export.h>
#include <string>

BEGIN_NCBI_SCOPE

/// Command line flag to specify the input
NCBI_XOBJREAD_EXPORT extern const std::string kInput;
/// Command line flag to specify the input format
NCBI_XOBJREAD_EXPORT extern const std::string kInputFormat;
/// Command line flag to specify the output
NCBI_XOBJREAD_EXPORT extern const std::string kOutput;
/// Command line flag to specify the output format
NCBI_XOBJREAD_EXPORT extern const std::string kOutputFormat;

/// Number of elements in kInputFormats
NCBI_XOBJREAD_EXPORT extern const size_t kNumInputFormats;
/// Number of elements in kOutputFormats
NCBI_XOBJREAD_EXPORT extern const size_t kNumOutputFormats;
/// Input formats allowed, the first one is the default
NCBI_XOBJREAD_EXPORT extern const char* kInputFormats[];
/// Output formats allowed, the first one is the default
NCBI_XOBJREAD_EXPORT extern const char* kOutputFormats[];

END_NCBI_SCOPE

#endif /* __MASK_CMDLINE_ARGS__HPP__ */


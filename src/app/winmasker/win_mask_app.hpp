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
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   Header file for CWinMaskApplication class.
 *
 */

#ifndef C_WIN_MASK_APPLICATION_H
#define C_WIN_MASK_APPLICATION_H

#include <corelib/ncbiapp.hpp>

BEGIN_NCBI_SCOPE

/** 
 **\brief Window based masker main class.
 **
 **/
class CWinMaskApplication : public CNcbiApplication
{
public:

    /// Application constructor
    CWinMaskApplication();

    /** 
     **\brief Short description of the program.
     **
     **/
    static const char * const USAGE_LINE;

    /** 
     **\brief Initialization. 
     **
     ** Setting up descriptions of command line parameters.
     **
     **/
    virtual void Init(void);

    /**
     **\brief Main routine of the window based masker.
     **
     ** @return the exit status
     **/
    virtual int Run (void);
};

END_NCBI_SCOPE

#endif

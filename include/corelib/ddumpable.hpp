#ifndef DDUMPABLE__HPP
#define DDUMPABLE__HPP

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
 *  Please say you saw it at NCBI
 *
 * ===========================================================================
 *
 * Author:  Andrei Gourianov
 *
 * File Description:
 *   Abstract base class: defines DebugDump() functionality
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ddump_context.hpp>

BEGIN_NCBI_SCOPE

class CDebugDumpable
{
public:
    CDebugDumpable(void) {}
    virtual ~CDebugDumpable(void);

    // Enable/disable debug dump
    static void EnableDebugDump(bool on);

    // Dump using text formatter
    void DebugDumpText(ostream& out,
                       const string& bundle, unsigned int depth) const;
    // Dump using external dump formatter
    void DebugDumpFormat(CDebugDumpFormatter& ddf, 
                   const string& bundle, unsigned int depth) const;

    // Function that does the dump - to be overloaded
    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const = 0;

private:
    static bool sm_DumpEnabled;
};

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2002/05/17 14:25:39  gouriano
 * added DebugDump base class and function to CObject
 *
 *
 *
 * ===========================================================================
 */

#endif // DDUMPABLE__HPP

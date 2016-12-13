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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write bed file
 *
 */

#ifndef OBJTOOLS_WRITERS___BED_GRAPH_RECORD__HPP
#define OBJTOOLS_WRITERS___BED_GRAPH_RECORD__HPP

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
/// Encapsulation of the BED feature record. That's columnar data with at least
/// three and at most twelve columns. Each column has a fixed, well defined 
/// meaning, and all records of the same track must have the same number of
/// columns.
///
class CBedGraphRecord
//  ============================================================================
{
public:
    CBedGraphRecord();

    ~CBedGraphRecord();

    bool Write(CNcbiOstream&);

    void SetChromId(const string&);
    void SetChromStart(size_t);
    void SetChromEnd(size_t);
    void SetChromValue(float);

protected:
    string m_chromId;
    string m_chromStart;
    string m_chromEnd; 
    string m_chromValue;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___BED_GRAPH_RECORD__HPP

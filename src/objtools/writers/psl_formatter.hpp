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
 * Authors: Frank Ludwig
 *
 * File Description:  Write alignment file
 *
 */

#ifndef OBJTOOLS_WRITERS_PSL_FORMATTER_HPP
#define OBJTOOLS_WRITERS_PSL_FORMATTER_HPP

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CPslRecord;

//  ----------------------------------------------------------------------------
class CPslFormatter
//  ----------------------------------------------------------------------------
{
public:
    CPslFormatter(
        CNcbiOstream& ostr,
        bool debugMode);

    ~CPslFormatter() = default;

    void
    Format(
        const CPslRecord& record);

protected:
    string xFieldMatches(
        const CPslRecord&) const;
    string xFieldMisMatches(
        const CPslRecord&) const;
    string xFieldRepMatches(
        const CPslRecord&) const;
    string xFieldCountN(
        const CPslRecord&) const;
    string xFieldNumInsertQ(
        const CPslRecord&) const;
    string xFieldBaseInsertQ(
        const CPslRecord&) const;
    string xFieldNumInsertT(
        const CPslRecord&) const;
    string xFieldBaseInsertT(
        const CPslRecord&) const;
    string xFieldStrand(
        const CPslRecord&) const;
    string xFieldNameQ(
        const CPslRecord&) const;
    string xFieldSizeQ(
        const CPslRecord&) const;
    string xFieldStartQ(
        const CPslRecord&) const;
    string xFieldEndQ(
        const CPslRecord&) const;
    string xFieldNameT(
        const CPslRecord&) const;
    string xFieldSizeT(
        const CPslRecord&) const;
    string xFieldStartT(
        const CPslRecord&) const;
    string xFieldEndT(
        const CPslRecord&) const;
    string xFieldBlockCount(
        const CPslRecord&) const;
    string xFieldBlockSizes(
        const CPslRecord&) const;
    string xFieldStartsQ(
        const CPslRecord&) const;
    string xFieldStartsT(
        const CPslRecord&) const;


    CNcbiOstream& mOstr;
    bool mDebugMode;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS_PSL_FORMATTER_HPP

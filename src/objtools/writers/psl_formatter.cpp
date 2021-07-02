
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
 * File Description:  Write alignment
 *
 */

#include <ncbi_pch.hpp>

#include "psl_record.hpp"
#include "psl_formatter.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
static string
sFormatInt(
    int value,
    int dflt)
//  ----------------------------------------------------------------------------
{
    if (value == dflt) {
        return ".";
    }
    return NStr::IntToString(value);
}

//  ----------------------------------------------------------------------------
CPslFormatter::CPslFormatter(
    CNcbiOstream& ostr):
//  ----------------------------------------------------------------------------
    mOstr(ostr)
{
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldMatches(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    return sFormatInt(record.GetMatches(), -1);
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldMisMatches(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    return "\t" + sFormatInt(record.GetMisMatches(), -1);
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldRepMatches(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    return "\t" + sFormatInt(record.GetRepMatches(), -1);
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldCountN(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    return "\t" + sFormatInt(record.GetCountN(), -1);
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldNumInsertQ(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    return "\t" + sFormatInt(record.GetNumInsertQ(), -1);
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldBaseInsertQ(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    return "\t" + sFormatInt(record.GetBaseInsertQ(), -1);
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldNumInsertT(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    return "\t" + sFormatInt(record.GetNumInsertT(), -1);
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldBaseInsertT(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    return "\t" + sFormatInt(record.GetBaseInsertT(), -1);
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldStrand(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    string rawString = ".";
    if (record.GetStrandT() != eNa_strand_unknown) {
        rawString = (record.GetStrandT() == eNa_strand_minus ? "-" : "+");
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldNameQ(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = record.GetNameQ();
    if (rawString.empty()) {
        rawString = ".";
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldSizeQ(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    return "\t" + sFormatInt(record.GetSizeQ(), -1);
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldStartQ(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    return "\t" + sFormatInt(record.GetStartQ(), -1);
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldEndQ(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    return "\t" + sFormatInt(record.GetEndQ(), -1);
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldNameT(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = record.GetNameT();
    if (rawString.empty()) {
        rawString = ".";
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldSizeT(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    return "\t" + sFormatInt(record.GetSizeT(), -1);
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldStartT(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    return "\t" + sFormatInt(record.GetStartT(), -1);
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldEndT(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    return "\t" + sFormatInt(record.GetEndT(), -1);
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldBlockCount(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    return "\t" + sFormatInt(record.GetBlockCount(), -1);
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldBlockSizes(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto blockSizes = record.GetBlockSizes();
    return "\t" + NStr::JoinNumeric(blockSizes.begin(), blockSizes.end(), ",");
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldStartsQ(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto blockStartsQ = record.GetBlockStartsQ();
    return "\t" + NStr::JoinNumeric(blockStartsQ.begin(), blockStartsQ.end(), ",");
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldStartsT(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto blockStartsT = record.GetBlockStartsT();
    return "\t" + NStr::JoinNumeric(blockStartsT.begin(), blockStartsT.end(), ",");
}

//  ----------------------------------------------------------------------------
void
CPslFormatter::Format(
    const CPslRecord& record)
//  ----------------------------------------------------------------------------
{
    mOstr << xFieldMatches(record)
         << xFieldMisMatches(record)
         << xFieldRepMatches(record)
         << xFieldCountN(record)
         << xFieldNumInsertQ(record)
         << xFieldBaseInsertQ(record)
         << xFieldNumInsertT(record)
         << xFieldBaseInsertT(record)
         << xFieldStrand(record)
         << xFieldNameQ(record)
         << xFieldSizeQ(record)
         << xFieldStartQ(record)
         << xFieldEndQ(record)
         << xFieldNameT(record)
         << xFieldSizeT(record)
         << xFieldStartT(record)
         << xFieldEndT(record)
         << xFieldBlockCount(record)
         << xFieldBlockSizes(record)
         << xFieldStartsQ(record)
         << xFieldStartsT(record)
         << endl;
}

END_NCBI_SCOPE


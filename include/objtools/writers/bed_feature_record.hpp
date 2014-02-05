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

#ifndef OBJTOOLS_WRITERS___BED_FEATURE_RECORD__HPP
#define OBJTOOLS_WRITERS___BED_FEATURE_RECORD__HPP

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
/// Encapsulation of the BED feature record. That's columnar data with at least
/// three and at most twelve columns. Each column has a fixed, well defined 
/// meaning, and all records of the same track must have the same number of
/// columns.
///
class CBedFeatureRecord
//  ============================================================================
{
public:
    CBedFeatureRecord();

    ~CBedFeatureRecord();

    bool AssignLocation(const CSeq_interval&);
    bool AssignDisplayData(const CMappedFeat&, bool);
    bool Write(CNcbiOstream&, unsigned int);

    const string& Chrom() const { return m_strChrom; };
    const string& ChromStart() const { return m_strChromStart; };
    const string& ChromEnd() const { return m_strChromEnd; };
    const string& Name() const { return m_strName; };
    const string& Score() const { return m_strScore; }; 
    const string& Strand() const { return m_strStrand; };   
    const string& ThickStart() const { return m_strThickStart; };   
    const string& ThickEnd() const { return m_strThickEnd; };   
    const string& ItemRgb() const { return m_strItemRgb; };   
    const string& BlockCount() const { return m_strBlockCount; };   
    const string& BlockSizes() const { return m_strBlockSizes; };   
    const string& BlockStarts() const { return m_strBlockStarts; };
   
    size_t ColumnCount() const { return m_uColumnCount; };

protected:
    size_t m_uColumnCount;
    string m_strChrom;
    string m_strChromStart;
    string m_strChromEnd; 
    string m_strName;
    string m_strScore;
    string m_strStrand;
    string m_strThickStart;
    string m_strThickEnd;
    string m_strItemRgb;
    string m_strBlockCount;
    string m_strBlockSizes;
    string m_strBlockStarts;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___BED_FEATURE_RECORD__HPP

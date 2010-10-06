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

//  ----------------------------------------------------------------------------
class CBedFeatureRecord
//  ----------------------------------------------------------------------------
{
public:
    CBedFeatureRecord();

    ~CBedFeatureRecord();

    bool AssignLocation(
        const CSeq_interval& );

    bool AssignDisplayData(
        CMappedFeat,
        bool );

    string Chrom() const;

    string ChromStart() const;

    string ChromEnd() const;

    string Name() const;
        
    string Score() const; 
    
    string Strand() const;   

    string ThickStart() const;   

    string ThickEnd() const;   

    string ItemRgb() const;   

    string BlockCount() const;   

    string BlockSizes() const;   

    string BlockStarts() const;   

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

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
 * Author: Frank Ludwig
 *
 * File Description:
 *   WIGGLE file reader
 *
 */

#ifndef OBJTOOLS_READERS___WIGGLEREADER__HPP
#define OBJTOOLS_READERS___WIGGLEREADER__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_annot.hpp>


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


//  ----------------------------------------------------------------------------
enum CWiggleLineType
//  ----------------------------------------------------------------------------
{
    TYPE_NONE,
    TYPE_COMMENT,
    TYPE_TRACK,
    TYPE_DECLARATION_VARSTEP,
    TYPE_DECLARATION_FIXEDSTEP,
    TYPE_DATA_BED,
    TYPE_DATA_VARSTEP,
    TYPE_DATA_FIXEDSTEP
};


//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CWiggleGraphData
//  ----------------------------------------------------------------------------
{
public:
    CWiggleGraphData();

    void Reset();

    bool ParseDeclarationVarstep(
        const std::vector<std::string>& );

    bool ParseDeclarationFixedstep(
        const std::vector<std::string>& );

    bool ParseDataBed(
        const std::vector<std::string>& );

    bool ParseDataVarstep(
        const std::vector<std::string>& );

    bool ParseDataFixedstep(
        const std::vector<std::string>& );

    unsigned int Span() const { return m_span; };
    double Value() const { return m_value; };
    unsigned int Start() const { return m_start; };
    const std::string& Chrom() const { return m_chrom; };

protected:
    unsigned int m_type;
    std::string m_chrom;
    unsigned int m_start;
    unsigned int m_step;
    unsigned int m_span;
    double m_value;
};

//  ----------------------------------------------------------------------------
class CWiggleTrackData
//  ----------------------------------------------------------------------------
{
public:
    CWiggleTrackData();
    ~CWiggleTrackData();

    bool ParseData(
        const std::vector<std::string>& );

    void Reset();

protected:
    std::string m_type;
    std::string m_name;
    std::string m_description;
};

//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CWiggleReader
//  ----------------------------------------------------------------------------
    : public CReaderBase
{
public:
    //
    //  object management:
    //
public:
    CWiggleReader( 
        int =fDefaults );
    virtual ~CWiggleReader();
    
    //
    //  object interface:
    //
public:
    enum EWiggleFlags {
        fDefaults = 0
    };
    typedef int TFlags;

    virtual void Read( 
        CNcbiIstream&, 
        CRef<CSeq_annot>& );

    //
    //  class interface:
    //
    static bool VerifyFormat(
        CNcbiIstream& );

    static void Tokenize(
        const string&,
        const string&,
        vector< string >& );

    //
    //  helpers:
    //
protected:
    bool x_ReadTrackData(
        CNcbiIstream&,
        string& );

    bool x_ReadGraphData(
        CNcbiIstream&,
        string& );

    bool x_IsCommentLine(
        const string& );

    bool x_ReadLine(
        CNcbiIstream&,
        string& );

    unsigned int x_GetLineType(
        const string& );

    void x_AddGraph(
         CSeq_annot::TData::TGraph& );

    //
    //  data:
    //
protected:
    CWiggleTrackData m_trackdata;
    CWiggleGraphData m_graphdata;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___WIGGLEREADER__HPP

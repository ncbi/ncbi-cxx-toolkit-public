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
 *   VCF file reader
 *
 */

#ifndef OBJTOOLS_READERS___VCFREADER__HPP
#define OBJTOOLS_READERS___VCFREADER__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/error_container.hpp>
#include <objects/seq/Seq_annot.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects) // namespace ncbi::objects::

class CVcfData;

//  ----------------------------------------------------------------------------
enum ESpecType
//  ----------------------------------------------------------------------------
{
    eType_Integer,
    eType_Float,
    eType_Flag,
    eType_Character,
    eType_String
};
ESpecType SpecType( const string& );

//  ----------------------------------------------------------------------------
enum ESpecNumber
//  ----------------------------------------------------------------------------
{
    eNumber_CountAlleles = -1,
    eNumber_CountGenotypes = -2,
    eNumber_CountUnknown = -3
};
ESpecNumber SpecNumber( const string& );

//  ----------------------------------------------------------------------------
class CVcfInfoSpec
//  ----------------------------------------------------------------------------
{
public:
    CVcfInfoSpec(){};

    CVcfInfoSpec(
        string id,
        string numvals,
        string type,
        string description ) :
        m_id( id ),
        m_numvals( SpecNumber( numvals ) ),
        m_type( SpecType( type ) ),
        m_description( description )
    {};

    string m_id;
    int m_numvals;
    ESpecType m_type;
    string m_description;
};

//  ----------------------------------------------------------------------------
class CVcfFilterSpec
//  ----------------------------------------------------------------------------
{
public:
    CVcfFilterSpec(){};

    CVcfFilterSpec(
        string id,
        string description ) :
        m_id( id ),
        m_description( description )
    {};

    string m_id;
    string m_description;
};

//  ----------------------------------------------------------------------------
class CVcfFormatSpec
//  ----------------------------------------------------------------------------
{
public:
    CVcfFormatSpec(){};

    CVcfFormatSpec(
        string id,
        string numvals,
        string type,
        string description ) :
        m_id( id ),
        m_numvals( SpecNumber( numvals ) ),
        m_type( SpecType( type ) ),
        m_description( description )
    {};
        
    string m_id;
    int m_numvals;
    ESpecType m_type;
    string m_description;
};


//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CVcfReader
//  ----------------------------------------------------------------------------
    : public CReaderBase
{
    //
    //  object management:
    //
public:
    CVcfReader( 
        int =0 );
    virtual ~CVcfReader();
    
    //
    //  object interface:
    //
public:
    virtual CRef< CSerialObject >
    ReadObject(
        ILineReader&,
        IErrorContainer* =0 );
                
    virtual CRef< CSeq_annot >
    ReadSeqAnnot(
        ILineReader&,
        IErrorContainer* =0 );

    virtual void
    ReadSeqAnnots(
        vector< CRef<CSeq_annot> >&,
        CNcbiIstream&,
        IErrorContainer* =0 );
                        
    virtual void
    ReadSeqAnnots(
        vector< CRef<CSeq_annot> >&,
        ILineReader&,
        IErrorContainer* =0 );
                        
    //
    //  helpers:
    //
protected:
    virtual bool
    x_ProcessMetaLine(
        const string&,
        CRef<CSeq_annot> );

    virtual bool
    x_ProcessMetaLineInfo(
        const string&,
        CRef<CSeq_annot> );

    virtual bool
    x_ProcessMetaLineFilter(
        const string&,
        CRef<CSeq_annot> );

    virtual bool
    x_ProcessMetaLineFormat(
        const string&,
        CRef<CSeq_annot> );

    virtual bool
    x_ProcessHeaderLine(
        const string&,
        CRef<CSeq_annot> );

    virtual bool
    x_ProcessDataLine(
        const string&,
        CRef<CSeq_annot> );
        
    virtual bool
    x_AssignFeatureLocation(
        const CVcfData&,
        CRef<CSeq_feat> );

    virtual bool
    x_AssignVariationIds(
        const CVcfData&,
        CRef<CSeq_feat> );

    virtual bool
    x_AssignVariationAlleles(
        const CVcfData&,
        CRef<CSeq_feat> );

    virtual bool
    x_ProcessScore(
        const CVcfData&,
        CRef<CSeq_feat> );

    virtual bool
    x_ProcessFilter(
        const CVcfData&,
        CRef<CSeq_feat> );

    virtual bool
    x_ProcessInfo(
        const CVcfData&,
        CRef<CSeq_feat> );

    virtual bool
    x_ProcessFormat(
        const CVcfData&,
        CRef<CSeq_feat> );

    virtual bool
    x_ParseData(
        const string&,
        CVcfData& );

    //
    //  data:
    //
protected:
    CRef< CAnnotdesc > m_Meta;
    map<string,CVcfInfoSpec> m_InfoSpecs;
    map<string,CVcfFormatSpec> m_FormatSpecs;
    map<string,CVcfFilterSpec> m_FilterSpecs;
    vector<string> m_MetaDirectives;
    vector<string> m_GenotypeHeaders;
    CErrorContainerLenient m_ErrorsPrivate;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___VCFREADER__HPP

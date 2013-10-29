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
#include <objtools/readers/message_listener.hpp>
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
    enum {
        fNormal = 0,
        fUseSetFormat = 1<<8,
    };

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
        IMessageListener* =0 );
                
    virtual CRef< CSeq_annot >
    ReadSeqAnnot(
        ILineReader&,
        IMessageListener* =0 );

    virtual void
    ReadSeqAnnots(
        vector< CRef<CSeq_annot> >&,
        CNcbiIstream&,
        IMessageListener* =0 );
                        
    virtual void
    ReadSeqAnnots(
        vector< CRef<CSeq_annot> >&,
        ILineReader&,
        IMessageListener* =0 );
                        
    //
    //  helpers:
    //
protected:
    virtual bool
    xProcessMetaLine(
        const string&,
        CRef<CSeq_annot>,
        IMessageListener*);

    virtual bool
    xProcessMetaLineInfo(
        const string&,
        CRef<CSeq_annot>,
        IMessageListener*);

    virtual bool
    xProcessMetaLineFilter(
        const string&,
        CRef<CSeq_annot>,
        IMessageListener*);

    virtual bool
    xProcessMetaLineFormat(
        const string&,
        CRef<CSeq_annot>,
        IMessageListener*);

    virtual bool
    xProcessHeaderLine(
        const string&,
        CRef<CSeq_annot> );

    virtual bool
    xProcessDataLine(
        const string&,
        CRef<CSeq_annot>,
        IMessageListener*);
        
    virtual bool
    xAssignVariationAlleleSet(
        const CVcfData&,
        CRef<CSeq_feat> );

    virtual bool
    xAssignFeatureLocationSet(
        const CVcfData&,
        CRef<CSeq_feat> );

    virtual bool
    xAssignVariationIds(
        CVcfData&,
        CRef<CSeq_feat> );

    virtual bool
    xAssignVariantSnv(
        const CVcfData&,
        unsigned int,
        CRef<CSeq_feat> );

    virtual bool
    xAssignVariantMnv(
        const CVcfData&,
        unsigned int,
        CRef<CSeq_feat> );

    virtual bool
    xAssignVariantDel(
        const CVcfData&,
        unsigned int,
        CRef<CSeq_feat> );

    virtual bool
    xAssignVariantIns(
        const CVcfData&,
        unsigned int,
        CRef<CSeq_feat> );

    virtual bool
    xAssignVariantDelins(
        const CVcfData&,
        unsigned int,
        CRef<CSeq_feat> );

    virtual bool
    xAssignVariantProps(
        CVcfData&,
        CRef<CSeq_feat>,
        IMessageListener*);

    virtual bool
    xProcessScore(
        CVcfData&,
        CRef<CSeq_feat> );

    virtual bool
    xProcessFilter(
        CVcfData&,
        CRef<CSeq_feat> );

    virtual bool
    xProcessInfo(
        CVcfData&,
        CRef<CSeq_feat>,
        IMessageListener*);

    virtual bool
    xProcessFormat(
        CVcfData&,
        CRef<CSeq_feat> );

    virtual bool
    xParseData(
        const string&,
        CVcfData& );

    virtual bool
    xNormalizeData(
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
    CMessageListenerLenient m_ErrorsPrivate;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___VCFREADER__HPP

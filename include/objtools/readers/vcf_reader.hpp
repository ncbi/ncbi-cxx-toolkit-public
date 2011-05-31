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

    //
    //  data:
    //
protected:
    CRef< CAnnotdesc > m_Meta;
    CErrorContainerLenient m_ErrorsPrivate;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___VCFREADER__HPP

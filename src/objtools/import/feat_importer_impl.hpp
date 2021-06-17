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
* Author:  Frank Ludwig, NCBI
*
* File Description:
*   Test application for the CFormatGuess component
*
* ===========================================================================
*/

#ifndef FEAT_IMPORTER_IMPL__HPP
#define FEAT_IMPORTER_IMPL__HPP

#include <corelib/ncbifile.hpp>
#include <util/line_reader.hpp>
#include <objtools/import/import_message_handler.hpp>
#include <objtools/import/feat_importer.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class NCBI_XOBJIMPORT_EXPORT CFeatImporter_impl :
    public CFeatImporter
//  ============================================================================
{
public:
    virtual void
    ReadSeqAnnot(
        CStreamLineReader&,
        CSeq_annot&) override;

    virtual void
    SetIdResolver(
        CIdResolver*) override;

protected:
    CFeatImporter_impl(
        unsigned int,
        CImportMessageHandler&);

    virtual ~CFeatImporter_impl() {};

    unsigned int mFlags;
    unique_ptr<CIdResolver> mpIdResolver;
    unique_ptr<CFeatLineReader> mpReader;
    unique_ptr<CFeatImportData> mpImportData;
    unique_ptr<CFeatAnnotAssembler> mpAssembler;
    CImportMessageHandler& mErrorHandler;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif

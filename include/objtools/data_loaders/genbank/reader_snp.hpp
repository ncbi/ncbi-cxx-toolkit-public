#ifndef READER_SNP__HPP_INCLUDED
#define READER_SNP__HPP_INCLUDED

/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko
*
*  File Description: SNP data reader and compressor
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <serial/objhook.hpp>
#include <objmgr/impl/snp_annot_info.hpp>

BEGIN_NCBI_SCOPE

class CObjectIStream;

BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_annot;
class CSeq_feat;
class CSeq_annot_SNP_Info;

class NCBI_XREADER_EXPORT CSeq_annot_SNP_Info_Reader
{
public:
    // parse ASN converting SNP features to packed table.
    static void Parse(CObjectIStream& in,
                      CSeq_annot_SNP_Info& snp_info);

    // store table in platform specific format
    static void Write(CNcbiOstream& stream,
                      const CSeq_annot_SNP_Info& snp_info);
    // load table in platform specific format
    static void Read(CNcbiIstream& stream,
                     CSeq_annot_SNP_Info& snp_info);
};

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* $Log$
* Revision 1.2  2004/01/13 16:55:53  vasilche
* CReader, CSeqref and some more classes moved from xobjmgr to separate lib.
* Headers moved from include/objmgr to include/objtools/data_loaders/genbank.
*
* Revision 1.1  2003/08/14 20:05:18  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
*/

#endif // READER_SNP__HPP_INCLUDED

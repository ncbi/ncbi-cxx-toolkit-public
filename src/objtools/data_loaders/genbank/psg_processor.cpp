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
 * Author: Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description: Event loop for PSG data loader
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/impl/psg_processor.hpp>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);
BEGIN_NAMESPACE(psgl);


CPSGL_Processor::CPSGL_Processor()
{
}


CPSGL_Processor::~CPSGL_Processor()
{
}


const char* CPSGL_Processor::GetProcessorName() const
{
    return "CPSGL_Processor";
}


ostream& CPSGL_Processor::PrintProcessorDescr(ostream& out) const
{
    return PrintProcessorArgs(out << GetProcessorName()<<'('<<this<<')');
}


ostream& CPSGL_Processor::PrintProcessorArgs(ostream& out) const
{
    return out;
}


CPSGL_Processor::EProcessResult
CPSGL_Processor::ProcessItemFast(EPSG_Status /*status*/,
                                 const shared_ptr<CPSG_ReplyItem>& /*item*/)
{
    return eToNextStage;
}


CPSGL_Processor::EProcessResult
CPSGL_Processor::ProcessItemSlow(EPSG_Status /*status*/,
                                 const shared_ptr<CPSG_ReplyItem>& /*item*/)
{
    return eFailed;
}


CPSGL_Processor::EProcessResult
CPSGL_Processor::ProcessReplyFast(EPSG_Status /*status*/,
                                  const shared_ptr<CPSG_Reply>& /*reply*/)
{
    return eToNextStage;
}


CPSGL_Processor::EProcessResult
CPSGL_Processor::ProcessReplySlow(EPSG_Status /*status*/,
                                  const shared_ptr<CPSG_Reply>& /*reply*/)
{
    return eFailed;
}


CPSGL_Processor::EProcessResult
CPSGL_Processor::ProcessReplyFinal()
{
    return eProcessed;
}


END_NAMESPACE(psgl);
END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // HAVE_PSG_LOADER

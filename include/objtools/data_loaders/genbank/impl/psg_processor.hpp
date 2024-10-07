#ifndef OBJTOOLS_DATA_LOADERS_PSG___PSG_PROCESSOR__HPP
#define OBJTOOLS_DATA_LOADERS_PSG___PSG_PROCESSOR__HPP

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
 * File Description: PSG reply processors
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <objtools/pubseq_gateway/client/psg_client.hpp>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);
BEGIN_NAMESPACE(psgl);


class CPSGL_Processor;

struct SProcessorDescrPrinter
{
    explicit SProcessorDescrPrinter(const CPSGL_Processor* processor)
        : m_Proocessor(processor)
    {
    }
    
private:
    friend ostream& operator<<(ostream& out, SProcessorDescrPrinter printer);
    
    const CPSGL_Processor* m_Proocessor;
};


class CPSGL_Processor : public CObject
{
public:
    CPSGL_Processor();
    virtual ~CPSGL_Processor();

    virtual const char* GetProcessorName() const;
    virtual ostream& PrintProcessorDescr(ostream& out) const;
    virtual ostream& PrintProcessorArgs(ostream& out) const;
    
    SProcessorDescrPrinter Descr() const
    {
        return SProcessorDescrPrinter(this);
    }

    // The processing of reply items and replies is done in several stages:
    //   1st (Fast) stage for quick actions that are quicker than MT overhead
    //     it will be executed in a main event loop
    //   2nd (Slow) stage for time consuming actions
    //     it will be executed in a background thread
    //     to avoid congestion of the main event loop
    //   3rh (Final) stage is for waiting for other requesrs to complete
    //     it will be executed in a request caller thread to avoid deadlocks
    //     only replies can be executed in this stage
    //
    // The result of processing indicates if the processing is either
    //   eProcessed - done for this reply or item, no further stage necessary
    //   eFailed - done processing with error, no further stage will be called
    //   eToNextStage - needs more processing in the next stage
    //     ProcessItemSlow() and ProcessReplyFinal() cannot return this code
    //
    // The stage logic is implemented by CPSGL_RequestTracker.
    // The CPSGL_Processor implementation need only define Process*() methods.

    // The Process*() methods return codes
    enum EProcessResult {
        eProcessed,
        eFailed,
        eToNextStage
    };
    
    // process in event loop thread, the method should return quickly
    // if longer processing is required the method should return 'false',
    // and dispatcher will schedule execution in a background thread
    // allowed to return:
    //   eProcessed
    //   eFailed
    //   eToNextStage
    virtual EProcessResult ProcessItemFast(EPSG_Status status,
                                           const shared_ptr<CPSG_ReplyItem>& item);
    // methods to perform longer processing in a background thread
    // allowed to return:
    //   eProcessed
    //   eFailed
    virtual EProcessResult ProcessItemSlow(EPSG_Status status,
                                           const shared_ptr<CPSG_ReplyItem>& item);
    
    // process in event loop thread, the method should return quickly
    // if longer processing is required the method should return 'false',
    // and dispatcher will schedule execution in a background thread
    // allowed to return:
    //   eProcessed
    //   eFailed
    //   eToNextStage
    virtual EProcessResult ProcessReplyFast(EPSG_Status status,
                                            const shared_ptr<CPSG_Reply>& reply);
    // allowed to return:
    //   eProcessed
    //   eFailed
    //   eToNextStage
    virtual EProcessResult ProcessReplySlow(EPSG_Status status,
                                            const shared_ptr<CPSG_Reply>& reply);
    // method will be called in a result receiving code
    // by default no action is done and result is eProcessed
    // redefine this method if some waiting is required
    // allowed to return:
    //   eProcessed
    //   eFailed
    virtual EProcessResult ProcessReplyFinal();

    const vector<string>& GetErrors() const
    {
        return m_Errors;
    }
    void AddError(const string& message);
    
protected:
    EProcessResult x_Failed(const string& message);
    static string x_Format(EPSG_Status status);
    static string x_Format(EPSG_Status status, const shared_ptr<CPSG_Reply>& reply);

    vector<string> m_Errors;
};


inline
ostream& operator<<(ostream& out, SProcessorDescrPrinter printer)
{
    return printer.m_Proocessor->PrintProcessorDescr(out);
}


END_NAMESPACE(psgl);
END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // HAVE_PSG_LOADER

#endif  // OBJTOOLS_DATA_LOADERS_PSG___PSG_PROCESSOR__HPP

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
 * Author: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <objtools/pubseq_gateway/client/psg_client.hpp>

USING_NCBI_SCOPE;

template <class TReply>
void ReportErrors(TReply r)
{
    for (auto message = r->GetNextMessage(); message; message = r->GetNextMessage()) {
        cout << message << endl;
    }
}

void OnItemComplete(EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item)
{
    if (status == EPSG_Status::eSuccess) {
        if (item->GetType() == CPSG_ReplyItem::eBioseqInfo) {
            auto bioseq_info = static_pointer_cast<CPSG_BioseqInfo>(item);
            cout << bioseq_info->GetCanonicalId().GetId() << endl;
        }
    } else {
        ReportErrors(item);
    }
}

void OnReplyComplete(EPSG_Status status, const shared_ptr<CPSG_Reply>& reply)
{
    if (status != EPSG_Status::eSuccess) {
        ReportErrors(reply);
    }
}

class CPsgClientSampleApp : public CNcbiApplication
{
public:
    virtual void Init();
    virtual int Run();
};

void CPsgClientSampleApp::Init()
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "PSG client API sample");
    arg_desc->AddDefaultKey("service", "SERVICE", "PSG service name or host:port pair", CArgDescriptions::eString, "");
    arg_desc->AddPositional("ID", "Bio ID", CArgDescriptions::eString);
    SetupArgDescriptions(arg_desc.release());
}

int CPsgClientSampleApp::Run()
{
    const auto& args = GetArgs();
    CPSG_EventLoop queue(args["service"].AsString(), &OnItemComplete, &OnReplyComplete);
    auto request = make_shared<CPSG_Request_Resolve>(args["ID"].AsString());
    request->IncludeInfo(CPSG_Request_Resolve::fCanonicalId);

    if (queue.SendRequest(request, CDeadline::eInfinite)) {
        queue.Stop();
        return queue.Run(CDeadline::eInfinite) ? 0 : -2;
    }

    return -1;
}

int main(int argc, const char* argv[])
{
    return CPsgClientSampleApp().AppMain(argc, argv);
}

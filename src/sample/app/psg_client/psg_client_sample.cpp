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
#include <objtools/pubseq_gateway/client/impl/misc.hpp>

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
        if (auto item_type = item->GetType(); item_type == CPSG_ReplyItem::eBioseqInfo) {
            auto bioseq_info = static_pointer_cast<CPSG_BioseqInfo>(item);
            cout << bioseq_info->GetCanonicalId().GetId() << endl;
        } else {
            cerr << "Unexpected item type: " << item_type << endl;
        }
    } else if (status == EPSG_Status::eInProgress) {
        cerr << "Timeout on getting an item status" << endl;
    } else {
        ReportErrors(item);
    }
}

void OnReplyComplete(EPSG_Status status, const shared_ptr<CPSG_Reply>& reply)
{
    if (status == EPSG_Status::eInProgress) {
        cerr << "Timeout on getting a reply status" << endl;
    } else if (status != EPSG_Status::eSuccess) {
        ReportErrors(reply);
    }
}

int EventLoop(const string& service, shared_ptr<CPSG_Request> request, CTimeout timeout)
{
    CPSG_EventLoop event_loop(service, &OnItemComplete, &OnReplyComplete);

    if (event_loop.SendRequest(request, timeout)) {
        event_loop.Stop();

        if (event_loop.Run(timeout)) {
            return 0;
        }

        cerr << "Timeout on getting a reply" << endl;
        return -2;
    }

    cerr << "Timeout on sending a request" << endl;
    return -1;
}

int Queue(const string& service, shared_ptr<CPSG_Request> request, CTimeout timeout)
{
    CPSG_Queue queue(service);

    if (auto reply = queue.SendRequestAndGetReply(request, timeout)) {
        for (auto item = reply->GetNextItem(timeout); item; item = reply->GetNextItem(timeout)) {
                if (item->GetType() == CPSG_ReplyItem::eEndOfReply) {
                    OnReplyComplete(reply->GetStatus(timeout), reply);
                    return 0;
                }

                OnItemComplete(item->GetStatus(timeout), item);
            }

        cerr << "Timeout on getting a reply" << endl;
        return -2;
    }

    cerr << "Timeout on sending a request" << endl;
    return -1;
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
    arg_desc->AddDefaultKey("timeout", "SECONDS", "Timeout value in seconds", CArgDescriptions::eDouble, "1.0");
    arg_desc->AddFlag("event-loop", "Use event loop instead of queue");
    arg_desc->AddFlag("https", "Enable HTTPS");
    arg_desc->AddPositional("ID", "Bio ID", CArgDescriptions::eString);
    SetupArgDescriptions(arg_desc.release());
}

int CPsgClientSampleApp::Run()
{
    const auto& args = GetArgs();

    if (args["https"].HasValue()) {
        TPSG_Https::SetDefault(true);
    }

    auto service = args["service"].AsString();
    auto request = make_shared<CPSG_Request_Resolve>(args["ID"].AsString());
    auto timeout = CTimeout(args["timeout"].AsDouble());
    request->IncludeInfo(CPSG_Request_Resolve::fCanonicalId);
    return args["event-loop"].HasValue() ? EventLoop(service, request, timeout) : Queue(service, request, timeout);
}

int main(int argc, const char* argv[])
{
    return CPsgClientSampleApp().AppMain(argc, argv);
}

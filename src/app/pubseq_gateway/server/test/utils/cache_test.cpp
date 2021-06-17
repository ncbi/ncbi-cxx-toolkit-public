#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <utility>
#include <thread>
#include <chrono>
#include <string>

#include "../../exclude_blob_cache.hpp"

USING_SCOPE(ncbi);


CExcludeBlobCache   cache(10, 1000, 800);


void gc(void)
{
    for ( ; ; ) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        cache.Purge();
        if (cache.Size() == 0)
            break;
    }
}


void add(string user, size_t  count)
{
    bool    val;
    for (size_t k = 0; k < count; ++k) {
        cache.AddBlobId(user, 10, k, val);
    }
}


class CCacheTestApplication : public CNcbiApplication
{
    virtual int  Run(void);
    virtual void Init(void);
};



void CCacheTestApplication::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->AddPositional("users", "number of users", CArgDescriptions::eInteger);
    arg_desc->AddPositional("threads_per_user", "number of threads per user", CArgDescriptions::eInteger);
    arg_desc->AddPositional("count", "number of cache.add() per thread", CArgDescriptions::eInteger);

    SetupArgDescriptions(arg_desc.release());
}


int CCacheTestApplication::Run(void)
{
    cout << "Cache test" << endl;

    const CArgs &   args = GetArgs();
    int     users = args["users"].AsInteger();
    int     threads_per_user = args["threads_per_user"].AsInteger();
    int     count = args["count"].AsInteger();

    list<thread *>  threads;

    for (int k = 0; k < users; ++k) {
        string      user_name = "User" + to_string(k);
        for (int j = 0; j < threads_per_user; ++j) {
            thread *  th = new thread(add, user_name, count);
            threads.push_back(th);
        }
    }

    thread  gc_thread(gc);

    for (auto &  t : threads)
        t->join();
    gc_thread.join();

    for (auto &  t : threads)
        delete t;

    return 0;
}


int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return CCacheTestApplication().AppMain(argc, argv);
}

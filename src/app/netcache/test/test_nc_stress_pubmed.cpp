#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/services/netcache_api.hpp>
#include <util/thread_pool.hpp>
#include <util/random_gen.hpp>

#if defined(HAVE_SIGNAL_H)  &&  defined(NCBI_OS_UNIX)
#  define USE_SIGNAL
#  include <signal.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef NCBI_OS_UNIX
#ifdef HAVE_NANOSLEEP

#define USE_UGLY_CODE_INSTEAD
#include <sys/time.h>

#endif /* HAVE_NANOSLEEP */
#endif /*NCBI_OS_UNIX */


USING_NCBI_SCOPE;


static void PrintAllStat();
static bool need_conf_reload = false;
static CAtomicCounter s_BlobId;
static CRandom s_Rnd((CRandom::TValue)CProcess::GetCurrentPid());

#ifdef USE_SIGNAL
/*******************************************************
 * Signals
 *******************************************************/

// not all platforms define sighandler_t

extern "C" {

typedef RETSIGTYPE (*TSigHandler)(int);

static void
ONDATR(TSigHandler handler, int n)
{
    if (handler == SIG_DFL) {
        exit(1);
    } else if (handler != SIG_IGN) {
        handler(n);
    }
}

TSigHandler old_SIGINT;
TSigHandler old_SIGQUIT;
TSigHandler old_SIGALRM;
TSigHandler old_SIGABRT;
TSigHandler old_SIGTERM;
TSigHandler old_SIGBUS;

static void /* RETSIGTYPE */
on_SIGNAL(int OnDatr)
{
    cerr << "PID : " << CProcess::GetCurrentPid() << " : signal caught : "
         << OnDatr << endl;
    PrintAllStat();

    switch(OnDatr) {
        case SIGQUIT : ONDATR(old_SIGQUIT, OnDatr); break;
        case SIGINT  : ONDATR(old_SIGINT, OnDatr); break;
        case SIGALRM : ONDATR(old_SIGALRM, OnDatr); break;
        case SIGABRT : ONDATR(old_SIGABRT, OnDatr); break;
        case SIGTERM : ONDATR(old_SIGTERM, OnDatr); break;
        case SIGBUS  : ONDATR(old_SIGBUS, OnDatr); break;
        default :
            cerr << "J-SIGNAL!" << endl;
            _exit(1);
    }
}

static void
on_SIGHUP(int onDatr)
{
    need_conf_reload = true;
}

static void
SetSigHandlers()
{
    old_SIGQUIT = signal(SIGQUIT, on_SIGNAL);
    old_SIGINT = signal(SIGINT, on_SIGNAL);
    old_SIGALRM = signal(SIGALRM, on_SIGNAL);
    old_SIGABRT = signal(SIGABRT, on_SIGNAL);
    old_SIGTERM = signal(SIGTERM, on_SIGNAL);
    old_SIGBUS = signal(SIGBUS, on_SIGNAL);
    signal(SIGHUP, on_SIGHUP);
}

}
#endif


static CStdPoolOfThreads  writeThreadPool(5, 1000000);
static CStdPoolOfThreads  readThreadPool(5, 1000000);
static CStdPoolOfThreads  drThreadPool(5, 1000000);


/*******************************************************
 * Stressing
 *******************************************************/
class TimePotam {
private :
    static const int EPOCH_V = 10000000;

public :
    TimePotam()
        :   mStartMilliSecs(0)
        ,   mLastMilliSecs(0)
        {
            mStartMilliSecs = mLastMilliSecs = TimeOfDayMilliSecs();
        };

    size_t FromStart()
        {
            return TimeOfDayMilliSecs() - mStartMilliSecs;
        };

    size_t FromLast()
        {
            size_t cTv = mLastMilliSecs;
            mLastMilliSecs = TimeOfDayMilliSecs();
            return mLastMilliSecs - cTv;
        };

    void WaitMilliSecs(size_t MilliSecs)
        {
#ifdef USE_UGLY_CODE_INSTEAD
            struct timespec sTS;
            sTS.tv_sec = MilliSecs / 1000;
            sTS.tv_nsec = (MilliSecs % 1000) * 1000000;

            nanosleep(&sTS, NULL);
#else
            cerr << "That platform does not support!" << endl;
            exit(1);
#endif /* USE_UGLY_CODE_INSTEAD */
        };

    size_t TimeOfDayMilliSecs()
        {
#ifdef USE_UGLY_CODE_INSTEAD
            struct timeval sTV;
            gettimeofday(&sTV, NULL);
            size_t Secs = (sTV.tv_sec % EPOCH_V) * 1000;
            size_t MilliSecs = sTV.tv_usec / 1000;
//
            return Secs + MilliSecs;
#else
            cerr << "That platform does not support!" << endl;
            exit(1);
//  Nice fix, then. Anyway, that fix will not work.
//  However, I need amount of seconds from Epoch. CTime
//  does not provides that method. Ofcourse, I can to generate
//  those seconds ... but I do not have time for it.
//  So, throving error, moreother, that test was written for
//  unix only
//
//            CTime now(CTime::eCurrent, CTime::eGmt);
//            size_t Secs = (now.Second() % EPOCH_V) * 1000;
//            size_t MilliSecs = now.MilliSecond();
//
//          return Secs + MilliSecs;
#endif /* USE_UGLY_CODE_INSTEAD */
        };

private :
    size_t mStartMilliSecs;
    size_t mLastMilliSecs;
};

class DelayPotam {
public :
    DelayPotam(const string &NcKey, Int4 DelayMilliSecs)
        :   mNcKey(NcKey)
        ,   mDelayMilliSecs(DelayMilliSecs)
        ,   mCreationTime(0)
        {
            mCreationTime = TimePotam().TimeOfDayMilliSecs();
        };

    DelayPotam(const DelayPotam &DPotam)
        :   mNcKey(DPotam.mNcKey)
        ,   mDelayMilliSecs(DPotam.mDelayMilliSecs)
        ,   mCreationTime(DPotam.mCreationTime)
        {
        };

    DelayPotam &operator = (const DelayPotam &DPotam)
        {
            if (this != &DPotam) {
                mNcKey = DPotam.mNcKey;
                mDelayMilliSecs = DPotam.mDelayMilliSecs;
                mCreationTime = DPotam.mCreationTime;
            }

            return *this;
        };

    bool JustInTime() const
        {
            return TimePotam().TimeOfDayMilliSecs()
                            > (mCreationTime + mDelayMilliSecs);
        }
    const string &NcKey() const { return mNcKey; };
    size_t CreationTimeMillisecs() const { return mCreationTime; };
    size_t DelayMilliSecs() const { return mDelayMilliSecs; };

private :
    string mNcKey;
    size_t mDelayMilliSecs;
    size_t mCreationTime;
};

CAtomicCounter theWQty;

class StressoPotam;

class CReadTask : public CStdRequest
{
public:
    CReadTask(StressoPotam* potam, CNetCacheAPI& cln, const string& key)
        : m_Potam(potam),
          m_Client(cln),
          m_Key(key)
    {}

    virtual void Process(void);


    StressoPotam* m_Potam;
    CNetCacheAPI& m_Client;
    string        m_Key;
};

class CDelayedReadTask : public CStdRequest
{
public:
    CDelayedReadTask(StressoPotam* potam, CNetCacheAPI& cln, const string& key)
        : m_Potam(potam),
          m_Client(cln),
          m_Key(key)
    {}

    virtual void Process(void);


    StressoPotam* m_Potam;
    CNetCacheAPI& m_Client;
    string        m_Key;
};

class StressoPotam {
public :
    StressoPotam(
        size_t BufferSize,
        Int4 ReadRatio,
        Int4 DelayRatio,
        Uint4 DelayMilliSecs
    )
        :   mRQty(0)
        ,   mREQty(0)
        ,   mRTime(0)
        ,   mWQty(0)
        ,   mWEQty(0)
        ,   mWTime(0)
        ,   mDQty(0)
        ,   mDEQty(0)
        ,   mDTime(0)
        ,   mBufferSize(BufferSize)
        ,   mpBuffer(NULL)
        ,   mReadRatio(ReadRatio)
        ,   mDelayRatio(DelayRatio)
        ,   mDelayMilliSecs(DelayMilliSecs)
        {
            if (mBufferSize == 0) {
                cerr << "Zero Buffer size" << endl;
                abort();
            }
            if ((mpBuffer = new char [mBufferSize]) == NULL) {
                cerr << "Can not allocate " << mBufferSize << " chars" << endl;
                abort();
            }

            *((size_t *)mpBuffer) = mBufferSize;
        };
    ~StressoPotam()
        {
            mRQty = 0;
            mREQty = 0;
            mRTime = 0;
            mWQty = 0;
            mWEQty = 0;
            mWTime = 0;
            mDQty = 0;
            mDEQty = 0;
            mDTime = 0;
            mBufferSize = 0;
            if (mpBuffer != NULL) {
                delete [] mpBuffer;
                mpBuffer = NULL;
            }
            mDelayRatio = 0;
            mDelayMilliSecs = 0;

            mDelayedPotams.clear();
        };

    void DoWrite(CNetCacheAPI& cln)
    {
        TimePotam TPot;
        string NcKey;
        CNetCacheKey::GenerateBlobKey(&NcKey, s_BlobId.Add(1), "130.14.24.171", 9000, 1, s_Rnd.GetRand() * 2);
        CNetCacheKey::AddExtensions(NcKey, cln.GetService().GetServiceName());
        int wtime = -1;
        try {
            cln.PutData(NcKey, mpBuffer, mBufferSize);
            //NcKey = cln.PutData(mpBuffer, mBufferSize);
            wtime = int(TPot.FromLast());

            CFastMutexGuard guard(lock);
            mWTime += wtime;
            mWQty ++;
            Uint4 w_qty = Uint4(theWQty.Add(1));

            if (mDelayRatio != 0 && w_qty % mDelayRatio == 0) {
                mDelayedPotams.insert(mDelayedPotams.end(),
                                      DelayPotam(NcKey, mDelayMilliSecs));
            }

            if (NcKey != "" && mReadRatio != 0 && w_qty % mReadRatio == 0) {
                try {
                    readThreadPool.AcceptRequest(CRef<CStdRequest>(new CReadTask(this, cln, NcKey)));
                }
                catch (CBlockingQueueException& ex) {
                    ERR_POST("Cannot add request: " << ex);
                }
            }
        }
        catch(exception& ex) {
            ERR_POST("Error writing blob " << NcKey << ": " << ex.what());
            NcKey = "";
            CFastMutexGuard guard(lock);
            mWEQty ++;
        }
    }

    void DoRead(CNetCacheAPI& cln, const string& NcKey)
        {
            try {
                TimePotam TPot;
                CSimpleBuffer Blob;
                CNetCacheAPI::EReadResult rres;

                rres = cln.GetData(NcKey, Blob);

                CFastMutexGuard guard(lock);
                if (rres == CNetCacheAPI::eNotFound) {
                    mREQty ++;
                }
                else {
                    if (*((Uint4 *)Blob.data()) == mBufferSize) {
                        int rtime = int(TPot.FromLast());
                        mRTime += rtime;
                        mRQty ++;
                    }
                    else {
                        mREQty ++;
                    }
                }
            }
            catch( exception& ex ) {
                ERR_POST("Error reading blob " << NcKey << ": " << ex.what());
                CFastMutexGuard guard(lock);
                mREQty ++;
            }
        };

    void DoDelayedRead(CNetCacheAPI& cln, const string& NcKey)
    {
        try {
            TimePotam TPot;
            CSimpleBuffer Blob;
            CNetCacheAPI::EReadResult rres;

            rres = cln.GetData(NcKey, Blob);

            CFastMutexGuard guard(lock);
            if (rres == CNetCacheAPI::eNotFound) {
                mDEQty ++;
            }
            else {
                if (*((Uint4 *)Blob.data()) == mBufferSize) {
                    int rtime = int(TPot.FromStart());
                    mDTime += rtime;
                    mDQty ++;
                }
                else {
                    mDEQty ++;
                }
            }
        }
        catch( exception& ex ) {
            ERR_POST("Error in delayed read of blob " << NcKey << ": " << ex.what());
            CFastMutexGuard guard(lock);
            mDEQty ++;
        }
    }

    void CheckReadDelayed(CNetCacheAPI& cln)
        {
            CFastMutexGuard guard(lock);
            while(true) {
                list < DelayPotam >::iterator Pos =
                                            mDelayedPotams.begin();
                if (Pos == mDelayedPotams.end()) {
                    break;
                }

                if (!(*Pos).JustInTime()) {
                    break;
                }
                try {
                    drThreadPool.AcceptRequest(CRef<CStdRequest>(new CDelayedReadTask(this, cln, (*Pos).NcKey())));
                }
                catch (CBlockingQueueException& ex) {
                    ERR_POST("Cannot add request: " << ex);
                }

                mDelayedPotams.erase(Pos);
            }
        };

    string Stat()
        {
            string RetVal = NStr::UInt8ToString(mBufferSize) + "(bs)";

            if (mWQty != 0) {
                RetVal += " " + NStr::UInt8ToString(mWQty) + "(wq)";
                size_t AVTime = (size_t)(mWTime / (long long)mWQty);
                RetVal += " " + NStr::UInt8ToString(AVTime) + "(awt)";

            }
            else {
                RetVal += " 0(wq) 0(awt)";
            }
            RetVal += " " + NStr::UInt8ToString(mWEQty) + "(we)";

            if (mRQty != 0) {
                RetVal += " " + NStr::UInt8ToString(mRQty) + "(rq)";
                size_t AVTime = (size_t)(mRTime / (long long)mRQty);
                RetVal += " " + NStr::UInt8ToString(AVTime) + "(art)";

            }
            else {
                RetVal += " 0(rq) 0(art)";
            }
            RetVal += " " + NStr::UInt8ToString(mREQty) + "(re)";

            if (mDQty != 0) {
                RetVal += " " + NStr::UInt8ToString(mDQty) + "(dq)";
                size_t AVTime = (size_t)(mDTime / (long long)mDQty);
                RetVal += " " + NStr::UInt8ToString(AVTime) + "(adt)";

            }
            else {
                RetVal += " 0(dq) 0(adt)";
            }
            RetVal += " " + NStr::UInt8ToString(mDEQty) + "(de)";

            return RetVal;
        };

    size_t RQty() { return mRQty; };
    size_t REQty() { return mREQty; };
    long long RTime() { return mRTime; };
    size_t WQty() { return mWQty; };
    size_t WEQty() { return mWEQty; };
    long long WTime() { return mWTime; };
    size_t DQty() { return mDQty; };
    size_t DEQty() { return mDEQty; };
    long long DTime() { return mDTime; };
    size_t BufferSize() { return mBufferSize; };

private :
    CFastMutex lock;
    size_t mRQty;
    size_t mREQty;
    long long mRTime;

    size_t mWQty;
    size_t mWEQty;
    long long mWTime;

    size_t mDQty;
    size_t mDEQty;
    long long mDTime;

    size_t mBufferSize;
    char *mpBuffer;

    Int4 mReadRatio;
    Int4 mDelayRatio;
    Uint4 mDelayMilliSecs;
    list < DelayPotam > mDelayedPotams;
};

class CWriteTask : public CStdRequest
{
public:
    CWriteTask(StressoPotam* potam, CNetCacheAPI& cln)
        : m_Potam(potam),
          m_Client(cln)
    {}

    virtual void Process(void)
    {
        m_Potam->DoWrite(m_Client);
    }


    StressoPotam* m_Potam;
    CNetCacheAPI& m_Client;
};

void
CReadTask::Process(void)
{
    m_Potam->DoRead(m_Client, m_Key);
}

void
CDelayedReadTask::Process(void)
{
    m_Potam->DoDelayedRead(m_Client, m_Key);
}

class StressoPotary {
public :
    StressoPotary()
        :   mService("")
        ,   mPerSec(100)
        ,   mReadDelayMinutes(3)
        ,   mDelayRatio(4)
        {
            mStartStressing = TimePotam().TimeOfDayMilliSecs();
            mEndStressing = TimePotam().TimeOfDayMilliSecs();
        };
    ~StressoPotary()
        {
            mReadDelayMinutes = 3;
            mDelayRatio = 4;
            mPerSec = 100;
            mService = "";
            mStartStressing = TimePotam().TimeOfDayMilliSecs();
            mEndStressing = TimePotam().TimeOfDayMilliSecs();

            mPotams.clear();

            while(mPotamMap.size()) {
                StressoPotam *pPotam = (*mPotamMap.begin()).second;
                if (pPotam != NULL) {
                    delete pPotam;
                }

                mPotamMap.erase(mPotamMap.begin());
            }
        };

    void Init(
                const string &Service,
                Int4 PerSec,
                Int4 ReadRatio,
                Int4 DelayRatio,
                Int4 ReadDelayMinutes
        )
        {
            mService = Service;
            mPerSec = PerSec;
            mReadDelayMinutes = ReadDelayMinutes;
            mReadRatio = ReadRatio;
            mDelayRatio = DelayRatio;

            if (mPerSec < 10 || 400 <= mPerSec) {
                cerr << "Invalid PerSecond value '" << NStr::IntToString(mPerSec) << "' should be less than 400 and greater than 10" << endl;
                abort();
            }
        };

    void AddPotam(size_t BufferSize)
        {
            StressoPotam *pPotam = NULL;

            map < size_t, StressoPotam * >::iterator Pos =
                                            mPotamMap.find(BufferSize);

            if (Pos == mPotamMap.end()) {
                pPotam = new StressoPotam(
                                    BufferSize,
                                    mReadRatio,
                                    mDelayRatio,
                                    mReadDelayMinutes * 60 * 1000
                                );
                mPotamMap[BufferSize] = pPotam;
            }
            else {
                pPotam = (*Pos).second;
            }

            mPotams.insert(mPotams.end(), pPotam);
        };

    void Stress()
        {
            TimePotam TPot;

            if (mPotams.size() == 0) {
                cerr << "No potams in potary" << endl;
                abort();
            }

            mStartStressing = TPot.TimeOfDayMilliSecs();

            size_t IterCnt = 0;
            size_t Pupiter = 0;
            size_t Puzator = 10000;
            // size_t Puzator = 100;
            CNetCacheAPI cln(mService,"test_nc_stress_pubmed");

            for( ; ; ) {
                vector < StressoPotam * >::iterator PB =
                                                        mPotams.begin();
                for( ; PB != mPotams.end(); PB ++) {
                    size_t NewT = (IterCnt * 1000) / mPerSec;
                    size_t Diffa = TPot.TimeOfDayMilliSecs()
                                                - mStartStressing;
                    if (Diffa < NewT) {
                        TPot.WaitMilliSecs(NewT - Diffa);
                    }
                    if (need_conf_reload) {
                        CNcbiApplication::Instance()->ReloadConfig();
                        need_conf_reload = false;
                    }

                    StressoPotam *pPotam = *PB;
                    try {
                        writeThreadPool.AcceptRequest(CRef<CStdRequest>(new CWriteTask(pPotam, cln)));
                    }
                    catch (CBlockingQueueException& ex) {
                        ERR_POST("Cannot add request: " << ex);
                    }
                    mEndStressing = TPot.TimeOfDayMilliSecs();

                    IterCnt ++;

                    ITERATE(vector<StressoPotam*>, it, mPotams) {
                        StressoPotam *pPotam = *it;
                        pPotam -> CheckReadDelayed(cln);
                    }
                }

                if (IterCnt / Puzator != Pupiter) {
                    cerr << Stat() << endl;

                    Pupiter = IterCnt / Puzator;
                }
            }
        };

    string Stat()
        {
            string RetVal = "<==========>\n";

            size_t Qty = 0;
            size_t Err = 0;

            map < size_t, StressoPotam * >::iterator PB = mPotamMap.begin();
            for( ; PB != mPotamMap.end(); PB ++) {
                StressoPotam *pPotam = (*PB).second;
                Qty += pPotam -> WQty();
                Err += pPotam -> WEQty();
                Err += pPotam -> REQty();
                Err += pPotam -> DEQty();
            }

            size_t Diffa = mEndStressing - mStartStressing;

            RetVal += "Blobs written : " + NStr::UInt8ToString(Qty) + "\n";
            RetVal += "All time   : " + NStr::UInt8ToString(Diffa / 1000) + " seconds\n";
            if (Qty != 0) {
                double PSec = 1000 * ((double)Qty / (double)Diffa);
                RetVal += "Per Second : " + NStr::DoubleToString(PSec) + "\n";
            }
            else {
                RetVal += "Per Second : 0\n";
            }
            RetVal += "All errors : " + NStr::UInt8ToString(Err) + "\n";

            PB = mPotamMap.begin();
            for( ; PB != mPotamMap.end(); PB ++) {
                StressoPotam *pPotam = (*PB).second;
                RetVal += "    " + pPotam -> Stat() + "\n";
            }

            return RetVal;
        };

private :
    string mService;

    Int4 mPerSec;
    Int4 mReadDelayMinutes;
    Int4 mReadRatio;
    Int4 mDelayRatio;

    size_t mStartStressing;
    size_t mEndStressing;

    vector < StressoPotam * > mPotams;
    map < size_t, StressoPotam * > mPotamMap;
};

StressoPotary *ThePotary = NULL;

void
PrintAllStat()
{
    if (ThePotary != NULL) {
        cerr << ThePotary -> Stat() << endl;
    }
}

/****************************************************
 * Main
 ****************************************************/

string theProgName = "stress_2";

// string theConnection = "pdev1:9001";
string theConnection = "";
string theService = "pdev1:9001";
Int4 thePerSec = 25;
Int4 theDelayTime = 30;
Int4 theReadRatio = 2;
Int4 theDelayRatio = 8;

string ConnTag   = "-c";
string PerSecTag = "-p";
string DelayTag  = "-d";
string ReadRatioTag = "-w";
string DelayedRatioTag = "-r";

string ConnDsc   = "connection";
string PerSecDsc = "per_second";
string DelayDsc  = "delay_time";
string ReadRatioDsc  = "read_ratio";
string DelayedRatioDsc = "delay_ratio";

void
usage()
{
    cerr << "Syntax\n";
    cerr << "    " << theProgName;
    cerr << " " << ConnTag << " " << ConnDsc;
//  cerr << " [" << ConnTag << " " << ConnDsc << "]";
    cerr << " [" << PerSecTag << " " << PerSecDsc << "]";
    cerr << " [" << DelayTag << " " << DelayDsc << "]";
    cerr << " [" << ReadRatioTag << " " << ReadRatioDsc << "]";
    cerr << " [" << DelayedRatioTag << " " << DelayedRatioDsc << "]";
    cerr << "\n";
    cerr << "Where\n";
    cerr << "    " << ConnDsc << "  - connection string (required)\n";
    cerr << "    " << PerSecDsc << "  - blobs per second (25)\n";
    cerr << "    " << DelayDsc << "  - delay minutes for secondary reading (30 minutes)\n";
    cerr << "    " << ReadRatioDsc << " - read ratio for first reading (2)\n";
    cerr << "    " << DelayedRatioDsc << " - delay ratio for secondary reading (8)\n";
    cerr << "\n";
}

bool
parseArgs(int argc, char **argv)
{
    theProgName = *argv;
    for(int llp = 1; llp < argc; llp ++) {
        string Arg = argv[llp];

        if (Arg == ConnTag) {
            llp ++;
            if (llp >= argc) {
                cerr << ConnDsc << " parameter missed" << endl;
                return false;
            }
            theConnection = argv[llp];
            continue;
        }

        if (Arg == PerSecTag) {
            llp ++;
            if (llp >= argc) {
                cerr << PerSecDsc << " parameter missed" << endl;
                return false;
            }
            thePerSec = NStr::StringToInt(argv[llp]);
            continue;
        }

        if (Arg == DelayTag) {
            llp ++;
            if (llp >= argc) {
                cerr << DelayDsc << " parameter missed" << endl;
                return false;
            }
            theDelayTime = NStr::StringToInt(argv[llp]);
            continue;
        }

        if (Arg == ReadRatioTag) {
            llp ++;
            if (llp >= argc) {
                cerr << ReadRatioDsc << " parameter missed" << endl;
                return false;
            }
            theReadRatio = NStr::StringToInt(argv[llp]);
            continue;
        }

        if (Arg == DelayedRatioTag) {
            llp ++;
            if (llp >= argc) {
                cerr << DelayedRatioDsc << " parameter missed" << endl;
                return false;
            }
            theDelayRatio = NStr::StringToInt(argv[llp]);
            continue;
        }

        cerr << "Unknown parameter '" << Arg << "'" << endl;
        return false;
    }

    if (theConnection == "") {
        cerr << "Connection string parameter undefined" << endl;
        return false;
    }

    theService = theConnection;

    // cerr << "NetCache : " << theServer << ":" << thePort << endl;
    // cerr << "PerSec   : " << thePerSec << endl;
    // cerr << "DelayTm  : " << theDelayTime << endl;
    // cerr << "DelayRt  : " << theDelayRatio << endl;

    return true;
}

class CMyApp : public CNcbiApplication
{
public:
    virtual int Run() { return 0; }
};

int
main(int argc, char **argv)
{
    if (!parseArgs(argc, argv)) {
        usage();
        return 1;
    }

    s_BlobId.Set(0);
    CMyApp app;
    app.AppMain(argc, argv);

#ifdef USE_SIGNAL
    SetSigHandlers();
#endif

    cerr << "Press <Cntrl-C> to inerrupt" << endl;

    ThePotary = new StressoPotary;

    ThePotary -> Init(
                    theService,
                    thePerSec,
                    theReadRatio,
                    theDelayRatio,
                    theDelayTime
                    );
    theWQty.Set(0);

    ThePotary -> AddPotam(250);
    ThePotary -> AddPotam(2000);
    ThePotary -> AddPotam(250);
    ThePotary -> AddPotam(250);
    ThePotary -> AddPotam(4000);
    ThePotary -> AddPotam(250);
    ThePotary -> AddPotam(250);
    ThePotary -> AddPotam(8000);
    ThePotary -> AddPotam(250);
    ThePotary -> AddPotam(16000);
    ThePotary -> AddPotam(250);
    ThePotary -> AddPotam(32000);
    ThePotary -> AddPotam(250);

    ThePotary -> Stress();

    cerr << ThePotary -> Stat() << endl;

    delete ThePotary;
    ThePotary = NULL;

    return 0;
}

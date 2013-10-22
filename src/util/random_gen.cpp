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
 * Authors: Clifford Clausen, Denis Vakatov, Jim Ostell, Jonathan Kans,
 *          Greg Schuler, Eugene Vasilchenko
 * Contact: Clifford Clausen
 *
 * File Description:
 *   CRandom is a lagged Fibonacci (LFG) random number generator (RNG)
 *   with lags 33 and 13, modulus 2^31, and operation '+'. It is a slightly
 *   modified version of Nlm_Random() found in the NCBI C toolkit.
 *   It generates uniform random numbers between 0 and 2^31 - 1 (inclusive).
 *
 *   CRandom has been tested using the Diehard RNG test package
 *   developed by George Marsaglia, Prof., Dept of Statistics, Florida
 *   State University. CRandom in particular, and LFG type RNGs in general,
 *   cannot pass all of the Diehard RNG tests. Specifically, it fails the
 *   "Birthday" test as do other LFG RNGs. CRandom performs as well as
 *   other LFG RNGS, as provided in the Diehard test package. The LFG
 *   class of RNGs was chosen as the RNG for the NCBI C++ Toolkit as it
 *   provides the best tradeoff between time to generate a random number
 *   and performance on tests for randomness.
 *
 *   For a download of Diehard software and documentation
 *   see http://stat.fsu.edu/~geo/diehard.html.
 *
 *   For further information also see
 *   http://random.mat.sbg.ac.at/,
 *   http://csep1.phy.ornl.gov/rn/rn.html, and
 *   http://www.agner.org/random/.
 *
 *   Some relevant papers are:
 *   1. Hellekalek, P.: "Inversive pseudorandom number generators: concepts
 *   results, and links", In Alexopoulos, C and Kang, K, and Lilegdon, WR,
 *   and Goldsman, D, editor(s), Proceedings of the 1995 Winter Simulation
 *   Conference, pp 255-262, 1995.
 *   2. Leeb, H: "Random Numbers for Computer Simulation", Master's thesis,
 *   University of Salzburg, 1995.
 *   3. Marsaglia, G., "A Current View of Random Number Generators",
 *   Proceedings of 16th Symposium on the Interface, Atlanta, 1984, Elsevier
 *   Press.
 *   4. Marsaglia, G. "Monkey Tests for Random Number Generators", Computers
 *   & Mathematics with Applications, Vol 9, pp. 1-10, 1993.
 *
 *   For a list of other published papers, see
 *   http://random.mat.sbg.ac.at/literature and
 *   http://www.evensen.org/marsaglia/.
 *
 *   class CRandom::
 */

#include <ncbi_pch.hpp>
#include <util/random_gen.hpp>

#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/ncbidiag.hpp>

#if defined(NCBI_OS_UNIX) || defined(NCBI_OS_DARWIN)
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <string.h>
#elif defined(NCBI_OS_MSWIN)
    #include <Wincrypt.h>
#endif /* NCBI_OS */


BEGIN_NCBI_SCOPE


static const size_t kStateOffset = 12;

#if defined(NCBI_OS_UNIX) || defined(NCBI_OS_DARWIN)

    static const char * kHardwareGeneratorDevice = "/dev/hwrng";
    static const char * kSoftwareGeneratorDevice = "/dev/urandom";

    class CRandomSupplier
    {
    public:
        CRandomSupplier() :
            m_Fd(-1)
        {
            m_Fd = open(kHardwareGeneratorDevice, O_RDONLY);
            if (m_Fd == -1)
                m_Fd = open(kSoftwareGeneratorDevice, O_RDONLY);
        }
        ~CRandomSupplier()
        {
            if (m_Fd != -1) {
                close(m_Fd);
                m_Fd = -1;
            }
        }

        bool IsInitialized(void) const
        { return m_Fd != -1; }

        bool GetRand(CRandom::TValue *  value, bool  throw_on_error)
        {
            if (m_Fd == -1)
                NCBI_THROW(CRandomException, eUnavailable,
                           "System-dependent generator is not available");

            for ( ; ; ) {
                if (read(m_Fd, value, sizeof(CRandom::TValue)) ==
                                            sizeof(CRandom::TValue))
                    return true;
                if (errno != EINTR)
                    break;
            }

            if (throw_on_error)
                NCBI_THROW(CRandomException, eSysGeneratorError,
                           string("Error getting random value from the "
                                  "system-dependent generator: ") +
                           strerror(errno));
            return false;
        }

    private:
        int     m_Fd;
    };

#elif defined(NCBI_OS_MSWIN)

    class CRandomSupplier
    {
    public:
        CRandomSupplier() :
            m_Provider(0), m_Initialized(false)
        {
            static DWORD    providers[] =
                                { PROV_RSA_AES, PROV_RSA_FULL, PROV_RSA_SIG,
                                  PROV_RSA_SCHANNEL, PROV_DSS, PROV_DSS_DH,
                                  PROV_DH_SCHANNEL, PROV_FORTEZZA,
                                  PROV_MS_EXCHANGE, PROV_SSL };
            for ( size_t  k = 0; k < sizeof(providers)/sizeof(providers[0]); ++k) {
                if (CryptAcquireContext(&m_Provider, 0, 0, providers[k],
                                        CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
                    m_Initialized = true;
                    break;
                }
            }
        }
        ~CRandomSupplier()
        {
            if (m_Initialized) {
                CryptReleaseContext(m_Provider, 0);
                m_Initialized = false;
            }
        }

        bool IsInitialized(void) const
        { return m_Initialized; }

        bool GetRand(CRandom::TValue *  value, bool  throw_on_error)
        {
            if (m_Initialized) {
                if (CryptGenRandom(m_Provider, sizeof(CRandom::TValue),
                                   reinterpret_cast<PBYTE>(value)))
                    return true;
            }
            if (throw_on_error) {
                if (!m_Initialized)
                    NCBI_THROW(CRandomException, eUnavailable,
                               "System-dependent generator is not available");

                NCBI_THROW(CRandomException, eSysGeneratorError,
                        string("Error getting random value from the "
                                "system-dependent generator. Error code: ") +
                                NStr::NumericToString(GetLastError()));
            }
            return false;
        }

    private:
        HCRYPTPROV  m_Provider;
        bool        m_Initialized;
    };

#endif /* NCBI_OS */

static CSafeStatic<CRandomSupplier> s_RandomSupplier;



CRandom::CRandom(EGetRandMethod method) :
    m_RandMethod(method)
{
    if (method == eGetRand_Sys) {
        if (!s_RandomSupplier->IsInitialized())
            NCBI_THROW(CRandomException, eUnavailable,
                       "System-dependent generator is not available");
    } else
        Reset();
}


CRandom::CRandom(TValue seed) :
    m_RandMethod(eGetRand_LFG)
{
    SetSeed(seed);
}


void CRandom::Reset(void)
{
    if (m_RandMethod == eGetRand_Sys)
        NCBI_THROW(CRandomException, eUnexpectedRandMethod,
                   "CRandom::Reset() is not allowed for "
                   "system-dependent generator");

    // Static array used to initialize "m_State"
    static const CRandom::TValue sm_State[CRandom::kStateSize] = {
        0xd53f1852,  0xdfc78b83,  0x4f256096,  0xe643df7,
        0x82c359bf,  0xc7794dfa,  0xd5e9ffaa,  0x2c8cb64a,
        0x2f07b334,  0xad5a7eb5,  0x96dc0cde,  0x6fc24589,
        0xa5853646,  0xe71576e2,  0xdae30df,   0xb09ce711,
        0x5e56ef87,  0x4b4b0082,  0x6f4f340e,  0xc5bb17e8,
        0xd788d765,  0x67498087,  0x9d7aba26,  0x261351d4,
        0x411ee7ea,  0x393a263,   0x2c5a5835,  0xc115fcd8,
        0x25e9132c,  0xd0c6e906,  0xc2bc5b2d,  0x6c065c98,
        0x6e37bd55
    };


    _ASSERT(sizeof(sm_State) == sizeof(m_State));
    _ASSERT(kStateOffset < kStateSize);

    for (int i = 0;  i < kStateSize;  ++i) {
        m_State[i] = sm_State[i];
    }

    m_RJ = kStateOffset;
    m_RK = kStateSize - 1;
}


void CRandom::Randomize(void)
{
    if (m_RandMethod == eGetRand_Sys)
        return;

    TValue  seed;
    if (s_RandomSupplier->GetRand(&seed, false)) {
        SetSeed(seed);
        return;
    }

    CTime now(CTime::eCurrent);

    SetSeed(TValue(now.Second() ^
                   now.NanoSecond() ^
                   CProcess::GetCurrentPid() * 19 ^
                   CThread::GetSelf() * 5));
}


void CRandom::SetSeed(TValue seed)
{
    if (m_RandMethod == eGetRand_Sys)
        NCBI_THROW(CRandomException, eUnexpectedRandMethod,
                   "CRandom::SetSeed(...) is not allowed for "
                   "system-dependent generator");

    _ASSERT(kStateOffset < kStateSize);

    m_State[0] = m_Seed = seed;

    // linear congruential initializer
    for (int i = 1;  i < kStateSize;  ++i) {
        m_State[i] = 1103515245 * m_State[i-1] + 12345;
    }

    m_RJ = kStateOffset;
    m_RK = kStateSize - 1;

    for (int i = 0;  i < 10 * kStateSize;  ++i) {
        GetRand();
    }
}


CRandom::TValue CRandom::GetSeed(void) const
{
    if (m_RandMethod == eGetRand_Sys)
        NCBI_THROW(CRandomException, eUnexpectedRandMethod,
                   "CRandom::GetSeed(...) is not allowed for "
                   "system-dependent generator");

    return m_Seed;
}


CRandom::TValue CRandom::x_GetSysRand32Bits(void) const
{
    TValue  r;
    s_RandomSupplier->GetRand(&r, true);
    return r;
}


END_NCBI_SCOPE

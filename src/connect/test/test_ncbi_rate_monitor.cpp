/* $Id$
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
 * Author:  Craig Wallin, Anton Lavrentiev
 *
 * File Description:
 *   CRateMonitor test
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_process.hpp>
#include <connect/ncbi_misc.hpp>
#include <stdlib.h>


int main(int argc, char* argv[])
{
    USING_NCBI_SCOPE;

    int seed;

    if (argc > 1) {
        seed = NStr::StringToInt(argv[1]);
    } else {
        seed = ((int) CTime(CTime::eCurrent).GetTimeT() ^
                (int) CProcess::GetCurrentPid());
    }
    srand(seed);

    const int    kIterationCount =  rand() % 1000 + 1;
    const double kMaxSpan        =  rand() % 100  + 1;
    const double kMinSpan        = (rand() % 100  + 1)/ 100.0;
    const double kDelta          =  kMinSpan *
        (rand() & 1 ? 1.0 / (rand() % 10 + 1) : (rand() % 5 + 1));

    cout << "Random seed: " << seed            << endl
         << "Iterations:  " << kIterationCount << endl
         << "MaxSpan:     " << kMaxSpan        << endl
         << "MinSpan:     " << kMinSpan        << endl
         << "Delta:       " << kDelta          << endl;

    CRateMonitor mon(kMinSpan, kMaxSpan);
    mon.SetSize(kIterationCount);

    double time = 0.0;
    int    done = 0;

    do {
        mon.Mark( done, time );

        cout << "---"                              << endl
             << "size: " << mon.GetSize()          << endl
             << "pos:  " << mon.GetPos()           << endl
             << "time: " << mon.GetTime()          << endl
             << "rate: " << mon.GetRate()          << endl
             << "pace: " << mon.GetPace()          << endl
             << "eta:  " << mon.GetETA()           << endl
             << "rem:  " << mon.GetTimeRemaining() << endl;

        time += kDelta;
        done += rand() & 1;
    } while (done <= kIterationCount);

    cout << endl << "Random seed: " << seed << endl;

    return 0;
}

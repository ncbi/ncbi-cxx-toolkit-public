/* Copyright 2011 JetBrains s.r.o.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * $Id$
*/

#ifndef H_TEAMCITY_MESSAGES
#define H_TEAMCITY_MESSAGES

#include <iostream>
#include <string>

namespace jetbrains {
namespace teamcity {

std::string getFlowIdFromEnvironment();
bool underTeamcity();

class TeamcityMessages {
    std::ostream *m_out;

protected:
    std::string escape(std::string s);

    void openMsg(const std::string &name);
    void writeProperty(std::string name, std::string value);
    void closeMsg();

public:
    static const bool StdErr = true;
    static const bool StdOut = false;

    TeamcityMessages();

    void setOutput(std::ostream &);

    void suiteStarted(std::string name, std::string flowid =  std::string());
    void suiteFinished(std::string name, std::string flowid =  std::string());

    void testStarted(std::string name, std::string flowid =  std::string(), bool captureStandardOutput = false);
    void testFailed(std::string name, std::string message, std::string details, std::string flowid =  std::string());
    void testIgnored(std::string name, std::string message, std::string flowid =  std::string());
    void testOutput(std::string name, std::string output, std::string flowid, bool isStdErr = StdOut);
    void testFinished(std::string name, int durationMs = -1, std::string flowid = std::string());
};

}
}

#endif /* H_TEAMCITY_MESSAGES */

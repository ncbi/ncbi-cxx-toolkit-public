#include <iostream>
#include <string>
#include <sstream>

using namespace std;

class CRequest {
public:
    enum EProperties {
        // server properties
        eProp_ServerSoftware = 0,
        eProp_ServerName,
        eProp_GatewayInterface,
        eProp_ServerProtocol,
        eProp_ServerPort,

        // client properties
        eProp_RemoteHost,
        eProp_RemoteAddr,

        // client data properties
        eProp_ContentType,
        eProp_ContentLength,

        // request properties
        eProp_RequestMethod,  // see "m_GetMethod()"
        eProp_PathInfo,
        eProp_PathTranslated,
        eProp_ScriptName,
        eProp_QueryString,

        // authentication info
        eProp_AuthType,
        eProp_RemoteUser,
        eProp_RemoteIdent
    };
    string m_GetProperty(EProperty property);


    // random client properties("HTTP_XXXXX")
    string m_GetRandomProperty(string property_key);

    enum EMethod {
        eMethod_Get,
        eMethod_Post
    };
    EMethod m_GetMethod(void);


private:

};


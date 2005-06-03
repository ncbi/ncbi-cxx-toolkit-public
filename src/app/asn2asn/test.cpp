#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <serial/strbuffer.hpp>

USING_NCBI_SCOPE;

class CTest : public CNcbiApplication
{
public:
    int Run(void);
};

int main(int argc, char** argv)
{
    return CTest().AppMain(argc, argv);
}

inline
bool FirstIdChar(char c)
{
    return isalpha((unsigned char) c) || c == '_';
}

inline
bool IdChar(char c)
{
    return isalnum((unsigned char) c) || c == '_' || c == '.';
}

inline
void SkipEndOfLine(CStreamBuffer& b, char lastChar)
{
    b.SkipEndOfLine(lastChar);
}

void SkipComments(CStreamBuffer& b)
{
    ERR_POST("SkipComments @ " << b.GetLine());
    try {
        for ( ;; ) {
            char c = b.GetChar();
            switch ( c ) {
            case '\r':
            case '\n':
                SkipEndOfLine(b, c);
                ERR_POST("-SkipComments @ " << b.GetLine());
                return;
            case '-':
                c = b.GetChar();
                switch ( c ) {
                case '\r':
                case '\n':
                    SkipEndOfLine(b, c);
                    ERR_POST("-SkipComments @ " << b.GetLine());
                    return;
                case '-':
                    ERR_POST("-SkipComments @ " << b.GetLine());
                    return;
                }
                continue;
            default:
                continue;
            }
        }
    }
    catch ( CSerialEofException& /* ignored */ ) {
        ERR_POST("-SkipComments @ " << b.GetLine());
        return;
    }
}

#if 1
#undef ERR_POST
#define ERR_POST(m)
#endif

char SkipWhiteSpace(CStreamBuffer& b)
{
    ERR_POST("SkipWhiteSpaceAndGetChar @ " << b.GetLine());
    for ( ;; ) {
		char c = b.SkipSpaces();
        switch ( c ) {
        case '\t':
            b.SkipChar();
            continue;
        case '\r':
        case '\n':
            b.SkipChar();
            SkipEndOfLine(b, c);
            continue;
        case '-':
            // check for comments
            if ( b.PeekChar(1) != '-' ) {
                ERR_POST("-SkipWhiteSpaceAndGetChar @ " << b.GetLine());
                return '-';
            }
            b.SkipChars(2);
            // skip comments
            SkipComments(b);
            continue;
        default:
            ERR_POST("-SkipWhiteSpaceAndGetChar @ " << b.GetLine());
            return c;
        }
    }
}

inline
char SkipWhiteSpaceAndGetChar(CStreamBuffer& b)
{
    char c = SkipWhiteSpace(b);
    b.SkipChar();
    return c;
}

inline
void UngetNonWhiteSpace(CStreamBuffer& b, char c)
{
    switch ( c ) {
    case ' ':
    case '\t':
        break;
    case '\r':
    case '\n':
        SkipEndOfLine(b, c);
        break;
    default:
        b.UngetChar();
        break;
    }
}

size_t ReadId(CStreamBuffer& b)
{
    ERR_POST("ReadId @ " << b.GetLine());
    char c = SkipWhiteSpace(b);
    if ( c == '[' ) {
        for ( size_t i = 1; ; ++i ) {
            switch ( b.PeekChar(i) ) {
            case '\r':
            case '\n':
                b.UngetChar();
                THROW1_TRACE(runtime_error, "end of line: expected ']'");
                break;
            case ']':
                b.SkipChars(i + 1);
                ERR_POST("-ReadId @ " << b.GetLine());
                return i;
            }
        }
    }
	else {
        if ( !FirstIdChar(c) ) {
            ERR_POST("-ReadId @ " << b.GetLine());
            return 0;
        }
        else {
            for ( size_t i = 1; ; ++i ) {
                c = b.PeekChar(i);
                if ( !IdChar(c) &&
                     (c != '-' || !IdChar(b.PeekChar(i + 1))) ) {
                    b.SkipChars(i);
                    ERR_POST("-ReadId @ " << b.GetLine());
                    return i;
                }
            }
        }
	}
}

void ReadNumber(CStreamBuffer& b)
{
    char c = SkipWhiteSpaceAndGetChar(b);
    switch ( c ) {
    case '-':
        c = b.GetChar();
        break;
    case '+':
        c = b.GetChar();
        break;
    }
    if ( c < '0' || c > '9' ) {
        b.UngetChar();
        THROW1_TRACE(runtime_error, "bad number start");
    }
    for (;;) {
        c = b.PeekChar();
        if ( c < '0' || c > '9' ) {
            return;
        }
        b.SkipChar();
    }
}

void ReadString(CStreamBuffer& b)
{
    ERR_POST("ReadStrign @ " << b.GetLine());
    string s;
    s.erase();
    for (;;) {
        char c = b.GetChar();
        switch ( c ) {
        case '\r':
        case '\n':
            SkipEndOfLine(b, c);
            ERR_POST("line: " << b.GetLine());
            continue;
        case '\"':
            if ( b.PeekChar() == '\"' ) {
                // double quote -> one quote
                b.SkipChar();
                s += '\"';
            }
            else {
                // end of string
                ERR_POST("-ReadStrign @ " << b.GetLine());
                return;
            }
            continue;
        default:
            if ( c < ' ' && c >= 0 ) {
                b.UngetChar();
                THROW1_TRACE(runtime_error,
                             "bad char in string: " + NStr::IntToString(c));
            }
            else {
                s += c;
            }
            continue;
        }
    }
}

int GetHexChar(CStreamBuffer& b)
{
    for ( ;; ) {
        char c = b.GetChar();
        if ( c >= '0' && c <= '9' ) {
            return c - '0';
        }
        else if ( c >= 'A' && c <= 'Z' ) {
            return c - 'A' + 10;
        }
        else if ( c >= 'a' && c <= 'z' ) {
            return c - 'a' + 10;
        }
        switch ( c ) {
        case '\'':
            return -1;
        case '\r':
        case '\n':
            SkipEndOfLine(b, c);
            ERR_POST("line: " << b.GetLine());
            break;
        default:
            b.UngetChar();
            THROW1_TRACE(runtime_error,
                         string("bad char in octet string: '") + c + "'");
        }
    }
}

pair<size_t, bool> ReadBytes(CStreamBuffer& b, char* dst, size_t length)
{
	size_t count = 0;
	while ( length-- > 0 ) {
        int c1 = GetHexChar(b);
        if ( c1 < 0 ) {
            return make_pair(count, true);
        }
        int c2 = GetHexChar(b);
        if ( c2 < 0 ) {
            *dst++ = c1 << 4;
            count++;
            return make_pair(count, true);
        }
        else {
            *dst++ = (c1 << 4) | c2;
            count++;
        }
	}
	return make_pair(count, false);
}

void ReadOctetString(CStreamBuffer& b)
{
    ERR_POST("ReadOctetString @ " << b.GetLine());
    char buffer[1024];
    while ( !ReadBytes(b, buffer, sizeof(buffer)).second ) {
    }
    if ( SkipWhiteSpaceAndGetChar(b) != 'H' ) {
        b.UngetChar();
        THROW1_TRACE(runtime_error, "no tailing 'H' in octet string");
    }
    ERR_POST("-ReadOctentStrign @ " << b.GetLine());
}

void ReadBlock(CStreamBuffer& b);
void ReadValue(CStreamBuffer& b);

void ReadValue(CStreamBuffer& b)
{
    ERR_POST("ReadValue @ " << b.GetLine());
    char c = SkipWhiteSpace(b);
    switch ( c ) {
    case '{':
        b.SkipChar();
        ReadBlock(b);
        break;
    case '-':
        ReadNumber(b);
        break;
    case '[':
        ReadId(b);
        break;
    case ',':
    case '}':
        return;
    case '\'':
        b.SkipChar();
        ReadOctetString(b);
        break;
    case '"':
        b.SkipChar();
        ReadString(b);
        break;
    default:
        if ( c >= '0' && c <= '9' ) {
            ReadNumber(b);
        }
        else if ( c >= 'a' && c <= 'z' ) {
            ReadId(b);
            ReadValue(b);
        }
        else if ( c >= 'A' && c <= 'Z' ) {
            ReadId(b);
        }
        break;
    }
    ERR_POST("-ReadValue @ " << b.GetLine());
}

void ReadBlock(CStreamBuffer& b)
{
    ERR_POST("ReadBlock @ " << b.GetLine());
    if ( SkipWhiteSpace(b) == '}' ) {
        b.SkipChar();
        ERR_POST("-ReadBlock @ " << b.GetLine());
        return;
    }
    for (;;) {
        ReadValue(b);
        switch ( SkipWhiteSpaceAndGetChar(b) ) {
        case ',':
            break;
        case '}':
            ERR_POST("-ReadBlock @ " << b.GetLine());
            return;
        default:
            b.UngetChar();
            THROW1_TRACE(runtime_error,
                         string("invalid block char: ") + b.PeekChar());
        }
    }
}

int CTest::Run(void)
{
    CNcbiIstream* in = &NcbiCin;
    enum EType {
        eByteType,
        eCharType,
        eWhiteSpaceType,
        eFullParseType
    } type = eFullParseType;
    for ( size_t i = 1; i < GetArguments().Size(); ++i ) {
        if ( GetArguments()[i] == "-f" )
            type = eFullParseType;
        else if ( GetArguments()[i] == "-w" )
            type = eWhiteSpaceType;
        else if ( GetArguments()[i] == "-c" )
            type = eCharType;
        else if ( GetArguments()[i] == "-b" )
            type = eByteType;
        else {
            CNcbiIstream* fin = new CNcbiIfstream(GetArguments()[i].c_str());
            in = fin;
        }
    }
    CStreamBuffer b(*in);
    try {
        switch ( type ) {
        case eByteType:
            {
                size_t count = 0;
                char buffer[8192];
                for (;;) {
                    in->read(buffer, sizeof(buffer));
                    size_t c = in->gcount();
                    if ( c == 0 )
                        break;
                    count += c;
                }
                NcbiCout << count << " chars" << NcbiEndl;
            }
            break;
        case eCharType:
            {
                size_t count = 0;
                try {
                    for (;;) {
                        b.GetChar();
                        ++count;
                    }
                }
                catch ( CSerialEofException& /*exc*/ ) {
                }
                NcbiCout << count << " chars" << NcbiEndl;
            }
            break;
        case eWhiteSpaceType:
            try {
                for (;;) {
                    SkipWhiteSpaceAndGetChar(b);
                }
            }
            catch ( CSerialEofException& /*exc*/ ) {
            }
            break;
        case eFullParseType:
            ReadId(b);
            if ( SkipWhiteSpaceAndGetChar(b) != ':' ||
                 b.GetChar() != ':' || b.GetChar() != '=' ) {
                b.UngetChar();
                THROW1_TRACE(runtime_error, "\"::=\" expected");
            }
            ReadValue(b);
            break;
        }
    }
    catch (exception& exc) {
        NcbiCerr << exc.what() << NcbiEndl;
    }
    NcbiCout << b.GetLine() << " lines" << NcbiEndl;
    return 0;
}

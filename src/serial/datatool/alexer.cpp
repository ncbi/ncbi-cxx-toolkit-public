#include <alexer.hpp>
#include <atoken.hpp>

USING_NCBI_SCOPE;

AbstractLexer::AbstractLexer(CNcbiIstream& in)
    : m_Input(in), m_Line(1), m_Position(0), m_TokenStart(-1)
{
}

AbstractLexer::~AbstractLexer(void)
{
}

void AbstractLexer::LexerError(const char* error)
{
    throw runtime_error(NStr::IntToString(m_Line) + ": lexer error: " + error);
}

void AbstractLexer::LexerWarning(const char* error)
{
    NcbiCerr << m_Line << ": lexer error: " << error << endl;
}

const AbstractToken& AbstractLexer::FillNextToken(void)
{
    if ( (m_NextToken.token = LookupToken()) == T_SYMBOL ) {
        StartToken();
        if ( m_Position == m_Buffer.size() ) {
            m_NextToken.token = T_EOF;
        }
        else if ( CurrentTokenLength() == 0 ) {
            AddChar();
        }
    }
    m_NextToken.start = &m_Buffer[m_TokenStart];
    m_NextToken.end = &m_Buffer[m_Position];
    return m_NextToken;
}

#define READ_EHEAD 1024

char AbstractLexer::FillChar(int index)
{
    int pos = m_Position + index;
    while ( pos >= m_Buffer.capacity() ) {
        // remove unused
        int unused = m_Position;
        if ( m_TokenStart >= 0 && m_TokenStart < unused )
            unused = m_TokenStart;
        if ( unused > 0 ) {
            m_Buffer.erase(m_Buffer.begin(), m_Buffer.begin() + unused);
            m_Position -= unused;
            pos -= unused;
            m_TokenStart -= unused;
        }
        else {
            m_Buffer.reserve(pos + 1 + READ_EHEAD);
            // reserved -> exit loop, no more checks needed
            break;
        }
    }
    while ( pos >= m_Buffer.size() ) {
        int fillpos = m_Buffer.size();
        int space = m_Buffer.capacity() - fillpos;
        m_Buffer.resize(m_Buffer.capacity());
        m_Input.read(&m_Buffer[fillpos], space);
        int read = m_Input.gcount();
        m_Buffer.resize(fillpos + read);
        if ( read == 0 )
            return 0;
    }
    return m_Buffer[pos];
}

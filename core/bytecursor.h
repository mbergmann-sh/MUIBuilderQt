// Small helper used only by MuibLoader to replicate the exact byte-level
// reading semantics of the original C loader (load.c's LitChaine/ReadInt/
// NextLine, all built on fgets/fscanf/fgetc against a FILE*).
//
// A hand-rolled cursor over the whole file's bytes (rather than
// QTextStream/QFile::readLine) is used deliberately: the .MUIB format
// mixes line-based text fields with raw fixed-length byte blobs (help
// text, read via fgetc in a tight loop with no line semantics at all -
// see ReadHelp in the original), and QTextStream's line-ending/encoding
// normalization is exactly the kind of "helpful" behavior that would
// silently corrupt that mix. Working from a plain QByteArray with an
// explicit offset reproduces the original's behavior byte-for-byte,
// deliberately including its quirks (e.g. a trailing '\r' before '\n'
// on a line is not stripped, matching the original's strchr(aux,'\n')
// which never touches '\r' either).
#pragma once

#include <QByteArray>
#include <QString>

class ByteCursor
{
public:
    explicit ByteCursor(const QByteArray &data) : m_data(data) {}

    bool atEnd() const { return m_pos >= m_data.size(); }

    // Ports LitChaine(): read up to (and consuming) the next '\n',
    // returning everything before it. If no '\n' is found before EOF,
    // returns whatever remains (the original leaves aux[0]='\0' in
    // that case - i.e. an empty string - which we replicate).
    QString readLine()
    {
        int nl = m_data.indexOf('\n', m_pos);
        QString result;
        if (nl < 0)
        {
            m_pos = m_data.size();
            return QString();   // matches original's aux[0] = '\0' on missing '\n'
        }
        result = QString::fromLatin1(m_data.mid(m_pos, nl - m_pos));
        m_pos = nl + 1;
        return result;
    }

    // Ports ReadInt(): fscanf("%d") semantics - skip leading whitespace
    // (spaces/tabs/newlines), parse an optionally-signed integer, then
    // discard the remainder of the current line exactly like the
    // original's NextLine(). Returns -1 on EOF/parse failure, matching
    // "if (fscanf(...) == EOF) i = -1;".
    int readInt()
    {
        // Skip leading whitespace (fscanf %d skips isspace()).
        while (m_pos < m_data.size() && isSpaceByte(m_data.at(m_pos)))
            ++m_pos;

        if (m_pos >= m_data.size())
        {
            return -1;
        }

        int start = m_pos;
        if (m_pos < m_data.size() && (m_data.at(m_pos) == '-' || m_data.at(m_pos) == '+'))
            ++m_pos;
        int digitsStart = m_pos;
        while (m_pos < m_data.size() && m_data.at(m_pos) >= '0' && m_data.at(m_pos) <= '9')
            ++m_pos;

        bool ok = (m_pos > digitsStart);
        int value = -1;
        if (ok)
            value = QByteArray(m_data.constData() + start, m_pos - start).toInt();

        // NextLine(): discard through (and including) the next '\n'.
        int nl = m_data.indexOf('\n', m_pos);
        m_pos = (nl < 0) ? m_data.size() : nl + 1;

        return ok ? value : -1;
    }

    // Ports the raw fgetc() loop in ReadHelp()/LoadFile() for help
    // text content: read exactly n bytes with no line semantics at all.
    QByteArray readRawBytes(int n)
    {
        if (n <= 0)
            return QByteArray();
        QByteArray result = m_data.mid(m_pos, n);
        m_pos += n;
        return result;
    }

private:
    static bool isSpaceByte(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
    }

    QByteArray m_data;
    int m_pos = 0;
};

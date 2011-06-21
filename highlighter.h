#ifndef HIGHLIGHTER_H
#define HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class QTextDocument;

class QueryHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    static QueryHighlighter& instance();

protected:
    QueryHighlighter(QTextDocument *parent = 0);
    void highlightBlock(const QString &text);

    QTextCharFormat m_queryFormat;
};

#endif

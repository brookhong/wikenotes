#include <QtGui>

#include "highlighter.h"
#include "mainwindow.h"

QueryHighlighter& QueryHighlighter::instance()
{
    static QueryHighlighter inst;
    return inst;
}
QueryHighlighter::QueryHighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent)
{
    m_queryFormat.setForeground(Qt::white);
    m_queryFormat.setBackground(Qt::blue);
}
void QueryHighlighter::highlightBlock(const QString &text)
{
    QRegExp queryExpression(MainWindow::s_query, Qt::CaseInsensitive);
    int index = text.indexOf(queryExpression);
    while (index >= 0) {
        int length = queryExpression.matchedLength();
        setFormat(index, length, m_queryFormat);
        index = text.indexOf(queryExpression, index + length);
    }
}

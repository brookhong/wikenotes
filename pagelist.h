#ifndef PAGELIST_H
#define PAGELIST_H

#include <QtGui>
class PageList : public QComboBox
{
    public:
        PageList( QWidget* parent = 0 ) : QComboBox(parent) {}
        int m_pageNum;
    protected:
        void focusInEvent(QFocusEvent * e);
};
#endif

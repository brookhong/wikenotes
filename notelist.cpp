#include "notelist.h"
#include "noteitem.h"
#include "mainwindow.h"
const int UNIT_PADDING = 240;
NoteList::NoteList( QWidget* parent ) : QWidget(parent)
{
    setStyleSheet(".NoteList { background-color : white;}");
}
QSize NoteList::sizeHint() const
{
    int h = m_notes.size()*UNIT_PADDING+m_heightDelta;
    if(h == 0)
        h = 100;
    int w = width();
    return QSize(w, h);
}
void NoteList::extend(int num)
{
    for(int i=0;i<num;++i)
        m_notes.append(0);
}
bool NoteList::createNote(int idx)
{
    QSqlQuery *q = g_mainWindow->getFoundNote(idx);

    bool ret = false;
    if(q->first()) {
        NoteItem* noteItem;
        noteItem = new NoteItem(this,q->value(0).toInt(),true);
        noteItem->autoSize();
        m_notes[idx] = noteItem;
        int delta = noteItem->height() - UNIT_PADDING;
        if(delta) {
            m_heightDelta += delta;
            adjustSize();
        }
        ret = true;
    }
    return ret;
}
void NoteList::fitSize(int w, int h)
{
    m_viewPortHeight = h;
    int rh = h;
    if(m_notes.size() == 1) {
        if(m_notes[0] == 0)
            createNote(0);
        m_notes[0]->resize(w,h);
    }
    else
        rh = size().height();
    resize(w, rh);
}
void NoteList::setTextNoteFont(const QFont& font)
{
    int i, n = m_notes.size();
    for(i=0;i<n;i++) {
        if(m_notes[i]) {
            m_notes[i]->setFont(font);
        }
    }
}
void NoteList::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), QBrush(Qt::white));

    QRect redrawRect = event->rect();
    int top = redrawRect.top();
    int btm = redrawRect.bottom();
    int i, n = m_notes.size();
    int h = 0;
    int beginRow = -1, endRow = -1;
    int beginTop = 0;
    int unitHeight;
    for(i=0;i<n;++i) {
        unitHeight = (m_notes[i] == 0)? UNIT_PADDING : m_notes[i]->height();
        h += unitHeight;
        if(beginRow == -1 && h >= top) {
            beginRow = i;
            beginTop = h-unitHeight;
        }
        if(endRow == -1 && h >= btm) {
            endRow = i+1;
            break;
        }
    }
    if(endRow == -1 || endRow >= n)
        endRow = n-1;
    if(beginRow>=0 && endRow-beginRow+1 > 0) {
        for (i = beginRow; i <= endRow; ++i) {
            if(m_notes[i] == 0)
                createNote(i);
            if(m_notes[i]) {
                m_notes[i]->setGeometry(QRect(16, beginTop, width()-16, m_notes[i]->height()));
                beginTop += m_notes[i]->height();
            }
        }
    }
    event->accept();
}
void NoteList::addNote(NoteItem* item)
{
    m_notes.append(item);
}
NoteItem* NoteList::getNextNote(NoteItem* item)
{
    NoteItem* ret = 0;
    int i, n = m_notes.size();
    for(i=0;i<n;i++) {
        if(m_notes[i] == item) {
            break;
        }
    }
    if(i<n-1)
        ret = m_notes[i+1];
    if(!ret && i>0)
        ret = m_notes[i-1];
    return ret;
}
void NoteList::removeNote(NoteItem* item)
{
    m_heightDelta -= item->height()-UNIT_PADDING;
    m_notes.removeOne(item);
    delete item;
    adjustSize();
}
void NoteList::clear()
{
    m_heightDelta = 0;
    NoteItem::setActiveItem(0);
    int i = 0, n = m_notes.size();
    NoteItem* item;
    n -= m_notes.removeAll(0);
    while(i++<n) {
        item = m_notes.takeFirst();
        delete item;
    }
}

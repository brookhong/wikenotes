#ifndef NOTELIST_H
#define NOTELIST_H

#include <QtGui>
#include "noteitem.h"

class NoteList : public QWidget
{
    Q_OBJECT

    public:
        NoteList( QWidget* parent = 0 );
        QSize sizeHint() const;
        void extend(int num);
        void addNote(NoteItem* item);
        void removeNote(NoteItem* item);
        void clear();
        NoteItem* getNextNote(NoteItem* item, int dir);
        void fitSize(int w, int h);
        void setTextNoteFont(const QFont& font);

    protected:
        void paintEvent(QPaintEvent *event);
        QList<NoteItem*> m_notes;

        bool createNote(int idx);
        int m_heightDelta;
        int m_viewPortHeight;
};

#endif


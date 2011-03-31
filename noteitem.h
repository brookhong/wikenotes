#ifndef NOTEITEM_H
#define NOTEITEM_H

#include <QtGUI>
#include <QtSQL>
#include <QtNetwork>
class ContentWidget : public QTextBrowser
{
    Q_OBJECT
    public:
        ContentWidget(QWidget* parent = 0);
        bool canInsertFromMimeData(const QMimeData* source) const;
        void insertFromMimeData(const QMimeData* source);
        void setContent(const QString& str);
        QString getContent();
        void toggleView();
        bool isHTMLView();

    private slots:
        void Finished(int requestId, bool error);
    private:
        void dropImage(const QImage& image);
        void dropTextFile(const QUrl& url, const QString& ext);
        void addImageUrl(const QString& str);
        QByteArray m_bytes;
        QBuffer *m_buffer;
        QHttp *m_http;
        int m_req;

        int m_viewType; //0: plaintext; 1: html
        QString m_htmlCode;
        QStringList m_urls;
};

class NoteItem : public QFrame
{
    Q_OBJECT
    public:
        NoteItem(QWidget *parent = 0, int row = 0, bool readOnly = true);
        int getNoteId() const;
        void setFont(const QFont& font);
        static NoteItem* getActiveItem();
        static void setActiveItem(NoteItem* item);
        void autoSize();
        void setReadOnly(bool readOnly);
        bool isReadOnly();
        bool saveNote();
        bool close();
        void toggleView();
        void cancelEdit();

    private:
        bool eventFilter(QObject *obj, QEvent *ev);
        bool shortCut(int k);
        static NoteItem* s_activeNote;
        QSqlQuery* m_q;
        int m_noteId;
        int m_widgetState; //0: uninitialized; 1: readOny; 2: editable
        bool m_sized;
        int m_totalLine;

        QVBoxLayout * m_verticalLayout;
        QLabel *m_title;
        QLineEdit *m_titleEdit;
        QLineEdit *m_tagEdit;
        QWidget* m_titleWidget;
        ContentWidget* m_contentWidget;

        void active();
        void populate();
        void loadResource();
        void exportFile();
};

#endif

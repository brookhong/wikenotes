#include "noteitem.h"
#include "highlighter.h"
#include "mainwindow.h"
RendererReply::RendererReply(QObject* object, const QNetworkRequest& request)
    : QNetworkReply(object)
      , m_position(0)
{
    setRequest(request);
    setOperation(QNetworkAccessManager::GetOperation);
    setHeader(QNetworkRequest::ContentTypeHeader,QVariant("image/png"));
    open(ReadOnly|Unbuffered);
    setUrl(request.url());

    QString fileName = request.url().host();
    g_mainWindow->loadImageFromDB(fileName, m_buffer);
    setHeader(QNetworkRequest::ContentTypeHeader, "image/png");
    m_position = 0;
    QTimer::singleShot(0, this, SIGNAL(readyRead()));
    QTimer::singleShot(0, this, SIGNAL(finished()));
}
qint64 RendererReply::readData(char* data, qint64 maxSize)
{
    const qint64 readSize = qMin(maxSize, (qint64)(m_buffer.size() - m_position));
    memcpy(data, m_buffer.constData() + m_position, readSize);
    m_position += readSize;
    return readSize;
}

qint64 RendererReply::bytesAvailable() const
{
    return m_buffer.size() - m_position;
}

qint64 RendererReply::pos () const
{
    return m_position;
}

bool RendererReply::seek( qint64 pos )
{
    if (pos < 0 || pos >= m_buffer.size())
        return false;
    m_position = pos;
    return true;
}

qint64 RendererReply::size () const
{
    return m_buffer.size();
}

void RendererReply::abort()
{
}

class UrlBasedRenderer : public QNetworkAccessManager
{
public:
    UrlBasedRenderer(QObject* parent = 0) : QNetworkAccessManager(parent)
    {
    }

    virtual QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData)
    {
        if (request.url().scheme() != "wike")
            return QNetworkAccessManager::createRequest(op, request, outgoingData);
        return new RendererReply(this, request);
    }
};
QString LocalFileDialog::selectFiles(const QString& filters) {
    QFileDialog fileDialog(static_cast<QWidget*>(parent()));
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    QStringList flters = filters.split(",");
    fileDialog.setNameFilters(flters);
    if(QDialog::Accepted == fileDialog.exec()) {
        return fileDialog.selectedFiles().join(",");
    }
    return "";
}
PlainTextBrowser::PlainTextBrowser(QWidget * parent) : QPlainTextEdit(parent)
{
    m_contextMenu = new QMenu(tr("External Commands"), this);
    const QList<QAction*>& lst = g_mainWindow->getExtActions();
    for (int i = 0; i < lst.size(); ++i) {
        m_contextMenu->addAction(lst[i]);
        addAction(lst[i]);
    }
}
void PlainTextBrowser::contextMenuEvent(QContextMenuEvent * event)
{
    QMenu* menu = createStandardContextMenu();
    menu->addMenu(m_contextMenu);
    menu->exec(event->globalPos());
    delete menu;
}
PlainTextBrowser::~PlainTextBrowser()
{
    delete m_contextMenu;
}
TextBrowser::TextBrowser(QWidget * parent) : QTextBrowser(parent)
{
    m_contextMenu = new QMenu(tr("External Commands"), this);
    const QList<QAction*>& lst = g_mainWindow->getExtActions();
    for (int i = 0; i < lst.size(); ++i) {
        m_contextMenu->addAction(lst[i]);
        addAction(lst[i]);
    }
}
void TextBrowser::contextMenuEvent(QContextMenuEvent * event)
{
    QMenu* menu = createStandardContextMenu();
    menu->addMenu(m_contextMenu);
    menu->exec(event->globalPos());
    delete menu;
}
TextBrowser::~TextBrowser()
{
    delete m_contextMenu;
}
NoteItem* NoteItem::s_activeNote = 0;
NoteItem::NoteItem(QWidget *parent, int row, bool readOnly, bool rich) :
    QFrame(parent)
{
    m_noteId = row;
    m_readOnly = readOnly;
    m_rich = rich;
    m_sized = false;
    m_q = g_mainWindow->getSqlQuery();
    installEventFilter(this);

    m_verticalLayout = new QVBoxLayout(this);
    m_verticalLayout->setSpacing(0);
    m_verticalLayout->setContentsMargins(0, 0, 0, 0);
    setStyleSheet(".NoteItem { background-color : white; padding: 6px; }");

    initControls();
    resize(parentWidget()->width() - 32, 240);
}
void NoteItem::initControls()
{
    QString rowId, title = tr("Untitled"), tag = tr("Untagged"), created;
    if(m_noteId > 0) {
        m_q->Sql(QString("select rowid,title,content,tag,created from notes left join notes_attr on notes.rowid=notes_attr.rowid where rowid=%1").arg(m_noteId).toUtf8());
        m_q->FetchRow();
        rowId = QString::fromUtf8((char*)m_q->GetColumnCString(0));
        title = QString::fromUtf8((char*)m_q->GetColumnCString(1));
        m_content = QString::fromUtf8((char*)m_q->GetColumnCString(2));
        tag = QString::fromUtf8((char*)m_q->GetColumnCString(3));
        created = QString::fromUtf8((char*)m_q->GetColumnCString(4));
        m_rich = Qt::mightBeRichText(m_content);
        m_totalLine = m_content.count('\n');;
    }

    if(m_readOnly) {
        m_title = new QLabel(this);
        m_title->setObjectName(QString::fromUtf8("title"));
        m_title->setWordWrap(true);
        m_title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_title->setText("<h2>"+Qt::escape(title)+"</h2><table width='100%'><tr><td align='left' width='40%'>"+tag+"</td><td align='center' width='20%'>"+rowId+"</td><td align='right' width='40%'>"+created+"</td></tr></table>");


        QWidget* contentWidget;
        if(m_rich) {
            TextBrowser* textBrowser = new TextBrowser(this);
            textBrowser->setOpenExternalLinks(true);
            textBrowser->setHtml(m_content);
            textBrowser->setStyleSheet("#contentWidget { background:url(:/html.png)}");
            if(MainWindow::s_query.length())
                textBrowser->find(MainWindow::s_query);

            contentWidget = textBrowser;
        }
        else {
            PlainTextBrowser* plainTextEdit = new PlainTextBrowser(this);
            plainTextEdit->setPlainText(m_content);
            plainTextEdit->setReadOnly(m_readOnly);
            plainTextEdit->setStyleSheet("#contentWidget { background:url(:/text.png)}");
            if(MainWindow::s_query.length())
                plainTextEdit->find(MainWindow::s_query);

            contentWidget = plainTextEdit;
        }
        contentWidget->setObjectName(QString::fromUtf8("contentWidget"));
        contentWidget->setFont(MainWindow::s_font);
        contentWidget->installEventFilter(this);

        m_verticalLayout->addWidget(m_title);
        m_verticalLayout->addWidget(contentWidget);
    }
    else {
        QFont font;
        font.setPointSize(12);
        font.setBold(true);

        m_titleEdit = new QLineEdit(this);
        m_titleEdit->setObjectName(QString::fromUtf8("titleEdit"));
        m_titleEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_titleEdit->setFont(font);
        m_titleEdit->setText(title);
        m_verticalLayout->addWidget(m_titleEdit);

        if(m_rich) {
            m_webView = new QWebView(this);
            m_webView->page()->setNetworkAccessManager(new UrlBasedRenderer);
            QFile file(":/editor.html");
            file.open(QIODevice::ReadOnly);
            QString html = file.readAll();
            html = html.replace(QRegExp("CONTENT_PLACEHOLDER"),m_content);
            m_webView->setHtml(html);
            m_webView->page()->mainFrame()->addToJavaScriptWindowObject("localFile", new LocalFileDialog(this));
            m_verticalLayout->addWidget(m_webView);
        }
        else {
            m_textEdit = new QPlainTextEdit(this);
            m_textEdit->setFont(MainWindow::s_font);
            m_textEdit->setPlainText(m_content);
            m_verticalLayout->addWidget(m_textEdit);
        }

        m_tagEdit = new QLineEdit(this);
        m_tagEdit->setObjectName(QString::fromUtf8("tagEdit"));
        m_tagEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_tagEdit->setFont(font);
        m_tagEdit->setText(tag);
        m_verticalLayout->addWidget(m_tagEdit);

        m_tagEdit->setCompleter(&(MainWindow::s_tagCompleter));
    }
    loadResource();
}
bool NoteItem::isReadOnly()
{
    return m_readOnly;
}
bool NoteItem::isRich()
{
    return m_rich;
}
int NoteItem::getNoteId() const
{
    return m_noteId;
}
void NoteItem::exportFile()
{
    m_q->Sql(QString("select title from notes where rowid=%1").arg(m_noteId).toUtf8());
    m_q->FetchRow();
    QString fileName = QString::fromUtf8((char*)m_q->GetColumnCString(0));

    QString content = m_content;
    fileName = fileName.replace(QRegExp("[\\\\/:*?\"<>|]"),"_");
    QRegExp rx("\"wike://([0-9a-f]+)\"");
    if(m_rich) {
        fileName += ".html";
        content = content.replace(rx, "\"\\1.png\"");
    }
    else
        fileName += ".txt";

    QFile file(fileName);
    if(file.open(QIODevice::WriteOnly)) 
        file.write(content.toLocal8Bit());

    if(m_rich) {
        TextBrowser* textBrowser = findChild<TextBrowser *>("contentWidget");
        int pos = 0;
        QString imgName;
        while ((pos = rx.indexIn(m_content, pos)) != -1) {
            imgName = rx.cap(1);
            QImage image = qvariant_cast<QImage>(textBrowser->document()->resource(QTextDocument::ImageResource, "wike://"+imgName));
            QImageWriter writer(imgName+".png", "PNG");
            writer.write(image);
            pos += rx.matchedLength();
        }
    }
}
QString NoteItem::selectedText()
{
    QString s;
    if(m_rich) {
        TextBrowser* textBrowser = findChild<TextBrowser *>("contentWidget");
        s = textBrowser->textCursor().selectedText().replace(QChar(0x2029), QChar('\n'));
    }
    else {
        PlainTextBrowser* textBrowser = findChild<PlainTextBrowser *>("contentWidget");
        s = textBrowser->textCursor().selectedText().replace(QChar(0x2029), QChar('\n'));
    }
    return s;
}
bool NoteItem::shortCut(int k)
{
    switch(k) {
        case Qt::Key_N:
            if(MainWindow::s_query.length()) {
                if(m_rich) {
                    TextBrowser* textBrowser = findChild<TextBrowser *>("contentWidget");
                    textBrowser->find(MainWindow::s_query);
                }
                else {
                    PlainTextBrowser* textBrowser = findChild<PlainTextBrowser *>("contentWidget");
                    textBrowser->find(MainWindow::s_query);
                }
            }
            return true;
        case Qt::Key_X:
            exportFile();
            return true;
        default:
            return false;
    }
}
bool NoteItem::eventFilter(QObject *obj, QEvent *ev)
{
    QEvent::Type type = ev->type();
    QObject* textBrowser = findChild<QObject *>("contentWidget");
    switch(type) {
        case QEvent::MouseButtonPress:
        case QEvent::FocusIn:
            if(s_activeNote != this){
                setActiveItem(this);
            }
            break;
        case QEvent::KeyPress:
            if(obj==textBrowser && s_activeNote == this){
                QKeyEvent *keyEvent = static_cast<QKeyEvent*>(ev);
                int k = keyEvent->key();
                if(m_readOnly || keyEvent->modifiers() == Qt::AltModifier) {
                    return shortCut(k);
                }
            }
            break;
        default:
            break;
    }
    return QFrame::eventFilter(obj,ev);
}
void NoteItem::setFont(const QFont& font)
{
    if(!m_rich) {
        PlainTextBrowser* textBrowser = findChild<PlainTextBrowser *>("contentWidget");
        QTextCursor cursor = textBrowser->textCursor();
        textBrowser->selectAll();
        textBrowser->setFont(font);
        textBrowser->setTextCursor(cursor);
    }
}
void NoteItem::autoSize()
{
    if(!m_sized) {
        show();
        m_sized = true;
        int h = 600;
        if(m_readOnly && m_totalLine<30) {
            if(m_rich) {
                TextBrowser* textBrowser = findChild<TextBrowser *>("contentWidget");
                QSize sz = textBrowser->document()->documentLayout()->documentSize().toSize();
                h = sz.height()+m_title->height()+30;
            }
            else {
                PlainTextBrowser* textBrowser = findChild<PlainTextBrowser *>("contentWidget");
                QRect rc = MainWindow::s_fontMetrics.boundingRect(0, 0, textBrowser->width(), 600, Qt::TextExpandTabs | Qt::TextWordWrap | Qt::TextIncludeTrailingSpaces, m_content);
                h = rc.height()+m_title->height()+30;
            }
            h = (h > 600)? 600 : h;
        }
        resize(width(), h);
    }
}
void NoteItem::loadResource()
{
    if(m_rich) {
        m_q->Sql(QString("select res_name,noteid,res_type,res_data from notes_res where noteid=%1").arg(m_noteId).toUtf8());
        while(m_q->FetchRow()) {
            QByteArray imgData = QByteArray((char*)m_q->GetColumnBlob(3), m_q->GetColumnBytes(3));
            QBuffer buffer(&imgData);
            buffer.open( QIODevice::ReadOnly );
            QImageReader reader(&buffer, "PNG");
            QImage image = reader.read();
            QString fileName = QString::fromUtf8((char*)m_q->GetColumnCString(0));
            if(m_readOnly) {
                TextBrowser* textBrowser = findChild<TextBrowser *>("contentWidget");
                textBrowser->document()->addResource(m_q->GetColumnInt(2), "wike://"+fileName, image);
            }
            else
                m_images[fileName] = image;
        }
    }
}
void NoteItem::setActiveItem(NoteItem* item)
{
    if(s_activeNote) {
        s_activeNote->setStyleSheet(".NoteItem { background-color : white; padding: 6px; }");
    }
    s_activeNote = item;
    if(s_activeNote && s_activeNote->m_readOnly) {
        s_activeNote->active();
    }
    g_mainWindow->noteSelected(s_activeNote!=0, s_activeNote && s_activeNote->m_rich);
}
NoteItem* NoteItem::getActiveItem()
{
    return s_activeNote;
}
bool NoteItem::saveNote()
{
    bool ret = false;
    QString date;
    if(m_noteId == 0) {
        QDateTime dt = QDateTime::currentDateTime();
        date = dt.toString("yyyy-MM-dd hh:mm:ss");
    }
    QString title = m_titleEdit->text();
    //m_webView->page()->mainFrame()->evaluateJavaScript(title); return false;
    if(m_rich) {
        QWebFrame* mainFrame = m_webView->page()->mainFrame();
        QWebFrame* iframe = mainFrame->childFrames()[0];
        QWebElementCollection col = iframe->findAllElements("img");
        QString imgName;
        QRegExp rx("\"wike://([0-9a-f]+)\"");
        foreach (QWebElement el, col) {
            imgName = el.attribute("src");
            if(el.attribute("embed").toLower() != "link" && !rx.exactMatch(imgName)) {
                QPixmap pix(el.geometry().size());
                QPainter p(&pix);
                el.render(&p);
                QImage image = pix.toImage();

                QByteArray imgBlock((const char *)image.bits(), image.byteCount());
                QByteArray sha1Sum = QCryptographicHash::hash(imgBlock, QCryptographicHash::Sha1);
                imgName = sha1Sum.toHex();
                el.setAttribute("src", "wike://"+imgName);
                m_images[imgName] = image;
            }
        }

        m_content = mainFrame->evaluateJavaScript("editor.updateTextArea();editor.$area.val();").toString();
        m_content = m_content.replace(QRegExp("<!--StartFragment-->"),"");
        m_content = m_content.replace(QRegExp("<!--EndFragment-->"),"");
    }
    else
        m_content = m_textEdit->toPlainText();
    QString tag = m_tagEdit->text();
    tag = tag.replace(QRegExp("^\\s+"),"");
    tag = tag.replace(QRegExp("\\s+$"),"");
    tag = tag.replace(QRegExp("\\s*,\\s*"),",");
    QStringList tags = tag.split(",");
    tags.sort();
    ret = g_mainWindow->saveNote(m_noteId, title, m_content, tags, date);
    if(ret) {
        if(m_rich) {
            QStringList res_to_remove;
            QStringList res_to_add;
            QStringList::Iterator it;
            QRegExp rx("<img[^>]*src=\"wike://([0-9a-f]+)\"[^>]*>");
            int pos = 0;
            QString imgName, sql;
            while ((pos = rx.indexIn(m_content, pos)) != -1) {
                imgName = rx.cap(1);
                if(!res_to_add.contains(imgName)) 
                    res_to_add << imgName;

                pos += rx.matchedLength();
            }
            if(m_noteId == 0)
                m_noteId = g_mainWindow->lastInsertId();
            else {
                sql = QString("select res_name from notes_res where noteid=%1").arg(m_noteId);
                m_q->Sql(sql.toUtf8());
                while(m_q->FetchRow()) {
                    res_to_remove << QString::fromUtf8((char*)m_q->GetColumnCString(0));
                }
                it = res_to_remove.begin();
                while(it != res_to_remove.end()) {
                    if(res_to_add.contains(*it)) {
                        res_to_add.removeOne(*it);
                        it = res_to_remove.erase(it);
                    }
                    else
                        it++;
                }
            }
            for(it = res_to_remove.begin(); it != res_to_remove.end(); it++) {
                sql = QString("delete from notes_res where noteid=%1 and res_name='%2'").arg(m_noteId).arg(*it);
                m_q->SqlStatement(sql.toUtf8());
            }

            for(it = res_to_add.begin(); it != res_to_add.end(); it++) {
                imgName = *it;
                QImage image = m_images[imgName];

                QBuffer buffer;
                QImageWriter writer(&buffer, "PNG");
                writer.write(image);

                g_mainWindow->insertNoteRes(imgName, m_noteId, (int)QTextDocument::ImageResource, buffer.data());
            }
        }
        //enable notelist update
        m_readOnly = true;
        g_mainWindow->setCurrentTag(tags[0]);
    }
    return ret;
}
void NoteItem::active()
{
    setStyleSheet(".NoteItem { background-color : rgb(235,242,252); padding: 5px; border: 1px dashed blue; border-radius: 8px;}");
    if(m_rich) {
        TextBrowser* textBrowser = findChild<TextBrowser *>("contentWidget");
        if(MainWindow::s_query.length())
            QueryHighlighter::instance().setDocument(textBrowser->document());
        textBrowser->setFocus();
    }
    else {
        PlainTextBrowser* textBrowser = findChild<PlainTextBrowser *>("contentWidget");
        if(MainWindow::s_query.length())
            QueryHighlighter::instance().setDocument(textBrowser->document());
        textBrowser->setFocus();
    }
    g_mainWindow->ensureVisible(this);
}
bool NoteItem::close()
{
    bool ret = false;
    if(m_titleEdit) {
        ret = saveNote();
    }
    else {
        ret = true;
    }
    return ret;
}

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
    QFileDialog fileDialog(this);
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    QStringList flters = filters.split(",");
    fileDialog.setNameFilters(flters);
    if(QDialog::Accepted == fileDialog.exec()) {
        return fileDialog.selectedFiles().join(",");
    }
    return "";
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

        m_textBrowser = new QTextBrowser(this);
        m_textBrowser->setObjectName(QString::fromUtf8("m_textBrowser"));
        m_textBrowser->setOpenExternalLinks(true);
        m_textBrowser->setFont(MainWindow::s_font);
        m_textBrowser->installEventFilter(this);

        if(m_rich) {
            m_textBrowser->setHtml(m_content);
            m_textBrowser->setStyleSheet("#m_textBrowser { background:url(:/html.png)}");
        }
        else {
            m_textBrowser->setPlainText(m_content);
            m_textBrowser->setStyleSheet("#m_textBrowser { background:url(:/text.png)}");
        }
        if(MainWindow::s_query.length())
            m_textBrowser->find(MainWindow::s_query);

        m_verticalLayout->addWidget(m_title);
        m_verticalLayout->addWidget(m_textBrowser);
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

    fileName = fileName.replace(QRegExp("[\\\\/:*?\"<>|]"),"_")+(m_rich?".html":".txt");
    QFile file(fileName);
    QRegExp rx("\"wike://([0-9a-f]+)\"");
    if(file.open(QIODevice::WriteOnly)) {
        QString content = m_content;
        content = content.replace(rx, "\"\\1.png\"");
        file.write(content.toLocal8Bit());
    }

    int pos = 0;
    QString imgName;
    while ((pos = rx.indexIn(m_content, pos)) != -1) {
        imgName = rx.cap(1);
        QImage image = qvariant_cast<QImage>(m_textBrowser->document()->resource(QTextDocument::ImageResource, "wike://"+imgName));
        QImageWriter writer(imgName+".png", "PNG");
        writer.write(image);
        pos += rx.matchedLength();
    }
}
void NoteItem::toggleView()
{
    if(m_rich) {
        m_textBrowser->setPlainText(m_content);
        m_textBrowser->setStyleSheet("#m_textBrowser { background:url(:/text.png)}");
    }
    else {
        m_textBrowser->setHtml(m_content);
        m_textBrowser->setStyleSheet("#m_textBrowser { background:url(:/html.png)}");
    }
    m_rich = !m_rich;
}
bool NoteItem::shortCut(int k)
{
    switch(k) {
        case Qt::Key_N:
            if(MainWindow::s_query.length())
                m_textBrowser->find(MainWindow::s_query);
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
    switch(type) {
        case QEvent::MouseButtonPress:
        case QEvent::FocusIn:
            if(s_activeNote != this){
                setActiveItem(this);
            }
            break;
        case QEvent::KeyPress:
            if(obj==m_textBrowser && s_activeNote == this){
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
    QTextCursor cursor = m_textBrowser->textCursor();
    m_textBrowser->selectAll();
    m_textBrowser->setFont(font);
    m_textBrowser->setTextCursor(cursor);
}
void NoteItem::autoSize()
{
    if(!m_sized) {
        show();
        m_sized = true;
        int h = 600;
        if(m_readOnly && m_totalLine<30) {
            QSize sz = m_textBrowser->document()->documentLayout()->documentSize().toSize();
            h = sz.height()+m_title->height()+30;
            h = (h > 600)? 600 : h;
        }
        resize(width(), h);
    }
}
void NoteItem::loadResource()
{
    m_q->Sql(QString("select res_name,noteid,res_type,res_data from notes_res where noteid=%1").arg(m_noteId).toUtf8());
    while(m_q->FetchRow()) {
        QByteArray imgData = QByteArray((char*)m_q->GetColumnBlob(3), m_q->GetColumnBytes(3));
        QBuffer buffer(&imgData);
        buffer.open( QIODevice::ReadOnly );
        QImageReader reader(&buffer, "PNG");
        QImage image = reader.read();
        QString fileName = QString::fromUtf8((char*)m_q->GetColumnCString(0));
        if(m_readOnly)
            m_textBrowser->document()->addResource(m_q->GetColumnInt(2), "wike://"+fileName, image);
        else
            m_images[fileName] = image;
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
    return ret;
}
void NoteItem::active()
{
    setStyleSheet(".NoteItem { background-color : rgb(235,242,252); padding: 5px; border: 1px dashed blue; border-radius: 8px;}");
    if(MainWindow::s_query.length())
        QueryHighlighter::instance().setDocument(m_textBrowser->document());
    m_textBrowser->setFocus();
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

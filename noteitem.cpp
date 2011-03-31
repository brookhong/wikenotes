#include "noteitem.h"
#include "highlighter.h"
#include "mainwindow.h"
ContentWidget::ContentWidget(QWidget* parent) : QTextBrowser(parent)
{
    m_viewType = 0;
    m_http = new QHttp(this);
    m_buffer = new QBuffer(&m_bytes);
    connect(m_http, SIGNAL(requestFinished(int, bool)),this, SLOT(Finished(int, bool)));
    setAcceptRichText((m_viewType == 1));
    setStyleSheet(".ContentWidget { background:url(:/text.png)}");
}
bool ContentWidget::canInsertFromMimeData(const QMimeData* source) const
{
    return (source->hasImage() || source->hasUrls() ||
        QTextEdit::canInsertFromMimeData(source));
}
void ContentWidget::addImageUrl(const QString& str)
{
    if(m_urls.empty()) {
        QUrl url(str);
        m_http->setHost(url.host());
        m_buffer->open(QIODevice::WriteOnly);
        m_req = m_http->get(url.path(), m_buffer);
    }
    m_urls.append(str);
}
void ContentWidget::Finished(int requestId, bool error)
{
    if(m_req == requestId) {
        QImage image;
        image.loadFromData(m_bytes);

        m_buffer->close();
        QString str = m_urls.takeFirst();
        QByteArray imgBlock((const char *)image.bits(), image.byteCount());
        QByteArray sha1Sum = QCryptographicHash::hash(imgBlock, QCryptographicHash::Sha1);
        QString imgName = sha1Sum.toHex();
        m_htmlCode = m_htmlCode.replace(QRegExp(str),imgName);
        document()->addResource(QTextDocument::ImageResource, imgName, image);
        setHtml(m_htmlCode);
        m_viewType = 1;
        setStyleSheet(".ContentWidget { background:url(:/html.png)}");

        if(!m_urls.empty()) {
            QUrl url(m_urls.first());
            m_http->setHost(url.host());
            m_buffer->open(QIODevice::WriteOnly);
            m_req = m_http->get(url.path(), m_buffer);
        }
    }
}
void ContentWidget::insertFromMimeData(const QMimeData* source)
{
    if (source->hasImage())
    {
        dropImage(qvariant_cast<QImage>(source->imageData()));
    }
    else if (source->hasUrls())
    {
        foreach (QUrl url, source->urls())
        {
            QFileInfo info(url.toLocalFile());
            if (QImageReader::supportedImageFormats().contains(info.suffix().toLower().toLatin1()))
                dropImage(QImage(info.filePath()));
            else
                dropTextFile(url, info.suffix());
        }
    }
    else if (source->hasHtml())
    {
        m_htmlCode = source->html();
        m_htmlCode = m_htmlCode.replace(QRegExp("^<!--StartFragment-->"),"");
        m_htmlCode = m_htmlCode.replace(QRegExp("<!--EndFragment-->$"),"");
        QRegExp rx("<img[^>]*src=\"(http://[^\"]+)\"[^>]*>");
        int pos = 0;
        QString url;
        while ((pos = rx.indexIn(m_htmlCode, pos)) != -1) {
            url = rx.cap(1);
            addImageUrl(url);
            pos += rx.matchedLength();
        }
        QTextEdit::insertFromMimeData(source);
    }
    else
    {
        QString text = source->text();
        QTextEdit::insertFromMimeData(source);
    }

}
void ContentWidget::dropImage(const QImage& image)
{
    if (!image.isNull())
    {
        QByteArray imgBlock((const char *)image.bits(), image.byteCount());
        QByteArray sha1Sum = QCryptographicHash::hash(imgBlock, QCryptographicHash::Sha1);
        QString imgName = sha1Sum.toHex();
        document()->addResource(QTextDocument::ImageResource, imgName, image);
        if(m_viewType)
            textCursor().insertImage(imgName);
        else
            textCursor().insertText("<img src=\""+imgName+"\" />");
    }
}
void ContentWidget::dropTextFile(const QUrl& url, const QString& ext)
{
    QFile file(url.toLocalFile());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        QString str;
        if(ext == "html" || ext == "htm") {
            QTextCodec *codec = Qt::codecForHtml(data);
            str = codec->toUnicode(data);
        }
        else
            str = QString::fromLocal8Bit(data);
        if(m_viewType)
            textCursor().insertText("<pre>"+str+"</pre>");
        else
            textCursor().insertText(str);
    }
}
void ContentWidget::setContent(const QString& str)
{
    document()->clear();
    if(Qt::mightBeRichText(str)) {
        m_htmlCode = str;
        setHtml(str);
        m_viewType = 1;
        setStyleSheet(".ContentWidget { background:url(:/html.png)}");
    }
    else {
        m_viewType = 0;
        setPlainText(str);
        setStyleSheet(".ContentWidget { background:url(:/text.png)}");
    }

    if(MainWindow::s_query.length())
        find(MainWindow::s_query);
}
QString ContentWidget::getContent()
{
    QString ret;
    if(m_viewType) {
        ret = m_htmlCode;
        if(ret.isEmpty())
            ret = toHtml();
    }
    else {
        ret = toPlainText();
    }
    /*
    QVector<QTextFormat> fmts = m_contentWidget->document()->allFormats();
    int fmtc = fmts.size();
    if(fmtc > 3) {
        content = m_contentWidget->toHtml();
    }
    else {
        content = m_contentWidget->toPlainText();
    }
    */
    return ret;
}
void ContentWidget::toggleView()
{
    if(m_viewType) {
        if(m_htmlCode.isEmpty())
            m_htmlCode = toHtml();
        setPlainText(m_htmlCode);
        setStyleSheet(".ContentWidget { background:url(:/text.png)}");
    }
    else {
        m_htmlCode = toPlainText();
        setHtml(m_htmlCode);
        setStyleSheet(".ContentWidget { background:url(:/html.png)}");
    }
    m_viewType = !m_viewType;
    setAcceptRichText((m_viewType == 1));
    setLineWrapColumnOrWidth(lineWrapColumnOrWidth());
}
bool ContentWidget::isHTMLView()
{
    return (m_viewType == 1);
}

NoteItem* NoteItem::s_activeNote = 0;
NoteItem::NoteItem(QWidget *parent, int row, bool readOnly) :
    QFrame(parent)
{
    m_noteId = row;
    m_widgetState = 0;

    m_title = new QLabel(this);
    m_title->setObjectName(QString::fromUtf8("title"));
    m_title->setWordWrap(true);
    m_title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QFont font;
    font.setPointSize(12);
    font.setBold(true);
    
    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setObjectName(QString::fromUtf8("titleEdit"));
    m_titleEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_titleEdit->setFont(font);
    m_titleEdit->setText(tr("Untitled"));

    m_tagEdit = new QLineEdit(this);
    m_tagEdit->setObjectName(QString::fromUtf8("tagEdit"));
    m_tagEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_tagEdit->setFont(font);
    m_tagEdit->setText(tr("Untagged"));

    m_contentWidget = new ContentWidget(this);
    m_contentWidget->setOpenExternalLinks(true);
    m_contentWidget->setFont(MainWindow::s_font);
    m_contentWidget->installEventFilter(this);
    installEventFilter(this);

    m_verticalLayout = new QVBoxLayout(this);
    m_verticalLayout->setSpacing(0);
    m_verticalLayout->setContentsMargins(0, 0, 0, 0);
    m_verticalLayout->addWidget(m_contentWidget);
    setReadOnly(readOnly);

    setFont(MainWindow::s_font);
    setStyleSheet(".NoteItem { background-color : white; padding: 6px; }");
    m_q = g_mainWindow->getSqlQuery();
    if(m_noteId > 0) 
        populate();
    m_sized = false;
    resize(parentWidget()->width() - 32, 240);
}
void NoteItem::setReadOnly(bool readOnly)
{
    if(readOnly) {
        if(m_widgetState == 1)
            return;
        m_title->show();
        m_titleEdit->hide();
        m_tagEdit->hide();
        m_titleWidget = m_title;
        m_widgetState = 1;
    }
    else {
        if(m_widgetState == 2)
            return;
        m_title->hide();
        m_titleEdit->show();
        m_tagEdit->show();
        m_titleWidget = m_titleEdit;
        m_widgetState = 2;
        m_verticalLayout->addWidget(m_tagEdit);
    }
    m_verticalLayout->insertWidget(0, m_titleWidget);
    m_contentWidget->setReadOnly(readOnly);
}
bool NoteItem::isReadOnly()
{
    return (m_widgetState == 1);
}
int NoteItem::getNoteId() const
{
    return m_noteId;
}
void NoteItem::exportFile()
{
    QString fileName = m_titleEdit->text();
    QString content = m_contentWidget->getContent();
    fileName = fileName.replace(QRegExp("[\\\\/:*?\"<>|]"),"_");
    QFile file(fileName);
    if(file.open(QIODevice::WriteOnly))
        file.write(content.toLocal8Bit());

    QRegExp rx("<img[^>]*src=\"([0-9a-f]+)\"[^>]*>");
    int pos = 0;
    QString imgName;
    while ((pos = rx.indexIn(content, pos)) != -1) {
        imgName = rx.cap(1);
        QImage image = qvariant_cast<QImage>(m_contentWidget->document()->resource(QTextDocument::ImageResource, imgName));
        QImageWriter writer(imgName+".png", "PNG");
        writer.write(image);
        pos += rx.matchedLength();
    }
}
void NoteItem::toggleView()
{
    m_contentWidget->toggleView();
}
bool NoteItem::shortCut(int k)
{
    switch(k) {
        case Qt::Key_N:
            if(MainWindow::s_query.length())
                m_contentWidget->find(MainWindow::s_query);
            return true;
        case Qt::Key_X:
            exportFile();
            return true;
        default:
            return false;
    }
}
void NoteItem::cancelEdit()
{
    if(m_noteId > 0) {
        populate();
        m_contentWidget->setLineWrapColumnOrWidth(m_contentWidget->lineWrapColumnOrWidth());
        setReadOnly(true);
        g_mainWindow->cancelEdit();
    }
    else
        g_mainWindow->loadNotes();
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
            if(obj==m_contentWidget && s_activeNote == this){
                QKeyEvent *keyEvent = static_cast<QKeyEvent*>(ev);
                int k = keyEvent->key();
                if(m_widgetState == 1 || keyEvent->modifiers() == Qt::AltModifier) {
                    return shortCut(k);
                }
            }
            break;
        default:
            break;
    }
    return QFrame::eventFilter(obj,ev);
}
void NoteItem::populate()
{
    m_q->exec(QString("select rowid,title,content,tag,created from notes left join notes_attr on notes.rowid=notes_attr.rowid where rowid=%1").arg(m_noteId));
    m_q->first();
    m_title->setText("<h2>"+Qt::escape(m_q->value(1).toString())+"</h2><table width='100%'><tr><td align='left' width='40%'>"+m_q->value(3).toString()+"</td><td align='center' width='20%'>"+m_q->value(0).toString()+"</td><td align='right' width='40%'>"+m_q->value(4).toString()+"</td></tr></table>");
    m_titleEdit->setText(m_q->value(1).toString());
    QString content = m_q->value(2).toString();
    m_contentWidget->setContent(content);
    m_tagEdit->setText(m_q->value(3).toString());
    m_totalLine = content.count('\n');;
    loadResource();
}
void NoteItem::setFont(const QFont& font)
{
    QTextCursor cursor = m_contentWidget->textCursor();
    m_contentWidget->selectAll();
    m_contentWidget->setFont(font);
    m_contentWidget->setTextCursor(cursor);
}
void NoteItem::autoSize()
{
    if(!m_sized) {
        show();
        m_sized = true;
        int h = 600;
        if(m_totalLine<30) {
            QSize sz = m_contentWidget->document()->documentLayout()->documentSize().toSize();
            h = sz.height()+m_titleWidget->height()+30;
            h = (h > 600)? 600 : h;
        }
        resize(width(), h);
    }
}
void NoteItem::loadResource()
{
    m_q->exec(QString("select res_name,noteid,res_type,res_data from notes_res where noteid=%1").arg(m_noteId));
    while(m_q->next()) {
        QByteArray imgData = m_q->value(3).toByteArray();
        QBuffer buffer(&imgData);
        buffer.open( QIODevice::ReadOnly );
        QImageReader reader(&buffer, "PNG");
        QImage image = reader.read();
        m_contentWidget->document()->addResource(m_q->value(2).toInt(), m_q->value(0).toString(), image);
    }
}
void NoteItem::setActiveItem(NoteItem* item)
{
    if(s_activeNote) {
        s_activeNote->setStyleSheet(".NoteItem { background-color : white; padding: 6px; }");
    }
    s_activeNote = item;
    bool htmlView = false;
    if(s_activeNote) {
        s_activeNote->active();
        htmlView = s_activeNote->m_contentWidget->isHTMLView();
    }
    g_mainWindow->noteSelected(s_activeNote!=0, htmlView);
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
    QString content = m_contentWidget->getContent();
    QString title = m_titleEdit->text();
    QString tag = m_tagEdit->text();
    tag = tag.replace(QRegExp("^\\s+"),"");
    tag = tag.replace(QRegExp("\\s+$"),"");
    tag = tag.replace(QRegExp("\\s*,\\s*"),",");
    QStringList tags = tag.split(",");
    tags.sort();
    ret = g_mainWindow->saveNote(m_noteId, title, content, tags, date);
    if(ret) {
        QStringList res_to_remove;
        QStringList res_to_add;
        QStringList::Iterator it;
        //QRegExp rx("<img src=\"([0-9a-f]+)\" />");
        QRegExp rx("<img[^>]*src=\"([0-9a-f]+)\"[^>]*>");
        int pos = 0;
        QString imgName, sql;
        while ((pos = rx.indexIn(content, pos)) != -1) {
            imgName = rx.cap(1);
            if(!res_to_add.contains(imgName)) 
                res_to_add << imgName;

            pos += rx.matchedLength();
        }
        if(m_noteId == 0)
            m_noteId = m_q->lastInsertId().toInt();
        else {
            sql = QString("select res_name from notes_res where noteid=%1").arg(m_noteId);
            m_q->exec(sql);
            while(m_q->next()) {
                res_to_remove << m_q->value(0).toString();
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
            m_q->exec(sql);
        }

        for(it = res_to_add.begin(); it != res_to_add.end(); it++) {
            imgName = *it;
            QImage image = qvariant_cast<QImage>(m_contentWidget->document()->resource(QTextDocument::ImageResource, imgName));

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
        QueryHighlighter::instance().setDocument(m_contentWidget->document());
    m_contentWidget->setFocus();
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

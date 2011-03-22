#include "mainwindow.h"
#include "hotkeysettings.h"
#include "ui_mainwindow.h"

QString MainWindow::s_query;
QFont MainWindow::s_font(tr("Tahoma"), 10);
const char* query_like[] = {
    "(content like '%KEYWORD%' or title like '%KEYWORD%' or tag like '%KEYWORD%')",
    "(content like '%KEYWORD%' or title like '%KEYWORD%')",
    "(content like '%KEYWORD%')",
    "(title like '%KEYWORD%')",
    "(tag like '%KEYWORD%')",
};
const char* tag_like = "tag like 'KEYWORD' or tag like 'KEYWORD,%' or tag like '%,KEYWORD,%' or tag like '%,KEYWORD'";

//QFile g_log;
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    initDB();
    ui->setupUi(this);
    createTrayIcon();

    m_hkToggleMain = new QxtGlobalShortcut(this);
    connect(m_hkToggleMain, SIGNAL(activated()), this, SLOT(toggleVisibility()));
    m_hkToggleMain->setShortcut(QKeySequence("Alt+Q"));
    loadSettings();
    m_bSettings = false;

    m_importDialog = new ImportDialog(this),
    ui->scrollArea->setStyleSheet("#scrollArea { background-color : white;}");
    ui->vsplitter->setHandleWidth(1);
    ui->vsplitter->setStretchFactor(0, 0);
    ui->vsplitter->setStretchFactor(1, 1);

    ui->searchBox->setStyleSheet("#searchBox {padding: 0 18px;background:url(:/search.png) no-repeat}");
    connect(ui->searchBox, SIGNAL(textChanged(const QString&)), this, SLOT(instantSearch(const QString&)));
    connect(ui->comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(loadNotes()));
    connect(ui->vsplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(splitterMoved()));

    connect(ui->actionE_Xit, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(ui->action_New_Note, SIGNAL(triggered()), this, SLOT(newNote()));
    connect(ui->action_Edit_Selected_Note, SIGNAL(triggered()), this, SLOT(editActiveNote()));
    connect(ui->action_Save_Note, SIGNAL(triggered()), this, SLOT(saveNote()));
    connect(ui->action_Delete_Selected_Note, SIGNAL(triggered()), this, SLOT(delActiveNote()));
    connect(ui->action_HTML_Preview, SIGNAL(triggered()), this, SLOT(toggleNoteView()));
    connect(ui->action_Import, SIGNAL(triggered()), this, SLOT(importNotes()));
    connect(ui->action_Export_Notes, SIGNAL(triggered()), this, SLOT(exportNotes()));
    connect(ui->actionText_Note_Font, SIGNAL(triggered()), this, SLOT(setNoteFont()));
    connect(ui->action_Hotkey_Settings, SIGNAL(triggered()), this, SLOT(setHotKey()));
    connect(ui->actionUsage, SIGNAL(triggered()), this, SLOT(usage()));
    connect(ui->action_About, SIGNAL(triggered()), this, SLOT(about()));

    connect(this, SIGNAL(tagAdded(const QString&)), this, SLOT(addTag(const QString&)));
    connect(this, SIGNAL(tagRemoved(const QString&)), this, SLOT(removeTag(const QString&)));

    connect(&m_importer, SIGNAL(importMsg(const QString&)), m_importDialog, SLOT(importMsg(const QString&)));
    connect(&m_importer, SIGNAL(importDone(int)), this, SLOT(importDone(int)));

    m_tagModel = new QStringListModel();
    refreshTag();
    ui->tagView->setModel(m_tagModel);
    ui->tagView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->tagView, SIGNAL(pressed(const QModelIndex&)), this, SLOT(tagPressed(const QModelIndex&)));
    ui->tagView->setStyleSheet("#tagView {font-size: 12px;} QListView::item:hover{background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #FAFBFE, stop: 1 #DCDEF1);color:rgb(0,0,128);}");
}
void MainWindow::createTrayIcon()
{
    QIcon icon(":/wike.png");
    setWindowIcon(icon);

    QAction *restoreAction = new QAction(tr("&Restore"), this);
    connect(restoreAction, SIGNAL(triggered()), this, SLOT(showNormal()));
    QAction *quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

    QMenu *trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(restoreAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(trayIconMenu);
    m_trayIcon->setIcon(icon);
    m_trayIcon->setToolTip(tr("wike"));
    m_trayIcon->show();
    connect(m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
}
void MainWindow::loadSettings()
{
    QFile file(QCoreApplication::applicationDirPath()+QDir::separator()+"config.xml");
    if (file.open(QIODevice::ReadOnly)) {
        QXmlStreamReader xml(&file);
        while (!xml.atEnd()) {
            if (xml.isStartElement()) {
                if(xml.name() == "text_font") {
                    QXmlStreamAttributes attrs = xml.attributes();
                    s_font = QFont(attrs.value("name").toString(), attrs.value("size").toString().toInt());
                }
                else if(xml.name() == "toggle_main_window") {
                    QXmlStreamAttributes attrs = xml.attributes();
                    m_hkToggleMain->setShortcut(QKeySequence(attrs.value("shortcut").toString()));
                }
            }
            xml.readNext();
        }
        file.close();
    }
}
void MainWindow::flushSettings()
{
    QFile file(QCoreApplication::applicationDirPath()+QDir::separator()+"config.xml");
    if (file.open(QIODevice::WriteOnly)) {
        QByteArray xmlData = QByteArray();

        QXmlStreamWriter writer(&xmlData);
        writer.setAutoFormatting(true);

        writer.writeStartDocument();

        writer.writeStartElement("config");

            writer.writeStartElement("text_font");
            writer.writeAttribute("name", s_font.family());
            writer.writeAttribute("size", QString("%1").arg(s_font.pointSize()));
            writer.writeEndElement();

            writer.writeStartElement("hotkeys");
                writer.writeStartElement("toggle_main_window");
                writer.writeAttribute("shortcut", m_hkToggleMain->shortcut().toString());
                writer.writeEndElement();
            writer.writeEndElement();

        writer.writeEndElement();

        writer.writeEndDocument();

        file.write(xmlData);
        file.flush();
        file.close();
    }
}
void MainWindow::toggleVisibility()
{
    if(isVisible()) {
        showMinimized();
    }
    else {
        show();
        setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    }
}
void MainWindow::handleSingleMessage(const QString&msg)
{
    //QMessageBox::information(this, tr("WikeNotes"), tr("Activated already running WikeNotes instance."));
    if(isMinimized()) {
        show();
        setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    }
    else
        activateWindow();
}
void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
            toggleVisibility();
            break;
        case QSystemTrayIcon::MiddleClick:
            m_trayIcon->showMessage(tr("wike"), tr("help"), QSystemTrayIcon::Information, 300);
            break;
        default:
            break;
    }
}
MainWindow::~MainWindow()
{
    if(m_bSettings)
        flushSettings();
    delete ui;
}
void MainWindow::initDB()
{
    QString dbName = QCoreApplication::applicationDirPath()+QDir::separator()+"wike.db";
    bool init = !QFile::exists(dbName);
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", dbName);
    db.setDatabaseName(dbName);
    if(!db.open()) {
        //to do
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(dbName);
    }
    m_q = new QSqlQuery(db);
    //g_log.setFileName(QCoreApplication::applicationDirPath()+QDir::separator()+"wike.log");
    //g_log.open(QIODevice::WriteOnly | QIODevice::Text);
    if(init) {        
        //m_q->exec("CREATE VIRTUAL TABLE notes USING fts3(title, content, tag)");
        m_q->exec("CREATE TABLE notes(title TEXT, content TEXT, tag TEXT)");
        m_q->exec("CREATE TABLE notes_attr(rowid INTEGER PRIMARY KEY ASC, created DATETIME)");
        m_q->exec("CREATE TABLE notes_res(res_name VARCHAR(40), noteid INTEGER, res_type INTEGER, res_data BLOB, CONSTRAINT unique_res_within_a_note UNIQUE (res_name, noteid) )");
        //g_log.write(QString(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")+" create notes_res %1\n").arg(int(a)).toLocal8Bit());
        //g_log.flush();
    }
}
QSqlQuery* MainWindow::getSqlQuery()
{
    return m_q;
}
QSqlQuery* MainWindow::getFoundNote(int idx)
{
    QString sql;
    if(idx == -1)
        sql = QString("select rowid,title,content,tag,created from notes left join notes_attr on notes.rowid=notes_attr.rowid %1 order by created asc").arg(m_criterion);
    else
        sql = QString("select rowid,title,content,tag,created from notes left join notes_attr on notes.rowid=notes_attr.rowid %1 order by created desc limit 1 offset %2")
            .arg(m_criterion)
            .arg(idx);

    m_q->exec(sql);
    return m_q;
}
void MainWindow::loadNotes()
{
    ui->noteList->clear();
    ui->action_Save_Note->setEnabled(false);

    m_criterion = "";
    if(s_query != "") {
        m_criterion  = query_like[ui->comboBox->currentIndex()];
        m_criterion  = " where " + m_criterion.replace(QRegExp("KEYWORD"),s_query);
    }
    if(!m_tagList.empty()) {
        if(m_criterion == "") 
            m_criterion = QString(" where (%1)").arg(tag_like);
        else
            m_criterion += QString(" and (%1)").arg(tag_like);
        m_criterion = m_criterion.replace(QRegExp("KEYWORD"),m_tagList.join(","));
    }
    QString sql = QString("select count(*) from notes %1").arg(m_criterion);
    if(m_q->exec(sql)) {
        m_q->first();
        int found = m_q->value(0).toInt();
        ui->statusbar->showMessage(QString("%1 notes found").arg(found));
        ui->noteList->extend(found);
        ui->noteList->update();
        ui->noteList->adjustSize();
        resizeEvent(0);
    }
}
void MainWindow::instantSearch(const QString& query)
{
    s_query = query;
    loadNotes();
}
void MainWindow::tagPressed(const QModelIndex &current)
{
    QModelIndexList modelList = ui->tagView->selectionModel()->selectedIndexes();
    m_tagList.clear();
    for(int i =0 ; i < modelList.size() ; i++) {
        m_tagList << modelList[i].data().toString();
    }
    if(m_tagList.indexOf(tr("All")) != -1) {
        m_tagList.clear();
        ui->tagView->selectionModel()->clearSelection();
        ui->tagView->setCurrentIndex(current);
        if(current.row() != 0)
            m_tagList << current.data().toString();
    }
    m_tagList.sort();
    loadNotes();
}
void MainWindow::splitterMoved()
{
    resizeEvent(0);
}
void MainWindow::newNote()
{
    ui->noteList->clear();
    NoteItem *noteItem = new NoteItem(ui->noteList,0,false);
    ui->noteList->addNote(noteItem);
    noteItem->autoSize();
    NoteItem::setActiveItem(noteItem);
    ui->action_Save_Note->setEnabled(true);
    resizeEvent(0);
}
bool MainWindow::insertNote(QString& title, QString& content, QString& tag, QString& datetime)
{
    bool ret = false;
    title.replace("'","''");
    content.replace("'","''");
    QString sql = "INSERT INTO notes(title, content, tag) VALUES('";
    sql += title+"','";
    sql += content+"','";
    sql += tag+"')";
    ret = m_q->exec(sql);
    if(ret) {
        sql = QString("INSERT INTO notes_attr(created) VALUES('%1')").arg(datetime);
        m_q->exec(sql);
    }
    return ret;
}
bool MainWindow::insertNoteRes(QString& res_name, int noteId, int res_type, const QByteArray& res_data)
{
    m_q->clear();
    m_q->prepare("INSERT INTO notes_res(res_name,noteid,res_type,res_data) values (:res_name, :noteid, :res_type, :res_data)");
    m_q->bindValue(":res_name",res_name);
    m_q->bindValue(":noteid",noteId);
    m_q->bindValue(":res_type",res_type);
    m_q->bindValue(":res_data", res_data);
    return m_q->exec();
}
bool MainWindow::saveNote(int row, QString& title, QString& content, QStringList& tags, QString& datetime)
{
    bool ret = false;
    if(title != "" && !tags.empty()) {
        int i, tagSize = tags.size();
        int *newTagCount = new int[tagSize];
        for(i=0; i<tagSize; ++i) {
            newTagCount[i] = getTagCount(tags[i]);
        }
        if(row > 0) {
            QString sql;
            QStringList oldTags = getTagsOf(row);
            QStringList ups;
            title.replace("'","''");
            content.replace("'","''");
            sql = "UPDATE notes SET ";
            if(title != "")
                ups << "title='"+title+"'";
            if(content != "")
                ups << "content='"+content+"'";
            ups << "tag='"+tags.join(",")+"'";
            sql += ups.join(",");
            sql += QString(" WHERE rowid=%1").arg(row);
            ret = m_q->exec(sql);
            if(ret) {
                int oldTagSize = oldTags.size();
                for(i=0; i<oldTagSize; ++i) {
                    if(getTagCount(oldTags[i]) == 0)
                        emit tagRemoved(oldTags[i]);
                }
                for(i=0; i<tagSize; ++i) {
                    if(newTagCount[i] == 0)
                        emit tagAdded(tags[i]);
                }
            }
        }
        else {
            QString tag = tags.join(",");
            ret = insertNote(title, content, tag, datetime);
            if(ret) {
                for(i=0; i<tagSize; ++i) {
                    if(newTagCount[i] == 0)
                        emit tagAdded(tags[i]);
                }
            }
        }
        delete newTagCount;
    }
    return ret;
}
void MainWindow::resizeEvent(QResizeEvent * event)
{
    (void)(event);
    int noteListWidth = ui->vsplitter->sizes()[1]-32;
    int vh = ui->scrollArea->viewport()->size().height();
    ui->noteList->fitSize(noteListWidth, vh);
}
bool MainWindow::event(QEvent* evt){
    QEvent::Type type = evt->type();
    if(type==QEvent::WindowStateChange&&isMinimized()){
        QTimer::singleShot(50, this, SLOT(hide()));
        evt->ignore();
        return true;
    }
    return QMainWindow::event(evt);
}
QStringList MainWindow::getTagsOf(int row)
{
    QStringList tags;
    QString sql = QString("select tag from notes where rowid=%1").arg(row);
    if(m_q->exec(sql)) {
        m_q->first();
        tags = m_q->value(0).toString().split(",");
    }
    return tags;
}
int MainWindow::getTagCount(const QString& tag)
{
    int ret = 0;
    QString sql = QString("select count(*) from notes where (%1)").arg(tag_like);
    sql = sql.replace(QRegExp("KEYWORD"),tag);
    if(m_q->exec(sql)) {
        m_q->first();
        ret = m_q->value(0).toInt();
    }
    return ret;
}
void MainWindow::addTag(const QString& tag)
{
    QStringList lst = m_tagModel->stringList();
    int i = 0, len = lst.count();
    while(i < len && lst[i] < tag) {
        i++;
    }
    if(m_tagModel->insertRows(i,1)) {
        m_tagModel->setData(m_tagModel->index(i),tag);
        //ui->tagView->setCurrentIndex(m_tagModel->index(m_tagModel->stringList().indexOf(tag)));
    }
}
void MainWindow::removeTag(const QString& tag)
{
    int i = m_tagModel->stringList().indexOf(tag);
    if(i > 0 && m_tagModel->removeRows(i,1)) {
        int n = m_tagModel->rowCount();
        if(i < n)
            ui->tagView->setCurrentIndex(m_tagModel->index(i));
        else
            ui->tagView->setCurrentIndex(m_tagModel->index(i-1));
    }
}
void MainWindow::toggleNoteView()
{
    NoteItem* activeItem = NoteItem::getActiveItem();
    activeItem->toggleView();
}
bool MainWindow::delActiveNote()
{
    NoteItem* activeItem = NoteItem::getActiveItem();
    int row = activeItem->getNoteId();
    bool ret = false;
    if(row > 0) {
        QStringList oldTags = getTagsOf(row);
        QString sql = QString("delete from notes where rowid=%1").arg(row);
        if(m_q->exec(sql)) {
            sql = QString("delete from notes_attr where rowid=%1").arg(row);
            ret = m_q->exec(sql);
            sql = QString("delete from notes_res where noteid=%1").arg(row);
            ret = m_q->exec(sql);

            int i, oldTagSize = oldTags.size();
            for(i=0; i<oldTagSize; ++i) {
                if(getTagCount(oldTags[i]) == 0)
                    emit tagRemoved(oldTags[i]);
            }
        }
    }
    else
        ret = true;
    if(ret) {
        NoteItem* item = ui->noteList->getNextNote(activeItem);
        NoteItem::setActiveItem(item);
        ui->noteList->removeNote(activeItem);
    }
    return ret;
}
void MainWindow::editActiveNote()
{
    NoteItem* activeItem = NoteItem::getActiveItem();
    activeItem->setReadOnly(false);
    ui->action_Save_Note->setEnabled(true);
}
void MainWindow::saveNote()
{
    NoteItem* activeItem = NoteItem::getActiveItem();
    if(activeItem->saveNote())
        loadNotes();
}
void MainWindow::statusMessage(const QString& msg)
{
    ui->statusbar->showMessage(msg);
}
void MainWindow::importDone(int action)
{
    if(action == 0)
        refreshTag();
    m_importDialog->setFinishFlag(true);
    m_importDialog->close();
}
void MainWindow::refreshTag()
{
    QStringList tagList;
    tagList << tr("All");
    m_q->exec("select distinct tag from notes");
    while(m_q->next()) {
        tagList << m_q->value(0).toString().split(",");
    }
    tagList.sort();
    tagList.removeDuplicates();
    m_tagModel->setStringList(tagList);
}
ImportDialog::ImportDialog(QWidget *parent) : QDialog(parent, Qt::Tool)
{
    m_label = new QLabel;
    m_label->resize(300, 400);
    QVBoxLayout *verticalLayout = new QVBoxLayout(this);
    verticalLayout->addWidget(m_label);
    setLayout(verticalLayout);
    setModal(true);
    m_finished = false;
}
void ImportDialog::importMsg(const QString& msg)
{
    m_label->setText(msg);
}
void ImportDialog::setFinishFlag(bool b)
{
    m_finished = b;
}
void ImportDialog::reject()
{
    if(m_finished)
        QDialog::reject();
}
void ImportDialog::closeEvent(QCloseEvent *event)
{
    if (m_finished) {
        event->accept();
    } else {
        event->ignore();
    }
}
void NotesImporter::run()
{
    int num = 0;
    QFile file(m_file);
    if(m_action == 0) {
        if (file.open(QIODevice::ReadOnly)) {
            QXmlStreamReader xml(&file);
            QString title,datetime,link,tag,content,res_name,res_type;
            int res_flag = 0;
            QByteArray res_data;
            QSqlQuery *q = g_mainWindow->getSqlQuery();
            q->exec("select rowid from notes order by rowid desc limit 1");
            int noteId = 1;
            if(q->first())
                noteId = q->value(0).toInt()+1;
            while (!xml.atEnd()) {
                if (xml.isStartElement()) {
                    if(xml.name() == "note") {
                        QXmlStreamAttributes attrs = xml.attributes();
                        title = attrs.value("title").toString();
                        datetime = attrs.value("created").toString();
                        tag = attrs.value("tags").toString();
                        tag = (tag == "") ? tr("Untagged") : tag;
                    }
                    else if(xml.name() == "resource") {
                        QXmlStreamAttributes attrs = xml.attributes();
                        res_name = attrs.value("name").toString();
                        res_type = attrs.value("type").toString();
                        res_flag = 1;
                    }
                }
                else if(xml.isCDATA()) {
                    if(res_flag)
                        res_data = QByteArray::fromBase64(xml.text().toString().toAscii());
                    else
                        content = xml.text().toString();
                }
                else if(xml.isEndElement()) {
                    if(xml.name() == "note") {
                        if(g_mainWindow->insertNote(title,content,tag, datetime)) {
                            num++;
                            noteId++;
                            emit importMsg(tr("%1 notes imported from %2.").arg(num).arg(m_file));
                        }
                        else {
                            emit importMsg(tr("SQL Error."));
                        }
                    }
                    else if(xml.name() == "resource") {
                        res_flag = 0;
                        g_mainWindow->insertNoteRes(res_name,noteId,res_type.toInt(),res_data);
                    }
                }
                xml.readNext();
                msleep(10);
            }
            file.close();
        }
    }
    else if(m_action == 1){
        if (file.open(QIODevice::WriteOnly)) {
            QByteArray xmlData = QByteArray();

            QXmlStreamWriter writer(&xmlData);
            writer.setAutoFormatting(true);

            writer.writeStartDocument();

            writer.writeStartElement("wikenotes");

            QSqlQuery* q = g_mainWindow->getFoundNote(-1);
            QSqlQuery q_res(*q);
            int noteId;
            while(q->next()) {
                noteId = q->value(0).toInt();
                writer.writeStartElement("note");
                writer.writeAttribute("title", q->value(1).toString());
                writer.writeAttribute("tags", q->value(3).toString());
                writer.writeAttribute("created", q->value(4).toString());
                writer.writeCDATA(q->value(2).toString());

                q_res.exec(QString("select res_name,res_type,res_data from notes_res where noteid=%1").arg(noteId));
                while(q_res.next()) {
                    writer.writeStartElement("resource");
                    writer.writeAttribute("name", q_res.value(0).toString());
                    writer.writeAttribute("type", q_res.value(1).toString());
                    writer.writeCDATA(q_res.value(2).toByteArray().toBase64());
                    writer.writeEndElement();
                }

                writer.writeEndElement();
            }

            writer.writeEndElement();

            writer.writeEndDocument();

            file.write(xmlData);
            file.flush();
            file.close();
        }
    }
    emit importDone(m_action);
}
void MainWindow::importNotes()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("import notes"), ".", tr("xml files (*.xml)"));

    if (!fileName.isEmpty()) {
        m_importer.importFile(fileName);
        m_importDialog->setFinishFlag(false);
        m_importDialog->exec();
    }
}
void MainWindow::exportNotes()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("export notes"), ".", tr("xml files (*.xml)"));

    if (!fileName.isEmpty()) {
        m_importer.exportFile(fileName);
        m_importDialog->setFinishFlag(false);
        m_importDialog->exec();
    }
}
void MainWindow::setNoteFont()
{
    bool ok;
    s_font = QFontDialog::getFont(&ok, s_font, this);
    if (ok) {
        m_bSettings = true;
        ui->noteList->setTextNoteFont(s_font);
    }
}
void MainWindow::setHotKey()
{
    QKeySequence ks = m_hkToggleMain->shortcut();
    HotkeySettings diag(ks, this);
    m_hkToggleMain->setShortcut(QKeySequence());
    if(diag.exec() == QDialog::Accepted) {
        m_bSettings = true;
        m_hkToggleMain->setShortcut(diag.m_hkTM);
    }
    else
        m_hkToggleMain->setShortcut(ks);
}
void MainWindow::usage()
{
    QFile file;
    file.setFileName(":/help.htm");
    file.open(QIODevice::ReadOnly);
    QString helpString = file.readAll();
    file.close();

    QDialog *dlgHelp = new QDialog(this);
    dlgHelp->setAttribute(Qt::WA_DeleteOnClose);
    QVBoxLayout *verticalLayout = new QVBoxLayout(dlgHelp);
    QTextBrowser *textEdit = new QTextBrowser(dlgHelp);
    textEdit->setText(helpString);
    verticalLayout->addWidget(textEdit);
    dlgHelp->resize(800,500);
    dlgHelp->show();
}
void MainWindow::about()
{
    QMessageBox::about(this, tr("WikeNotes"), tr("Version: "APP_VERSION"\nAuthor: hzgmaxwell@hotmail.com"));
}
void MainWindow::cancelEdit()
{
    ui->action_Save_Note->setEnabled(false);
}
void MainWindow::noteSelected(bool has, bool htmlView)
{
    ui->action_Delete_Selected_Note->setEnabled(has);
    ui->action_Edit_Selected_Note->setEnabled(has);
    ui->action_HTML_Preview->setEnabled(has);
    ui->action_HTML_Preview->setChecked(htmlView);
}
void MainWindow::ensureVisible(NoteItem* item)
{
    ui->scrollArea->ensureWidgetVisible(item);
}

#include "mainwindow.h"
#include "hotkeysettings.h"
#include "ui_mainwindow.h"
#ifdef WIN32
#include <windows.h>
#endif

QString MainWindow::s_query;
QFont MainWindow::s_font(tr("Tahoma"), 10);
QFontMetrics MainWindow::s_fontMetrics(MainWindow::s_font);
QCompleter MainWindow::s_tagCompleter;
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
    ui->setupUi(this);
    createTrayIcon();

    m_hkToggleMain = new QxtGlobalShortcut(this);
    connect(m_hkToggleMain, SIGNAL(activated()), this, SLOT(toggleVisibility()));
    m_hkToggleMain->setShortcut(QKeySequence("Alt+Q"));

    m_hkNewTextNote = new QxtGlobalShortcut(this);
    connect(m_hkNewTextNote, SIGNAL(activated()), this, SLOT(silentNewTextNote()));
    m_hkNewTextNote->setShortcut(QKeySequence("Ctrl+1"));

    loadSettings();
    m_bSettings = false;
    if(m_dbName.isEmpty())
        m_dbName = QCoreApplication::applicationDirPath()+QDir::separator()+"default.wike";
    if(m_lang.isEmpty())
        m_lang = "English";
    else if(m_translator.load(m_lang+".qm", ".")){
        qApp->installTranslator(&m_translator);
        ui->retranslateUi(this);
    }

    m_importDialog = new ImportDialog(this);
    ui->scrollArea->setStyleSheet("#scrollArea { background-color : white;}");
    ui->vsplitter->setHandleWidth(1);
    ui->vsplitter->setStretchFactor(0, 0);
    ui->vsplitter->setStretchFactor(1, 1);

    ui->searchBox->setStyleSheet("#searchBox {padding: 0 18px;background:url(:/search.png) no-repeat}");
    connect(ui->searchBox, SIGNAL(textChanged(const QString&)), this, SLOT(instantSearch(const QString&)));
    connect(ui->comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(loadNotes()));
    connect(ui->vsplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(splitterMoved()));

    addAction(ui->actionFocusSearchBox);
    connect(ui->actionFocusSearchBox, SIGNAL(triggered()), ui->searchBox, SLOT(setFocus()));
    connect(ui->actionE_Xit, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(ui->actionNew_Library, SIGNAL(triggered()), this, SLOT(newDB()));
    connect(ui->action_Open_Notes_Library, SIGNAL(triggered()), this, SLOT(selectDB()));
    connect(ui->action_New_Plain_Text_Note, SIGNAL(triggered()), this, SLOT(newPlainNote()));
    connect(ui->action_New_Note, SIGNAL(triggered()), this, SLOT(newHTMLNote()));
    connect(ui->action_Edit_Selected_Note, SIGNAL(triggered()), this, SLOT(editActiveNote()));
    connect(ui->action_Save_Note, SIGNAL(triggered()), this, SLOT(saveNote()));
    connect(ui->action_Delete_Selected_Note, SIGNAL(triggered()), this, SLOT(delActiveNote()));
    connect(ui->action_Import, SIGNAL(triggered()), this, SLOT(importNotes()));
    connect(ui->action_Export_Notes, SIGNAL(triggered()), this, SLOT(exportNotes()));
    connect(ui->actionText_Note_Font, SIGNAL(triggered()), this, SLOT(setNoteFont()));
    connect(ui->action_Hotkey_Settings, SIGNAL(triggered()), this, SLOT(setHotKey()));
    connect(ui->action_English, SIGNAL(triggered()), this, SLOT(changeLanguage()));
    connect(ui->action_Chinese, SIGNAL(triggered()), this, SLOT(changeLanguage()));
    connect(ui->actionUsage, SIGNAL(triggered()), this, SLOT(usage()));
    connect(ui->action_About, SIGNAL(triggered()), this, SLOT(about()));

    connect(this, SIGNAL(tagAdded(const QString&)), this, SLOT(addTag(const QString&)));
    connect(this, SIGNAL(tagRemoved(const QString&)), this, SLOT(removeTag(const QString&)));

    connect(&m_importer, SIGNAL(importMsg(const QString&)), m_importDialog, SLOT(importMsg(const QString&)));
    connect(&m_importer, SIGNAL(importDone(int)), this, SLOT(importDone(int)));

    m_tagModel = new QStringListModel();
    ui->tagView->setModel(m_tagModel);
    s_tagCompleter.setModel(m_tagModel);
    ui->tagView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->tagView, SIGNAL(pressed(const QModelIndex&)), this, SLOT(tagPressed(const QModelIndex&)));
    connect(ui->tagView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)), this, SLOT(tagChanged(const QItemSelection&, const QItemSelection&)));
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
    m_trayIcon->setToolTip("WikeNotes");
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
                    s_fontMetrics = QFontMetrics(s_font);
                }
                else if(xml.name() == "toggle_main_window") {
                    QXmlStreamAttributes attrs = xml.attributes();
                    m_hkToggleMain->setShortcut(QKeySequence(attrs.value("shortcut").toString()));
                }
                else if(xml.name() == "new_text_note") {
                    QXmlStreamAttributes attrs = xml.attributes();
                    m_hkNewTextNote->setShortcut(QKeySequence(attrs.value("shortcut").toString()));
                }
                else if(xml.name() == "default_notes_library") {
                    QXmlStreamAttributes attrs = xml.attributes();
                    m_dbName = attrs.value("name").toString();
                }
                else if(xml.name() == "language") {
                    QXmlStreamAttributes attrs = xml.attributes();
                    m_lang = attrs.value("name").toString();
                }
                else if(xml.name() == "external_command") {
                    QXmlStreamAttributes attrs = xml.attributes();
                    QAction* action = new QAction(attrs.value("name").toString(), this);
                    action->setData(attrs.value("command").toString());
                    action->setShortcut(QKeySequence(attrs.value("shortcut").toString()));
                    connect(action, SIGNAL(triggered()), this, SLOT(extActions()));
                    m_extActions.append(action);
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

            writer.writeStartElement("default_notes_library");
            writer.writeAttribute("name", m_dbName);
            writer.writeEndElement();

            writer.writeStartElement("language");
            writer.writeAttribute("name", m_lang);
            writer.writeEndElement();

            writer.writeStartElement("text_font");
            writer.writeAttribute("name", s_font.family());
            writer.writeAttribute("size", QString("%1").arg(s_font.pointSize()));
            writer.writeEndElement();

            writer.writeStartElement("hotkeys");
                writer.writeStartElement("toggle_main_window");
                writer.writeAttribute("shortcut", m_hkToggleMain->shortcut().toString());
                writer.writeEndElement();
                writer.writeStartElement("new_text_note");
                writer.writeAttribute("shortcut", m_hkNewTextNote->shortcut().toString());
                writer.writeEndElement();
            writer.writeEndElement();

            writer.writeStartElement("external_commands");
                for (int i = 0; i < m_extActions.size(); ++i) {
                    writer.writeStartElement("external_command");
                    writer.writeAttribute("name", m_extActions[i]->text());
                    writer.writeAttribute("command", m_extActions[i]->data().toString());
                    writer.writeAttribute("shortcut", m_extActions[i]->shortcut().toString());
                    writer.writeEndElement();
                }
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
void MainWindow::silentNewTextNote()
{
#ifdef WIN32
    ::Sleep(300);
    keybd_event(VK_CONTROL,MapVirtualKey (VK_CONTROL, 0),0,0);
    keybd_event('C', MapVirtualKey ('C', 0), 0, 0);
    keybd_event('C', MapVirtualKey ('C', 0), KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL,MapVirtualKey (VK_CONTROL, 0),KEYEVENTF_KEYUP,0);
    ::Sleep(300);
#endif

    QClipboard *clipboard = QApplication::clipboard();
    QString content = clipboard->text();
    QString title;
    if(content.length() > 30)
        title = content.mid(0,28)+"...";
    else
        title = content;
    QStringList tags;
    tags << tr("Untagged");
    QDateTime dt = QDateTime::currentDateTime();
    QString date = dt.toString("yyyy-MM-dd hh:mm:ss");

    if(saveNote(0, title, content, tags, date)) {
        setCurrentTag(tags[0]);
        loadNotes();
    }
}
void MainWindow::handleSingleMessage(const QString&msg)
{
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
            m_trayIcon->showMessage("WikeNotes", tr("help"), QSystemTrayIcon::Information, 300);
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
void MainWindow::newDB()
{
    QString dbName = QFileDialog::getSaveFileName(this, tr("Create New Library"), ".", tr("wike files (*.wike)"));

    if (!dbName.isEmpty()) {
        closeDB();
        if(!openDB(dbName)) {
            QMessageBox::warning(this, "WikeNotes", tr("Failed to create notes library: %1").arg(dbName)); 
            openDB();
        }
    }
}
void MainWindow::selectDB()
{
    QString dbName = QFileDialog::getOpenFileName(this, tr("Open Notes Library"), ".", tr("wike files (*.wike)"));
    if (!dbName.isEmpty()) {
        closeDB();
        if(!openDB(dbName)) {
            QMessageBox::warning(this, "WikeNotes", tr("Failed to open notes library: %1").arg(dbName)); 
            openDB();
        }
    }
}
void MainWindow::openDB()
{
    if(!openDB(m_dbName)) {
        QString dbName;
        do {
            QMessageBox::StandardButton choice = QMessageBox::warning(this, "WikeNotes",
                    tr("Would you like to create a new Library? No to select another existing Library,  Abort to exit."),
                    QMessageBox::Yes|QMessageBox::No|QMessageBox::Abort);
            if(choice == QMessageBox::Yes) 
                dbName = QFileDialog::getSaveFileName(this, tr("Create New Library"), ".", tr("wike files (*.wike)"));
            else if(choice == QMessageBox::No)
                dbName = QFileDialog::getOpenFileName(this, tr("Open Notes Library"), ".", tr("wike files (*.wike)"));
            else {
                QTimer::singleShot(0, this, SLOT(close()));
                break;
            }
        }while(dbName.isEmpty() || !openDB(dbName));
    }
}
bool MainWindow::openDB(const QString& dbName)
{
    bool ret = false;
    bool init = !QFile::exists(dbName);
    m_db = new SQLiteDatabase(dbName.toUtf8(), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
    m_q = new SQLiteStatement(m_db);
    QString defaultKey = "jessie&brook";
    if(init) {        
        QString password = QInputDialog::getText(this, "WikeNotes",
                tr("Would you like to protect your notes library with a password?\nCancel to set no password.\n\nPassword: "), QLineEdit::Password);
        if (password.isEmpty()) 
            password = defaultKey;
        m_q->SqlStatement("PRAGMA key = '"+QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha1).toHex()+"';");
        m_q->SqlStatement("CREATE TABLE notes(title TEXT, content TEXT, tag TEXT, hash TEXT)");
        m_q->SqlStatement("CREATE TABLE notes_attr(rowid INTEGER PRIMARY KEY ASC, created DATETIME)");
        m_q->SqlStatement("CREATE TABLE notes_res(res_name VARCHAR(40), noteid INTEGER, res_type INTEGER, res_data BLOB, CONSTRAINT unique_res_within_a_note UNIQUE (res_name, noteid) )");
        ret = true;
    }
    else {
        QString password = defaultKey;
        int retry = 0;
        while(retry < 3) {
            try {
                retry++;
                m_q->SqlStatement("PRAGMA key = '"+QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha1).toHex()+"';");
                m_q->SqlStatement("select rowid from notes limit 1");
                ret = true;
                break;
            }
            catch(Kompex::SQLiteException &exception) {
                password = QInputDialog::getText(this, "WikeNotes", tr("Password for %1:").arg(dbName), QLineEdit::Password);
                m_db->Close();
                m_db->Open(dbName.toUtf8(),SQLITE_OPEN_READWRITE, 0);
            }
        }
    }
    if(ret) {
        setWindowTitle(QString("WikeNotes (%1)").arg(dbName));
        refreshTag();
        loadNotes();
        m_dbName = dbName;
    }
    else 
        closeDB();
    return ret;
}
void MainWindow::closeDB()
{
    ui->noteList->clear();

    delete m_q;
    m_db->Close();
    delete m_db;
}
SQLiteStatement* MainWindow::getSqlQuery()
{
    return m_q;
}
SQLiteStatement* MainWindow::getFoundNote(int idx)
{
    QString sql;
    if(idx == -1)
        sql = QString("select rowid,title,content,tag,created from notes left join notes_attr on notes.rowid=notes_attr.rowid %1 order by created asc").arg(m_criterion);
    else
        sql = QString("select rowid,title,content,tag,created from notes left join notes_attr on notes.rowid=notes_attr.rowid %1 order by created desc limit 1 offset %2")
            .arg(m_criterion)
            .arg(idx);

    m_q->Sql(sql.toUtf8());
    return m_q;
}
void MainWindow::loadImageFromDB(const QString& fileName, QByteArray& imgData)
{
    m_q->Sql(QString("select res_name,noteid,res_type,res_data from notes_res where res_name='%1'").arg(fileName).toUtf8());
    if(m_q->FetchRow()) {
        imgData = QByteArray((char*)m_q->GetColumnBlob(3), m_q->GetColumnBytes(3));
    }
    m_q->FreeQuery();
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
            m_criterion = QString(" where (%1)").arg(tag_like).replace(QRegExp("KEYWORD"),m_tagList[0]);
        else
            m_criterion += QString(" and (%1)").arg(tag_like).replace(QRegExp("KEYWORD"),m_tagList[0]);
        for(int i =1 ; i < m_tagList.size() ; i++) {
            m_criterion += QString(" and (%1)").arg(tag_like).replace(QRegExp("KEYWORD"),m_tagList[i]);
        }
    }
    QString sql = QString("select count(*) from notes %1").arg(m_criterion);
    int found = m_q->SqlAggregateFuncResult(sql.toUtf8());
    ui->statusbar->showMessage(tr("%1 notes found").arg(found));
    ui->noteList->extend(found);
    ui->noteList->update();
    ui->noteList->adjustSize();
    resizeEvent(0);
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
        ui->tagView->selectionModel()->clearSelection();
        ui->tagView->setCurrentIndex(current);
    }
}
void MainWindow::tagChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    NoteItem* activeItem = NoteItem::getActiveItem();
    //Do NOT update notelist whenever editing a note
    if(activeItem && !activeItem->isReadOnly()) 
        return;

    QModelIndexList modelList = ui->tagView->selectionModel()->selectedIndexes();
    m_tagList.clear();
    for(int i =0 ; i < modelList.size() ; i++) {
        m_tagList << modelList[i].data().toString();
    }
    if(m_tagList.indexOf(tr("All")) != -1) 
        m_tagList.clear();
    else
        m_tagList.sort();
    loadNotes();
}
void MainWindow::splitterMoved()
{
    resizeEvent(0);
}
int MainWindow::insertNote(QString& title, QString& content, QString& tag, QString& hashKey, QString& datetime)
{
    int ret = 2;
    QString sql = "select rowid from notes where hash='"+hashKey+"'";
    m_q->Sql(sql.toUtf8());
    bool got = m_q->FetchRow();
    m_q->FreeQuery();
    if(got) 
        ret = 1;
    else {
        title.replace("'","''");
        content.replace("'","''");
        sql = "INSERT INTO notes(title, content, tag, hash) VALUES('";
        sql += title+"','";
        sql += content+"','";
        sql += tag+"','";
        sql += hashKey+"')";
        m_q->SqlStatement(sql.toUtf8());
        sql = QString("INSERT INTO notes_attr(created) VALUES('%1')").arg(datetime);
        m_q->SqlStatement(sql.toUtf8());
        ret = 0;
    }
    return ret;
}
bool MainWindow::insertNoteRes(QString& res_name, int noteId, int res_type, const QByteArray& res_data)
{
    m_q->Sql("INSERT INTO notes_res(res_name,noteid,res_type,res_data) values (?, ?, ?, ?)");
    m_q->BindString(1,res_name.toStdString());
    m_q->BindInt(2,noteId);
    m_q->BindInt(3,res_type);
    m_q->BindBlob(4, res_data.data(), res_data.size());
    m_q->ExecuteAndFree();
    return true;
}
int MainWindow::lastInsertId()
{
    return m_db->GetLastInsertRowId();
}
bool MainWindow::saveNote(int row, QString& title, QString& content, QStringList& tags, QString& datetime)
{
    bool ret = false;
    QString hashKey = QCryptographicHash::hash(content.toUtf8(), QCryptographicHash::Sha1).toHex();
    if(title != "" && !tags.empty()) {
        int i, tagSize = tags.size();
        int *newTagCount = new int[tagSize];
        for(i=0; i<tagSize; ++i) {
            newTagCount[i] = getTagCount(tags[i]);
        }
        if(row > 0) {
            QString sql = "select rowid from notes where hash='"+hashKey+"'";
            m_q->Sql(sql.toUtf8());
            int conflictRow = row;
            if(m_q->FetchRow()) 
                conflictRow = m_q->GetColumnInt(0);
            m_q->FreeQuery();

            if(conflictRow != row) {
                statusMessage(tr("There exists a note with the same content, thus I will NOT same this one."));
            }
            else {
                QStringList oldTags = getTagsOf(row);
                QStringList ups;
                title.replace("'","''");
                content.replace("'","''");
                sql = "UPDATE notes SET ";
                if(title != "")
                    ups << "title='"+title+"'";
                if(content != "") {
                    ups << "content='"+content+"'";
                    ups << "hash='"+hashKey+"'";
                }
                ups << "tag='"+tags.join(",")+"'";
                sql += ups.join(",");
                sql += QString(" WHERE rowid=%1").arg(row);
                m_q->SqlStatement(sql.toUtf8());
                int oldTagSize = oldTags.size();
                for(i=0; i<oldTagSize; ++i) {
                    if(getTagCount(oldTags[i]) == 0)
                        emit tagRemoved(oldTags[i]);
                }
                for(i=0; i<tagSize; ++i) {
                    if(newTagCount[i] == 0)
                        emit tagAdded(tags[i]);
                }
                ret = true;
            }
        }
        else {
            QString tag = tags.join(",");
            if(insertNote(title, content, tag, hashKey, datetime) == 0) {
                ret = true;
                for(i=0; i<tagSize; ++i) {
                    if(newTagCount[i] == 0)
                        emit tagAdded(tags[i]);
                }
            }
            else
                statusMessage(tr("There exists a note with the same content, thus I will NOT same this one."));
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
    else if(QEvent::KeyPress == type) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(evt);
        int k = keyEvent->key();
        if(k == Qt::Key_Escape) {
            NoteItem* activeItem = NoteItem::getActiveItem();
            if(activeItem && !activeItem->isReadOnly()) {
                cancelEdit();
                loadNotes();
            }
            else if(!ui->searchBox->text().isEmpty())
                ui->searchBox->setText("");
            else
                showMinimized();
            return true;
        }
    }
    return QMainWindow::event(evt);
}
QStringList MainWindow::getTagsOf(int row)
{
    QStringList tags;
    QString sql = QString("select tag from notes where rowid=%1").arg(row);
    m_q->Sql(sql.toUtf8());
    if(m_q->FetchRow()) {
        tags = QString::fromUtf8((char*)m_q->GetColumnCString(0)).split(",");
    }
    m_q->FreeQuery();
    return tags;
}
int MainWindow::getTagCount(const QString& tag)
{
    int ret = 0;
    QString sql = QString("select count(*) from notes where (%1)").arg(tag_like);
    sql = sql.replace(QRegExp("KEYWORD"),tag);
    m_q->Sql(sql.toUtf8());
    if(m_q->FetchRow()) {
        ret = m_q->GetColumnInt(0);
    }
    m_q->FreeQuery();
    return ret;
}
void MainWindow::addTag(const QString& tag)
{
    QStringList lst = m_tagModel->stringList();
    int i = 0, len = lst.count();
    while(i < len && lst[i] < tag) {
        i++;
    }
    if(m_tagModel->insertRows(i,1)) 
        m_tagModel->setData(m_tagModel->index(i),tag);
}
void MainWindow::removeTag(const QString& tag)
{
    int i = m_tagModel->stringList().indexOf(tag);
    if(i > 0) 
        m_tagModel->removeRows(i,1);
}
bool MainWindow::delActiveNote()
{
    NoteItem* activeItem = NoteItem::getActiveItem();
    int row = activeItem->getNoteId();
    bool ret = false;
    if(row > 0) {
        QStringList oldTags = getTagsOf(row);
        QString sql = QString("delete from notes where rowid=%1").arg(row);
        m_q->SqlStatement(sql.toUtf8());
        sql = QString("delete from notes_attr where rowid=%1").arg(row);
        m_q->SqlStatement(sql.toUtf8());
        sql = QString("delete from notes_res where noteid=%1").arg(row);
        m_q->SqlStatement(sql.toUtf8());

        NoteItem* item = ui->noteList->getNextNote(activeItem);
        NoteItem::setActiveItem(item);
        ui->noteList->removeNote(activeItem);

        int i, oldTagSize = oldTags.size();
        for(i=0; i<oldTagSize; ++i) {
            if(getTagCount(oldTags[i]) == 0)
                emit tagRemoved(oldTags[i]);
        }
        ret = true;
    }
    else
        ret = true;
    return ret;
}
void MainWindow::newPlainNote()
{
    newNote(false);
}
void MainWindow::newHTMLNote()
{
    newNote(true);
}
void MainWindow::newNote(bool rich)
{
    ui->noteList->clear();
    NoteItem *noteItem = new NoteItem(ui->noteList,0,false,rich);
    ui->noteList->addNote(noteItem);
    noteItem->autoSize();
    NoteItem::setActiveItem(noteItem);
    ui->action_Save_Note->setEnabled(true);
    resizeEvent(0);
}
void MainWindow::editActiveNote()
{
    NoteItem* activeItem = NoteItem::getActiveItem();
    int noteId = activeItem->getNoteId();

    NoteItem *noteItem = new NoteItem(ui->noteList,noteId,false, activeItem->isRich());
    ui->noteList->clear();
    ui->noteList->addNote(noteItem);
    noteItem->autoSize();
    NoteItem::setActiveItem(noteItem);
    ui->action_Save_Note->setEnabled(true);
    resizeEvent(0);
}
void MainWindow::setCurrentTag(const QString& tag) {
    ui->tagView->setCurrentIndex(m_tagModel->index(m_tagModel->stringList().indexOf(tag)));
}
void MainWindow::saveNote()
{
    NoteItem* activeItem = NoteItem::getActiveItem();
    activeItem->saveNote();
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
    m_q->Sql("select distinct tag from notes");
    QString tag;
    while(m_q->FetchRow()) {
        tagList << QString::fromUtf8((char*)m_q->GetColumnCString(0)).split(",");
    }
    m_q->FreeQuery();
    tagList.sort();
    tagList.removeDuplicates();
    tagList.prepend(tr("All"));
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
    QFile file(m_file);
    if(m_action == 0) {
        if (file.open(QIODevice::ReadOnly)) {
            int success = 0, fail = 0;
            QFile log(m_file+".log");
            QString logString;
            log.open(QIODevice::WriteOnly | QIODevice::Append);

            QXmlStreamReader xml(&file);
            QString title,datetime,link,tag,content,hashKey,res_name,res_type;
            int res_flag = 0;
            QByteArray res_data;
            SQLiteStatement *q = g_mainWindow->getSqlQuery();
            q->Sql("select rowid from notes order by rowid desc limit 1");
            int noteId = 1;
            if(q->FetchRow())
                noteId = q->GetColumnInt(0)+1;
            q->FreeQuery();
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
                        hashKey = QCryptographicHash::hash(content.toUtf8(), QCryptographicHash::Sha1).toHex();
                        int ret = g_mainWindow->insertNote(title,content,tag,hashKey,datetime);
                        if(ret == 0) {
                            success++;
                            noteId++;
                        }
                        else if(ret == 1) {
                            fail++;
                            logString = tr("Note already exists in the notes library: %1\n").arg(title); 
                            log.write(logString.toLocal8Bit());
                        }
                        else {
                            fail++;
                            logString = tr("SQL Error: %1\n").arg(title);
                            log.write(logString.toLocal8Bit());
                        }
                        emit importMsg(tr("Status of importing notes from %1:\n\nSuccess:\t%2\nFail:\t%3\nLog:\t%4.log").arg(m_file).arg(success).arg(fail).arg(m_file));
                    }
                    else if(xml.name() == "resource") {
                        res_flag = 0;
                        g_mainWindow->insertNoteRes(res_name,noteId,res_type.toInt(),res_data);
                    }
                }
                xml.readNext();
            }
            file.close();
            logString = tr("\n\nSummary:\n\nSuccess:\t%2\nFail:\t%3").arg(m_file).arg(success).arg(fail);
            log.write(logString.toLocal8Bit());
        }
    }
    else if(m_action == 1){
        if (file.open(QIODevice::WriteOnly)) {
            QByteArray xmlData = QByteArray();

            QXmlStreamWriter writer(&xmlData);
            writer.setAutoFormatting(true);

            writer.writeStartDocument();

            writer.writeStartElement("wikenotes");

            SQLiteStatement* q = g_mainWindow->getFoundNote(-1);
            SQLiteStatement q_res(*q);
            int noteId;
            while(q->FetchRow()) {
                noteId = q->GetColumnInt(0);
                writer.writeStartElement("note");
                writer.writeAttribute("title", QString::fromUtf8((char*)q->GetColumnCString(1)));
                writer.writeAttribute("tags", QString::fromUtf8((char*)q->GetColumnCString(3)));
                writer.writeAttribute("created", QString::fromUtf8((char*)q->GetColumnCString(4)));
                writer.writeCDATA(QString::fromUtf8((char*)q->GetColumnCString(2)));

                q_res.Sql(QString("select res_name,res_type,res_data from notes_res where noteid=%1").arg(noteId).toUtf8());
                QByteArray res_data;
                while(q_res.FetchRow()) {
                    writer.writeStartElement("resource");
                    writer.writeAttribute("name", QString::fromUtf8((char*)q_res.GetColumnCString(0)));
                    writer.writeAttribute("type", QString::fromUtf8((char*)q_res.GetColumnCString(1)));
                    res_data = QByteArray((char*)q_res.GetColumnBlob(2), q_res.GetColumnBytes(2));
                    writer.writeCDATA(res_data.toBase64());
                    writer.writeEndElement();
                }
                q->FreeQuery();

                writer.writeEndElement();
            }
            q->FreeQuery();

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
    QString fileName = QFileDialog::getOpenFileName(this, tr("Import Notes"), ".", tr("xml files (*.xml)"));

    if (!fileName.isEmpty()) {
        m_importer.importFile(fileName);
        m_importDialog->setFinishFlag(false);
        m_importDialog->exec();
    }
}
void MainWindow::exportNotes()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export Notes"), ".", tr("xml files (*.xml)"));

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
    s_fontMetrics = QFontMetrics(s_font);
    if (ok) {
        m_bSettings = true;
        ui->noteList->setTextNoteFont(s_font);
    }
}
void MainWindow::setHotKey()
{
    QKeySequence tm = m_hkToggleMain->shortcut();
    QKeySequence ntn = m_hkNewTextNote->shortcut();
    HotkeySettings diag(tm, ntn, this);
    m_hkToggleMain->setShortcut(QKeySequence());
    m_hkNewTextNote->setShortcut(QKeySequence());
    if(diag.exec() == QDialog::Accepted) {
        m_bSettings = true;
        m_hkToggleMain->setShortcut(diag.m_hkTM);
        m_hkNewTextNote->setShortcut(diag.m_hkNTN);
    }
    else {
        m_hkToggleMain->setShortcut(tm);
        m_hkNewTextNote->setShortcut(ntn);
    }
}
void MainWindow::changeLanguage()
{
    QAction *act = qobject_cast<QAction*>(sender());
    QString lang = act->toolTip();

    if(lang == "English") {
        qApp->removeTranslator(&m_translator);
    }
    else if(lang == "Chinese") {
        m_translator.load("chinese.qm", ".");
        qApp->installTranslator(&m_translator);
    }
    if(m_lang != lang) {
        m_bSettings = true;
        m_lang = lang;
        ui->retranslateUi(this);
        m_tagModel->setData(m_tagModel->index(0),tr("All"));
    }
}
void MainWindow::usage()
{
    QFile file;
    file.setFileName(":/help.html");
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
    QMessageBox::about(this, "WikeNotes", tr("Version: "APP_VERSION"\nAuthor: hzgmaxwell@hotmail.com"));
}
void MainWindow::cancelEdit()
{
    ui->action_Save_Note->setEnabled(false);
}
void MainWindow::noteSelected(bool has, bool htmlView)
{
    ui->action_Delete_Selected_Note->setEnabled(has);
    ui->action_Edit_Selected_Note->setEnabled(has);
}
void MainWindow::ensureVisible(NoteItem* item)
{
    ui->scrollArea->ensureWidgetVisible(item);
}
void MainWindow::extProcFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess *proc = qobject_cast<QProcess*>(sender());
    if(proc) {
        QString fn = proc->property("file_name").toString();
        if(fn != "") {
            QFile::remove(fn);
        }
        m_extProcs.removeOne(proc);
        delete proc;
    }
}
void MainWindow::extActions()
{
    QAction *act = qobject_cast<QAction*>(sender());
    QString cmdLine = act->data().toString();
    cmdLine = cmdLine.replace("%select%", NoteItem::getActiveItem()->selectedText());

    QRegExp rx("^http://.*", Qt::CaseInsensitive);
    if(rx.exactMatch(cmdLine))
        QDesktopServices::openUrl(QUrl(cmdLine));
    else {
        int s = cmdLine.indexOf("<");
        if(s == -1)
            QProcess::startDetached(cmdLine);
        else {
            QByteArray msg = cmdLine.mid(s+1).trimmed().toLocal8Bit();
            QString fn = QDir::tempPath()+QDir::separator()+QCryptographicHash::hash(msg, QCryptographicHash::Sha1).toHex()+".txt";
            QFile file(fn);
            file.open(QIODevice::WriteOnly);
            file.write(msg);
            file.close();
            
            cmdLine = cmdLine.replace(s, cmdLine.size()-s, fn);
            QProcess* proc = new QProcess;
            proc->setProperty("file_name",fn);
            connect(proc, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(extProcFinished(int, QProcess::ExitStatus)));
            proc->start(cmdLine);
            m_extProcs.append(proc);
        }
    }
}
const QList<QAction*>& MainWindow::getExtActions()
{
    return m_extActions;
}

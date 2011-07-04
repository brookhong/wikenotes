#include "mainwindow.h"
#include "openssl/aes.h"
#include "pagelist.h"
#include "accountsettings.h"
#include "hotkeysettings.h"
#include "ui_mainwindow.h"
#ifdef WIN32
#include <windows.h>
#endif
#ifndef NDEBUG
const QString C_ServerAddr = "localhost";
#else
const QString C_ServerAddr = "wikenotes.com";
#endif
QString MainWindow::s_query;
QFont MainWindow::s_font("Tahoma", 10);
QFontMetrics MainWindow::s_fontMetrics(MainWindow::s_font);
TagCompleter MainWindow::s_tagCompleter;
const int page_size = 20;
const char* query_like[] = {
    "(content like '%KEYWORD%' escape '\\' or title like '%KEYWORD%' escape '\\' or tag like '%KEYWORD%' escape '\\')",
    "(content like '%KEYWORD%' escape '\\' or title like '%KEYWORD%' escape '\\')",
    "(content like '%KEYWORD%' escape '\\')",
    "(title like '%KEYWORD%' escape '\\')",
    "(tag like '%KEYWORD%' escape '\\')",
};
const char* sort_by[] = {
    "create_time",
    "title",
    "tag",
    "length(content)",
};
const char* tag_like = "tag like 'KEYWORD' or tag like 'KEYWORD,%' or tag like '%,KEYWORD,%' or tag like '%,KEYWORD'";


QStringList TagCompleter::splitPath(const QString &path) const
{
    QString p = path;
    p = p.replace(QString::fromUtf8("，"),",");
    QStringList lst = p.split(",");
    QString last = lst[lst.size()-1];
    return QStringList(last);
}
QString TagCompleter::pathFromIndex(const QModelIndex &index) const
{
    QString path = QCompleter::pathFromIndex(index);
    QLineEdit *le = qobject_cast<QLineEdit*>(widget());
    QString tags = le->text();
    tags = tags.replace(QString::fromUtf8("，"),",");
    QStringList lst = tags.split(',');
    int len = lst.size();
    if(len > 1) {
        lst[len-1] = path;
        path = lst.join(",");
    }
    return path;
}
void PageList::focusInEvent(QFocusEvent * e)
{
    if(m_pageNum != count()) {
        QStringList pages;
        for(int i=1; i<=m_pageNum;i++)
            pages << QString("%1").arg(i);
        clear();
        insertItems(0, pages);
    }
}
//QFile g_log;
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_syncLog(QCoreApplication::applicationDirPath()+"/wikenotes.sync.log")
{
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkFinished(QNetworkReply*)));

    ui->setupUi(this);
    ui->comboBoxPage->insertItem(0,"1");
    ui->comboBoxPage->setFixedWidth(40);
    createTrayIcon();

    m_hkToggleMain = new QxtGlobalShortcut(this);
    connect(m_hkToggleMain, SIGNAL(activated()), this, SLOT(toggleVisibility()));
    m_hkToggleMain->setShortcut(QKeySequence("Alt+Q"));

    m_hkNewTextNote = new QxtGlobalShortcut(this);
    connect(m_hkNewTextNote, SIGNAL(activated()), this, SLOT(silentNewTextNote()));
    m_hkNewTextNote->setShortcut(QKeySequence("Ctrl+1"));

    m_hkNewHtmlNote = new QxtGlobalShortcut(this);
    connect(m_hkNewHtmlNote, SIGNAL(activated()), this, SLOT(silentNewHtmlNote()));
    m_hkNewHtmlNote->setShortcut(QKeySequence("Ctrl+2"));

    m_leftPanel = true;
    connect(ui->action_Tag_List, SIGNAL(triggered()), this, SLOT(changeLeftPanel()));
    connect(ui->actionMonthly_List, SIGNAL(triggered()), this, SLOT(changeLeftPanel()));

    m_uid = 0;
    m_user = "";
    m_pass = "";
    m_syncMode = 0;
    m_syncTimer = 0;

    m_syncLog.open(QIODevice::WriteOnly | QIODevice::Append);
    loadSettings();

    QAction *action_English = new QAction(this);
    action_English->setText("en_US");
    ui->menu_Language->addAction(action_English);
    connect(action_English, SIGNAL(triggered()), this, SLOT(changeLanguage()));

    QDir dir = QDir(QCoreApplication::applicationDirPath());
    QStringList files = dir.entryList(QStringList("*.qm"), QDir::Files);
    foreach (QString str,files) {
        action_English = new QAction(this);
        action_English->setText(str.replace(".qm",""));
        ui->menu_Language->addAction(action_English);
        connect(action_English, SIGNAL(triggered()), this, SLOT(changeLanguage()));
    }

    m_importDialog = new ImportDialog(this);
    ui->scrollArea->setStyleSheet("#scrollArea { background-color : white;}");
    ui->vsplitter->setHandleWidth(1);
    ui->vsplitter->setStretchFactor(0, 0);
    ui->vsplitter->setStretchFactor(1, 1);
    QList<int> rightSizes;
    rightSizes<<160<<480;
    ui->vsplitter->setSizes(rightSizes);

    ui->searchBox->setStyleSheet("#searchBox {padding: 0 18px;background:url(:/search.png) no-repeat}");
    connect(ui->searchBox, SIGNAL(textChanged(const QString&)), this, SLOT(instantSearch(const QString&)));
    connect(ui->comboBoxMatch, SIGNAL(currentIndexChanged(int)), this, SLOT(loadNotes()));
    connect(ui->comboBoxSort, SIGNAL(currentIndexChanged(int)), this, SLOT(loadNotes()));
    connect(ui->checkBox, SIGNAL(stateChanged(int)), this, SLOT(loadNotes()));
    connect(ui->comboBoxPage, SIGNAL(currentIndexChanged(int)), this, SLOT(loadPage()));
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
    connect(ui->action_Publish_Notes, SIGNAL(triggered()), this, SLOT(syncNotes()));
    connect(ui->action_Logon_Server, SIGNAL(triggered()), this, SLOT(accountSettings()));
    connect(ui->actionLog_Off, SIGNAL(triggered()), this, SLOT(requestLogoff()));
    connect(ui->actionText_Note_Font, SIGNAL(triggered()), this, SLOT(setNoteFont()));
    connect(ui->action_Hotkey_Settings, SIGNAL(triggered()), this, SLOT(setHotKey()));
    connect(ui->actionUsage, SIGNAL(triggered()), this, SLOT(usage()));
    connect(ui->action_About, SIGNAL(triggered()), this, SLOT(about()));

    connect(&m_importer, SIGNAL(importMsg(const QString&)), m_importDialog, SLOT(importMsg(const QString&)));
    connect(&m_importer, SIGNAL(importDone(int)), this, SLOT(importDone(int)));

    m_monthModel = new QStringListModel();
    m_tagModel = new QStringListModel();
    ui->tagView->setModel(m_leftPanel?m_tagModel:m_monthModel);
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
    QSettings settings(QCoreApplication::applicationDirPath()+"/wikenotes.conf", QSettings::IniFormat, this);

    if(settings.contains("default_notes_library")) {
        QString dbName = settings.value("default_notes_library").toString();
        QDir dir(dbName.mid(0, dbName.lastIndexOf('/')));
        if(!dir.exists()) 
            QMessageBox::warning(this, "WikeNotes", tr("Directory %1 not exists, default notes library will be used.").arg(dir.path())); 
        else
            m_dbName = dbName;
    }
    if(m_dbName.isEmpty())
        m_dbName = QCoreApplication::applicationDirPath()+"/default.wike";

    if(settings.contains("font_name") || settings.contains("font_size")) {
        s_font = QFont(settings.value("font_name").toString(), settings.value("font_size").toInt());
        s_fontMetrics = QFontMetrics(s_font);
    }
    if(settings.contains("toggle_main_window")) 
        m_hkToggleMain->setShortcut(QKeySequence(settings.value("toggle_main_window").toString()));
    if(settings.contains("new_text_note")) 
        m_hkNewTextNote->setShortcut(QKeySequence(settings.value("new_text_note").toString()));
    if(settings.contains("new_html_note")) 
        m_hkNewHtmlNote->setShortcut(QKeySequence(settings.value("new_html_note").toString()));

    if(settings.contains("language")) 
        m_lang = settings.value("language").toString();
    if(m_lang.isEmpty()) 
        m_lang = QLocale::system().name();
    if(m_lang != "en_US" && m_translator.load(m_lang+".qm", ".")) {
        qApp->installTranslator(&m_translator);
        ui->retranslateUi(this);
    }

    if(settings.contains("taglist")) 
        m_leftPanel = (settings.value("language").toString() == "true");
    if(settings.contains("matched_in")) 
        ui->comboBoxMatch->setCurrentIndex(settings.value("matched_in").toInt());
    if(settings.contains("sort_by")) 
        ui->comboBoxSort->setCurrentIndex(settings.value("sort_by").toInt());
    if(settings.contains("reverse_sorting")) 
        ui->checkBox->setCheckState((settings.value("reverse_sorting").toInt()==0)?Qt::Unchecked:Qt::Checked);

    QAction* action = new QAction("google search", this);
    action->setData("http://www.google.com/#q=%select%");
    action->setShortcut(QKeySequence("Alt+G"));
    connect(action, SIGNAL(triggered()), this, SLOT(extActions()));
    m_extActions.append(action);

    action = new QAction("baidu search", this);
    action->setData("http://www.baidu.com/s?wd=%select%");
    action->setShortcut(QKeySequence("Alt+B"));
    connect(action, SIGNAL(triggered()), this, SLOT(extActions()));
    m_extActions.append(action);

    if(settings.contains("user")) 
        m_user = settings.value("user").toString();
    if(settings.contains("sync_mode")) 
        m_syncMode = settings.value("sync_mode").toUInt();
    if(settings.contains("pass")) {
        QByteArray safePass = QByteArray::fromBase64(settings.value("pass").toByteArray());
        unsigned char cipher1[64]={0};
        AES_KEY key;
        AES_set_decrypt_key((unsigned char *)sort_by[3],128,&key);
        AES_decrypt((unsigned char*)safePass.data(),cipher1,&key);
        m_pass = (char*)cipher1;
    }
}
void MainWindow::updateSettings(QSettings& settings, const QString& current, const QString& def, const QString& key)
{
    if(current != def && current != "")
        settings.setValue(key, current);
    else if(settings.contains(key))
        settings.remove(key);
}
void MainWindow::flushSettings()
{
    QSettings conf(QCoreApplication::applicationDirPath()+"/wikenotes.conf", QSettings::IniFormat, this);

    updateSettings(conf, m_dbName, QCoreApplication::applicationDirPath()+"/default.wike", "default_notes_library");
    updateSettings(conf, s_font.family(), "Tahoma", "font_name");
    updateSettings(conf, QString("%1").arg(s_font.pointSize()), "10", "font_size");
    updateSettings(conf, m_hkToggleMain->shortcut().toString(), "Alt+Q", "toggle_main_window");
    updateSettings(conf, m_hkNewTextNote->shortcut().toString(), "Ctrl+1", "new_text_note");
    updateSettings(conf, m_hkNewHtmlNote->shortcut().toString(), "Ctrl+2", "new_html_note");
    updateSettings(conf, m_lang, QLocale::system().name(), "language");
    updateSettings(conf, m_leftPanel?"true":"false", "true", "taglist");
    updateSettings(conf, QString("%1").arg(ui->comboBoxMatch->currentIndex()), "0", "matched_in");
    updateSettings(conf, QString("%1").arg(ui->comboBoxSort->currentIndex()), "0", "sort_by");
    updateSettings(conf, QString("%1").arg(ui->checkBox->checkState()), "0", "reverse_sorting");

    updateSettings(conf, m_user, "", "user");
    updateSettings(conf, QString("%1").arg(m_syncMode), "0", "sync_mode");

    if(m_pass != "") {
        AES_KEY key;
        AES_set_encrypt_key((unsigned char *)sort_by[3],128,&key);
        unsigned char cipher1[64]={0};

        AES_encrypt((unsigned char *)m_pass.toUtf8().data(), cipher1, &key);
        QString safePass = QByteArray::fromRawData((char*)cipher1,64).toBase64();
        conf.setValue("pass", safePass);
    }
    else 
        conf.remove("pass");
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

    QString content = QApplication::clipboard()->text();
    QString title = getTitleFromContent(content);
#ifdef WIN32
    ::Sleep(300);
    keybd_event(VK_F6, 0, 0, 0);
    keybd_event(VK_F6, 0, KEYEVENTF_KEYUP, 0);
    ::Sleep(300);
    keybd_event(VK_CONTROL,MapVirtualKey (VK_CONTROL, 0),0,0);
    keybd_event('C', MapVirtualKey ('C', 0), 0, 0);
    keybd_event('C', MapVirtualKey ('C', 0), KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL,MapVirtualKey (VK_CONTROL, 0),KEYEVENTF_KEYUP,0);
    ::Sleep(300);
    keybd_event(VK_F6, 0, 0, 0);
    keybd_event(VK_F6, 0, KEYEVENTF_KEYUP, 0);
#endif
    content = QApplication::clipboard()->text()+"\n"+content;

    _saveNote(0, title, content, QStringList(tr("Untagged")), false, 0, 2, 0, 0, NULL);
}
void MainWindow::silentNewHtmlNote()
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
    QString title = getTitleFromContent(clipboard->text());
    QString content = clipboard->mimeData()->html();
    content = content.replace(QRegExp("<!--StartFragment-->"),"");
    content = content.replace(QRegExp("<!--EndFragment-->"),"");

#ifdef WIN32
    ::Sleep(300);
    keybd_event(VK_F6, 0, 0, 0);
    keybd_event(VK_F6, 0, KEYEVENTF_KEYUP, 0);
    ::Sleep(300);
    keybd_event(VK_CONTROL,MapVirtualKey (VK_CONTROL, 0),0,0);
    keybd_event('C', MapVirtualKey ('C', 0), 0, 0);
    keybd_event('C', MapVirtualKey ('C', 0), KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL,MapVirtualKey (VK_CONTROL, 0),KEYEVENTF_KEYUP,0);
    ::Sleep(300);
    keybd_event(VK_F6, 0, 0, 0);
    keybd_event(VK_F6, 0, KEYEVENTF_KEYUP, 0);
#endif
    content = "<a href='"+QApplication::clipboard()->text()+"'>"+QApplication::clipboard()->text()+"</a><br>"+content;

    prepareSavingNote(0, title, content, QStringList(tr("Untagged")), 0, 2, 0, 0);
}
void MainWindow::prepareSavingNote(int noteId, QString title, QString content, QStringList tags, 
                int gid, int status, int createTime, int updateTime)
{
    SavingNote* savingNote = new SavingNote(noteId, title, content, tags, gid, status, createTime, updateTime);
    QRegExp rx("<\\s*img[^>]+src=[\"']([^\"']+)[\"'][^>]*>");
    QString imgName;
    QString imgElem;
    int pos = 0;
    while ((pos = rx.indexIn(content, pos)) != -1) {
        imgElem = rx.cap(0);
        imgName = rx.cap(1);
        if(imgElem.indexOf(" embed=\"link\"") == -1
                && imgName.indexOf("wike://") == -1
                && !savingNote->images.contains(imgName)) {
            QNetworkReply *reply = m_networkManager->get(QNetworkRequest(QUrl(imgName)));
            savingNote->images[imgName] = QImage();
            savingNote->countDown++;
            m_savingNotes[reply] = savingNote;
        }

        pos += rx.matchedLength();
    }
    if(savingNote->countDown == 0) {
        delete savingNote;
        savingNote = NULL;
        _saveNote(noteId, title, content, tags, true, gid, status, createTime, updateTime, NULL);
    }
}
void MainWindow::networkFinished(QNetworkReply* reply)
{
    QNetworkReply::NetworkError err = reply->error();
    QByteArray response;
    if(QNetworkReply::NoError == err) 
        response = reply->readAll();

    QNetworkAccessManager::Operation op = reply->operation();
    QString urlPath = reply->url().path();
    QString contentType = reply->rawHeader("Content-Type");
    if(op == QNetworkAccessManager::GetOperation) {
        if(urlPath == "/index.php/post/get") {
            if(QNetworkReply::NoError == err && contentType == "text/plain") {
                QStringList rs = QString(response).split(",");
                QMap<int,int> rNotes;
                int len = rs.size()-1;
                for(int i=0;i<len;i+=2 ) {
                    rNotes[rs[i].toUInt()] = rs[i+1].toUInt();
                }
                _syncNotes(rNotes);
            }
            else
                m_syncLog.write(QString("[%1] sync error: %2\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(err).toUtf8());
        }
        else if(m_savingNotes.contains(reply)) {
            QImage image;
            image.loadFromData(response);
            QByteArray sha1Sum = QCryptographicHash::hash(response, QCryptographicHash::Sha1);
            QString imgName = sha1Sum.toHex();

            SavingNote* savingNote = m_savingNotes[reply];
            savingNote->countDown--;
            QString imgUrl = reply->url().toString();
            savingNote->images.remove(imgUrl);
            savingNote->images[imgName] = image;
            savingNote->content = savingNote->content.replace(imgUrl, "wike://"+imgName);
            m_savingNotes.remove(reply);

            if(savingNote->countDown == 0) {
                _saveNote(savingNote->noteId, savingNote->title, savingNote->content, savingNote->tags, true,
                        savingNote->gid, savingNote->status, savingNote->created, savingNote->updated, savingNote);
                delete savingNote;
            }
        }
        else if(m_pullingNotes.contains(reply)) {
            if(QNetworkReply::NoError == err && contentType == "text/plain") {
                int sp = response.indexOf(":");
                QList<QByteArray> rs = response.left(sp).split(',');
                if(rs.size() == 8) {
                    //echo $id.",".$type.",".$status.",".$create_time.",".$update_time.",".strlen($title).",".strlen($content).",".strlen($tags).":";
                    //echo $title.$content.$tags;
                    sp++;
                    QString title = QString::fromUtf8(response.mid(sp,rs[5].toUInt()));
                    sp += rs[5].toUInt();
                    QString content = QString::fromUtf8(response.mid(sp,rs[6].toUInt()));
                    content = content.replace("src=\"/resources/","src=\"http://"+C_ServerAddr+"/resources/");
                    sp += rs[6].toUInt();
                    QStringList tags = QString::fromUtf8(response.mid(sp,rs[7].toUInt())).split(QRegExp("\\s*,\\s*"));

                    if(rs[1].toUInt() == 1)
                        prepareSavingNote(0, title, content, tags, rs[0].toUInt(), rs[2].toUInt(), rs[3].toUInt(), rs[4].toUInt());
                    else
                        _saveNote(0, title, content, tags, false, rs[0].toUInt(), rs[2].toUInt(), rs[3].toUInt(), rs[4].toUInt(), NULL);
                }
            }
            else
                m_syncLog.write(QString("[%1] pull note error: %2 - %3\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(err).arg(urlPath).toUtf8());
            m_pullingNotes.remove(reply);
        }
    }
    else if(op == QNetworkAccessManager::PostOperation) {
        if(urlPath == "/index.php/user/login") {
            if(QNetworkReply::NoError == err && contentType == "text/plain") {
                m_uid = QString(response).toUInt();
                if(m_uid > 0) {
                    ui->action_Publish_Notes->setEnabled(true);
                    ui->action_Logon_Server->setEnabled(false);
                    ui->actionLog_Off->setEnabled(true);
                    setWindowTitle(QString("WikeNotes (%1) %2@%3").arg(m_dbName).arg(m_user).arg(C_ServerAddr));
                    if(m_syncMode)
                        m_syncTimer = startTimer(3000);
                }
                else {
                    ui->action_Publish_Notes->setEnabled(false);
                    ui->action_Logon_Server->setEnabled(true);
                    ui->actionLog_Off->setEnabled(false);
                    setWindowTitle(QString("WikeNotes (%1)").arg(m_dbName));
                    QMessageBox::warning(this, "WikeNotes", tr("Failed to login.")); 
                }
            }
            else
                m_syncLog.write(QString("[%1] login error: %2\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(err).toUtf8());
        }
        else if(urlPath.startsWith("/post/delete/id/")) {
            if(QNetworkReply::NoError == err && contentType == "text/plain") 
                delNoteByGid(urlPath.mid(16).toUInt());
            else
                m_syncLog.write(QString("[%1] delete note error: %2 - %3\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(err).arg(urlPath).toUtf8());
        }
        else if(m_pushingNotes.contains(reply)) {
            if(QNetworkReply::NoError == err && contentType == "text/plain") {
                int gid = QString(response).toUInt();
                if(gid > 0) 
                    m_q->SqlStatement(QString("UPDATE notes SET gid=%1 where rowid=%2").arg(gid).arg(m_pushingNotes[reply]).toUtf8());
                m_pushingNotes.remove(reply);
            }
            else
                m_syncLog.write(QString("[%1] push note error: %2 - %3\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(err).arg(urlPath).toUtf8());
        }
    }
    reply->deleteLater();
}
void MainWindow::timerEvent(QTimerEvent *event)
{
    syncNotes();
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
    if(init) {        
        QString password = QInputDialog::getText(this, "WikeNotes",
                tr("Would you like to protect your notes library with a password?\nCancel to set no password.\n\nPassword: "), QLineEdit::Password);
        if (!password.isEmpty()) 
            m_q->SqlStatement("PRAGMA key = '"+QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha1).toHex()+"';");
        m_q->SqlStatement("CREATE TABLE notes(title TEXT, content TEXT, tag TEXT, hash TEXT, create_time TEXT, update_time INTEGER, gid INTEGER, status INTEGER)");
        m_q->SqlStatement("CREATE TABLE notes_res(res_name VARCHAR(40), noteid INTEGER, res_type INTEGER, res_data BLOB, CONSTRAINT unique_res_within_a_note UNIQUE (res_name, noteid) )");
        ret = true;
    }
    else {
        QString password;
        int retry = 0;
        while(retry < 3) {
            try {
                retry++;
                if (!password.isEmpty()) 
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
        refreshCat();
        loadNotes();
        m_dbName = dbName;
        if(m_dbName == QCoreApplication::applicationDirPath()+"/default.wike") {
            ui->action_Logon_Server->setEnabled(true);
            if(m_pass != "") 
                requestLogin();
        }
        else {
            ui->action_Logon_Server->setEnabled(false);
        }
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
    if(m_uid > 0)
        requestLogoff();
}
SQLiteStatement* MainWindow::getSqlQuery()
{
    return m_q;
}
SQLiteStatement* MainWindow::getFoundNote(int idx)
{
    QString sql;
    if(idx == -1)
        sql = QString("select rowid,title,content,tag,create_time,update_time,gid,status from notes %1 order by create_time asc").arg(m_criterion);
    else {
        sql = "select rowid,title,content,tag,create_time from notes "
            + m_criterion
            + " order by "
            + sort_by[ui->comboBoxSort->currentIndex()]
            + ((ui->checkBox->checkState() == Qt::Checked)?" asc":" desc")
            + QString(" limit 1 offset %1").arg(m_page*page_size+idx);
    }

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
    QString catString = "status!=3";
    if(!m_catList.empty()) {
        if(m_leftPanel) {
            catString = QString("( (%1) ").arg(tag_like).replace(QRegExp("KEYWORD"),m_catList[0]);
            for(int i =1 ; i < m_catList.size() ; i++) 
                catString += QString(" and (%1) ").arg(tag_like).replace(QRegExp("KEYWORD"),m_catList[i]);
            catString += ")";
        }
        else {
            catString = QString("( create_time like '%1%' ").arg(m_catList[0]);
            for(int i =1 ; i < m_catList.size() ; i++) 
                catString += QString("or create_time like '%1%' ").arg(m_catList[i]);
            catString += ")";
        }
    }
    m_criterion = "";
    if(s_query != "") {
        QString query = s_query;
        query.replace("'", "''");
        query.replace("%", "\\%");
        m_criterion  = query_like[ui->comboBoxMatch->currentIndex()];
        m_criterion  = " where " +m_criterion.replace(QRegExp("KEYWORD"),query)+" and "+catString;
    }
    else
        m_criterion = " where " + catString;

    QString sql;
    sql = QString("select count(*) from notes %1").arg(m_criterion);
    m_found = m_q->SqlAggregateFuncResult(sql.toUtf8());
    ui->comboBoxPage->m_pageNum = qCeil(m_found/(float)page_size);
    ui->comboBoxPage->setCurrentIndex(0);

    ui->statusbar->showMessage(tr("%1 notes found").arg(m_found));
    ui->action_Save_Note->setEnabled(false);

    loadPage();
}
void MainWindow::loadPage()
{
    ui->noteList->clear();
    m_page = ui->comboBoxPage->currentIndex();
    ui->noteList->extend((m_page < ui->comboBoxPage->m_pageNum-1)?page_size:m_found-m_page*page_size);
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
    m_catList.clear();
    for(int i =0 ; i < modelList.size() ; i++) {
        m_catList << modelList[i].data().toString();
    }
    if(m_catList.indexOf(tr("All")) != -1) {
        ui->tagView->selectionModel()->clearSelection();
        ui->tagView->setCurrentIndex(current);
    }
}
void MainWindow::tagChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QModelIndexList modelList = ui->tagView->selectionModel()->selectedIndexes();
    m_catList.clear();
    for(int i =0 ; i < modelList.size() ; i++) {
        m_catList << modelList[i].data().toString();
    }
    if(m_catList.indexOf(tr("All")) != -1) 
        m_catList.clear();
    else
        m_catList.sort();
    loadNotes();
}
void MainWindow::splitterMoved()
{
    resizeEvent(0);
}
int MainWindow::insertNote(QString& title, QString& content, QString& tag, QString& hashKey, QString& createTime, int updateTime, int gid, int status)
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
        sql = "INSERT INTO notes(title, content, tag, hash, create_time, update_time, gid, status) VALUES('";
        sql += title+"','";
        sql += content+"','";
        sql += tag+"','";
        sql += hashKey+"','";
        sql += createTime+"',";
        sql += QString("%1").arg(updateTime)+",";
        sql += QString("%1").arg(gid)+",";
        sql += QString("%1").arg(status)+")";
        m_q->SqlStatement(sql.toUtf8());
        ret = 0;

        QString month = createTime.mid(0,7);
        QStringList lst = m_monthModel->stringList();
        if(lst.indexOf(month) == -1) {
            int i = 1, len = lst.count();
            while(i < len && lst[i] < month) {
                i++;
            }
            if(m_monthModel->insertRows(i,1)) 
                m_monthModel->setData(m_monthModel->index(i),month);
        }
    }
    return ret;
}
QString MainWindow::getTitleFromContent(const QString& content)
{
    QString title;
    QString temp = content.simplified();
    int len = temp.length();
    if(len > 30) 
        title = temp.mid(0,28)+"...";
    else if(len == 0)
        title = tr("Untitled");
    else
        title = temp;
    return title;
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
    QString sql = QString("select count(*) from notes where (%1) and status!=3").arg(tag_like);
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
    int i = 1, len = lst.count();
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
void MainWindow::delNoteByGid(int gid)
{
    int row = 0;
    QString sql = QString("select rowid from notes where gid=%1").arg(gid);
    m_q->Sql(sql.toUtf8());
    if(m_q->FetchRow()) 
        row = m_q->GetColumnInt(0);
    m_q->FreeQuery();

    sql = QString("delete from notes where rowid=%1").arg(row);
    m_q->SqlStatement(sql.toUtf8());
    sql = QString("delete from notes_res where noteid=%1").arg(row);
    m_q->SqlStatement(sql.toUtf8());
}
void MainWindow::requestDeleteNote(int gid)
{
    QNetworkRequest req(QUrl(QString("http://"+C_ServerAddr+"/index.php/post/delete/id/%1").arg(gid)));
    m_networkManager->post(req, "");
}
void MainWindow::delActiveNote()
{
    NoteItem* activeItem = NoteItem::getActiveItem();
    int row = activeItem->getNoteId();
    int gid = activeItem->getGID();
    QString oldMonth;
    QString sql = QString("select create_time from notes where rowid=%1").arg(row);
    m_q->Sql(sql.toUtf8());
    if(m_q->FetchRow()) {
        oldMonth = (char*)m_q->GetColumnCString(0);
        oldMonth = oldMonth.mid(0,7);
    }
    m_q->FreeQuery();

    QStringList oldTags = getTagsOf(row);
    if(gid > 0) {
        //update status to deleted
        sql = QString("UPDATE notes SET status=3 WHERE rowid=%1").arg(row);
        m_q->SqlStatement(sql.toUtf8());
        if(m_uid > 0)
            requestDeleteNote(gid);
    }
    else {
        sql = QString("delete from notes where rowid=%1").arg(row);
        m_q->SqlStatement(sql.toUtf8());
        sql = QString("delete from notes_res where noteid=%1").arg(row);
        m_q->SqlStatement(sql.toUtf8());
    }

    NoteItem* item = ui->noteList->getNextNote(activeItem, 1);
    if(item == 0)
        item = ui->noteList->getNextNote(activeItem, -1);
    NoteItem::setActiveItem(item);
    ui->noteList->removeNote(activeItem);

    int i, oldTagSize = oldTags.size();
    for(i=0; i<oldTagSize; ++i) {
        if(getTagCount(oldTags[i]) == 0)
            removeTag(oldTags[i]);
    }

    sql = QString("select count(*) from notes where (create_time like '%1%')").arg(oldMonth);
    m_q->Sql(sql.toUtf8());
    if(m_q->FetchRow() && m_q->GetColumnInt(0) == 0) {
        i = m_monthModel->stringList().indexOf(oldMonth);
        if(i > 0) 
            m_monthModel->removeRows(i,1);
    }
    m_q->FreeQuery();
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
void MainWindow::setCurrentCat(const QString& cat) {
    if(m_catList.size() == 1 && m_catList[0] == cat)
        loadNotes();
    else {
        QModelIndex idx;
        if(m_leftPanel) 
            idx = m_tagModel->index(m_tagModel->stringList().indexOf(cat));
        else 
            idx = m_monthModel->index(m_monthModel->stringList().indexOf(cat));
        ui->tagView->setCurrentIndex(idx);
    }
}
void MainWindow::_saveNote(int noteId, QString title, QString content, QStringList tags, bool rich,
                int gid, int status, int createTime, int updateTime, SavingNote* savingNote)
{
    QString sql;

    if(noteId == 0 && gid > 0) {
        sql = QString("select rowid from notes where gid=%1").arg(gid);
        m_q->Sql(sql.toUtf8());
        if(m_q->FetchRow()) 
            noteId = m_q->GetColumnInt(0);
        m_q->FreeQuery();
    }
    QDateTime updated = QDateTime::currentDateTime();
    if(updateTime > 0)
        updated = QDateTime::fromTime_t(updateTime);
    else
        updateTime = updated.toTime_t();

    QString created;
    if(createTime > 0) 
        created = QDateTime::fromTime_t(createTime).toString("yyyy-MM-dd hh:mm:ss");
    else 
        created = updated.toString("yyyy-MM-dd hh:mm:ss");

    bool ret = false;
    QString hashKey = QCryptographicHash::hash(content.toUtf8(), QCryptographicHash::Sha1).toHex();
    if(!tags.empty()) {
        int i, tagSize = tags.size();
        int *newTagCount = new int[tagSize];
        for(i=0; i<tagSize; ++i) {
            newTagCount[i] = getTagCount(tags[i]);
        }
        if(noteId > 0) {
            sql = "select rowid from notes where hash='"+hashKey+"'";
            m_q->Sql(sql.toUtf8());
            int conflictRow = noteId;
            if(m_q->FetchRow()) 
                conflictRow = m_q->GetColumnInt(0);
            m_q->FreeQuery();

            if(conflictRow != noteId) {
                statusMessage(tr("There exists a note with the same content, thus I will NOT same this one."));
            }
            else {
                QStringList oldTags = getTagsOf(noteId);
                QStringList ups;
                title.replace("'","''");
                content.replace("'","''");
                sql = "UPDATE notes SET ";
                ups << "title='"+title+"'";
                ups << "content='"+content+"'";
                ups << "tag='"+tags.join(",")+"'";
                ups << "hash='"+hashKey+"'";
                ups << "update_time="+QString("%1").arg(updateTime);
                ups << "status="+QString("%1").arg(status);
                sql += ups.join(",");
                sql += QString(" WHERE rowid=%1").arg(noteId);
                m_q->SqlStatement(sql.toUtf8());
                int oldTagSize = oldTags.size();
                for(i=0; i<oldTagSize; ++i) {
                    if(getTagCount(oldTags[i]) == 0)
                        removeTag(oldTags[i]);
                }
                for(i=0; i<tagSize; ++i) {
                    if(newTagCount[i] == 0)
                        addTag(tags[i]);
                }
                ret = true;
            }
        }
        else {
            QString tag = tags.join(",");
            if(insertNote(title, content, tag, hashKey, created, updateTime, gid, status) == 0) {
                ret = true;
                for(i=0; i<tagSize; ++i) {
                    if(newTagCount[i] == 0)
                        addTag(tags[i]);
                }
            }
            else
                statusMessage(tr("There exists a note with the same content, thus I will NOT same this one."));
        }
        delete newTagCount;
    }

    if(ret) {
        if(rich) {
            QStringList res_to_remove;
            QStringList res_to_add;
            QStringList::Iterator it;
            QRegExp rx("<img[^>]*src=\"wike://([0-9a-f]+)\"[^>]*>");
            int pos = 0;
            QString imgName, sql;
            while ((pos = rx.indexIn(content, pos)) != -1) {
                imgName = rx.cap(1);
                if(!res_to_add.contains(imgName)) 
                    res_to_add << imgName;

                pos += rx.matchedLength();
            }
            if(noteId == 0)
                noteId = lastInsertId();
            else {
                sql = QString("select res_name from notes_res where noteid=%1").arg(noteId);
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
                sql = QString("delete from notes_res where noteid=%1 and res_name='%2'").arg(noteId).arg(*it);
                m_q->SqlStatement(sql.toUtf8());
            }

            for(it = res_to_add.begin(); it != res_to_add.end(); it++) {
                imgName = *it;
                QImage image = savingNote->images[imgName];

                QBuffer buffer;
                QImageWriter writer(&buffer, "PNG");
                writer.write(image);

                insertNoteRes(imgName, noteId, (int)QTextDocument::ImageResource, buffer.data());
            }
        }
        if(m_leftPanel)
            setCurrentCat(tags[0]);
        else 
            setCurrentCat(created.mid(0,7));
    }
}
void MainWindow::saveNote()
{
    NoteItem* activeItem = NoteItem::getActiveItem();
    QString content = activeItem->getContent();
    if(activeItem->isRich())
        prepareSavingNote(activeItem->getNoteId(), activeItem->getTitle(), content, activeItem->getTags(),
                activeItem->getGID(), activeItem->getStatus(), 0, 0);
    else
        _saveNote(activeItem->getNoteId(), activeItem->getTitle(), content, activeItem->getTags(), false,
                activeItem->getGID(), activeItem->getStatus(), 0, 0, NULL);
}
void MainWindow::statusMessage(const QString& msg)
{
    ui->statusbar->showMessage(msg);
}
void MainWindow::importDone(int action)
{
    if(action == 0)
        refreshCat();
    m_importDialog->setFinishFlag(true);
    m_importDialog->close();
}
void MainWindow::refreshCat()
{
    QStringList tagList;
    m_q->Sql("select distinct tag from notes where status!=3");
    while(m_q->FetchRow()) {
        tagList << QString::fromUtf8((char*)m_q->GetColumnCString(0)).split(",");
    }
    m_q->FreeQuery();
    tagList.sort();
    tagList.removeDuplicates();
    tagList.prepend(tr("All"));
    m_tagModel->setStringList(tagList);

    QStringList monthList;
    m_q->Sql("select distinct substr(create_time,0,8) from notes where status!=3");
    while(m_q->FetchRow()) {
        monthList << (char*)m_q->GetColumnCString(0);
    }
    m_q->FreeQuery();
    monthList.sort();
    monthList.prepend(tr("All"));
    m_monthModel->setStringList(monthList);

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
void NotesImporter::doImport()
{
    QFile file(m_para);
    if (file.open(QIODevice::ReadOnly)) {
        int success = 0, fail = 0;
        QFile log(m_para+".log");
        QString logString;
        log.open(QIODevice::WriteOnly | QIODevice::Append);

        QXmlStreamReader xml(&file);
        QString title,create_time,link,tag,content,hashKey,res_name,res_type;
        int gid, status, update_time;
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
                    create_time = attrs.value("created").toString();
                    if(attrs.hasAttribute("updated"))
                        update_time = attrs.value("updated").toString().toUInt();
                    else
                        update_time = QDateTime::fromString(create_time, "yyyy-MM-dd hh:mm:ss").toTime_t();
                    if(attrs.hasAttribute("gid"))
                        gid = attrs.value("gid").toString().toUInt();
                    else
                        gid = 0;
                    if(attrs.hasAttribute("status"))
                        status = attrs.value("status").toString().toUInt();
                    else
                        status = 2;
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
                    int ret = g_mainWindow->insertNote(title,content,tag,hashKey,create_time,update_time,gid,status);
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
                    emit importMsg(tr("Status of importing notes from %1:\n\nSuccess:\t%2\nFail:\t%3\nLog:\t%4.log").arg(m_para).arg(success).arg(fail).arg(m_para));
                }
                else if(xml.name() == "resource") {
                    res_flag = 0;
                    g_mainWindow->insertNoteRes(res_name,noteId,res_type.toInt(),res_data);
                }
            }
            xml.readNext();
        }
        file.close();
        logString = tr("\nSummary of importing notes from %1:\nSuccess:\t%2\nFail:\t%3\n").arg(m_para).arg(success).arg(fail);
        log.write(logString.toLocal8Bit());
    }
}
void NotesImporter::doExport()
{
    QFile file(m_para);
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
            writer.writeAttribute("created", (char*)q->GetColumnCString(4));
            writer.writeAttribute("updated", QString("%1").arg(q->GetColumnInt(5)));
            writer.writeAttribute("gid", QString("%1").arg(q->GetColumnInt(6)));
            writer.writeAttribute("status", QString("%1").arg(q->GetColumnInt(7)));
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
            q_res.FreeQuery();

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
void NotesImporter::run()
{
    if(m_action == 0) 
        doImport();
    else if(m_action == 1)
        doExport();
    emit importDone(m_action);
}
void MainWindow::importNotes()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Import Notes"), ".", tr("xml files (*.xml)"));

    if (!fileName.isEmpty()) {
        m_importer.start(0, fileName);
        m_importDialog->setFinishFlag(false);
        m_importDialog->exec();
    }
}
void MainWindow::exportNotes()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export Notes"), ".", tr("xml files (*.xml)"));

    if (!fileName.isEmpty()) {
        m_importer.start(1, fileName);
        m_importDialog->setFinishFlag(false);
        m_importDialog->exec();
    }
}
void MainWindow::syncNotes()
{
    QNetworkRequest req;
    req.setRawHeader("User-Agent", "wikenotes");
    req.setUrl(QUrl("http://"+C_ServerAddr+"/index.php/post/get"));
    m_networkManager->get(req);
}
void MainWindow::_syncNotes(QMap<int,int>& notesData)
{
    //0: notes_to_push; 1: notes_to_pull; 2: notes_to_rm; 3: notes_to_rm_remote
    QVector<int> notes_to_op[4];
    SQLiteStatement q(*m_q);
    q.Sql("select rowid,update_time,gid,status from notes");
    while(q.FetchRow()) {
        int rid = q.GetColumnInt(0);
        int gid = q.GetColumnInt(2);
        if(gid == 0) 
            notes_to_op[0].append(rid);
        else {
            if(notesData.contains(gid)) {
                if(q.GetColumnInt(3) == 3)
                    notes_to_op[3].append(gid);
                else if(notesData[gid] > q.GetColumnInt(1))
                    notes_to_op[1].append(gid);
                else if(notesData[gid] < q.GetColumnInt(1))
                    notes_to_op[0].append(rid);
                notesData.remove(gid);
            }
            else
                notes_to_op[2].append(rid);
        }
    }
    q.FreeQuery();
    QMap<int,int>::iterator it;
    for(it= notesData.begin(); it != notesData.end(); it++) 
        notes_to_op[1].append(it.key());

    QString sql;
    int i;
    for(i=0; i<notes_to_op[3].size(); i++) {
        requestDeleteNote(notes_to_op[3][i]);
        m_syncLog.write(QString("[%1] request to delete note (gid: %2)\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(notes_to_op[3][i]).toUtf8());
    }
    for(i=0; i<notes_to_op[2].size(); i++) {
        sql = QString("delete from notes where rowid=%1").arg(notes_to_op[2][i]);
        q.SqlStatement(sql.toUtf8());
        sql = QString("delete from notes_res where noteid=%1").arg(notes_to_op[2][i]);
        q.SqlStatement(sql.toUtf8());
        m_syncLog.write(QString("[%1] delete note from remote (id: %2)\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(notes_to_op[2][i]).toUtf8());
    }

    for(i=0; i<notes_to_op[0].size(); i++) {
        pushNote(notes_to_op[0][i]);
        m_syncLog.write(QString("[%1] request to push note (id:%2)\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(notes_to_op[0][i]).toUtf8());
    }

    for(i=0; i<notes_to_op[1].size(); i++) {
        pullNote(notes_to_op[1][i]);
        m_syncLog.write(QString("[%1] request to pull note (gid:%2)\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).arg(notes_to_op[1][i]).toUtf8());
    }
}
void MainWindow::pullNote(int gid)
{
    QNetworkRequest req;
    req.setRawHeader("User-Agent", "wikenotes");
    req.setUrl(QUrl(QString("http://"+C_ServerAddr+"/index.php/post/get/id/%1").arg(gid)));
    QNetworkReply *reply = m_networkManager->get(req);
    m_pullingNotes[reply] = gid;
}
void MainWindow::pushNote(int rowid)
{
    SQLiteStatement q(*m_q);
    q.Sql(QString("select rowid,title,content,tag,create_time,update_time,gid from notes where rowid=%1").arg(rowid).toUtf8());

    SQLiteStatement q_res(q);
    QRegExp rx("<img[^>]*src=\"wike://([0-9a-f]+)\"[^>]*>");
    if(q.FetchRow()) {
        QByteArray postData;
        QNetworkRequest req;
        req.setRawHeader("User-Agent", "wikenotes");
        QString content = QString::fromUtf8((char*)q.GetColumnCString(2));
        content = content.replace(rx,"<img src=\"/resources/\\1.png\">");
        postData.append("Post[title]="+QUrl::toPercentEncoding(QString::fromUtf8((char*)q.GetColumnCString(1)))+"&");
        postData.append("Post[content]="+QUrl::toPercentEncoding(content)+"&");
        postData.append("Post[tags]="+QUrl::toPercentEncoding(QString::fromUtf8((char*)q.GetColumnCString(3)))+"&");
        postData.append("Post[status]=2&");
        postData.append((Qt::mightBeRichText(content))?"Post[type]=1&":"Post[type]=0&");
        int gid = q.GetColumnInt(6);
        if(gid == 0) {
            postData.append("Post[create_time]="+QString("%1").arg(QDateTime::fromString((char*)q.GetColumnCString(4), "yyyy-MM-dd hh:mm:ss").toTime_t()));
            req.setUrl(QUrl("http://"+C_ServerAddr+"/index.php/post/create"));
            QNetworkReply *reply = m_networkManager->post(req, postData);
            m_pushingNotes[reply] = rowid;
        }
        else {
            req.setUrl(QUrl(QString("http://"+C_ServerAddr+"/index.php/post/update/id/%1").arg(gid)));
            postData.append("Post[update_time]="+QString("%1").arg(q.GetColumnInt(5)));
            m_networkManager->post(req, postData);
        }

        q_res.Sql(QString("select res_name,res_type,res_data from notes_res where noteid=%1").arg(rowid).toUtf8());
        QByteArray res_data;
        req.setRawHeader("Content-Type", "image/png");
        
        QString uploadUrl = "http://"+C_ServerAddr+"/index.php/post/upload";
        while(q_res.FetchRow()) {
            res_data = QByteArray((char*)q_res.GetColumnBlob(2), q_res.GetColumnBytes(2));
            req.setUrl(QUrl(uploadUrl+QString("?qqfile=%1.png").arg((char*)q_res.GetColumnCString(0))));
            req.setRawHeader("Content-Length", QString::number(res_data.size()).toAscii());
            m_networkManager->post(req, res_data);
            //writer.writeAttribute("type", QString::fromUtf8((char*)q_res.GetColumnCString(1)));
        }
        q_res.FreeQuery();
    }
    q.FreeQuery();
}
void MainWindow::requestLogin()
{
    QByteArray postData;
    postData.append("LoginForm[username]="+m_user+"&");
    postData.append("LoginForm[password]="+m_pass+"&");
    postData.append("LoginForm[rememberMe]=0");
    QNetworkRequest req(QUrl("http://"+C_ServerAddr+"/index.php/user/login"));
    req.setRawHeader("User-Agent", "wikenotes");
    m_networkManager->post(req, postData);
}
void MainWindow::accountSettings()
{
    AccountSettings diag(this, m_user, m_pass, m_syncMode);
    if(diag.exec() == QDialog::Accepted) {
        m_user = diag.getUser();
        m_pass = diag.getPass();
        m_syncMode = diag.getSyncMode();
        if(m_syncMode == 0 && m_syncTimer>0) {
            killTimer(m_syncTimer);
            m_syncTimer = 0;
        }
        requestLogin();
    }
}
void MainWindow::requestLogoff()
{
    m_uid = 0;
    ui->action_Publish_Notes->setEnabled(false);
    ui->action_Logon_Server->setEnabled(true);
    ui->actionLog_Off->setEnabled(false);
    setWindowTitle(QString("WikeNotes (%1)").arg(m_dbName));
    if(m_syncTimer>0) {
        killTimer(m_syncTimer);
        m_syncTimer = 0;
    }
    m_networkManager->get(QNetworkRequest(QUrl("http://"+C_ServerAddr+"/index.php/user/logout")));
}
void MainWindow::setNoteFont()
{
    bool ok;
    s_font = QFontDialog::getFont(&ok, s_font, this);
    s_fontMetrics = QFontMetrics(s_font);
    if (ok) {
        ui->noteList->setTextNoteFont(s_font);
    }
}
void MainWindow::setHotKey()
{
    QKeySequence tm = m_hkToggleMain->shortcut();
    QKeySequence ntn = m_hkNewTextNote->shortcut();
    QKeySequence nhn = m_hkNewHtmlNote->shortcut();
    HotkeySettings diag(tm, ntn, nhn, this);
    m_hkToggleMain->setShortcut(QKeySequence());
    m_hkNewTextNote->setShortcut(QKeySequence());
    m_hkNewHtmlNote->setShortcut(QKeySequence());
    if(diag.exec() == QDialog::Accepted) {
        m_hkToggleMain->setShortcut(diag.m_hkTM);
        m_hkNewTextNote->setShortcut(diag.m_hkNTN);
        m_hkNewHtmlNote->setShortcut(diag.m_hkNHN);
    }
    else {
        m_hkToggleMain->setShortcut(tm);
        m_hkNewTextNote->setShortcut(ntn);
        m_hkNewHtmlNote->setShortcut(nhn);
    }
}
void MainWindow::changeLanguage()
{
    QAction *act = qobject_cast<QAction*>(sender());
    QString lang = act->toolTip();

    if(lang == "en_US") {
        qApp->removeTranslator(&m_translator);
    }
    else {
        m_translator.load(lang+".qm", ".");
        qApp->installTranslator(&m_translator);
    }
    if(m_lang != lang) {
        m_lang = lang;
        ui->retranslateUi(this);
        m_tagModel->setData(m_tagModel->index(0),tr("All"));
        m_monthModel->setData(m_monthModel->index(0),tr("All"));
    }
}
void MainWindow::changeLeftPanel()
{
    QAction *act = qobject_cast<QAction*>(sender());
    if(act == ui->action_Tag_List) {
        if(!m_leftPanel) {
            ui->tagView->setModel(m_tagModel);
            m_leftPanel = true;
            connect(ui->tagView, SIGNAL(pressed(const QModelIndex&)), this, SLOT(tagPressed(const QModelIndex&)));
            connect(ui->tagView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)), this, SLOT(tagChanged(const QItemSelection&, const QItemSelection&)));
        }
    }
    else if(m_leftPanel) {
        ui->tagView->setModel(m_monthModel);
        m_leftPanel = false;
        connect(ui->tagView, SIGNAL(pressed(const QModelIndex&)), this, SLOT(tagPressed(const QModelIndex&)));
        connect(ui->tagView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)), this, SLOT(tagChanged(const QItemSelection&, const QItemSelection&)));
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
            QString fn = QDir::tempPath()+"/"+QCryptographicHash::hash(msg, QCryptographicHash::Sha1).toHex()+".txt";
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

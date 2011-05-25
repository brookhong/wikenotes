#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGUI>

#include "noteitem.h"
#include "qxt/qxtglobalshortcut.h"

#include "KompexSQLitePrerequisites.h"
#include "KompexSQLiteDatabase.h"
#include "KompexSQLiteStatement.h"
#include "KompexSQLiteException.h"
#include "KompexSQLiteStreamRedirection.h"

using namespace Kompex;

namespace Ui {
    class MainWindow;
}
class ImportDialog : public QDialog
{
    Q_OBJECT
public:
    ImportDialog(QWidget *parent = 0);
    void setFinishFlag(bool b);
    void closeEvent(QCloseEvent *event);

public slots:
    void reject();

private slots:
    void importMsg(const QString& msg);

private:
    bool m_finished;
    QLabel *m_label;
};
class NotesImporter : public QThread
{
    Q_OBJECT
    public:
        void importFile(const QString& fn) {
            m_action = 0;
            m_file = fn;
            start();
        }
        void exportFile(const QString& fn) {
            m_action = 1;
            m_file = fn;
            start();
        }
        void run();
    signals:
        void importMsg(const QString& msg);
        void importDone(int action);
    private:
        int m_action; //0: import; 1: export
        QString m_file;
};
class TagCompleter : public QCompleter
{
    protected:
        QStringList splitPath(const QString &path) const;
        QString pathFromIndex(const QModelIndex &index) const;
};
class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = 0);
        ~MainWindow();
        void openDB();
        bool openDB(const QString& name);

        //0: success; 1: already exists; 2: other error
        int insertNote(QString& title, QString& content, QString& tag, QString& hashKey, QString& datetime);
        static QString getTitleFromContent(const QString& content);

        bool insertNoteRes(QString& res_name, int noteId, int res_type, const QByteArray& res_data);
        SQLiteStatement* getFoundNote(int idx);
        void loadImageFromDB(const QString& fileName, QByteArray& imgData);
        SQLiteStatement* getSqlQuery();
        void cancelEdit();
        void noteSelected(bool has, bool htmlView);
        void ensureVisible(NoteItem* item);
        static QString s_query;
        static QFont s_font;
        static QFontMetrics s_fontMetrics;
        static TagCompleter s_tagCompleter;

        const QList<QAction*>& getExtActions();

    public slots:
        void newDB();
        void loadNotes();
        void editActiveNote();

    protected:
        void resizeEvent(QResizeEvent * event);
        bool event(QEvent *event);

    private slots:
        void handleSingleMessage(const QString&msg);
        void iconActivated(QSystemTrayIcon::ActivationReason reason);
        void toggleVisibility();
        void silentNewTextNote();
        void selectDB();
        void newPlainNote();
        void newHTMLNote();
        void newNote(bool rich);
        void saveNote();
        bool delActiveNote();
        void importNotes();
        void exportNotes();
        void instantSearch(const QString& query);
        void tagPressed(const QModelIndex &current);
        void tagChanged(const QItemSelection &selected, const QItemSelection &deselected);
        void splitterMoved();
        void statusMessage(const QString& msg);
        void setNoteFont();
        void setHotKey();
        void changeLanguage();
        void changeLeftPanel();
        void usage();
        void about();
        void importDone(int action);
        void extActions();
        void extProcFinished(int exitCode, QProcess::ExitStatus exitStatus);
    private:
        QSystemTrayIcon *m_trayIcon;
        void createTrayIcon();
        void loadSettings();
        inline void updateSettings(QSettings& settings, const QString& current, const QString& def, const QString& key);
        void flushSettings();
        QString m_dbName;
        QxtGlobalShortcut* m_hkToggleMain;
        QxtGlobalShortcut* m_hkNewTextNote;
        ImportDialog *m_importDialog;
        Ui::MainWindow *ui;
        QTranslator m_translator;
        QString m_lang;
        //true: tag list; false: monthly list
        bool m_leftPanel;

        QStringListModel *m_tagModel;
        QStringListModel *m_monthModel;
        SQLiteDatabase* m_db;
        SQLiteStatement* m_q;
        void closeDB();

        QStringList m_catList;
        QString m_criterion;
        NotesImporter m_importer;
        QStringList getTagsOf(int row);
        int getTagCount(const QString& tag);
        void addTag(const QString& tag);
        void removeTag(const QString& tag);
        void refreshCat();
        int lastInsertId();
        void setCurrentCat(const QString& cat);

        QList<QAction*> m_extActions;
        QList<QProcess*> m_extProcs;
};

extern MainWindow* g_mainWindow;
#endif // MAINWINDOW_H

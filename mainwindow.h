#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGUI>
#include <QtSQL>

#include "noteitem.h"
#include "qxt/qxtglobalshortcut.h"
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
class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = 0);
        ~MainWindow();
        bool openDB();
        bool saveNote(int row, QString& title, QString& content, QStringList& tags, QString& datetime);

        //0: success; 1: already exists; 2: other error
        int insertNote(QString& title, QString& content, QString& tag, QString& hashKey, QString& datetime);

        bool insertNoteRes(QString& res_name, int noteId, int res_type, const QByteArray& res_data);
        QSqlQuery* getFoundNote(int idx);
        QSqlQuery* getSqlQuery();
        void cancelEdit();
        void noteSelected(bool has, bool htmlView);
        void ensureVisible(NoteItem* item);
        static QString s_query;
        static QFont s_font;

    public slots:
        void newDB();
        void loadNotes();

    signals:
        void tagAdded(const QString&);
        void tagRemoved(const QString&);

    protected:
        void resizeEvent(QResizeEvent * event);
        bool event(QEvent *event);

    private slots:
        void handleSingleMessage(const QString&msg);
        void iconActivated(QSystemTrayIcon::ActivationReason reason);
        void toggleVisibility();
        void selectDB();
        void newNote();
        void editActiveNote();
        void saveNote();
        bool delActiveNote();
        void toggleNoteView();
        void importNotes();
        void exportNotes();
        void addTag(const QString& tag);
        void removeTag(const QString& tag);
        void instantSearch(const QString& query);
        void tagPressed(const QModelIndex &current);
        void splitterMoved();
        void statusMessage(const QString& msg);
        void setNoteFont();
        void setHotKey();
        void changeLanguage();
        void usage();
        void about();
        void importDone(int action);
    private:
        QSystemTrayIcon *m_trayIcon;
        void createTrayIcon();
        void loadSettings();
        void flushSettings();
        QString m_dbName;
        bool m_bSettings;
        QxtGlobalShortcut* m_hkToggleMain;
        ImportDialog *m_importDialog;
        Ui::MainWindow *ui;
        QTranslator m_translator;
        QString m_lang;

        QStringListModel *m_tagModel;
        QSqlQuery* m_q;
        void closeDB();

        QStringList m_tagList;
        QString m_criterion;
        NotesImporter m_importer;
        QStringList getTagsOf(int row);
        int getTagCount(const QString& tag);
        void refreshTag();
};

extern MainWindow* g_mainWindow;
#endif // MAINWINDOW_H

#ifndef ACCOUNTSETTINGS_H
#define ACCOUNTSETTINGS_H

#include <QDialog>

namespace Ui {
    class AccountSettings;
}

class AccountSettings : public QDialog
{
    Q_OBJECT

public:
    explicit AccountSettings(QWidget *parent, const QString& user, const QString& pass, int syncMode);
    ~AccountSettings();

    QString getUser();
    QString getPass();
    int getSyncMode();

private:
    Ui::AccountSettings *ui;
};

#endif // ACCOUNTSETTINGS_H

#include "accountsettings.h"
#include "ui_accountsettings.h"

AccountSettings::AccountSettings(QWidget *parent, const QString& user, const QString& pass, int syncMode) :
    QDialog(parent),
    ui(new Ui::AccountSettings)
{
    ui->setupUi(this);
    ui->lblNewAccount->setOpenExternalLinks(true);
    ui->leUser->setText(user);
    ui->lePass->setText(pass);
    ui->cmbSync->setCurrentIndex(syncMode);
}
QString AccountSettings::getUser()
{
    return ui->leUser->text();
}
QString AccountSettings::getPass()
{
    return ui->lePass->text();
}
int AccountSettings::getSyncMode()
{
    return ui->cmbSync->currentIndex();
}
AccountSettings::~AccountSettings()
{
    delete ui;
}

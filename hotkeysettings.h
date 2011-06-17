#ifndef HOTKEYSETTINGS_H
#define HOTKEYSETTINGS_H

#include <QtGUI>
#include "ui_hotkeysettings.h"

class HotkeySettings : public QDialog
{
    Q_OBJECT

    public:
        HotkeySettings(const QKeySequence &hkTM, const QKeySequence &hkNTN, const QKeySequence &hkNHN, QWidget *parent = 0);

        QKeySequence m_hkTM;
        QKeySequence m_hkNTN;
        QKeySequence m_hkNHN;
    private:
        Ui::HotkeySettings ui;
        bool eventFilter(QObject *obj, QEvent *ev);
};

#endif // HOTKEYSETTINGS_H

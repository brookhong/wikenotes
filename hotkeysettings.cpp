#include "hotkeysettings.h"

HotkeySettings::HotkeySettings(const QKeySequence &hkTM, const QKeySequence &hkNTN, QWidget *parent) : QDialog(parent)
{
    ui.setupUi(this);

    m_hkTM = hkTM;
    ui.lineEdit->setText(hkTM.toString());
    ui.lineEdit->installEventFilter(this);

    m_hkNTN = hkNTN;
    ui.leNewTextNote->setText(hkNTN.toString());
    ui.leNewTextNote->installEventFilter(this);
}
bool HotkeySettings::eventFilter(QObject *obj, QEvent *ev)
{
    QEvent::Type type = ev->type();
    QLineEdit *sdr = qobject_cast<QLineEdit*>(obj);
    if(sdr && type == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(ev);
        Qt::KeyboardModifiers mod = keyEvent->modifiers();
        int k = keyEvent->key();
        QString modseq;
        if (mod & Qt::ControlModifier)
            modseq += "Ctrl+";
        if (mod & Qt::AltModifier)
            modseq += "Alt+";
        if (mod & Qt::MetaModifier)
            modseq += "Meta+";
        if(!modseq.isEmpty() && (k != Qt::Key_Control)
                && (k != Qt::Key_Alt)
                && (k != Qt::Key_Meta)
                && (k != Qt::Key_Shift)) {
            if (mod & Qt::ShiftModifier)
                modseq += "Shift+";
            modseq += (QString)QKeySequence(k);
            sdr->setText(modseq);
            if(sdr == ui.lineEdit)
                m_hkTM = QKeySequence(modseq);
            else if(sdr == ui.leNewTextNote)
                m_hkNTN = QKeySequence(modseq);
        }
        return true;
    }
    return false;
}
HotkeySettings::~HotkeySettings()
{
}

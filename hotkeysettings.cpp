#include "hotkeysettings.h"

HotkeySettings::HotkeySettings(const QKeySequence &hkTM, QWidget *parent) : QDialog(parent)
{
    ui.setupUi(this);
    ui.lineEdit->setText(hkTM.toString());
    m_hkTM = hkTM;
    ui.lineEdit->installEventFilter(this);
}
bool HotkeySettings::eventFilter(QObject *obj, QEvent *ev)
{
    QEvent::Type type = ev->type();
    switch(type) {
        case QEvent::KeyPress:
            if(obj == ui.lineEdit) {
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
                    ui.lineEdit->setText(modseq);
                    m_hkTM = QKeySequence(modseq);
                }
                return true;
            }
        default:
            break;
    }
    return false;
}
HotkeySettings::~HotkeySettings()
{
}

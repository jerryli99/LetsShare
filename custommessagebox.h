#ifndef CUSTOMMESSAGEBOX_H
#define CUSTOMMESSAGEBOX_H

#include <QMessageBox>
#include <QPixmap>

class CustomMessageBox : public QMessageBox {
public:
    CustomMessageBox(QWidget *parent = nullptr) : QMessageBox(parent) {
        QPixmap msgIcon(":/icons/messagebox_icon.png");
        msgIcon = msgIcon.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        setIconPixmap(msgIcon);
    }

    static void showWarning(QWidget *parent, const QString &title, const QString &text) {
        CustomMessageBox msgBox(parent);
        msgBox.setWindowTitle(title);
        msgBox.setText(text);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    }
};

#endif // CUSTOMMESSAGEBOX_H

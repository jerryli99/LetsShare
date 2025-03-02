#include <QApplication>
#include "applink.c"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/app_icon.ico"));

    MainWindow window;

    window.show();
    return app.exec();
}

// #ifndef PACKAGE_H
// #define PACKAGE_H

// #include <QByteArray>
// #include <QString>

// struct Package {
//     enum Type {
//         REQUEST_RSA_PUBLIC_KEY,
//         RSA_PUBLIC_KEY,
//         ENCRYPTED_AES_KEY,
//         OK_TO_SEND,
//         ENCRYPTED_FILE_METADATA,
//         OK_META,
//         ENCRYPTED_FILE_CHUNK,
//         FILE_TRANSFER_COMPLETE
//     };

//     Type type;
//     QByteArray data;
// };

// QDataStream &operator<<(QDataStream &out, const Package &package);
// QDataStream &operator>>(QDataStream &in, Package &package);

// #endif // PACKAGE_H

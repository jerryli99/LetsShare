#ifndef FILESERVER_H
#define FILESERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QMap>
#include <QDataStream>
#include <QSharedPointer>

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

class FileServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit FileServer(QObject *parent = nullptr);
    void setDownloadLocation(const QString &path);
    bool isListening() const;
    void setAllowedIPs(const QSet<QString> &allowedIPs);
    void setRSAPrivateKeyPath(const QString &path);

signals:
    void fileReceived(const QString &filePath);
    void statusUpdated(const QString &message);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private:
    void readFile(QTcpSocket *socket);
    QString downloadLocation;
    QSet<QString> allowedIPs;

    struct FileTransferInfo {
        QSharedPointer<QFile> file;
        QString fileName;
        qint64 fileSize;
        qint64 bytesReceived;
    };

    QMap<QTcpSocket*, FileTransferInfo> transferInfo;

    QString rsaPrivateKeyPath;

};

#endif // FILESERVER_H

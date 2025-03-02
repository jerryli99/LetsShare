#ifndef FILECLIENT_H
#define FILECLIENT_H

#include <QTcpSocket>
#include <QFile>

#include <QSharedPointer>
#include <QObject>

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>

class FileClient : public QObject
{
    Q_OBJECT

public:
    explicit FileClient(QObject *parent = nullptr);
    ~FileClient();
    void sendFiles(const QStringList &files, const QString &ipAddress);
    void reset();
    bool connectToServer(const QString &ipAddress); // Return connection status
    void disconnectFromServer();

signals:
    void statusUpdated(const QString &message);
    void progressUpdated(int percentage);

private:
    QSharedPointer<QTcpSocket> socket;
    QStringList filesToSend;
    qint64 currentFileIndex;

    qint64 totalFilesSize; // Total size of all files
    qint64 totalBytesSent; // Total bytes sent so far

    void sendNextFile();
    void calculateProgress(); // Helper function to calculate progress

};

#endif // FILECLIENT_H

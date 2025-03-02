#include "fileserver.h"
#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include <QMessagebox>

FileServer::FileServer(QObject *parent) : QTcpServer(parent), downloadLocation(QDir::homePath())
{
    listen(QHostAddress::Any, 12345);
}

void FileServer::setDownloadLocation(const QString &path)
{
    downloadLocation = path;
}

void FileServer::setAllowedIPs(const QSet<QString> &allowedIPs)
{
    this->allowedIPs = allowedIPs;
    // qDebug() << "I am in server code: " << this->allowedIPs ;
}

// bool FileServer::isListening() const
// {
//     return QTcpServer::isListening();
// }

void FileServer::incomingConnection(qintptr socketDescriptor)
{
    QTcpSocket *socket = new QTcpSocket(this);
    socket->setSocketDescriptor(socketDescriptor);

    QString clientIP = socket->peerAddress().toString();
    clientIP = clientIP.remove("::ffff:"); // Normalize IPv4-mapped IPv6

    if (allowedIPs.isEmpty() || !allowedIPs.contains(clientIP)) {
        // qDebug() << "Blocked connection from" << clientIP;
        socket->write("BLOCKED");  // Send a rejection message
        socket->flush();
        socket->waitForBytesWritten(500); // Ensure message is sent before disconnect
        socket->disconnectFromHost();
        emit statusUpdated("Blocked " + clientIP + " for attempting to connect");
        return;
    }

    qDebug() << "Connection allowed from" << clientIP;
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { readFile(socket); });
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}


void FileServer::readFile(QTcpSocket *socket)
{
    if (!transferInfo.contains(socket)) {
        QDataStream in(socket);
        in.setVersion(QDataStream::Qt_6_8);

        if (socket->bytesAvailable() < sizeof(qint64) * 2) {
            return;
        }

        QString fileName;
        qint64 fileSize;
        in >> fileName >> fileSize;

        if (fileName.isEmpty() || fileSize <= 0) {
            emit statusUpdated("Invalid metadata received.");
            socket->disconnectFromHost();
            return;
        }

        QString filePath = QDir(downloadLocation).filePath(fileName);
        QSharedPointer<QFile> file = QSharedPointer<QFile>::create(filePath);
        if (!file->open(QIODevice::WriteOnly)) {
            emit statusUpdated("Failed to save file: " + fileName);
            socket->disconnectFromHost();
            return;
        }

        transferInfo[socket] = {file, fileName, fileSize, 0};
    }

    FileTransferInfo &info = transferInfo[socket];
    const qint64 chunkSize = 64 * 1024;

    while (info.bytesReceived < info.fileSize) {
        if (socket->bytesAvailable() < qMin(chunkSize, info.fileSize - info.bytesReceived)) {
            return;
        }

        QByteArray chunk = socket->read(qMin(chunkSize, info.fileSize - info.bytesReceived));
        if (chunk.isEmpty()) {
            emit statusUpdated("Failed to read file chunk.");
            socket->disconnectFromHost();
            return;
        }

        //chunk = qUncompress(chunk);

        info.file->write(chunk);
        info.bytesReceived += chunk.size();
    }

    emit fileReceived(info.file->fileName());
    transferInfo.remove(socket);
}

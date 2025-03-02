#include "fileclient.h"
#include <QFileInfo>
#include <QDebug>
#include <QThread>


FileClient::FileClient(QObject *parent) : QObject(parent),
    currentFileIndex(0), totalFilesSize(0), totalBytesSent(0)
{
    socket = QSharedPointer<QTcpSocket>::create(this);
}

FileClient::~FileClient()
{
    disconnectFromServer();
}


bool FileClient::connectToServer(const QString &ipAddress)
{
    if (socket->state() == QAbstractSocket::ConnectedState) {
        return true; // Already connected
    }

    socket->connectToHost(ipAddress, 12345); // Replace with your port

    if (!socket->waitForConnected(5000)) { // Wait for 5 seconds
        return false; // Connection failed
    }

    // Check if server rejected the connection
    if (socket->waitForReadyRead(500)) {  // Wait up to 500ms for a response
        QByteArray response = socket->readAll();
        if (response == "BLOCKED") {
            // qDebug() << "Connection blocked by server.";
            socket->disconnectFromHost();
            return false;
        }
    }

    return true; // Connection successful
}


void FileClient::sendFiles(const QStringList &files, const QString &ipAddress)
{
    if (files.isEmpty()) {
        emit statusUpdated("No files to send.");
        return;
    }

    filesToSend = files;
    currentFileIndex = 0;

    totalFilesSize = 0;
    totalBytesSent = 0;

    // Calculate the total size of all files
    for (const QString &filePath : filesToSend) {
        QFileInfo fileInfo(filePath);
        totalFilesSize += fileInfo.size();
    }

    // Attempt to connect to the server
    if (!connectToServer(ipAddress)) {
        emit statusUpdated("Connection Failed.");
        return; // Stop if connection fails
    }

    // Start sending files if connection is successful
    sendNextFile();
}


void FileClient::sendNextFile()
{
    while (currentFileIndex < filesToSend.size())
    {
        QString filePath = filesToSend[currentFileIndex];
        QFile file(filePath);

        if (!file.open(QIODevice::ReadOnly)) {
            emit statusUpdated("Failed to open file: " + filePath);
            currentFileIndex++;
            continue;  // Skip to the next file
        }

        QFileInfo fileInfo(file);
        QString fileName = fileInfo.fileName();
        qint64 fileSize = fileInfo.size();

        if (fileSize == 0) {
            emit statusUpdated("Skipping 0-byte file: " + fileName);
            file.close();
            currentFileIndex++;
            continue;  // Skip to the next file
        }

        QDataStream out(socket.data());
        out.setVersion(QDataStream::Qt_6_8);
        out << fileName << fileSize;

        const qint64 chunkSize = 64 * 1024;  // 64 KB
        qint64 bytesRemaining = fileSize;

        while (bytesRemaining > 0) {
            QByteArray chunk = file.read(qMin(chunkSize, bytesRemaining));
            if (chunk.isEmpty()) {
                emit statusUpdated("Failed to read file chunk: " + fileName);
                break;
            }

            qint64 bytesSent = socket->write(chunk);
            if (bytesSent == -1) {
                emit statusUpdated("Failed to send file chunk: " + fileName);
                break;
            }

            bytesRemaining -= bytesSent;
            totalBytesSent += bytesSent;

            // Calculate and emit progress after each chunk
            calculateProgress();

            // Wait for data to be written before proceeding
            if (!socket->waitForBytesWritten(30000)) {
                emit statusUpdated("Timeout while sending file: " + fileName);
                break;
            }
        }

        file.close();
        emit statusUpdated("File sent: " + fileName);
        currentFileIndex++;
    }

    emit statusUpdated("All files sent successfully.");
    emit progressUpdated(100);
}

void FileClient::calculateProgress()
{
    if (totalFilesSize == 0) {
        emit progressUpdated(0); // Avoid division by zero
        return;
    }

    int percentage = static_cast<int>((totalBytesSent * 100) / totalFilesSize);
    emit progressUpdated(percentage);
}

void FileClient::reset()
{
    currentFileIndex = 0;
    filesToSend.clear();
    totalFilesSize = 0;
    totalBytesSent = 0;
    if (socket->state() != QAbstractSocket::UnconnectedState) {
        socket->disconnectFromHost();
    }
}

void FileClient::disconnectFromServer()
{
    if (socket->state() == QAbstractSocket::ConnectedState) {
        socket->disconnectFromHost(); // Disconnect from the server
    }
}

#include "httpserver.h"
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QRegularExpression>
#include <QRandomGenerator>

HttpServer::HttpServer(QObject *parent)
    : QObject(parent), tcpServer(new QTcpServer(this)), port(11234)
{
    //ok buggy program, i think that is all. I cannot waste too much time on this small project.
    //need to learn more C++ and computer system stuff now. This app should be enough for me
    //to view and download and transfer files.

    serverThread = new QThread(this);
    tcpServer->moveToThread(serverThread);

    connect(serverThread, &QThread::started, tcpServer, [this]() {
        if (!tcpServer->listen(QHostAddress::Any, port)) {
            // qDebug() << "Server could not start!";
        } else {
            sessionKey = generateSessionKey();
            // qDebug() << "Server started on port:" << port;
            emit serverStarted(QString("http://%1:%2/%3/Share/")
                                   .arg(QHostAddress(QHostAddress::LocalHost).toString())
                                   .arg(port)
                                   .arg(sessionKey));
        }
    });

    connect(tcpServer, &QTcpServer::newConnection, this, &HttpServer::handleNewConnection);
    connect(this, &HttpServer::requestStopServer, this, &HttpServer::onStopServer); // Connect the stop signal to the slot
    connect(this, &HttpServer::serverStopped, serverThread, &QThread::quit);
    connect(serverThread, &QThread::finished, this, &HttpServer::deleteLater);
    connect(serverThread, &QThread::finished, tcpServer, &QTcpServer::deleteLater);

    serverThread->start();
}

HttpServer::~HttpServer()
{
    // Disconnect all signals
    disconnect(this, nullptr, nullptr, nullptr);

    // Stop the server and clean up clients
    stopServer();

    // Clean up the thread
    if (serverThread) {
        serverThread->quit();
        serverThread->wait();
        delete serverThread;
    }

    // Clean up the TCP server
    if (tcpServer) {
        delete tcpServer; // Directly delete the TCP server
        tcpServer = nullptr; // Set to nullptr to avoid dangling pointer
    }
}

void HttpServer::startServer(quint16 port)
{
    this->port = port;
    serverThread->start();
}

void HttpServer::onStopServer() {
    if (tcpServer->isListening()) {
        tcpServer->close();
        cleanupClients();
    }
    emit serverStopped();
}

void HttpServer::stopServer() {
    emit requestStopServer(); // Emit the signal to stop the server
}

bool HttpServer::isRunning() const {
    return tcpServer && tcpServer->isListening();
}


void HttpServer::cleanupClients() {
    QMutexLocker locker(&clientMutex);
    for (QTcpSocket *clientSocket : buffers.keys()) {
        clientSocket->disconnectFromHost();
        clientSocket->deleteLater();
    }
    buffers.clear();
    sizes.clear();
}

void HttpServer::setSharedFiles(const QStringList &files)
{
    sharedFiles = files;
}

QString HttpServer::generateSessionKey()
{
    const QString possibleCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    const int length = 8;
    QString randomString;
    for (int i = 0; i < length; ++i) {
        int index = QRandomGenerator::global()->generate() % possibleCharacters.length();
        QChar nextChar = possibleCharacters.at(index);
        randomString.append(nextChar);
    }
    return randomString;
}

void HttpServer::setAllowedIPs(const QSet<QString> &allowedIPs)
{
    this->allowedIPs = allowedIPs;
}

void HttpServer::handleNewConnection()
{
    QTcpSocket *clientSocket = tcpServer->nextPendingConnection();
    QString clientIp = clientSocket->peerAddress().toString();

    // Extract IPv4 address if it's in IPv6-mapped format (::ffff:192.168.1.100)
    if (clientIp.startsWith("::ffff:")) {
        clientIp = clientIp.mid(7);
    }

    // qDebug() << "Incoming connection from IP:" << clientIp;

    // Check if IP is allowed
    if (!allowedIPs.contains(clientIp)) {
        // qDebug() << "Blocked IP:" << clientIp;
        sendErrorResponse(clientSocket, "403 Forbidden", "You are blocked my friend.");
        clientSocket->disconnectFromHost();
        return;
    }

    // Proceed with normal handling
    connect(clientSocket, &QTcpSocket::readyRead, this, &HttpServer::readClient);
    connect(clientSocket, &QTcpSocket::disconnected, this, &HttpServer::discardClient);
    buffers.insert(clientSocket, new QByteArray());
    sizes.insert(clientSocket, new qint64(0));
}


void HttpServer::readClient()
{
    QTcpSocket *clientSocket = static_cast<QTcpSocket*>(sender());
    QByteArray *buffer = buffers.value(clientSocket);
    qint64 *s = sizes.value(clientSocket);

    qint64 bytesAvailable = clientSocket->bytesAvailable();
    buffer->append(clientSocket->read(bytesAvailable));

    if (buffer->contains("\r\n\r\n")) {
        QString request = QString(*buffer);
        QRegularExpression regex("GET /([^ ]+) HTTP/1.1");
        QRegularExpressionMatch match = regex.match(request);

        if (match.hasMatch()) {
            QString path = match.captured(1);
            handleGetRequest(clientSocket, path);
        }

        clientSocket->disconnectFromHost();
    }
}

void HttpServer::handleGetRequest(QTcpSocket *clientSocket, const QString &path) {
    // qDebug() << "Requested path:" << path; // Debug: Print the requested path

    // Check if the path starts with the session key
    if (path.startsWith(sessionKey + "/Share/")) {
        QString filePath = QUrl::fromPercentEncoding(path.mid(sessionKey.length() + 7).toUtf8()); // Decode URL-encoded file path
        // qDebug() << "Decoded file path:" << filePath; // Debug: Print the decoded file path

        // If the file path is empty, return the file list
        if (filePath.isEmpty()) {
            // qDebug() << "Requested directory, returning file list.";
            sendResponse(clientSocket, "200 OK", "text/html", generateFileListHtml().toUtf8());
            return;
        }

        bool fileFound = false;

        for (const QString &sharedFile : sharedFiles) {
            QFileInfo fileInfo(sharedFile);
            // qDebug() << "Checking shared file:" << fileInfo.fileName(); // Debug: Print each shared file name

            if (fileInfo.fileName() == filePath) {
                qDebug() << "File found:" << sharedFile; // Debug: Print the matched file
                QFile file(sharedFile);
                if (file.open(QIODevice::ReadOnly)) {
                    QString mimeType = getMimeType(sharedFile);
                    if (mimeType == "application/octet-stream") {
                        // Force download for unsupported file types
                        sendResponse(clientSocket, "200 OK", "application/octet-stream", file.readAll());
                    } else {
                        // Display in browser for supported file types
                        sendResponse(clientSocket, "200 OK", mimeType, file.readAll());
                    }
                    file.close();
                    fileFound = true;
                    break;
                }
            }
        }

        if (!fileFound) {
            // qDebug() << "File not found:" << filePath; // Debug: Print if the file is not found
            sendErrorResponse(clientSocket, "404 Not Found", "File not found.");
        }
    } else {
        sendErrorResponse(clientSocket, "403 Forbidden", "Access denied.");
        return;
    }
}


void HttpServer::sendResponse(QTcpSocket *clientSocket, const QString &status, const QString &contentType, const QByteArray &body)
{
    QByteArray response = "HTTP/1.1 " + status.toUtf8() + "\r\n";
    response += "Content-Type: " + contentType.toUtf8() + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "\r\n";
    response += body;
    clientSocket->write(response.data(), response.size());
}

void HttpServer::sendErrorResponse(QTcpSocket *clientSocket, const QString &status, const QString &message)
{
    QByteArray body = "<h1><center>" + message.toUtf8() + " </center></h1>";
    sendResponse(clientSocket, status, "text/html", body);
}

QString HttpServer::generateFileListHtml() {
    QString html = "<h1>Shared Files</h1><ul>";
    for (const QString &filePath : sharedFiles) {
        QFileInfo fileInfo(filePath);
        QString fileName = fileInfo.fileName();
        QString fileUrl = "/" + sessionKey + "/Share/" + QUrl::toPercentEncoding(fileName); // Include session key in the URL
        QString mimeType = getMimeType(filePath);

        html += "<li>";
        if (mimeType == "application/octet-stream") {
            html += "<a href='" + fileUrl + "'>" + fileName + "</a> ";
            html += "<span style='color: red;'>(File type not supported for viewing. Click to download.)</span>";
        } else {
            html += "<a href='" + fileUrl + "'>" + fileName + "</a>";
        }
        html += "</li>";
    }
    html += "</ul>";

    return html;
}


QString HttpServer::getMimeType(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();

    if (extension == "txt" || extension == "py" ||
        extension == "cpp" || extension == "c") return "text/plain";
    if (extension == "html" || extension == "htm") return "text/html";
    if (extension == "css") return "text/css";
    if (extension == "js") return "application/javascript";
    if (extension == "json") return "application/json";
    if (extension == "jpg" || extension == "jpeg") return "image/jpeg";
    if (extension == "png") return "image/png";
    if (extension == "gif") return "image/gif";
    if (extension == "pdf") return "application/pdf";
    if (extension == "zip") return "application/zip";
    if (extension == "mp3") return "audio/mpeg";
    if (extension == "mp4") return "video/mp4";

    // Default to binary data
    return "application/octet-stream";
}

void HttpServer::discardClient()
{
    QTcpSocket *clientSocket = static_cast<QTcpSocket*>(sender());
    if (clientSocket) {
        // Clean up buffers and sizes
        delete buffers.take(clientSocket);
        delete sizes.take(clientSocket);

        // Disconnect and delete the socket
        clientSocket->disconnectFromHost();
        clientSocket->deleteLater();
    }
}


void HttpServer::addSharedFile(const QString &filePath) {
    QMutexLocker locker(&sharedFilesMutex);
    if (!sharedFiles.contains(filePath)) {
        sharedFiles.append(filePath);
    }
}

void HttpServer::removeSharedFile(const QString &filePath) {
    QMutexLocker locker(&sharedFilesMutex);
    sharedFiles.removeAll(filePath);
}

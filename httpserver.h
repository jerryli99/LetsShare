#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QMap>
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QMutex>

class HttpServer : public QObject
{
    Q_OBJECT

public:
    explicit HttpServer(QObject *parent = nullptr);
    ~HttpServer();

    void startServer(quint16 port);
    void stopServer();// This will now be called via a signal
    void setSharedFiles(const QStringList &files); // Changed from setSharedFolders
    QString generateSessionKey();
    void setUploadSizeLimit(qint64 limit); // Set upload size limit per file
    void addSharedFile(const QString &filePath);
    void removeSharedFile(const QString &filePath);
    bool isRunning() const;
    void setAllowedIPs(const QSet<QString> &allowedIPs);

signals:
    void serverStarted(const QString &url);
    void serverStopped();
    void requestStopServer();// New signal to request stopping the server


private slots:
    void handleNewConnection();
    void readClient();
    void discardClient();
    void onStopServer();

private:
    QTcpServer *tcpServer;
    QMap<QTcpSocket*, QByteArray*> buffers;
    QMap<QTcpSocket*, qint64*> sizes;
    QStringList sharedFiles; // Changed from sharedFolders
    QString sessionKey;
    quint16 port;
    qint64 uploadSizeLimit; // Upload size limit per file
    QThread *serverThread; // Add a QThread member
    QMutex sharedFilesMutex;
    QMutex clientMutex;
    QSet<QString> allowedIPs;

    void handleGetRequest(QTcpSocket *clientSocket, const QString &path);
    void sendResponse(QTcpSocket *clientSocket, const QString &status, const QString &contentType, const QByteArray &body);
    void sendErrorResponse(QTcpSocket *clientSocket, const QString &status, const QString &message);
    QString generateFileListHtml();
    QString getMimeType(const QString &filePath); // Add this line
    void cleanupClients();
    void resetServer(); // Reset the server state
};

#endif // HTTPSERVER_H

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QListWidget>
#include <QStatusBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QComboBox>
#include <QThread>
#include <QProgressBar>
#include <QLineEdit>
#include <QCheckBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QSslKey>
#include <QSet>

#include "fileserver.h"
#include "fileclient.h"
#include "httpserver.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onSendFiles();
    void onFileReceived(const QString &filePath);
    void updateStatus(const QString &message);
    void onBrowseDownloadLocation();
    void clearSentFiles();
    void clearReceivedFiles();
    void showFileListContextMenu(const QPoint &pos);
    void removeSelectedFiles();
    void updateProgress(int percentage);
    void onConnect();
    void onDisconnect();
    void saveConfiguration();
    void loadConfiguration();
    void addAllowedIP();
    void showAllowedIPsContextMenu(const QPoint &pos);
    void removeSelectedIPs();

    void onHttpServerStarted(const QString &url);
    void onHttpServerStopped();
    void onAddHttpSharedFiles();
    void onRemoveHttpSharedFiles();
    void showHttpSharedFilesContextMenu(const QPoint &pos);

    void showPowerShellScript();

private:
    QListWidget *fileListWidget;
    QListWidget *sentFilesWidget;
    QListWidget *receivedFilesWidget;
    QStatusBar *statusBar;
    QComboBox *ipComboBox;
    QPushButton *browseDownloadLocationButton;
    QLabel *downloadLocationLabel;
    QString downloadLocation;
    FileServer *fileServer;
    FileClient *fileClient;
    QThread *serverThread;

    // QThread *clientThread; // New thread for FileClient

    QProgressBar *progressBar;
    QLineEdit *ipInput;
    QPushButton *connectButton;
    QPushButton *disconnectButton;
    QListWidget *allowedIPsList;
    QLineEdit *allowedIPInput;

    QLineEdit *rsaPublicKeyPathInput;
    QLineEdit *rsaPrivateKeyPathInput;
    QLineEdit *aesKeyPathInput;

    bool isConnected;

    void setupUI();
    void setupMainTab(QWidget *tab);
    void setupHelpTab(QWidget *tab);
    void setupConfigureTab(QWidget *tab);
    bool isValidIP(const QString &ipAddress);
    void updateAllowedIPs();

    HttpServer *httpServer;
    QThread *httpServerThread;
    QLineEdit *httpUrlInput;
    QPushButton *startHttpServerButton;
    QPushButton *stopHttpServerButton;
    QListWidget *httpSharedFilesList;
    QPushButton *addHttpSharedFilesButton;
    QPushButton *removeHttpSharedFilesButton;

    void setupHttpServerTab(QWidget *tab);

};

#endif // MAINWINDOW_H

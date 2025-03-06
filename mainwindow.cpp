#include "mainwindow.h"
#include "scriptdialog.h"

#include <QNetworkInterface>
#include <QDir>
#include <QFileInfo>
#include <QTextEdit>
#include <QMenu>
#include <QMenuBar>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QTabWidget>
#include <QCheckBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QMessageBox>
#include <QRegularExpression>
#include <QThreadPool>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), downloadLocation(QDir::homePath())
{
    setAcceptDrops(true);
    // Initialize the status bar
    statusBar = new QStatusBar(this);
    setStatusBar(statusBar);

    setupUI();

    fileServer = new FileServer();
    fileClient = new FileClient(this);

    fileServer->setDownloadLocation(downloadLocation);

    serverThread = new QThread(this);
    fileServer->moveToThread(serverThread);
    serverThread->start();

    connect(fileServer, &FileServer::fileReceived, this, &MainWindow::onFileReceived);
    connect(fileClient, &FileClient::statusUpdated, this, &MainWindow::updateStatus);
    connect(fileClient, &FileClient::progressUpdated, this, &MainWindow::updateProgress);

    // Initialize HTTP server
    httpServer = new HttpServer();

    connect(httpServer, &HttpServer::serverStarted, this, &MainWindow::onHttpServerStarted);
    connect(httpServer, &HttpServer::serverStopped, this, &MainWindow::onHttpServerStopped);

    // Start the HTTP server automatically
    httpServer->startServer(11234);

    loadConfiguration();
}

MainWindow::~MainWindow()
{
    serverThread->quit();
    serverThread->wait();

    delete fileServer;
    delete fileClient;

    // Stop the HTTP server
    if (httpServer) {
        httpServer->stopServer();
        delete httpServer;
        httpServer = nullptr;
    }
}

void MainWindow::setupUI()
{
    QTabWidget *tabWidget = new QTabWidget(this);
    setCentralWidget(tabWidget);

    // User Guide Tab
    QWidget *helpTab = new QWidget();
    setupHelpTab(helpTab);
    tabWidget->addTab(helpTab, "User Guide");

    // Configure Tab
    QWidget *configureTab = new QWidget();
    setupConfigureTab(configureTab);
    tabWidget->addTab(configureTab, "Configure");

    // HTTP Server Tab
    QWidget *httpServerTab = new QWidget();
    setupHttpServerTab(httpServerTab);
    tabWidget->addTab(httpServerTab, "HTTP Server");

    // socket transfer Tab
    QWidget *mainTab = new QWidget();
    setupMainTab(mainTab);
    tabWidget->addTab(mainTab, "Share/Receive");

}

void MainWindow::setupMainTab(QWidget *tab)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(tab);

    // IP Address Input
    QLabel *ipLabel = new QLabel("Enter IPv4 to Connect:", tab);
    ipInput = new QLineEdit(tab);
    ipInput->setPlaceholderText("e.g., 192.168.1.101");

    // Connect Button
    connectButton = new QPushButton("Connect", tab);
    connectButton->setFixedWidth(120);
    // Disconnect Button (initially hidden)
    disconnectButton = new QPushButton("Disconnect", tab);
    disconnectButton->setFixedWidth(120);
    disconnectButton->setVisible(false); // Hide initially

    // Connect button signals to slots
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::onConnect);
    connect(disconnectButton, &QPushButton::clicked, this, &MainWindow::onDisconnect);

    QHBoxLayout *ipLayout = new QHBoxLayout();
    ipLayout->addWidget(ipLabel);
    ipLayout->addWidget(ipInput);
    ipLayout->addWidget(connectButton);
    ipLayout->addWidget(disconnectButton);

    // File List Panel
    QLabel *fileListLabel = new QLabel("Selected:", tab);
    fileListWidget = new QListWidget(tab);
    fileListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    fileListWidget->setAcceptDrops(true);
    fileListWidget->setDropIndicatorShown(true);
    fileListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(fileListWidget, &QListWidget::customContextMenuRequested, this, &MainWindow::showFileListContextMenu);

    QPushButton *addFilesButton = new QPushButton("Add/Drag Files", tab);
    addFilesButton->setFixedWidth(120);
    connect(addFilesButton, &QPushButton::clicked, this, [this]() {
        QStringList filesAndFolders = QFileDialog::getOpenFileNames(this, "Select Files or Folders", QDir::homePath(), "All Files (*)");
        QStringList allFiles;
        for (const QString &path : filesAndFolders) {
            QFileInfo fileInfo(path);
            if (fileInfo.isDir()) {
                QDir dir(path);
                QStringList folderFiles = dir.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
                for (const QString &file : folderFiles) {
                    allFiles.append(dir.filePath(file));
                }
            } else if (fileInfo.isFile()) {
                allFiles.append(path);
            }
        }
        if (!allFiles.isEmpty()) {
            fileListWidget->addItems(allFiles);
        }
    });

    QVBoxLayout *fileListLayout = new QVBoxLayout();
    fileListLayout->addWidget(fileListLabel);
    fileListLayout->addWidget(fileListWidget);

    QHBoxLayout *addFilesButtonLayout = new QHBoxLayout();
    addFilesButtonLayout->addWidget(addFilesButton, 0, Qt::AlignLeft);
    fileListLayout->addLayout(addFilesButtonLayout);

    // Sent Files Panel
    QLabel *sentFilesLabel = new QLabel("Sent Files:", tab);
    sentFilesWidget = new QListWidget(tab);
    QPushButton *clearSentFilesButton = new QPushButton("Clear Sent History", tab);
    clearSentFilesButton->setFixedWidth(120);
    connect(clearSentFilesButton, &QPushButton::clicked, this, &MainWindow::clearSentFiles);

    QVBoxLayout *sentFilesLayout = new QVBoxLayout();
    sentFilesLayout->addWidget(sentFilesLabel);
    sentFilesLayout->addWidget(sentFilesWidget);
    sentFilesLayout->addWidget(clearSentFilesButton);

    // Received Files Panel
    QLabel *receivedFilesLabel = new QLabel("Received Files:", tab);
    receivedFilesWidget = new QListWidget(tab);
    QPushButton *clearReceivedFilesButton = new QPushButton("Clear Recv History", tab);
    clearReceivedFilesButton->setFixedWidth(120);
    connect(clearReceivedFilesButton, &QPushButton::clicked, this, &MainWindow::clearReceivedFiles);

    QVBoxLayout *receivedFilesLayout = new QVBoxLayout();
    receivedFilesLayout->addWidget(receivedFilesLabel);
    receivedFilesLayout->addWidget(receivedFilesWidget);
    receivedFilesLayout->addWidget(clearReceivedFilesButton);

    // Download Location
    QLabel *downloadLocationTitle = new QLabel("Download Location:", tab);
    downloadLocationLabel = new QLabel(downloadLocation, tab);
    browseDownloadLocationButton = new QPushButton("Browse", tab);
    browseDownloadLocationButton->setFixedWidth(120);
    connect(browseDownloadLocationButton, &QPushButton::clicked, this, &MainWindow::onBrowseDownloadLocation);

    QHBoxLayout *downloadLocationLayout = new QHBoxLayout();
    downloadLocationLayout->addWidget(downloadLocationTitle);
    downloadLocationLayout->addWidget(downloadLocationLabel);
    downloadLocationLayout->addWidget(browseDownloadLocationButton);

    // Send Button
    QPushButton *sendButton = new QPushButton("Send Files", tab);
    sendButton->setFixedWidth(150);
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::onSendFiles);

    // Progress Bar
    progressBar = new QProgressBar(tab);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);

    // Add to Main Layout
    mainLayout->addLayout(ipLayout);
    mainLayout->addLayout(fileListLayout);
    mainLayout->addLayout(sentFilesLayout);
    mainLayout->addWidget(progressBar);
    mainLayout->addLayout(receivedFilesLayout);
    mainLayout->addLayout(downloadLocationLayout);
    mainLayout->addWidget(sendButton, 0, Qt::AlignCenter);

    tab->setLayout(mainLayout);
}

void MainWindow::setupHelpTab(QWidget *tab)
{
    QVBoxLayout *layout = new QVBoxLayout(tab);
    QTextEdit *helpText = new QTextEdit(tab);
    helpText->setReadOnly(true);
    helpText->setHtml(
        "<h2>Config Tab</h2>"
        "<p>1. <b>Enter IPv4 Address:</b> This IP address will be the IP address you allow to connect to this app.</p>"
        "<p>2. <b>Press SaveConfiguration:</b> Saves the list of IPs in config.json file. </p>"
        "<p>3. <b>Remove IP from List:</b> You can multiple select the IPs by Ctrl + left click and then right click to Remove</p>"
        "<h2>Http Server Tab</h2>"
        "<p>1. <b>Click add files to add files for sharing</b></p>"
        "<p>2. <b>If you want to access the webpage from outside, just not use 127.0.0.1, replace it with 192.168.xxx.xxx stuff. </b></p>"
        "<p>Do remember that if you want to try it using 127.0.0.1 on local computer for testing, add 127.0.0.1 to the allowed IPs in Config Tab :)</p>"
        "<h2>Share/Receive Tab</h2>"
        "<p>1. <b>Enter IPv4 Address:</b> Enter the target IP address. </p>"
        "<p>2. <b>Select Files/Folders:</b> Click 'Add Files' to choose files or folders to send.</p>"
        "<p>3. <b>Send Files:</b> Click 'Send Files' to start the transfer.</p>"
        "<p>4. <b>Receive Files:</b> Files sent to you will appear in the 'Received Files' section.</p>"
        "<p>5. <b>Change Download Location:</b> Use the 'Browse' button to set where received files are saved.</p>"
        "<h2>Downloading Files</h2>"
        "<p>To download all shared files from the HTTP server:</p>"
        "<ol>"
        "<li>Open PowerShell as an administrator.</li>"
        "<li>Navigate to the directory where the <code>DownloadFiles.ps1</code> script is saved or click the button below to view the script.</li>"
        "<li>Run the script: <code>.\\DownloadFiles.ps1</code></li>"
        "<li>Enter the HTTP server URL when prompted (e.g., <code>http://192.168.0.15:11234/abc123def/Share/</code>).</li>"
        "<li>The script will download all files to the <code>Downloads\\SharedFiles</code> directory.</li>"
        "</ol>"
        );

    // Add a button to show the PowerShell script
    QPushButton *showScriptButton = new QPushButton("Show PowerShell Script", tab);
    showScriptButton->setMaximumWidth(160);
    connect(showScriptButton, &QPushButton::clicked, this, &MainWindow::showPowerShellScript);

    layout->addWidget(helpText);
    layout->addWidget(showScriptButton);
    tab->setLayout(layout);
}

void MainWindow::showPowerShellScript() {
    ScriptDialog *dialog = new ScriptDialog(this);
    dialog->exec(); // Show the dialog as a modal window
}

void MainWindow::setupConfigureTab(QWidget *tab)
{
    QVBoxLayout *layout = new QVBoxLayout(tab);

    // Allowed IPs
    QLabel *allowedIPLabel = new QLabel("Allowed to Connect IPs:", tab);
    allowedIPInput = new QLineEdit(tab);
    allowedIPInput->setPlaceholderText("Enter IP address");
    QPushButton *addAllowedIPButton = new QPushButton("Add IP", tab);
    connect(addAllowedIPButton, &QPushButton::clicked, this, &MainWindow::addAllowedIP);

    allowedIPsList = new QListWidget(tab);
    allowedIPsList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    allowedIPsList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(allowedIPsList, &QListWidget::customContextMenuRequested, this, &MainWindow::showAllowedIPsContextMenu);

    QHBoxLayout *allowedIPLayout = new QHBoxLayout();
    allowedIPLayout->addWidget(allowedIPLabel);
    allowedIPLayout->addWidget(allowedIPInput);
    allowedIPLayout->addWidget(addAllowedIPButton);
    layout->addLayout(allowedIPLayout);
    layout->addWidget(allowedIPsList);

    // Save Configuration Button
    QPushButton *saveConfigButton = new QPushButton("Save Configuration", tab);
    saveConfigButton->setFixedWidth(150);
    connect(saveConfigButton, &QPushButton::clicked, this, &MainWindow::saveConfiguration);
    layout->addWidget(saveConfigButton);

    tab->setLayout(layout);
}

void MainWindow::updateAllowedIPs()
{
    QSet<QString> allowedIPs;
    for (int i = 0; i < allowedIPsList->count(); ++i) {
        allowedIPs.insert(allowedIPsList->item(i)->text());
    }

    fileServer->setAllowedIPs(allowedIPs);
    httpServer->setAllowedIPs(allowedIPs);
}

void MainWindow::addAllowedIP()
{
    QString ip = allowedIPInput->text();
    if (isValidIP(ip)) {
        allowedIPsList->addItem(ip);
        allowedIPInput->clear();
        updateAllowedIPs();
        saveConfiguration();
    } else {
        QMessageBox::warning(this, "Invalid IP", "Please enter a valid IPv4 address.");
    }
}

void MainWindow::showAllowedIPsContextMenu(const QPoint &pos)
{
    if (allowedIPsList->count() == 0) {
        return;
    }

    QMenu contextMenu(this);
    contextMenu.setStyleSheet(
        "QMenu::item {"
        "    padding: 5px;"
        "    text-align: center;"
        "    color: black;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #2389db;"
        "    color: white;"
        "}"
        );

    QAction *removeAction = contextMenu.addAction("Remove Selected IPs");
    connect(removeAction, &QAction::triggered, this, &MainWindow::removeSelectedIPs);

    contextMenu.exec(allowedIPsList->mapToGlobal(pos));
}

void MainWindow::removeSelectedIPs()
{
    QList<QListWidgetItem*> selectedItems = allowedIPsList->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::warning(this, "No Selection", "Please select one or more IPs to remove.");
        return;
    }

    for (QListWidgetItem *item : selectedItems) {
        delete item;
    }

    updateAllowedIPs();
    saveConfiguration();
    updateStatus("Selected IPs removed.");
}

void MainWindow::saveConfiguration()
{
    QSet<QString> allowedIPs;
    for (int i = 0; i < allowedIPsList->count(); ++i) {
        allowedIPs.insert(allowedIPsList->item(i)->text());
    }

    QJsonObject config;
    config["allowedIPs"] = QJsonArray::fromStringList(allowedIPs.values());

    QFile configFile("config.json");
    if (configFile.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(config);
        configFile.write(doc.toJson());
        configFile.close();
    }

    emit updateStatus("Saved Configuration");
}

void MainWindow::loadConfiguration()
{
    QFile configFile("config.json");
    if (!configFile.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open config file";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
    QJsonObject config = doc.object();

    allowedIPsList->clear();
    QJsonArray allowedIPsArray = config["allowedIPs"].toArray();
    for (const QJsonValue &value : allowedIPsArray) {
        QString ip = value.toString();
        if (!ip.isEmpty()) {
            allowedIPsList->addItem(ip);
        }
    }

    QSet<QString> allowedIPs;
    for (int i = 0; i < allowedIPsList->count(); ++i) {
        allowedIPs.insert(allowedIPsList->item(i)->text());
    }
    fileServer->setAllowedIPs(allowedIPs);
    httpServer->setAllowedIPs(allowedIPs);
}

void MainWindow::updateProgress(int percentage)
{
    progressBar->setValue(percentage);
}

bool MainWindow::isValidIP(const QString &ipAddress)
{
    QRegularExpression ipRegex("^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\."
                               "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\."
                               "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\."
                               "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    return ipRegex.match(ipAddress).hasMatch();
}

void MainWindow::onConnect()
{
    QString ipAddress = ipInput->text();
    if (ipAddress.isEmpty()) {
        updateStatus("Please enter an IP address.");
        return;
    }

    if (!isValidIP(ipAddress)) {
        updateStatus("Invalid IP address. Please enter a valid IPv4 address (e.g., 192.168.1.101).");
        QMessageBox::warning(this, "Invalid IP", "Please enter a valid IPv4 address (e.g., 192.168.1.101).");
        return;
    }

    bool connectionSuccessful = fileClient->connectToServer(ipAddress);

    if (connectionSuccessful) {
        isConnected = true;
        connectButton->setVisible(false);
        disconnectButton->setVisible(true);
        ipInput->setEnabled(false);
        updateStatus("Connected to " + ipAddress + " successfully.");
    } else {
        isConnected = false;
        updateStatus("Connection failed. Unable to connect to " + ipAddress + ".");
        QMessageBox::warning(this, "Connection Failed", "Unable to connect to the specified IP address.");
    }
}

void MainWindow::onDisconnect()
{
    fileClient->disconnectFromServer();
    isConnected = false;
    connectButton->setVisible(true);
    disconnectButton->setVisible(false);
    ipInput->setEnabled(true);
    updateStatus("Disconnected.");
}

void MainWindow::onSendFiles()
{
    if (!isConnected) {
        QMessageBox::warning(this, "Not Connected", "Please connect to an IP address first.");
        return;
    }

    progressBar->setValue(0);

    QStringList files;
    for (int i = 0; i < fileListWidget->count(); ++i) {
        files.append(fileListWidget->item(i)->text());
    }

    if (files.isEmpty()) {
        QMessageBox::warning(this, "No Files", "Please select files to send.");
        return;
    }

    QString ipAddress = ipInput->text();
    if (ipAddress.isEmpty()) {
        QMessageBox::warning(this, "No IP Address", "Please select an IP address to send to.");
        return;
    }

    fileClient->sendFiles(files, ipAddress);
    sentFilesWidget->addItems(files);
    fileListWidget->clear();
}

void MainWindow::onFileReceived(const QString &filePath)
{
    receivedFilesWidget->addItem(filePath);
    updateStatus("File received: " + filePath);
}

void MainWindow::updateStatus(const QString &message)
{
    statusBar->showMessage(message);

    if (message.contains("successful", Qt::CaseInsensitive)) {
        statusBar->setStyleSheet("background-color: #4CAF50; color: white;");
    } else if (message.contains("Failed", Qt::CaseInsensitive)) {
        statusBar->setStyleSheet("background-color: #F44336; color: white;");
    } else {
        statusBar->setStyleSheet("background-color: #f0f0f0; color: black;");
    }
}

void MainWindow::onBrowseDownloadLocation()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Download Location", downloadLocation);
    if (!dir.isEmpty()) {
        downloadLocation = dir;
        downloadLocationLabel->setText(downloadLocation);
        fileServer->setDownloadLocation(downloadLocation);
    }
}

void MainWindow::clearSentFiles()
{
    sentFilesWidget->clear();
    progressBar->setValue(0);
}

void MainWindow::clearReceivedFiles()
{
    receivedFilesWidget->clear();
}

void MainWindow::showFileListContextMenu(const QPoint &pos)
{
    if (fileListWidget->count() == 0) {
        return;
    }

    QMenu contextMenu(this);
    contextMenu.setStyleSheet(
        "QMenu::item {"
        "    padding: 5px;"
        "    text-align: center;"
        "    color: black;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #2389db;"
        "    color: white;"
        "}"
        );

    QAction *removeAction = contextMenu.addAction("Remove");
    connect(removeAction, &QAction::triggered, this, &MainWindow::removeSelectedFiles);

    contextMenu.exec(fileListWidget->mapToGlobal(pos));
}

void MainWindow::removeSelectedFiles()
{
    QList<QListWidgetItem*> selectedItems = fileListWidget->selectedItems();
    for (QListWidgetItem *item : selectedItems) {
        delete item;
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QStringList allFiles;
        for (const QUrl &url : mimeData->urls()) {
            QString path = url.toLocalFile();
            if (!path.isEmpty()) {
                QFileInfo fileInfo(path);
                if (fileInfo.isDir()) {
                    QDir dir(path);
                    QStringList folderFiles = dir.entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
                    for (const QString &file : folderFiles) {
                        allFiles.append(dir.filePath(file));
                    }
                } else if (fileInfo.isFile()) {
                    allFiles.append(path);
                }
            }
        }

        if (!allFiles.isEmpty()) {
            fileListWidget->addItems(allFiles);
        }
    }
}


//TODO:
void MainWindow::setupHttpServerTab(QWidget *tab) {
    QVBoxLayout *layout = new QVBoxLayout(tab);

    // Shared Files Section
    QLabel *sharedFilesLabel = new QLabel("Shared Files:", tab);
    httpSharedFilesList = new QListWidget(tab);
    httpSharedFilesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // httpSharedFilesList->setAcceptDrops(true); // Enable drag-and-drop
    // httpSharedFilesList->setDropIndicatorShown(true);
    httpSharedFilesList->setContextMenuPolicy(Qt::CustomContextMenu);

    // Connect the context menu signal
    connect(httpSharedFilesList, &QListWidget::customContextMenuRequested, this, &MainWindow::showHttpSharedFilesContextMenu);

    QHBoxLayout *sharedFilesButtonLayout = new QHBoxLayout();
    addHttpSharedFilesButton = new QPushButton("Add Files", tab);
    removeHttpSharedFilesButton = new QPushButton("Remove Files", tab);
    sharedFilesButtonLayout->addWidget(addHttpSharedFilesButton);
    sharedFilesButtonLayout->addWidget(removeHttpSharedFilesButton);

    connect(addHttpSharedFilesButton, &QPushButton::clicked, this, &MainWindow::onAddHttpSharedFiles);
    connect(removeHttpSharedFilesButton, &QPushButton::clicked, this, &MainWindow::onRemoveHttpSharedFiles);

    // HTTP URL Display
    QLabel *httpUrlLabel = new QLabel("HTTP URL:", tab);
    httpUrlInput = new QLineEdit(tab);
    httpUrlInput->setReadOnly(true);

    // Add to Layout
    layout->addWidget(sharedFilesLabel);
    layout->addWidget(httpSharedFilesList);
    layout->addLayout(sharedFilesButtonLayout);
    layout->addWidget(httpUrlLabel);
    layout->addWidget(httpUrlInput);

    tab->setLayout(layout);
}

void MainWindow::onHttpServerStarted(const QString &url)
{
    httpUrlInput->setText(url);
}

void MainWindow::onHttpServerStopped()
{
    httpUrlInput->clear();
}

void MainWindow::onAddHttpSharedFiles() {
    QStringList filesAndFolders = QFileDialog::getOpenFileNames(this, "Select Files or Folders", QDir::homePath(), "All Files (*)");
    QStringList allFiles;

    for (const QString &path : filesAndFolders) {
        QFileInfo fileInfo(path);
        if (fileInfo.isDir()) {
            QDir dir(path);
            QFileInfoList fileList = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name);
            for (const QFileInfo &file : fileList) {
                allFiles.append(file.absoluteFilePath());
            }
        } else if (fileInfo.isFile()) {
            allFiles.append(fileInfo.absoluteFilePath());
        }
    }

    if (!allFiles.isEmpty()) {
        for (const QString &file : allFiles) {
            httpServer->addSharedFile(file);
        }
        httpSharedFilesList->addItems(allFiles);
    }
}

void MainWindow::onRemoveHttpSharedFiles() {
    QList<QListWidgetItem*> selectedItems = httpSharedFilesList->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::warning(this, "No Selection", "Please select one or more files to remove.");
        return;
    }

    for (QListWidgetItem *item : selectedItems) {
        httpServer->removeSharedFile(item->text()); // Remove from the server's shared files list
        delete item; // Remove from the UI
    }
}


void MainWindow::showHttpSharedFilesContextMenu(const QPoint &pos) {
    if (httpSharedFilesList->count() == 0) {
        return; // No files to remove
    }

    // Create the context menu
    QMenu contextMenu(this);
    contextMenu.setStyleSheet(
        "QMenu::item {"
        "    padding: 5px;"
        "    text-align: center;"
        "    color: black;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #2389db;"
        "    color: white;"
        "}"
        );

    // Add a "Remove" action to the context menu
    QAction *removeAction = contextMenu.addAction("Remove Selected Files");
    connect(removeAction, &QAction::triggered, this, &MainWindow::onRemoveHttpSharedFiles);

    // Show the context menu at the requested position
    contextMenu.exec(httpSharedFilesList->mapToGlobal(pos));
}


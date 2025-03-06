#include "scriptdialog.h"

ScriptDialog::ScriptDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("PowerShell Script");
    setMinimumSize(600, 400);

    QVBoxLayout *layout = new QVBoxLayout(this);

    // Create a QTextEdit to display the script
    scriptTextEdit = new QTextEdit(this);
    scriptTextEdit->setReadOnly(true);
    scriptTextEdit->setPlainText(
        "# Prompt the user to enter the HTTP server URL\n"
        "$serverUrl = Read-Host \"Enter the HTTP server URL (e.g., http://192.168.0.15:11234/abc123def/Share/)\"\n\n"
        "# Create a download directory\n"
        "$downloadDir = \"$env:USERPROFILE\\Downloads\\SharedFiles\"\n"
        "if (-not (Test-Path $downloadDir)) {\n"
        "    New-Item -ItemType Directory -Path $downloadDir | Out-Null\n"
        "}\n\n"
        "# Fetch the list of files from the server\n"
        "$fileListUrl = \"$serverUrl\"\n"
        "$response = Invoke-WebRequest -Uri $fileListUrl -UseBasicParsing\n\n"
        "# Parse the HTML to extract file links\n"
        "$fileLinks = $response.Links | Where-Object { $_.href -like \"*/Share/*\" } | Select-Object -ExpandProperty href\n\n"
        "# Download each file\n"
        "foreach ($fileLink in $fileLinks) {\n"
        "    $fileName = $fileLink.Split(\"/\")[-1] # Extract the file name from the URL\n"
        "    $fileUrl = \"$serverUrl$fileName\"\n"
        "    $outputPath = Join-Path $downloadDir $fileName\n\n"
        "    Write-Host \"Downloading $fileName...\"\n"
        "    Invoke-WebRequest -Uri $fileUrl -OutFile $outputPath -UseBasicParsing\n"
        "}\n\n"
        "Write-Host \"All files have been downloaded to $downloadDir.\"\n"
        );

    // Add a close button
    QPushButton *closeButton = new QPushButton("Close", this);
    closeButton->setMaximumWidth(120);
    connect(closeButton, &QPushButton::clicked, this, &ScriptDialog::close);

    layout->addWidget(scriptTextEdit);
    layout->addWidget(closeButton);
    setLayout(layout);
}

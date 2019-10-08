#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "manifest.h"
#include "manifestitem.h"

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QString *switchProcess = nullptr, QWidget *parent = nullptr);
    ~MainWindow();

private:
    QNetworkAccessManager netMan;
    Ui::MainWindow *ui;
    Manifest* manifest;
    QByteArray manifestChecksum;
    long currentFiles;
    long maxFiles;

    bool validate(QString fname, QString checksum);
    void setup();
    void download(QUrl baseUrl, QString fname, QString checksum, QString version, QString *switchProcess, QProgressDialog *progress);
    void checkUpdate(QString *switchProcess);
    void selfUpdate();
    void setManifest(Manifest* manifest);
    void validateManifest(Manifest* manifest);
    void downloadManifest(QUrl url);
    void openManifest(QString fname);
    void downloadItem(ManifestItem* item);
    void deleteItem(QString *item);
    void loadManifests();

};
#endif // MAINWINDOW_H

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "manifest.h"
#include "optionswindow.h"
#include "errorwindow.h"
#include "launchprofileitemdelegate.h"

#include <QtConcurrent>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDesktopServices>

MainWindow::MainWindow (
        QWidget *parent )
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {

    setup();

}

void MainWindow::setup() {

    ui->setupUi(this);
    ui->listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->listWidget->setItemDelegate(new LaunchProfileItemDelegate);

    loadManifests();

    connect (
        ui->ScreenshotButton,
        &QPushButton::released,
        [] {
            QString path = QDir::cleanPath(QDir::currentPath() + QDir::separator() + "screenshots");
            QDir(path).mkpath(".");
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });

    connect (
        ui->OptionsButton,
        &QPushButton::released,
        [this] {
            ui->OptionsButton->setEnabled(false);
            OptionsWindow *w = new OptionsWindow(this);
            w->show();
            connect(w, &QDialog::finished, [this] {
                ui->OptionsButton->setEnabled(true);
                loadManifests();
            });
        });

    connect (
        ui->listWidget,
        &QListWidget::itemClicked,
        [this] {
            QListWidgetItem *item = ui->listWidget->currentItem();
            ServerEntry *entry = item->data(Qt::UserRole + 1).value<ServerEntry*>();
            setManifest(entry->manifest);
        });

    connect (
        ui->LaunchButton,
        &QPushButton::released,
        [this] {
            QProcess *proc = new QProcess(this);
            QListWidgetItem *item = ui->listWidget->currentItem();
            ServerEntry *server = item->data(Qt::UserRole + 1).value<ServerEntry*>();
            proc->startDetached(server->client, server->args.split(" "));
        });

    connect (
        ui->ValidateButton,
        &QPushButton::released,
        [this] {
            validateManifest(manifest);
        });

}

void MainWindow::deleteItem(QString *item) {

    QFile file(*item);
    if(!file.exists()) {
        qInfo() << *item << " does not exist, so not deleting";
        return;
    }

    QMessageBox::StandardButton choice = QMessageBox::question(this, "Delete File", "Delete " + *item);
    if(choice == QMessageBox::Yes)
        if(!file.remove())
            QMessageBox::warning(this, "File Delete Error", "Unable to delete " + *item);

}

void MainWindow::downloadItem(ManifestItem *item) {

    QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>(this);
    QFuture<bool> future = QtConcurrent::run([item]{
        return item->validate();
    });

    connect(watcher, &QFutureWatcher<bool>::finished, [=] {

        if(future.result()) {
            qInfo() << item->fname + " validated";
            currentFiles++;
            ui->UpdateProgress->setValue(currentFiles);
            if(currentFiles + errorFiles.length() >= maxFiles) {
                qInfo() << "last file";
                if(errorFiles.length() <= 0) {
                    QSettings settings;
                    settings.setValue("manifestChecksum", manifest->checksum);
                    settings.setValue("oldDir", QDir::currentPath());
                    qInfo() << QDir::currentPath();
                    qInfo() << settings.value("oldDir").toString();
                    ui->LaunchButton->setEnabled(true);
                } else {
                    qWarning() << "Opening error window.";
                    ErrorWindow *w = new ErrorWindow(this);
                    w->addErrors(errorFiles);
                    w->show();
                }
                ui->ValidateButton->setEnabled(true);
                ui->listWidget->setEnabled(true);
            }
            return;
        }

        if(item->urls.isEmpty()) {
            qWarning() << "failed to download " << item->fname;
            errorFiles.append(item->fname + " failed to download");
            if(currentFiles + errorFiles.length() >= maxFiles) {
                ui->ValidateButton->setEnabled(true);
                ui->listWidget->setEnabled(true);
                qWarning() << "Opening error window.";
                ErrorWindow *w = new ErrorWindow(this);
                w->addErrors(errorFiles);
                w->show();
            }
            return;
        }

        QFileInfo(item->fname).dir().mkpath(".");
        QSaveFile *file = new QSaveFile(item->fname);
        if(!file->open(QIODevice::WriteOnly)) {
            qWarning() << "failed to write to " << item->fname;
            errorFiles.append(item->fname + " failed to create");
            if(currentFiles + errorFiles.length() >= maxFiles) {
                ui->ValidateButton->setEnabled(true);
                ui->listWidget->setEnabled(true);
                qWarning() << "Opening error window.";
                ErrorWindow *w = new ErrorWindow(this);
                w->addErrors(errorFiles);
                w->show();
            }
            return;
        }

        QNetworkRequest req(*item->urls.takeLast());
        req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        QNetworkReply *res = netMan.get(req);
        connect (
            res,
            &QNetworkReply::readyRead,
            [=] {
               file->write(res->read(res->bytesAvailable()));
            });
        connect (
            res,
            &QNetworkReply::finished,
            [=] {

               if(res->error() != QNetworkReply::NoError)
                   qWarning() << res->request().url() << res->errorString();

               res->deleteLater();
               file->commit();
               this->downloadItem(item);

            });

        watcher->deleteLater();

    });

    watcher->setFuture(future);

}

void MainWindow::addServerEntry(ServerEntry *server) {

    QListWidgetItem *item = new QListWidgetItem(server->name, ui->listWidget);
    item->setData(Qt::UserRole + 1, QVariant::fromValue(server));

    if(!server->icon.isEmpty()) {
        QNetworkRequest req(server->icon);
        req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        QNetworkReply *res = netMan.get(req);
        connect (
            res,
            &QNetworkReply::finished,
            [=] {

               if(res->error() != QNetworkReply::NoError) {
                   qWarning() << "icon: " << res->errorString();
                   return;
               }

               QPixmap pixels;
               if(!pixels.loadFromData(res->readAll()))
                   qWarning() << "unable to read icon: " << server->icon;
               else
                   item->setIcon(QIcon(pixels));

               res->deleteLater();

            });
    }

    if(!server->motd.isEmpty()) {
        QNetworkRequest req(server->motd);
        req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        QNetworkReply *res = netMan.get(req);
        item->setData(Qt::UserRole, "Retrieving MoTD");
        connect (
            res,
            &QNetworkReply::finished,
            [=] {

               if(res->error() != QNetworkReply::NoError) {
                   qWarning() << "motd: " << res->errorString();
                   item->setData(Qt::UserRole, "Failed to retrieve MoTD");
                   return;
               }

               QString motd(res->read(140));
               item->setData(Qt::UserRole, motd);

               res->deleteLater();

            });
    }

}

void MainWindow::openManifest(QString fname) {

    ui->ValidateButton->setEnabled(false);
    ui->UpdateProgress->setValue(false);

    QDomDocument doc;
    QFile file(fname);
    if(file.open(QIODevice::ReadOnly) && doc.setContent(&file)) {
        QCryptographicHash md5(QCryptographicHash::Md5);
        md5.addData(doc.toByteArray());
        Manifest *manifest = new Manifest(doc, md5.result());
        for(ServerEntry *server : manifest->servers)
            addServerEntry(server);
    }
    else
        qWarning() << "unable to read manifest: " + fname;
    file.close();

}

void MainWindow::downloadManifest(QUrl url) {

    ui->ValidateButton->setEnabled(false);
    ui->UpdateProgress->setValue(false);

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    QNetworkReply *res = netMan.get(req);
    connect (
        res,
        &QNetworkReply::finished,
        [=] {

           if(res->error() != QNetworkReply::NoError)
               qCritical() << "manifest: " << res->errorString();

           QByteArray content = res->readAll();
           QDomDocument doc = QDomDocument();
           doc.setContent(content);

           QCryptographicHash md5(QCryptographicHash::Md5);
           md5.addData(content);

           Manifest *manifest = new Manifest(doc, md5.result());
           for(ServerEntry *server : manifest->servers)
               addServerEntry(server);

           res->deleteLater();

        });

}

void MainWindow::setManifest(Manifest *manifest) {

    this->manifest = manifest;

    ui->LaunchButton->setEnabled(false);
    ui->ValidateButton->setEnabled(true);
    ui->UpdateProgress->setValue(0);

    QSettings settings;
    QByteArray oldChecksum = settings.value("manifestChecksum").toByteArray();
    QString oldDir = settings.value("oldDir").toString();
    qInfo() << "old manifest: " + oldChecksum.toHex();
    qInfo() << "new manifest: " + manifest->checksum.toHex();
    qInfo() << "old dir: " + oldDir;
    qInfo() << "new dir: " + QDir::currentPath();

    if(oldChecksum == manifest->checksum && oldDir == QDir::currentPath()) {
        currentFiles = manifest->items.size();
        ui->UpdateProgress->setValue(currentFiles);
        ui->UpdateProgress->setMaximum(currentFiles);
        ui->LaunchButton->setEnabled(true);
    }

}

void MainWindow::validateManifest(Manifest *manifest) {
    QSettings settings;
    settings.remove("manifestChecksum");
    settings.remove("oldDir");
    for(QString *item : manifest->deletions)
        deleteItem(item);
    currentFiles = 0;
    errorFiles.clear();
    maxFiles = manifest->items.size();
    ui->ValidateButton->setEnabled(false);
    ui->LaunchButton->setEnabled(false);
    ui->listWidget->setEnabled(false);
    ui->UpdateProgress->setMaximum(maxFiles);
    for(ManifestItem *item : manifest->items)
        downloadItem(item);
}

void MainWindow::loadManifests() {

    ui->listWidget->clear();
    QSettings settings;
    QStringList manifests = settings.value("manifests").toString().split(" ");

    for(QString manifest : manifests) {
        QUrl url = QUrl::fromUserInput(manifest);
        if(!url.isLocalFile())
            downloadManifest(url);
        else
            openManifest(manifest);
    }

}

MainWindow::~MainWindow() {
    delete ui;
}

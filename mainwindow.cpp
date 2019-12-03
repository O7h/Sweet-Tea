#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "manifest.h"
#include "optionswindow.h"

#include <QtConcurrent>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDesktopServices>
#include <QSettings>

MainWindow::MainWindow (
        QString *switchProcess,
        QWidget *parent )
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {

    checkUpdate(switchProcess);

}

void MainWindow::setup() {

    ui->setupUi(this);
    ui->WebView->setUrl(QDir(QCoreApplication::applicationDirPath())
                         .filePath("html/index.html"));
    ui->listWidget->setSelectionMode(QAbstractItemView::SingleSelection);

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
            ServerEntry *entry = item->data(Qt::UserRole).value<ServerEntry*>();
            ui->WebView->setUrl(entry->site);
            setManifest(entry->manifest);
        });

    connect (
        ui->LaunchButton,
        &QPushButton::released,
        [this] {
            QProcess *proc = new QProcess(this);
            QListWidgetItem *item = ui->listWidget->currentItem();
            ServerEntry *server = item->data(Qt::UserRole).value<ServerEntry*>();
			QList<QString> args = server->args.split(" ");
			args.prepend(server->client);
            proc->startDetached("wine", args);
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
            if(currentFiles + errorFiles == maxFiles) {
                if(errorFiles <= 0) {
					QSettings settings(QSettings::UserScope, "ThunderspyGaming");
                    settings.setValue("manifestChecksum", manifest->checksum);
                    settings.setValue("oldDir", QDir::currentPath());
                    ui->LaunchButton->setEnabled(true);
                }
                ui->ValidateButton->setEnabled(true);
                ui->listWidget->setEnabled(true);
            }
            return;
        }

        if(item->urls.isEmpty()) {
            qWarning() << "failed to download " << item->fname;
            errorFiles++;
            if(currentFiles + errorFiles == maxFiles) {
                ui->ValidateButton->setEnabled(true);
                ui->listWidget->setEnabled(true);
            }
            return;
        }

        QFileInfo(item->fname).dir().mkpath(".");
        QSaveFile *file = new QSaveFile(item->fname);
        if(!file->open(QIODevice::WriteOnly)) {
            qWarning() << "failed to write to " << item->fname;
            errorFiles++;
            if(currentFiles + errorFiles == maxFiles) {
                ui->ValidateButton->setEnabled(true);
                ui->listWidget->setEnabled(true);
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
    item->setData(Qt::UserRole, QVariant::fromValue(server));

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

    QSettings settings(QSettings::UserScope, "ThunderspyGaming");
    QByteArray oldChecksum = settings.value("manifestChecksum").toByteArray();
    QString oldDir = settings.value("oldDIr").toString();
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
    QSettings settings(QSettings::UserScope, "ThunderspyGaming");
    settings.remove("manifestChecksum");
    settings.remove("oldDir");
    for(QString *item : manifest->deletions)
        deleteItem(item);
    currentFiles = 0;
    errorFiles = 0;
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
    QSettings settings(QDir(QCoreApplication::applicationDirPath())
                       .filePath("sweet-tea.ini"),
                       QSettings::IniFormat);
    QStringList manifests = settings.value("manifests").toStringList();

    for(QString manifest : manifests) {
        QUrl url = QUrl::fromUserInput(manifest);
        if(!url.isLocalFile())
            downloadManifest(url);
        else
            openManifest(manifest);
    }

}

void MainWindow::checkUpdate(QString *switchProcess) {

    qDebug() << "checking for updates";    

    QUrl url("http://files.thunderspygaming.net/sweet-tea/manifest");
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    QNetworkReply *res = netMan.get(req);
    connect (
        res,
        &QNetworkReply::finished,
        [=] {

           if(res->error() != QNetworkReply::NoError) {
               qWarning() << res->request().url() << res->errorString();
               setup();
               return;
           }

           QString lastUpdate = QSettings(QDir(QCoreApplication::applicationDirPath())
                                 .filePath("sweet-tea.ini"),
                                 QSettings::IniFormat)
                   .value("lastUpdate")
                   .toString();
           QString version = res->readLine().trimmed();
           if(lastUpdate == version) {
               qInfo() << "no update; starting app";
               setup();
               return;
           }

           QMessageBox::StandardButton button = QMessageBox::question (
                       this,
                       "New Update",
                       "A new update of Sweet Tea is available. Update?"
                       );

          if(button != QMessageBox::Yes) {
              qInfo() << "canceled update; starting app";
              setup();
              return;
          }

          QProgressBar *bar = new QProgressBar;
          bar->setFormat("%v/%m");
          QProgressDialog *progress = new QProgressDialog();
          progress->setBar(bar);
          progress->setAutoReset(true);
          progress->setAutoClose(true);
          progress->show();

          QUrl baseUrl(res->readLine().trimmed());
          QList<QByteArray> lines = res->readAll().trimmed().split('\n');
          currentFiles = 0;
          maxFiles = lines.size();
          progress->setMaximum(maxFiles);
          for(QString line : lines) {
              QStringList entry = line.split(' ');
              download(baseUrl, entry.at(1), entry.at(0), version, switchProcess, progress);
          }

           res->deleteLater();

    });

}

bool MainWindow::validate(QString fname, QString checksum) {
    QFile file(QDir(QCoreApplication::applicationDirPath()).filePath(fname));
    QCryptographicHash hash(QCryptographicHash::Md5);
    return file.exists()
            && file.open(QFile::ReadOnly)
            && hash.addData(&file)
            && hash.result() == QByteArray::fromHex(checksum.toLatin1());
}

void MainWindow::download(QUrl baseUrl, QString fname, QString checksum, QString version, QString *switchProcess, QProgressDialog *progress) {

    QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>(this);
    QFuture<bool> future = QtConcurrent::run([this, fname, checksum] {
         return validate(fname, checksum);
    });
    connect (
         watcher,
         &QFutureWatcher<bool>::finished,
         [=] {
             if(future.result()) {
                 currentFiles++;
                 qInfo() << fname << " validated";
                 qInfo() << currentFiles << "/" << maxFiles;
                 progress->setValue(currentFiles);
                 if(currentFiles == maxFiles) {
                     progress->deleteLater();
                     QSettings settings(QDir(QCoreApplication::applicationDirPath())
                             .filePath("sweet-tea.ini"),
                             QSettings::IniFormat);
                     settings.setValue("lastUpdate", version);
                     qInfo() << "validated last file";
                     if(switchProcess == nullptr) {
                         qInfo() << "starting main app";
                        setup();
                     }
                     else {
                         qInfo() << "switching processes " << *switchProcess;
                         if(QProcess::startDetached(*switchProcess))
                            QApplication::quit();
                         else
                             qCritical() << "can't swap process: " << *switchProcess;
                     }
                 }
                 watcher->deleteLater();
                 return;
             }

             qInfo() << fname << " downloading";

             QFileInfo(fname).dir().mkpath(".");
             QSaveFile *file = new QSaveFile(QDir(QCoreApplication::applicationDirPath()).filePath(fname));

             if(QFileInfo(fname).fileName() == QFileInfo(QCoreApplication::applicationFilePath()).fileName()) {
                 selfUpdate();
             }

             if(!file->open(QIODevice::WriteOnly)) {
                 qWarning() << "failed to write create " << fname;
                 return;
             }

             QNetworkRequest req(QUrl(baseUrl.toString() + fname));
             req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
             QNetworkReply *res = netMan.get(req);
             connect (
                 res,
                 &QNetworkReply::readyRead,
                 [res, file] {
                    if(file->write(res->read(res->bytesAvailable())) < 0)
                        qWarning() << "failed to write to " << file->fileName();
                 });
             connect (
                 res,
                 &QNetworkReply::finished,
                 [=] {

                    if(res->error() != QNetworkReply::NoError)
                        qWarning() << res->request().url() << res->errorString();

                    res->deleteLater();
                    file->commit();

                    QFutureWatcher<bool> *otherWatcher = new QFutureWatcher<bool>(this);
                    QFuture<bool> future = QtConcurrent::run([this, fname, checksum] {
                             return validate(fname, checksum);
                        });
                    connect (
                        otherWatcher,
                        &QFutureWatcher<bool>::finished,
                        [=] {
                            if(future.result()) {
                                currentFiles++;
                                qInfo() << fname << " validated";
                                qInfo() << currentFiles << "/" << maxFiles;
                                progress->setValue(currentFiles);
                                if(currentFiles == maxFiles) {
                                    progress->deleteLater();
                                    QSettings settings(QDir(QCoreApplication::applicationDirPath())
                                                       .filePath("sweet-tea.ini"),
                                                       QSettings::IniFormat);
                                    settings.setValue("lastUpdate", version);
                                    qInfo() << "validated last file";
                                    if(switchProcess == nullptr) {
                                        qInfo() << "starting main app";
                                       setup();
                                    }
                                    else {
                                        qInfo() << "switching processes " << *switchProcess;
                                        if(QProcess::startDetached(*switchProcess))
                                           QApplication::quit();
                                        else
                                            qCritical() << "can't swap process: " << *switchProcess;
                                    }
                                }
                                else {
                                    progress->setLabelText("error: " + fname);
                                }
                            }
                            otherWatcher->deleteLater();
                            watcher->deleteLater();
                        });
                    otherWatcher->setFuture(future);

                 });

         });
    watcher->setFuture(future);

}

void MainWindow::selfUpdate() {

    QFile self(QCoreApplication::applicationFilePath());
    QFile(QCoreApplication::applicationFilePath() + ".old").remove();
    if (
            self.open(QIODevice::ReadOnly)
            && self.copy(QCoreApplication::applicationFilePath() + ".old")
            && QProcess::startDetached(QCoreApplication::applicationFilePath() + ".old", QStringList() << "Sweet-Tea.exe") ) {
        qInfo() << "switching process: " << QCoreApplication::applicationFilePath() + ".old";
        QApplication::quit();
    }
    else
        qWarning() << "can't copy " << QCoreApplication::applicationFilePath();

}

MainWindow::~MainWindow() {
    delete ui;
}

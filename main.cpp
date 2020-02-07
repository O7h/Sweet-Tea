#include "mainwindow.h"
#include "manifest.h"

#include <QtDebug>
#include <QApplication>

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);

    a.setApplicationName("Sweet Tea");
    a.setOrganizationName("Thunderspy Gaming");

    QSettings settings(QDir(QCoreApplication::applicationDirPath())
                       .filePath("sweet-tea.ini"),
                       QSettings::IniFormat);
    QString datadir = settings.value (
                "datadir",
                QStandardPaths::writableLocation(QStandardPaths::DataLocation)
                ).toString();
    if(!QDir::setCurrent(datadir)) {
        qWarning() << "unable to access: " + datadir;
        if(QDir(datadir).mkpath(".") && QDir::setCurrent(datadir))
            settings.remove("oldDir");
        else
            qWarning() << "unable to create: " + datadir;
    }

    MainWindow w;
    w.show();

    return a.exec();

}

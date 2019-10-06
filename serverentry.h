#ifndef SERVERENTRY_H
#define SERVERENTRY_H

#include <QObject>
#include <QUrl>
#include <QFile>

class ServerEntry : public QObject
{
    Q_OBJECT
public:
    explicit ServerEntry (
            QString name,
            QUrl site,
            QString client,
            QString args,
            QObject *parent = nullptr );

    QString name;
    QUrl site;
    QString client;
    QString args;

};

#endif // SERVERENTRY_H

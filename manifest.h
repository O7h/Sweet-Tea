#ifndef MANIFEST_H
#define MANIFEST_H

#include "manifestitem.h"
#include "serverentry.h"

#include <QObject>
#include <QtXml>

class Manifest : public QObject
{
    Q_OBJECT
public:
    explicit Manifest(QDomDocument &doc, QObject *parent = nullptr);
    bool validate();

    QList<ManifestItem*> items;
    QList<ServerEntry*> servers;

};

#endif // MANIFEST_H

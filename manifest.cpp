#include "manifest.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtDebug>

Manifest::Manifest(QDomDocument &doc, QObject *parent)
    : QObject(parent) {

    QDomNodeList filelists = doc.elementsByTagName("file");
    for(int i = 0; i < filelists.size(); i++) {
        QDomNode node = filelists.item(i);
        QString name = node
                .attributes()
                .namedItem("name")
                .nodeValue()
                .trimmed();
        long size = node
                .attributes()
                .namedItem("size")
                .nodeValue()
                .trimmed()
                .toLong();
        QByteArray md5 = QByteArray::fromHex(node
                                             .attributes()
                                             .namedItem("md5")
                                             .nodeValue()
                                             .trimmed()
                                             .toLatin1());
        QList<QUrl*> urls;
        QDomNodeList children = node.childNodes();
        for(int j = 0; j < children.size(); j++) {
            QDomNode child = children.item(j);
            urls.append(new QUrl(child.toElement().text().trimmed()));
        }
        if(!QDir(name).isAbsolute() && !name.contains(".."))
            items.append(new ManifestItem(name, md5, size, urls, this));
        else
            qWarning() << "insecure path not allowed for file: " << name;
    }

    QDomNodeList deleteList = doc.elementsByTagName("deletefile");
    for(int i = 0; i < deleteList.size(); i++) {
        QDomNode node = deleteList.item(i);
        QString name = node.toElement().text().trimmed();
        if(!QDir(name).isAbsolute() && !name.contains(".."))
            deletions.append(new QString(name));
        else
            qWarning() << "insecure path not allowed for file: " << name;
    }

    QDomNodeList profiles = doc.elementsByTagName("launch");
    for(int i = 0; i < profiles.size(); i++) {
        QDomNode node = profiles.item(i);
        QString name = node.toElement().text();
        QString client = node.attributes()
                .namedItem("exec")
                .nodeValue();
        QUrl site(node.attributes()
                  .namedItem("website")
                  .nodeValue());
        QString args = node.attributes()
                .namedItem("params")
                .nodeValue();
        if(!QDir(client).isAbsolute() && !client.contains(".."))
            servers.append(new ServerEntry(name, site, client, args, this));
        else
            qWarning() << "insecure path not allowed for client: " << client;
    }

}

bool Manifest::validate() {
    for(ManifestItem *item : items)
        item->validate();
    return true;
}

#include "serverentry.h"

ServerEntry::ServerEntry (
        QString name,
        QUrl site,
        QString client,
        QString args,
        QObject *parent )
    : QObject(parent),
      name(name),
      site(site),
      client(client),
      args(args) {}

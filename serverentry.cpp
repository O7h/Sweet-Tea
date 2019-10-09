#include "serverentry.h"

ServerEntry::ServerEntry (
        QString name,
        QUrl site,
        QString client,
        QString args,
        Manifest *manifest,
        QObject *parent )
    : QObject(parent),
      name(name),
      site(site),
      client(client),
      args(args),
      manifest(manifest) {}

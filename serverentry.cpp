#include "serverentry.h"

ServerEntry::ServerEntry (
        QString name,
        QUrl site,
        QUrl icon,
        QString client,
        QString args,
        Manifest *manifest,
        QObject *parent )
    : QObject(parent),
      name(name),
      site(site),
      icon(icon),
      client(client),
      args(args),
      manifest(manifest) {}

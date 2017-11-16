#include "dumpgnashprovider.h"

DumpGnashProvider::DumpGnashProvider(QObject *parent) : QObject(parent)
  , QQuickImageProvider(QQuickImageProvider::Image)
{

}

DumpGnashProvider::~DumpGnashProvider()
{

}


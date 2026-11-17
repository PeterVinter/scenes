#pragma once

#include <QQuickImageProvider>

#include "catalog.h"

// image://appicon/<desktop id>: the icon of an installed app, from the icon
// theme, with a generic one for an app that isn't installed here.
class AppIcons : public QQuickImageProvider {
public:
    explicit AppIcons(const Catalog &catalog);
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    const Catalog &m_catalog;
};

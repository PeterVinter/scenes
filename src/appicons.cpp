#include "appicons.h"

#include <QFileInfo>
#include <QIcon>

AppIcons::AppIcons(const Catalog &catalog) : QQuickImageProvider(QQuickImageProvider::Pixmap), m_catalog(catalog) {}

QPixmap AppIcons::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QSize target = requestedSize.isValid() ? requestedSize : QSize(48, 48);
    const AppEntry entry = m_catalog.entry(id);
    QIcon icon;
    if (!entry.icon.isEmpty())
        icon = QFileInfo(entry.icon).isAbsolute() ? QIcon(entry.icon) : QIcon::fromTheme(entry.icon);
    if (icon.isNull())
        icon = QIcon::fromTheme("application-x-executable");
    const QPixmap pixmap = icon.pixmap(target);
    if (size)
        *size = pixmap.size();
    return pixmap;
}

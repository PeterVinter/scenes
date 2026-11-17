#pragma once

#include <QString>

#include "scene.h"

// One JSON file per scene in ~/.config/scenes/<slug>.json.
class SceneStore {
public:
    explicit SceneStore(const QString &directory = {});

    QString directory() const { return m_directory; }
    QString pathFor(const QString &slug) const;
    bool exists(const QString &slug) const;

    // Sorted by name. A file that can't be read is described in problems, never
    // dropped silently, so the window and `scenes list` can say so.
    QList<Scene> list(QStringList *problems = nullptr) const;
    bool load(const QString &nameOrSlug, Scene *scene, QString *error) const;
    // Written beside the target and renamed into place; a failure leaves the
    // old file untouched.
    bool save(const Scene &scene, QString *error) const;
    bool remove(const QString &slug, QString *error) const;

private:
    QString m_directory;
};

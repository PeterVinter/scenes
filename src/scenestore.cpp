#include "scenestore.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>

SceneStore::SceneStore(const QString &directory)
    : m_directory(directory.isEmpty() ? QDir::homePath() + "/.config/scenes" : directory)
{
}

QString SceneStore::pathFor(const QString &slug) const
{
    return m_directory + "/" + slug + ".json";
}

bool SceneStore::exists(const QString &slug) const
{
    return !slug.isEmpty() && QFile::exists(pathFor(slug));
}

bool SceneStore::load(const QString &nameOrSlug, Scene *scene, QString *error) const
{
    const QString slug = Scene::slugify(nameOrSlug);
    if (slug.isEmpty() || !exists(slug)) {
        if (error)
            *error = QString("no scene named \"%1\"").arg(nameOrSlug);
        return false;
    }
    QFile file(pathFor(slug));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QString("can't read %1: %2").arg(file.fileName(), file.errorString());
        return false;
    }
    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QString("%1 is not valid JSON: %2").arg(file.fileName(), parse.errorString());
        return false;
    }
    QString problem;
    if (!Scene::fromJson(document.object(), scene, &problem)) {
        if (error)
            *error = QString("%1: %2").arg(file.fileName(), problem);
        return false;
    }
    return true;
}

QList<Scene> SceneStore::list(QStringList *problems) const
{
    QList<Scene> scenes;
    const QStringList files = QDir(m_directory).entryList({"*.json"}, QDir::Files);
    for (const auto &fileName : files) {
        Scene scene;
        QString error;
        if (load(fileName.left(fileName.size() - 5), &scene, &error))
            scenes.append(scene);
        else if (problems)
            problems->append(error);
    }
    std::sort(scenes.begin(), scenes.end(), [](const Scene &a, const Scene &b) {
        return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
    });
    return scenes;
}

bool SceneStore::save(const Scene &scene, QString *error) const
{
    if (scene.slug().isEmpty()) {
        if (error)
            *error = "a scene needs a name";
        return false;
    }
    if (!QDir().mkpath(m_directory)) {
        if (error)
            *error = QString("can't create %1").arg(m_directory);
        return false;
    }
    QSaveFile file(pathFor(scene.slug()));
    // Without this QSaveFile falls back to writing in place when it can't make
    // its temporary file, which could leave a half-written scene behind.
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QString("can't write %1: %2").arg(file.fileName(), file.errorString());
        return false;
    }
    file.write(QJsonDocument(scene.toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = QString("can't write %1: %2").arg(file.fileName(), file.errorString());
        return false;
    }
    return true;
}

bool SceneStore::remove(const QString &slug, QString *error) const
{
    if (!QFile::remove(pathFor(slug))) {
        if (error)
            *error = QString("can't delete %1").arg(pathFor(slug));
        return false;
    }
    return true;
}

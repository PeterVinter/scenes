#include "catalog.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

QStringList Catalog::defaultDirectories()
{
    QStringList directories{QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/applications"};
    for (const auto &dir : QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation))
        if (!directories.contains(dir))
            directories.append(dir);
    return directories;
}

// Splits an Exec line the way the desktop entry spec quotes it.
QStringList Catalog::splitExec(const QString &exec)
{
    QStringList words;
    QString word;
    bool quoted = false, started = false;
    for (int i = 0; i < exec.size(); ++i) {
        const QChar c = exec[i];
        if (c == '\\' && i + 1 < exec.size()) {
            word += exec[++i];
            started = true;
        } else if (c == '"') {
            quoted = !quoted;
            started = true;
        } else if (c.isSpace() && !quoted) {
            if (started)
                words.append(word);
            word.clear();
            started = false;
        } else {
            word += c;
            started = true;
        }
    }
    if (started)
        words.append(word);
    return words;
}

Catalog::Catalog(const QStringList &directories)
{
    const QStringList dirs = directories.isEmpty() ? defaultDirectories() : directories;
    // Earlier directories win, as they do for the launcher: a user's own entry
    // overrides the packaged one.
    struct Ranked { int priority; QString id; };
    QHash<QString, Ranked> classes;
    auto offer = [&](const QString &key, int priority, const QString &id) {
        const QString lower = key.toLower();
        if (lower.isEmpty())
            return;
        const auto found = classes.constFind(lower);
        if (found == classes.constEnd() || priority < found->priority || (priority == found->priority && id < found->id))
            classes.insert(lower, {priority, id});
    };

    for (const auto &directory : dirs) {
        QDirIterator it(directory, {"*.desktop"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            QString id = QDir(directory).relativeFilePath(path);
            id.chop(8); // ".desktop"
            id.replace('/', '-');
            if (m_entries.contains(id))
                continue;

            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                continue;
            AppEntry entry;
            entry.id = id;
            bool inEntry = false, hidden = false, haveName = false;
            QTextStream stream(&file);
            while (!stream.atEnd()) {
                const QString line = stream.readLine().trimmed();
                if (line.startsWith('[')) {
                    inEntry = line == "[Desktop Entry]";
                    continue;
                }
                const int eq = line.indexOf('=');
                if (!inEntry || eq < 1)
                    continue;
                const QString key = line.left(eq), value = line.mid(eq + 1);
                if (key == "Name" && !haveName) { entry.name = value; haveName = true; }
                else if (key == "Icon") entry.icon = value;
                else if (key == "Exec") entry.exec = value;
                else if (key == "StartupWMClass") entry.wmClass = value;
                else if (key == "Terminal") entry.terminal = value.toLower() == "true";
                else if (key == "Hidden") hidden = value.toLower() == "true";
            }
            if (hidden)
                continue;
            if (entry.name.isEmpty())
                entry.name = id;
            m_entries.insert(id, entry);

            offer(entry.wmClass, 1, id);
            offer(id, 2, id);
            offer(id.section('.', -1), 3, id);
            static const QRegularExpression appId(R"(--(?:app-id|class)[= ]([^\s"']+))");
            const auto match = appId.match(entry.exec);
            if (match.hasMatch())
                offer(match.captured(1), 2, id);
        }
    }
    for (auto it = classes.cbegin(); it != classes.cend(); ++it)
        m_classes.insert(it.key(), it->id);
}

QString Catalog::idForClass(const QString &windowClass) const
{
    return m_classes.value(windowClass.toLower());
}

QStringList Catalog::terminalCommand(const QString &id) const
{
    const AppEntry app = entry(id);
    if (!app.terminal)
        return {};
    static const QRegularExpression fieldCode("^%[a-zA-Z]$");
    QStringList words;
    for (const auto &word : splitExec(app.exec))
        if (!fieldCode.match(word).hasMatch())
            words.append(word);
    return words;
}

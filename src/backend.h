#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <memory>

#include "catalog.h"
#include "hypr.h"
#include "hyprevents.h"
#include "plan.h"
#include "restorer.h"
#include "scene.h"
#include "scenestore.h"

// The app's state and work, as QML sees it: the saved scenes, the live
// "current desktop", the selected scene drawn as a monitor map, and restore.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList scenes READ scenes NOTIFY scenesChanged)
    // The slug of the selected scene; empty means the current desktop.
    Q_PROPERTY(QString selected READ selected NOTIFY selectedChanged)
    Q_PROPERTY(QVariantMap view READ view NOTIFY viewChanged)
    Q_PROPERTY(QStringList sceneProblems READ sceneProblems NOTIFY scenesChanged)
    Q_PROPERTY(QString currentError READ currentError NOTIFY viewChanged)
    Q_PROPERTY(bool restoring READ restoring NOTIFY restoringChanged)
    Q_PROPERTY(int progressDone READ progressDone NOTIFY progressChanged)
    Q_PROPERTY(int progressTotal READ progressTotal NOTIFY progressChanged)
    Q_PROPERTY(QString progressApp READ progressApp NOTIFY progressChanged)
    // How the last restore went; empty until one has run.
    Q_PROPERTY(QVariantMap result READ result NOTIFY resultChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(bool eventsConnected READ eventsConnected NOTIFY eventsConnectedChanged)

public:
    Backend(const SceneStore &store, const Hypr &hypr, const Catalog &catalog, HyprEvents *events = nullptr,
            const QString &hyprctl = "hyprctl", QObject *parent = nullptr);
    ~Backend() override;

    QVariantList scenes() const { return m_sceneList; }
    QString selected() const { return m_selected; }
    QVariantMap view() const { return m_view; }
    QStringList sceneProblems() const { return m_sceneProblems; }
    QString currentError() const { return m_currentError; }
    bool restoring() const { return m_restoring; }
    int progressDone() const { return m_progressDone; }
    int progressTotal() const { return m_progressTotal; }
    QString progressApp() const { return m_progressApp; }
    QVariantMap result() const { return m_result; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    bool eventsConnected() const { return m_events && m_events->connected(); }

    Q_INVOKABLE void select(const QString &slug);
    Q_INVOKABLE void selectOffset(int delta);
    Q_INVOKABLE void refreshNow();
    // Each returns an empty string on success, or a message to show.
    Q_INVOKABLE QString saveCurrent(const QString &name);
    Q_INVOKABLE QString rename(const QString &name);
    Q_INVOKABLE QString remove();
    Q_INVOKABLE QString undo();
    Q_INVOKABLE QString redo();
    Q_INVOKABLE bool nameTaken(const QString &name, bool ignoreSelected = false) const;
    Q_INVOKABLE QString selectedName() const;
    Q_INVOKABLE void restore();
    Q_INVOKABLE void dismissResult();
    // Tests point restores at a fake socket; empty means Hyprland's own.
    void setEventSocket(const QString &path) { m_eventSocket = path; }
    void setWindowTimeout(int milliseconds) { m_windowTimeout = milliseconds; }

signals:
    void scenesChanged();
    void selectedChanged();
    void viewChanged();
    void restoringChanged();
    void progressChanged();
    void resultChanged();
    void historyChanged();
    void eventsConnectedChanged();

private:
    struct CaptureResult {
        Scene scene;
        CaptureReport report;
        QString error;
    };
    struct History {
        std::function<bool(QString *)> undo, redo;
        QString selectAfterUndo, selectAfterRedo;
    };

    void reloadScenes();
    void refreshCurrent();
    void rebuildView();
    QVariantMap viewFor(const Scene &scene, bool current) const;
    void finishRestore(const Plan &plan, const QString &slug);
    void push(const History &entry);
    const Scene *findScene(const QString &slug) const;

    SceneStore m_store;
    Hypr m_hypr;
    const Catalog &m_catalog;
    HyprEvents *m_events;
    QString m_hyprctl;
    QString m_eventSocket;
    int m_windowTimeout = 10000;

    QList<Scene> m_savedScenes;
    QVariantList m_sceneList;
    QStringList m_sceneProblems;
    QString m_selected;
    QVariantMap m_view;

    Scene m_current;
    CaptureReport m_currentReport;
    QString m_currentError;
    bool m_haveCurrent = false;
    QFutureWatcher<CaptureResult> m_captureWatcher;
    bool m_captureQueued = false;

    bool m_restoring = false;
    int m_progressDone = 0, m_progressTotal = 0;
    QString m_progressApp;
    QVariantMap m_result;
    QFutureWatcher<Plan> m_planWatcher;
    std::unique_ptr<Restorer> m_restorer;

    QList<History> m_undo, m_redo;
};

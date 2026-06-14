#pragma once

// 
// Shijima-Qt - Cross-platform shimeji simulation app for desktop
// Copyright (C) 2025 pixelomer
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
// 

#include <QMainWindow>
#include <QString>
#include <shijima/mascot/manager.hpp>
#include <shijima/mascot/factory.hpp>
#include <vector>
#include <QMap>
#include <QListWidgetItem>
#include <QListWidget>
#include <QSettings>
#include <QScreen>
#include "PlatformWidget.hpp"
#include "MascotData.hpp"
#include <set>
#include <list>
#include <mutex>
#include "Platform/ActiveWindowObserver.hpp"
#include "ShijimaWidget.hpp"
#include "ShijimaHttpApi.hpp"
#include <condition_variable>

// Forward declarations for AI chat widgets
class QLineEdit;
class QPushButton;

class ShijimaManager : public PlatformWidget<QMainWindow>
{
    Q_OBJECT

public:
    static ShijimaManager *defaultManager();
    static void finalize();
    void updateEnvironment();
    void updateEnvironment(QScreen *);
    QString const& mascotsPath();
    ShijimaWidget *spawn(std::string const& name);
    void killAll();
    void killAll(QString const& name);
    void killAllButOne(ShijimaWidget *widget);
    void killAllButOne(QString const& name);
    void setManagerVisible(bool visible);
    void importOnShow(QString const& path);
    void makeMascotSpeak(const QString& text);
    QMap<QString, MascotData *> const& loadedMascots();
    QMap<int, MascotData *> const& loadedMascotsById();
    std::list<ShijimaWidget *> const& mascots();
    std::map<int, ShijimaWidget *> const& mascotsById();
    ShijimaWidget *hitTest(QPoint const& screenPos);
    std::string chatWithAI(const std::string& userMessage,
                           int depth = 0,
                           bool toolResultMode = false,
                           const std::string& toolOutput = {},
                           const std::string& originalQuestion = {},
                           bool isWindowComment = false);
    void onTickSync(std::function<void(ShijimaManager *)> callback);
    void processUserCommand(const QString& msg);
    ~ShijimaManager();

    // New methods for idle AI actions
    void tickIdleLogic();
    void triggerIdleAction();
    void applyAIAction(const std::string& actionName);

private slots:
    void sendChatMessage();

protected:
    void timerEvent(QTimerEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    explicit ShijimaManager(QWidget *parent = nullptr);
    bool tryHandlePostureCommand(const QString& msg);
    static std::string imgRootForTemplatePath(std::string const& path);
    std::unique_lock<std::mutex> acquireLock();
    void loadDefaultMascot();
    void loadData(MascotData *data);
    void spawnClicked();
    void reloadMascot(QString const& name);
    void askClose();
    void itemDoubleClicked(QListWidgetItem *qItem);
    void reloadMascots(std::set<std::string> const& mascots);
    void loadAllMascots();
    void refreshListWidget();
    void buildToolbar();
    void importAction();
    void deleteAction();
    void updateSandboxBackground();
    bool windowedMode();
    QWidget *mascotParent();
    void setWindowedMode(bool windowedMode);
    void screenAdded(QScreen *);
    void screenRemoved(QScreen *);
    void quitAction();
    std::set<std::string> import(QString const& path) noexcept;
    void importWithDialog(QList<QString> const& paths);
    void tick();
    QScreen *mascotScreen();

    // --- Member variables ---
    QColor m_sandboxBackground;
    QAction *m_windowedModeAction;
    QWidget *m_sandboxWidget;
    QSettings m_settings;
    Platform::ActiveWindow m_previousWindow;
    Platform::ActiveWindow m_currentWindow;
    Platform::ActiveWindowObserver m_windowObserver;
    int m_mascotTimer = -1;
    bool m_allowClose = false;
    bool m_firstShow = true;
    bool m_wasVisible = false;
    int m_idCounter;
    double m_userScale = 1.0;
    int m_windowObserverTimer = -1;
    QMap<QString, MascotData *> m_loadedMascots;
    QMap<int, MascotData *> m_loadedMascotsById;
    QSet<QString> m_listItemsToRefresh;
    QMap<QScreen *, std::shared_ptr<shijima::mascot::environment>> m_env;
    QMap<shijima::mascot::environment *, QScreen *> m_reverseEnv;
    shijima::mascot::factory m_factory;
    QString m_importOnShowPath;
    std::list<ShijimaWidget *> m_mascots;
    std::map<int, ShijimaWidget *> m_mascotsById;
    QString m_mascotsPath;
    QListWidget m_listWidget;
    ShijimaHttpApi m_httpApi;
    bool m_hasTickCallbacks;
    std::mutex m_mutex;
    std::condition_variable m_tickCallbackCompletion;
    std::list<std::function<void(ShijimaManager *)>> m_tickCallbacks;

    // AI Chat UI elements
    QString m_lastAIExpr;    
    QLineEdit *m_chatInput;
    QPushButton *m_sendButton;
    QString m_lastWindowUid;
    bool m_aiCommentActive = false;
    bool m_aiRequestActive = false;

    // Idle AI logic
    int m_idleTicksRemaining = 30;        // countdown until next idle action (approx seconds)
    bool m_idleBusy = false;              // whether idle action is currently in progress
    // AI posture lock
    bool m_postureExprLocked = false;
    void moveMascotTo(int x, int y);
};

#pragma once

// 
// Shijima-Qt - Cross-platform shimeji simulation app for desktop
// Copyright (C) 2025 pixelomer
// Modified by Azkiah Darojah (17 y/o, Indonesia) — AI integration, expression system & user memory
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
#include <QDateTime>
#include <QMutex>
#include <QProcess>
#if SHIJIMA_USE_QTMULTIMEDIA
#include <QAudioSource>
#include <QAudioFormat>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QTimer>
#endif

// Forward declarations
class QLineEdit;
class QPushButton;

class ShijimaManager : public PlatformWidget<QMainWindow>
{
    Q_OBJECT

public:
    // Tool execution result structure
    struct ToolResult {
        bool success = false;
        QString output;
        QString error;
    };

    // Static manager methods
    static ShijimaManager *defaultManager();
    static void finalize();
    static QMutex g_filesMutex;
    static QMutex g_memoryMutex;
    static QMutex g_historyMutex;

    // Environment and mascot management
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
    
    // Mascot data accessors
    QMap<QString, MascotData *> const& loadedMascots();
    QMap<int, MascotData *> const& loadedMascotsById();
    std::list<ShijimaWidget *> const& mascots();
    std::map<int, ShijimaWidget *> const& mascotsById();
    ShijimaWidget *hitTest(QPoint const& screenPos);

    // AI Chat and tool execution
    std::string chatWithAI(const std::string& userMessage,
                           int depth = 0,
                           bool toolResultMode = false,
                           const std::string& toolOutput = {},
                           const std::string& originalQuestion = {},
                           bool isWindowComment = false);
    void processUserCommand(const QString& msg);
    
    // Tool execution methods
    ToolResult executeBrowserTool(const QString&, const QString&);
    ToolResult executeCmdTool(const QString&);
    ToolResult executeWriteFileTool(const QString&, const QString&);
    ToolResult executeEditFileTool(const QString&, const QString&, const QString&);
    ToolResult executeRunPythonTool(const QString&);
    ToolResult executeRunShTool(const QString&);
    
    // AI decision and behavior
    void applyDecision(const QString& jsonDecision);
    void applyAIAction(const std::string& actionName);
    void applyAIBehavior(const std::string& behaviorName);
    
    // Memory management
    void loadMemoryFromFile();
    void saveMemoryToFile();
    void updateMemorySummary();
    QStringList retrieveRelevantMemory(const QString& query, int maxResults);
    QString buildHybridPrompt(const QString& userMessage, bool isWindowComment);
    
    // Command validation
    bool isCommandWhitelisted(const QString& command);
    QString parseAndExecuteTools(const QString& aiResponse);
    
    // Idle AI logic
    void tickIdleLogic();
    void triggerIdleAction();
    
    // Voice
    void speakText(const QString& text);
    void toggleRecording();
    bool isTtsEnabled() const { return m_ttsEnabled; }
    void setTtsEnabled(bool enabled);
    bool isRecording() const { return m_isRecording; }
    
    // Tick callback
    void onTickSync(std::function<void(ShijimaManager *)> callback);
    
    ~ShijimaManager();

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
    
    // Posture and movement handling
    bool tryHandlePostureCommand(const QString& msg);
    void moveMascotTo(int x, int y);
    
    // Template and data loading
    static std::string imgRootForTemplatePath(std::string const& path);
    std::unique_lock<std::mutex> acquireLock();
    void loadDefaultMascot();
    void loadData(MascotData *data);
    
    // Mascot management
    void spawnClicked();
    void reloadMascot(QString const& name);
    void reloadMascots(std::set<std::string> const& mascots);
    void loadAllMascots();
    void refreshListWidget();
    
    // UI and toolbar
    void buildToolbar();
    void updateSandboxBackground();
    bool windowedMode();
    QWidget *mascotParent();
    void setWindowedMode(bool windowedMode);
    
    // Screen management
    void screenAdded(QScreen *);
    void screenRemoved(QScreen *);
    QScreen *mascotScreen();
    
    // Import and delete actions
    void importAction();
    void deleteAction();
    void quitAction();
    std::set<std::string> import(QString const& path) noexcept;
    void importWithDialog(QList<QString> const& paths);
    
    // Close and ask dialogs
    void askClose();
    void itemDoubleClicked(QListWidgetItem *qItem);
    
    // Main tick loop
    void tick();

    // --- Member variables ---
    
    // Voice recording
    QPushButton *m_micButton = nullptr;
    bool m_isRecording = false;
    bool m_isProcessingVoice = false;
    bool m_ttsEnabled = true;
    QByteArray m_audioBuffer;
    QAudioFormat m_audioFormat;
    QAudioSource *m_audioSource = nullptr;
    QIODevice *m_audioIODevice = nullptr;
    QTimer *m_rmsTimer = nullptr;
    
    // Sandbox and window management
    QColor m_sandboxBackground;
    QAction *m_windowedModeAction;
    QWidget *m_sandboxWidget;
    QSettings m_settings;
    
    // Window observation
    Platform::ActiveWindow m_previousWindow;
    Platform::ActiveWindow m_currentWindow;
    Platform::ActiveWindowObserver m_windowObserver;
    
    // Timers and state
    int m_mascotTimer = -1;
    int m_windowObserverTimer = -1;
    bool m_allowClose = false;
    bool m_firstShow = true;
    bool m_wasVisible = false;
    
    // Mascot data and scaling
    int m_idCounter;
    double m_userScale = 1.0;
    QMap<QString, MascotData *> m_loadedMascots;
    QMap<int, MascotData *> m_loadedMascotsById;
    QSet<QString> m_listItemsToRefresh;
    
    // Environment and factory
    QMap<QScreen *, std::shared_ptr<shijima::mascot::environment>> m_env;
    QMap<shijima::mascot::environment *, QScreen *> m_reverseEnv;
    shijima::mascot::factory m_factory;
    
    // Import and paths
    QString m_importOnShowPath;
    QString m_mascotsPath;
    
    // Active mascots
    std::list<ShijimaWidget *> m_mascots;
    std::map<int, ShijimaWidget *> m_mascotsById;
    
    // UI widgets
    QListWidget m_listWidget;
    ShijimaHttpApi m_httpApi;
    
    // AI Chat UI elements
    QLineEdit *m_chatInput;
    QPushButton *m_sendButton;
    QString m_lastAIExpr;
    
    // Window comment tracking
    QString m_lastWindowUid;
    bool m_aiCommentActive = false;
    bool m_aiRequestActive = false;
    
    // Idle AI logic
    int m_idleTicksRemaining = 30;
    bool m_idleBusy = false;
    
    // AI posture and expression lock
    bool m_postureExprLocked = false;
    
    // Memory and decision tracking
    QString m_memorySummary;
    int m_messageCountSinceSummary = 0;
    QDateTime m_lastDecisionTime;
    qint64 m_decisionCooldownMs = 3000;
    
    // Thread synchronization
    bool m_hasTickCallbacks;
    std::mutex m_mutex;
    std::condition_variable m_tickCallbackCompletion;
    std::list<std::function<void(ShijimaManager *)>> m_tickCallbacks;
};

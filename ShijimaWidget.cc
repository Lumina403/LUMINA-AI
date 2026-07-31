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

#include "ShijimaWidget.hpp"
#include <QWidget>
#include <QPainter>
#include <QFile>
#include <QDir>
#include <QScreen>
#include <QMouseEvent>
#include <QMenu>
#include <QWindow>
#include <QDebug>
#include <QGuiApplication>
#include <QTextStream>
#include <QLabel>
#include <QTimer>
#include <QMetaObject>
#include <QPropertyAnimation>
#include <QPointer>
#include <shijima/shijima.hpp>
#include "Platform/Platform.hpp"
#include "ShimejiInspectorDialog.hpp"
#include "AssetLoader.hpp"
#include "ShijimaContextMenu.hpp"
#include "ShijimaManager.hpp"
#include <shimejifinder/utils.hpp>

using namespace shijima;

ShijimaWidget::ShijimaWidget(MascotData *mascotData,
    std::unique_ptr<shijima::mascot::manager> mascot,
    int mascotId, bool windowedMode, QWidget *parent):
#if defined(__APPLE__)
    PlatformWidget(nullptr, PlatformWidget::ShowOnAllDesktops),
#else
    PlatformWidget(parent, PlatformWidget::ShowOnAllDesktops),
#endif
    m_windowedMode(windowedMode), m_data(mascotData),
    m_inspector(nullptr), m_mascotId(mascotId),
    m_aiFullControl(true), m_aiBehaviorPending(false),
    m_speechBubble(nullptr), m_speechTimer(nullptr)
{
    m_windowHeight = 128;
    m_windowWidth = 128;
    m_mascot = std::move(mascot);
    
    QDir dir { m_data->imgRoot() };
    if (dir.exists() && dir.cdUp() && dir.cd("sound")) {
        m_sounds.searchPaths.push_back(dir.path());
    }
    
    if (!m_windowedMode) {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_MacShowFocusRect, false);
        Qt::WindowFlags flags = Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint
            | Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint
            | Qt::WindowOverridesSystemGestures;
        #if defined(__APPLE__)
        flags |= Qt::Window;
        #else
        flags |= Qt::Tool;
        #endif
        setWindowFlags(flags);
    }
    setFixedSize(m_windowWidth, m_windowHeight);
}


ShijimaWidget::ShijimaWidget(ShijimaWidget &old, bool windowedMode,
    QWidget *parent) : ShijimaWidget(old.mascotData(),
    std::move(old.m_mascot), old.m_mascotId,
    windowedMode, parent)
{
    m_aiFullControl = old.m_aiFullControl;
    m_aiForcedBehavior = old.m_aiForcedBehavior;
    m_aiBehaviorPending = old.m_aiBehaviorPending;
}

void ShijimaWidget::showInspector() {
    if (m_inspector == nullptr) {
        m_inspector = new ShimejiInspectorDialog { this };
    }
    m_inspector->show();
}

bool ShijimaWidget::inspectorVisible() {
    return m_inspector != nullptr && m_inspector->isVisible();
}

Asset const& ShijimaWidget::getActiveAsset() {
    auto &name = m_mascot->state->active_frame.get_name(m_mascot->state->looking_right);
    auto lowerName = shimejifinder::to_lower(name);
    // FIX: tambahkan ekstensi .png jika belum ada
    QString lowerQ = QString::fromStdString(lowerName);
    if (!lowerQ.endsWith(".png") && !lowerQ.endsWith(".PNG")) {
        lowerName += ".png";
    }
    auto imagePath = QDir::cleanPath(m_data->imgRoot()
        + QDir::separator() + QString::fromStdString(lowerName));
    return AssetLoader::defaultLoader()->loadAsset(imagePath);
}

bool ShijimaWidget::isMirroredRender() const {
    return m_mascot->state->active_frame.right_name.empty() &&
        m_mascot->state->looking_right;
}

void ShijimaWidget::paintEvent(QPaintEvent *event) {
    if (!m_visible) {
        return;
    }
    auto &asset = getActiveAsset();
    auto &image = asset.image(isMirroredRender());
    auto scaledSize = image.size() / m_drawScale;
    QPainter painter(this);
    painter.drawImage(QRect { m_drawOrigin, scaledSize }, image);
#ifdef __linux__
    if (Platform::useWindowMasks()) {
        auto maskPixmap = asset.mask(isMirroredRender());
        if (!maskPixmap.isNull()) {
            m_windowMask = QBitmap::fromPixmap(maskPixmap.scaled(scaledSize));
            m_windowMask.translate(m_drawOrigin);
            auto bounding = m_windowMask.boundingRect();
            bounding.setTop(0);
            bounding.setLeft(0);
            if (bounding.width() > 0 && bounding.height() > 0) {
                setMask(m_windowMask);
            }
            else {
                setMask(QRect { m_windowWidth - 2, m_windowHeight - 2, 1, 1 });
            }
        }
        else {
            setMask(QRect { m_windowWidth - 2, m_windowHeight - 2, 1, 1 });
        }
    }
#endif
}

void ShijimaWidget::showEvent(QShowEvent *event) {
    PlatformWidget::showEvent(event);
}

bool ShijimaWidget::updateOffsets() {
    bool needsRepaint = false;
    auto &frame = m_mascot->state->active_frame;
    auto &asset = getActiveAsset();
    
    int originalWidth = asset.originalSize().width();
    int originalHeight = asset.originalSize().height();
    double scale = m_mascot->state->env->get_scale();
    int screenWidth = (int)(m_mascot->state->env->screen.width() / scale);
    int screenHeight = (int)(m_mascot->state->env->screen.height() / scale);
    int windowWidth = (int)(originalWidth / scale);
    int windowHeight = (int)(originalHeight / scale);

    if (windowWidth != m_windowWidth) {
        m_windowWidth = windowWidth;
        setFixedWidth(m_windowWidth);
        needsRepaint = true;
    }
    if (windowHeight != m_windowHeight) {
        m_windowHeight = windowHeight;
        setFixedHeight(m_windowHeight);
        needsRepaint = true;
    }

    if (isMirroredRender()) {
        m_anchorInWindow = {
            (int)((originalWidth - frame.anchor.x) / scale),
            (int)(frame.anchor.y / scale) };
    }
    else {
        m_anchorInWindow = { (int)(frame.anchor.x / scale),
            (int)(frame.anchor.y / scale) };
    }

    QPoint drawOffset;
    m_visible = true;
    int winX = (int)m_mascot->state->anchor.x - m_anchorInWindow.x()
        - (int)env()->screen.left;
    int winY = (int)m_mascot->state->anchor.y - m_anchorInWindow.y()
        - (int)env()->screen.top;
    if (winX < 0) {
        drawOffset.setX(winX);
        winX = 0;
    }
    else if (winX + windowWidth > screenWidth) {
        drawOffset.setX(winX - screenWidth + windowWidth);
        winX = screenWidth - windowWidth;
    }
    if (winY < 0) {
        drawOffset.setY(winY);
        winY = 0;
    }
    else if (winY + windowHeight > screenHeight) {
        drawOffset.setY(winY - screenHeight + windowHeight);
        winY = screenHeight - windowHeight;
    }
    winX += (int)env()->screen.left;
    winY += (int)env()->screen.top;

    if (isMirroredRender()) {
        drawOffset += QPoint {
            (int)((originalWidth - asset.offset().topRight().x()) / scale),
            (int)(asset.offset().topLeft().y() / scale) };
    }
    else {
        drawOffset += asset.offset().topLeft() / scale;
    }
    if (drawOffset != m_drawOrigin) {
        needsRepaint = true;
        m_drawOrigin = drawOffset;
    }
    if (scale != m_drawScale) {
        needsRepaint = true;
        m_drawScale = scale;
    }
    move(winX, winY);

    updateBubblePosition();

    return needsRepaint;
}

bool ShijimaWidget::pointInside(QPoint const& point) {
    if (!m_visible) {
        return false;
    }
    auto &asset = getActiveAsset();
    auto image = asset.image(isMirroredRender());
    int drawnWidth = (int)(image.width() / m_drawScale);
    int drawnHeight = (int)(image.height() / m_drawScale);
    auto imagePos = point - m_drawOrigin;
    if (imagePos.x() < 0 || imagePos.y() < 0 ||
        imagePos.x() > drawnWidth || imagePos.y() > drawnHeight)
    {
        return false;
    }
    auto color = image.pixelColor(imagePos * m_drawScale);
    if (color.alpha() == 0) {
        return false;
    }
    return true;
}

// ==================== AI FULL CONTROL TICK ====================
void ShijimaWidget::tick() {
    if (m_markedForDeletion) {
        close();
        return;
    }
    if (paused()) {
        return;
    }

    // === EXECUTE TICK ===

    auto prev_frame = m_mascot->state->active_frame;
    try {
        m_mascot->tick();
    } catch (const std::exception& e) {
        std::cerr << "[ShijimaWidget] Exception in tick(): " << e.what() << std::endl;
        // Clear forced behavior state and recover via Fall
        m_aiFullControl = false;
        m_aiForcedBehavior.clear();
        m_aiBehaviorPending = false;
        if (m_mascot->has_behavior("Fall")) {
            try {
                m_mascot->state->next_subtick = 0;
                m_mascot->next_behavior("Fall");
            } catch (...) {}
        }
    }

    // === NORMAL UPDATE / REPAINT / SOUND ===
    auto &new_frame = m_mascot->state->active_frame;
    auto &new_sound = m_mascot->state->active_sound;
    bool forceRepaint = prev_frame.name != new_frame.name;
    bool offsetsChanged = updateOffsets();
    if (m_mascot->state->dead) {
        forceRepaint = true;
        new_frame.name = "";
        new_sound = "";
        m_mascot->state->active_sound_changed = true;
        markForDeletion();
    }
    if (offsetsChanged || forceRepaint) {
        repaint();
        update();
    }
    if (m_mascot->state->active_sound_changed) {
        m_sounds.stop();
        if (!new_sound.empty()) {
            m_sounds.play(QString::fromStdString(new_sound));
        }
    }
    else if (!m_sounds.playing()) {
        m_mascot->state->active_sound.clear();
    }

    if (m_inspector != nullptr && m_inspector->isVisible()) {
        m_inspector->tick();
    }
}

void ShijimaWidget::contextMenuClosed(QCloseEvent *event) {
    m_contextMenuVisible = false;
}

void ShijimaWidget::showContextMenu(QPoint const& pos) {
    m_contextMenuVisible = true;
    ShijimaContextMenu *menu = new ShijimaContextMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(pos);
}

ShijimaWidget::~ShijimaWidget() {
    if (m_dragTargetPt != nullptr) {
        *m_dragTargetPt = nullptr;
        m_dragTargetPt = nullptr;
    }
    if (m_inspector != nullptr) {
        m_inspector->close();
        delete m_inspector;
    }
    setDragTarget(nullptr);
    // Stop timer synchronously so its lambda never fires after our destruction
    if (m_speechTimer) {
        m_speechTimer->disconnect();
        m_speechTimer->stop();
    }
    // m_speechBubble and m_speechTimer are QPointer → auto-cleared when object is deleted
}

void ShijimaWidget::setDragTarget(ShijimaWidget *target) {
    if (m_dragTarget != nullptr) {
        m_dragTarget->m_dragTargetPt = nullptr;
    }
    if (target != nullptr) {
        if (target->m_dragTargetPt != nullptr) {
            throw std::runtime_error("target widget being dragged by multiple widgets");
        }
        m_dragTarget = target;
        m_dragTarget->m_dragTargetPt = &m_dragTarget;
    }
    else {
        m_dragTarget = nullptr;
    }
}

void ShijimaWidget::mousePressEvent(QMouseEvent *event) {
    auto pos = event->pos();
    if (m_dragTarget != nullptr) {
        m_dragTarget->m_mascot->state->dragging = false;
    }
    if (pointInside(pos)) {
        setDragTarget(this);
    }
    else {
        QPoint envPos;
        if (m_windowedMode) {
            envPos = mapToParent(pos);
        }
        else {
            envPos = mapToGlobal(pos);
        }
        ShijimaWidget *target = ShijimaManager::defaultManager()->hitTest(envPos);
        setDragTarget(target);
        if (target == nullptr) {
            event->ignore();
            return;
        }
    }
    if (event->button() == Qt::MouseButton::LeftButton) {
        m_dragTarget->m_mascot->state->dragging = true;
    }
    else if (event->button() == Qt::MouseButton::RightButton) {
        auto screenPos = mapToGlobal(pos);
        m_dragTarget->showContextMenu(screenPos);
        setDragTarget(nullptr);
    }
}

void ShijimaWidget::closeAction() {
    close();
}

void ShijimaWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (m_dragTarget == nullptr) {
        return;
    }
    if (event->button() == Qt::MouseButton::LeftButton) {
        m_dragTarget->m_mascot->state->dragging = false;
        setDragTarget(nullptr);
    }
}

// ==================== AI BEHAVIOR CONTROL ====================

void ShijimaWidget::forceBehavior(const QString& behavior) {
    QString trimmed = behavior.trimmed();
    if (trimmed.isEmpty()) return;

    // NOTE: This is always called from the main thread (Qt event loop).
    // No mutex needed — mascot tick() also runs on main thread.
    std::cout << "[ShijimaWidget] forceBehavior: "
              << trimmed.toStdString() << std::endl;

    if (m_aiForcedBehavior == trimmed && !m_aiBehaviorPending) {
        auto active = m_mascot->active_behavior();
        if (active != nullptr && active->name == trimmed.toStdString()) {
            return; // Already in the requested behavior, no-op
        }
    }

    m_aiForcedBehavior = trimmed;
    m_aiBehaviorPending = true;

    try {
        auto active = m_mascot->active_behavior();
        if (active == nullptr || active->name != trimmed.toStdString()) {
            m_mascot->state->next_subtick = 0;
            m_mascot->next_behavior(trimmed.toStdString());
        }
        m_aiBehaviorPending = false;
    } catch (const std::exception& e) {
        m_aiBehaviorPending = false;
        std::cerr << "[ShijimaWidget] Failed to force behavior: "
                  << e.what() << std::endl;
    }
}

void ShijimaWidget::setAIBehavior(const QString& behaviorName) {
    if (!m_aiFullControl) return;
    m_aiForcedBehavior = behaviorName;
    m_aiBehaviorPending = true;
}

void ShijimaWidget::enableAIFullControl(bool enable) {
    m_aiFullControl = enable;
    if (!enable) {
        m_aiBehaviorPending = false;
        m_aiForcedBehavior.clear();
        
        // Reset to a default behavior when disabling AI control
        static const std::vector<std::string> fallbacks = {
            "SitWhileDanglingLegs", "Fall"
        };
        for (auto& fb : fallbacks) {
            if (m_mascot->has_behavior(fb)) {
                try { m_mascot->next_behavior(fb); } catch (...) {}
                break;
            }
        }
    }
}

// ==================== SPEECH BUBBLE ====================

void ShijimaWidget::updateBubblePosition() {
    if (m_speechBubble) {
        int bubbleX = (width() - m_speechBubble->width()) / 2;
        int bubbleY = -m_speechBubble->height() - 8;
        QPoint pos = mapToGlobal(QPoint(bubbleX, bubbleY));
        m_speechBubble->move(pos);
    }
}

void ShijimaWidget::speak(const QString& text, bool isThinking) {
    std::cout << "[ShijimaWidget] speak: " << text.toStdString() << std::endl;
    
    if (m_speechTimer) {
        m_speechTimer->disconnect();
        m_speechTimer->stop();
        m_speechTimer->deleteLater();
        m_speechTimer = nullptr;
    }

    if (m_speechBubble) {
        m_speechBubble->setText(text);
        m_speechBubble->adjustSize();
        m_speechBubble->setWindowOpacity(1.0);
        updateBubblePosition();
        m_speechBubble->show();
        m_speechBubble->raise();
    } else {
        m_speechBubble = new QLabel(text);
        m_speechBubble->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
        m_speechBubble->setAttribute(Qt::WA_TranslucentBackground);
        m_speechBubble->setWordWrap(true);
        m_speechBubble->setMaximumWidth(220);
        m_speechBubble->setStyleSheet(
            "QLabel {"
            "  background-color: #ffffff;"
            "  color: #111111;"
            "  border: 2px solid #000000;"
            "  border-radius: 8px;"
            "  padding: 6px 10px;"
            "  font-size: 13px;"
            "  font-weight: bold;"
            "  font-family: sans-serif;"
            "}"
        );
        m_speechBubble->adjustSize();
        updateBubblePosition();
        m_speechBubble->setWindowOpacity(0.0);
        m_speechBubble->show();
        m_speechBubble->raise();

        QPropertyAnimation *fadeIn = new QPropertyAnimation(m_speechBubble, "windowOpacity");
        fadeIn->setDuration(150);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);
        fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
    }

    m_speechTimer = new QTimer(this);
    m_speechTimer->setSingleShot(true);

    // Use QPointer so the lambda is safe even if widget is destroyed before timer fires
    QPointer<ShijimaWidget> self = this;
    connect(m_speechTimer, &QTimer::timeout, [self]() {
        if (!self) return;  // Widget already destroyed — safe exit
        if (self->m_speechBubble) {
            QPointer<QLabel> bubble = self->m_speechBubble;
            QPropertyAnimation *fadeOut = new QPropertyAnimation(self->m_speechBubble, "windowOpacity");
            fadeOut->setDuration(200);
            fadeOut->setStartValue(1.0);
            fadeOut->setEndValue(0.0);
            connect(fadeOut, &QPropertyAnimation::finished, [bubble]() {
                if (bubble) bubble->deleteLater();
            });
            fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
            self->m_speechBubble = nullptr;
        }
        if (self->m_speechTimer) {
            self->m_speechTimer->deleteLater();
            self->m_speechTimer = nullptr;
        }
    });

    // isThinking: bubble bertahan 60 detik, di-override seketika saat AI menjawab
    int duration = isThinking ? 60000 : qMax(4000, (int)text.length() * 100);
    m_speechTimer->start(duration);
}

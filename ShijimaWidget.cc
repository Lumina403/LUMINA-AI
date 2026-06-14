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
#include <QPropertyAnimation>      // IMPORTANT: for fade effect
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
    windowedMode, parent) {}

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
        m_windowMask = QBitmap::fromPixmap(asset.mask(isMirroredRender())
            .scaled(scaledSize));
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
#endif
}

bool ShijimaWidget::updateOffsets() {
    bool needsRepaint = false;
    auto &frame = m_mascot->state->active_frame;
    auto &asset = getActiveAsset();
    
    // Does the image go outside of the minimum boundary? If so,
    // extend the window boundary
    int originalWidth = asset.originalSize().width();
    int originalHeight = asset.originalSize().height();
    double scale = m_mascot->state->env->get_scale();
    int screenWidth = (int)(m_mascot->state->env->screen.width()
        / scale);
    int screenHeight = (int)(m_mascot->state->env->screen.height()
        / scale);
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

    // Determine the frame anchor within the window
    if (isMirroredRender()) {
        m_anchorInWindow = {
            (int)((originalWidth - frame.anchor.x) / scale),
            (int)(frame.anchor.y / scale) };
    }
    else {
        m_anchorInWindow = { (int)(frame.anchor.x / scale),
            (int)(frame.anchor.y / scale) };
    }

    // Detemine draw offsets and window positions
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

    // Update bubble position after moving the character
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
    //FIXME: is this position correct?
    auto color = image.pixelColor(imagePos * m_drawScale);
    if (color.alpha() == 0) {
        return false;
    }
    return true;
}

void ShijimaWidget::tick() {
    if (m_markedForDeletion) {
        close();
        return;
    }
    if (paused()) {
        return;
    }

    // Tick
    auto prev_frame = m_mascot->state->active_frame;
    m_mascot->tick();
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

    // Update inspector
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
    if (m_speechBubble) {
        m_speechBubble->deleteLater();
        m_speechBubble = nullptr;
    }
    if (m_speechTimer) {
        m_speechTimer->deleteLater();
        m_speechTimer = nullptr;
    }
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

// ==================== AI SPEECH BUBBLE IMPLEMENTATION ====================
void ShijimaWidget::updateBubblePosition() {
    if (m_speechBubble && m_speechBubble->isVisible()) {
        QPoint pos = mapToGlobal(QPoint(20, -m_speechBubble->height() - 10));
        m_speechBubble->move(pos);
    }
}

void ShijimaWidget::speak(const QString& text) {
    std::cout << "[ShijimaWidget] speak: " << text.toStdString() << std::endl;
    
    // Hapus bubble lama jika ada
    if (m_speechBubble) {
        m_speechBubble->deleteLater();
        m_speechBubble = nullptr;
    }
    if (m_speechTimer) {
        m_speechTimer->stop();
        m_speechTimer->deleteLater();
        m_speechTimer = nullptr;
    }

    // Buat QLabel sebagai jendela terpisah (tooltip) agar tidak terpotong
    m_speechBubble = new QLabel(text);
    m_speechBubble->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    m_speechBubble->setAttribute(Qt::WA_TranslucentBackground);
    m_speechBubble->setWordWrap(true);
    m_speechBubble->setMaximumWidth(200);
    m_speechBubble->setStyleSheet(
        "QLabel {"
        "  background-color: white;"
        "  border: 2px solid black;"
        "  border-radius: 10px;"
        "  padding: 8px;"
        "  font-size: 12px;"
        "  font-family: sans-serif;"
        "}"
    );
    m_speechBubble->adjustSize();

    // Posisikan di atas karakter
    updateBubblePosition();
    m_speechBubble->show();

    // Efek fade in
    QPropertyAnimation *fadeIn = new QPropertyAnimation(m_speechBubble, "windowOpacity");
    fadeIn->setDuration(200);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    // Timer untuk menghilangkan gelembung setelah 4 detik
    m_speechTimer = new QTimer(this);
    m_speechTimer->setSingleShot(true);
    connect(m_speechTimer, &QTimer::timeout, [this]() {
        if (m_speechBubble) {
            QPropertyAnimation *fadeOut = new QPropertyAnimation(m_speechBubble, "windowOpacity");
            fadeOut->setDuration(200);
            fadeOut->setStartValue(1.0);
            fadeOut->setEndValue(0.0);
            connect(fadeOut, &QPropertyAnimation::finished, [this]() {
                if (m_speechBubble) {
                    m_speechBubble->deleteLater();
                    m_speechBubble = nullptr;
                }
            });
            fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
        }
        if (m_speechTimer) {
            m_speechTimer->deleteLater();
            m_speechTimer = nullptr;
        }
    });
    m_speechTimer->start(4000);
}
// ============================================================o

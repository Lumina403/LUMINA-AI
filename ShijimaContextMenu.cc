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

#include "ShijimaContextMenu.hpp"
#include "ShijimaWidget.hpp"
#include "ShijimaManager.hpp"
#include <QAction>
#include <QInputDialog>
#include <QtConcurrent>
#include <QMetaObject>
#include <QPointer>
#include <QGuiApplication>

ShijimaContextMenu::ShijimaContextMenu(ShijimaWidget *parent)
    : QMenu("Context menu", parent), m_parent(parent)
{
    QAction *action;

    // === Chat with AI ===
    QAction *chatAction = this->addAction("Chat with AI...");
    connect(chatAction, &QAction::triggered, [this]() {
        bool ok;
        QString prompt = QInputDialog::getText(this, "Chat with AI", "Pesan untuk AI:", QLineEdit::Normal, "", &ok);
        if (!ok || prompt.isEmpty()) return;
        this->close(); // tutup menu sementara

        // Gunakan method terpusat yang bisa menangani /search dan AI
        ShijimaManager::defaultManager()->processUserCommand(prompt);
    });

    // === Behaviors menu ===
    {
        std::vector<std::string> behaviors;
        auto &list = m_parent->m_mascot->initial_behavior_list();
        auto flat = list.flatten_unconditional();
        for (auto &behavior : flat) {
            if (!behavior->hidden) {
                behaviors.push_back(behavior->name);
            }
        }
        auto behaviorsMenu = addMenu("Behaviors");
        for (std::string &behavior : behaviors) {
            action = behaviorsMenu->addAction(QString::fromStdString(behavior));
            connect(action, &QAction::triggered, [this, behavior](){
                m_parent->m_mascot->next_behavior(behavior);
            });
        }
    }

    // Show manager
    action = addAction("Show manager");
    connect(action, &QAction::triggered, [](){
        ShijimaManager::defaultManager()->setManagerVisible(true);
    });

    // Inspect
    action = addAction("Inspect");
    connect(action, &QAction::triggered, [this](){
        m_parent->showInspector();
    });

    // Pause checkbox
    action = addAction("Pause");
    action->setCheckable(true);
    action->setChecked(m_parent->m_paused);
    connect(action, &QAction::triggered, [this](bool checked){
        m_parent->m_paused = checked;
    });

    // Call another
    action = addAction("Call another");
    connect(action, &QAction::triggered, [this](){
        ShijimaManager::defaultManager()->spawn(m_parent->mascotName().toStdString());
    });

    // Dismiss all but one
    action = addAction("Dismiss all but one");
    connect(action, &QAction::triggered, [this](){
        ShijimaManager::defaultManager()->killAllButOne(m_parent);
    });

    // Dismiss all
    action = addAction("Dismiss all");
    connect(action, &QAction::triggered, [](){
        ShijimaManager::defaultManager()->killAll();
    });

    // Dismiss
    action = addAction("Dismiss");
    connect(action, &QAction::triggered, m_parent, &ShijimaWidget::closeAction);
}

ShijimaContextMenu::~ShijimaContextMenu() {}

void ShijimaContextMenu::closeEvent(QCloseEvent *event) {
    m_parent->contextMenuClosed(event);
    QMenu::closeEvent(event);
}

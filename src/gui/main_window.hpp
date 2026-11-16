#pragma once

#include <QLabel>
#include <QMainWindow>
#include <QString>
#include <QTimer>
#include <vector>

#include "ai/evolution_log.hpp"
#include "gui/arena_widget.hpp"
#include "gui/udp_watcher.hpp"

namespace gui {

// Two tabs:
//   Live       the arena as the server sends it over UDP, plus loss and latency figures
//   Evolution  best and mean fitness per generation, from an arena_evolve log (Qt Charts)
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(const QString& host, quint16 port, const std::vector<ai::GenerationStats>& evolution);

private slots:
    void refreshStats();

private:
    QWidget* makeEvolutionTab(const std::vector<ai::GenerationStats>& evolution);

    UdpWatcher watcher_;
    ArenaWidget* arena_;  // owned by Qt's parent-child tree (deleted with the window)
    QLabel* stats_;       // likewise
    QTimer stats_timer_;
};

}  // namespace gui

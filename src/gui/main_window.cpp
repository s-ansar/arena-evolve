#include "gui/main_window.hpp"

#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QTabWidget>
#include <QValueAxis>
#include <QVBoxLayout>

#include <algorithm>

namespace gui {

MainWindow::MainWindow(const QString& host, quint16 port, const std::vector<ai::GenerationStats>& evolution)
    : watcher_(host, port) {
    setWindowTitle(QString("Arena Evolve - %1:%2").arg(host).arg(port));

    // Widgets created with `new` and given a parent are deleted by that parent. This is
    // Qt's ownership model, so these are not leaks and need no unique_ptr.
    auto* tabs = new QTabWidget(this);

    auto* live = new QWidget(tabs);
    auto* layout = new QVBoxLayout(live);
    arena_ = new ArenaWidget(live);
    stats_ = new QLabel("no states yet", live);
    layout->addWidget(arena_, 1);
    layout->addWidget(stats_);
    tabs->addTab(live, "Live");
    tabs->addTab(makeEvolutionTab(evolution), "Evolution");
    setCentralWidget(tabs);

    connect(&watcher_, &UdpWatcher::stateReceived, arena_, &ArenaWidget::showState);
    connect(&stats_timer_, &QTimer::timeout, this, &MainWindow::refreshStats);
    stats_timer_.start(500);
    resize(720, 640);
}

void MainWindow::refreshStats() {
    const net::StreamStats& s = watcher_.stats();
    if (s.received() == 0) {
        return;
    }
    const double mean_us = s.latency().mean().count() / 1000.0;
    stats_->setText(QString("UDP states received %1   lost %2   out of order %3   latency mean %4 us")
                        .arg(s.received())
                        .arg(s.lost())
                        .arg(s.out_of_order())
                        .arg(mean_us, 0, 'f', 0));
}

QWidget* MainWindow::makeEvolutionTab(const std::vector<ai::GenerationStats>& evolution) {
    if (evolution.empty()) {
        auto* hint = new QLabel("Run arena_evolve, then start arena_view with --fitness evolution.csv");
        hint->setAlignment(Qt::AlignCenter);
        return hint;
    }

    auto* best = new QLineSeries();
    best->setName("best");
    auto* mean = new QLineSeries();
    mean->setName("mean");
    double highest = 0.0;
    for (const ai::GenerationStats& row : evolution) {
        best->append(row.generation, row.best);
        mean->append(row.generation, row.mean);
        highest = std::max(highest, row.best);
    }

    auto* chart = new QChart();  // the QChartView below takes ownership
    chart->setTitle("Fitness against the chase bot, per generation");
    chart->addSeries(best);
    chart->addSeries(mean);

    auto* x = new QValueAxis();
    x->setTitleText("generation");
    x->setLabelFormat("%d");
    x->setRange(0, evolution.back().generation);
    auto* y = new QValueAxis();
    y->setTitleText("fitness (max 48 on 16 training matches)");
    y->setRange(0, std::max(48.0, highest));
    chart->addAxis(x, Qt::AlignBottom);
    chart->addAxis(y, Qt::AlignLeft);
    for (QLineSeries* series : {best, mean}) {
        series->attachAxis(x);
        series->attachAxis(y);
    }

    auto* view = new QChartView(chart);
    view->setRenderHint(QPainter::Antialiasing);
    return view;
}

}  // namespace gui

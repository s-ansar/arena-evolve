#pragma once

#include <QWidget>

#include "net/protocol.hpp"

namespace gui {

// Draws the ring, the robots and an arrow for each robot's velocity.
// The arena is in metres with y pointing up; the screen is in pixels with y pointing down,
// so painting flips y and scales the 1 m ring to fit the widget.
class ArenaWidget : public QWidget {
    Q_OBJECT

public:
    explicit ArenaWidget(QWidget* parent = nullptr);

public slots:
    // Keep the newest state and ask Qt to repaint. update() only schedules a repaint, so
    // states arriving faster than the screen refreshes cost almost nothing.
    void showState(const net::State& state);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    net::State state_;
    bool has_state_ = false;
};

}  // namespace gui

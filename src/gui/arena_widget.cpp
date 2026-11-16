#include "gui/arena_widget.hpp"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace gui {

namespace {

constexpr double kRingRadius = 1.0;   // metres; must match sim::Arena's default
constexpr double kRobotRadius = 0.1;  // metres; must match sim::Robot's default
const QColor kRobotColours[] = {QColor(0x2a, 0x78, 0xd6), QColor(0xe0, 0x7b, 0x1f)};

}  // namespace

ArenaWidget::ArenaWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(320, 320);
}

void ArenaWidget::showState(const net::State& state) {
    state_ = state;
    has_state_ = true;
    update();
}

void ArenaWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), palette().window());

    // Metres to pixels: the ring (plus a small margin) fills the shorter side of the widget.
    const double scale = 0.45 * std::min(width(), height()) / kRingRadius;
    const QPointF centre(width() / 2.0, height() / 2.0);
    const auto to_screen = [&](double x, double y) { return centre + QPointF(x * scale, -y * scale); };

    // The ring: a filled disc with a white edge line, like a sumo dohyo.
    painter.setPen(QPen(Qt::white, 3));
    painter.setBrush(QColor(0x3b, 0x3b, 0x3b));
    painter.drawEllipse(centre, kRingRadius * scale, kRingRadius * scale);

    if (!has_state_) {
        painter.setPen(palette().text().color());
        painter.drawText(rect(), Qt::AlignCenter, "Waiting for the server...");
        return;
    }

    for (std::size_t i = 0; i < state_.robots.size(); ++i) {
        const net::RobotState& robot = state_.robots[i];
        const QPointF at = to_screen(robot.x, robot.y);
        const QColor colour = robot.in_ring ? kRobotColours[i % 2] : QColor(Qt::gray);

        painter.setPen(Qt::NoPen);
        painter.setBrush(colour);
        painter.drawEllipse(at, kRobotRadius * scale, kRobotRadius * scale);

        // Velocity arrow: 1 m/s draws as 0.5 m on screen, enough to see without clutter.
        painter.setPen(QPen(Qt::white, 2));
        painter.drawLine(at, to_screen(robot.x + 0.5 * robot.vx, robot.y + 0.5 * robot.vy));

        painter.drawText(at + QPointF(-4, 5), QString::number(i));
        if (!robot.in_ring) {
            painter.setPen(Qt::red);
            painter.drawText(at + QPointF(-10, -kRobotRadius * scale - 4), "OUT");
        }
    }

    painter.setPen(palette().text().color());
    painter.drawText(QPointF(10, 20), QString("tick %1").arg(state_.tick));
}

}  // namespace gui

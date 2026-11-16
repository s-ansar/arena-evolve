// arena_view [host] [port] [--fitness evolution.csv] [--screenshot file.png]
// A live Qt view of a running server (it joins as a UDP watcher) plus a chart of an
// evolution run. --screenshot saves the window after 3 seconds and exits (used for the
// README and for checking the GUI without anyone looking at it).

#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QTabWidget>
#include <QTimer>

#include <fstream>
#include <iostream>

#include "ai/evolution_log.hpp"
#include "gui/main_window.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Live view of an Arena Evolve server");
    parser.addHelpOption();
    parser.addPositionalArgument("host", "server host (default 127.0.0.1)");
    parser.addPositionalArgument("port", "server port (default 5555)");
    const QCommandLineOption fitness("fitness", "chart this arena_evolve log", "csv");
    const QCommandLineOption screenshot("screenshot", "save the window after 3 s and quit", "png");
    const QCommandLineOption tab("tab", "start on this tab: 0 live, 1 evolution", "index", "0");
    parser.addOptions({fitness, screenshot, tab});
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    const QString host = args.size() > 0 ? args[0] : "127.0.0.1";
    const auto port = static_cast<quint16>(args.size() > 1 ? args[1].toUShort() : 5555);

    try {
        std::vector<ai::GenerationStats> evolution;
        if (parser.isSet(fitness)) {
            std::ifstream file(parser.value(fitness).toStdString());
            if (!file) {
                throw std::runtime_error("cannot read " + parser.value(fitness).toStdString());
            }
            evolution = ai::read_evolution_log(file);
        }

        gui::MainWindow window(host, port, evolution);
        if (auto* tabs = qobject_cast<QTabWidget*>(window.centralWidget())) {
            tabs->setCurrentIndex(parser.value(tab).toInt());
        }
        window.show();

        if (parser.isSet(screenshot)) {
            const QString path = parser.value(screenshot);
            QTimer::singleShot(3000, &app, [&window, &app, path] {
                window.grab().save(path);
                app.quit();
            });
        }
        return app.exec();
    } catch (const std::exception& error) {
        std::cerr << "arena_view: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
}

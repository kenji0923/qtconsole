#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

#include "main_window.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName("kshu");
  QCoreApplication::setApplicationName("qtconsole_UDP9000");

  QCommandLineParser parser;
  parser.setApplicationDescription("qtconsole");
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption protocol_option("protocol", "Receiver protocol: udp, ws, websocket.",
                                     "protocol");
  QCommandLineOption port_option({"p", "port"}, "Receiver port.", "port");
  QCommandLineOption measurement_option({"m", "measurement"}, "Measurement title.", "title");
  parser.addOption(protocol_option);
  parser.addOption(port_option);
  parser.addOption(measurement_option);

  parser.process(app);

  MainWindow::StartupOptions startup_options;
  if (parser.isSet(protocol_option)) {
    const QString protocol = parser.value(protocol_option).trimmed().toLower();
    if (protocol != "udp" && protocol != "ws" && protocol != "websocket") {
      QTextStream(stderr) << "Invalid --protocol value: " << protocol
                          << ". Use udp|ws|websocket.\n";
      return 2;
    }
    startup_options.has_protocol = true;
    startup_options.protocol = protocol;
  }

  if (parser.isSet(port_option)) {
    bool ok = false;
    const int port = parser.value(port_option).toInt(&ok);
    if (!ok || port < 1 || port > 65535) {
      QTextStream(stderr) << "Invalid --port value: " << parser.value(port_option)
                          << ". Use 1..65535.\n";
      return 2;
    }
    startup_options.has_port = true;
    startup_options.port = port;
  }

  if (parser.isSet(measurement_option)) {
    startup_options.has_measurement_title = true;
    startup_options.measurementTitle = parser.value(measurement_option);
  }

  MainWindow window(startup_options);
  window.show();

  return app.exec();
}

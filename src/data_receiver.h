#pragma once

#include <QList>
#include <QObject>

QT_BEGIN_NAMESPACE
class QUdpSocket;
class QWebSocket;
class QWebSocketServer;
class QTimer;
QT_END_NAMESPACE

class DataReceiver : public QObject {
  Q_OBJECT
 public:
  enum class Mode { Udp, WebSocket };
  Q_ENUM(Mode)

  explicit DataReceiver(QObject* parent = nullptr);
  ~DataReceiver() override;

  bool start(Mode mode, quint16 port);
  void stop();

  Mode mode() const;
  quint16 port() const;
  bool running() const;

 signals:
  void sample_received(double value);
  void status_changed(bool running, const QString& message);

 private slots:
  void on_udp_ready_read();
  void on_web_socket_connected();
  void on_web_socket_text(const QString& message);
  void on_web_socket_binary(const QByteArray& payload);
  void on_web_socket_disconnected();
  void send_web_socket_keep_alive();

 private:
  bool handle_payload(const QByteArray& payload);
  bool parse_double(const QByteArray& payload, double* out_value) const;

  Mode mode_ = Mode::Udp;
  quint16 port_ = 0;
  bool running_ = false;

  QUdpSocket* udp_socket_ = nullptr;
  QWebSocketServer* ws_server_ = nullptr;
  QList<QWebSocket*> ws_clients_;
  QTimer* keep_alive_timer_ = nullptr;
};

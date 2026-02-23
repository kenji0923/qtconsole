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
  void sampleReceived(double value);
  void statusChanged(bool running, const QString& message);

 private slots:
  void onUdpReadyRead();
  void onWebSocketConnected();
  void onWebSocketText(const QString& message);
  void onWebSocketBinary(const QByteArray& payload);
  void onWebSocketDisconnected();
  void sendWebSocketKeepAlive();

 private:
  bool handlePayload(const QByteArray& payload);
  bool parseDouble(const QByteArray& payload, double* out_value) const;

  Mode mode_ = Mode::Udp;
  quint16 port_ = 0;
  bool running_ = false;

  QUdpSocket* udp_socket_ = nullptr;
  QWebSocketServer* ws_server_ = nullptr;
  QList<QWebSocket*> ws_clients_;
  QTimer* keep_alive_timer_ = nullptr;
};

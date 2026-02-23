#include "data_receiver.h"

#include <QByteArray>
#include <QDataStream>
#include <QTimer>
#include <QUdpSocket>
#include <QWebSocket>
#include <QWebSocketServer>

DataReceiver::DataReceiver(QObject* parent) : QObject(parent), keep_alive_timer_(new QTimer(this)) {
  keep_alive_timer_->setInterval(5000);
  connect(keep_alive_timer_, &QTimer::timeout, this, &DataReceiver::send_web_socket_keep_alive);
}

DataReceiver::~DataReceiver() { stop(); }

bool DataReceiver::start(Mode mode, quint16 port) {
  stop();

  mode_ = mode;
  port_ = port;

  if (mode == Mode::Udp) {
    udp_socket_ = new QUdpSocket(this);
    connect(udp_socket_, &QUdpSocket::readyRead, this, &DataReceiver::on_udp_ready_read);
    if (!udp_socket_->bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress)) {
      emit status_changed(false, QStringLiteral("Failed to bind UDP port %1").arg(port));
      udp_socket_->deleteLater();
      udp_socket_ = nullptr;
      return false;
    }
    running_ = true;
    emit status_changed(true, QStringLiteral("Listening UDP on %1").arg(port));
    return true;
  }

  ws_server_ =
      new QWebSocketServer(QStringLiteral("qtconsole"), QWebSocketServer::NonSecureMode, this);
  connect(ws_server_, &QWebSocketServer::newConnection, this,
          &DataReceiver::on_web_socket_connected);

  if (!ws_server_->listen(QHostAddress::AnyIPv4, port)) {
    emit status_changed(false, QStringLiteral("Failed to listen WebSocket on %1").arg(port));
    ws_server_->deleteLater();
    ws_server_ = nullptr;
    return false;
  }

  keep_alive_timer_->start();
  running_ = true;
  emit status_changed(true, QStringLiteral("Listening WebSocket on %1").arg(port));
  return true;
}

void DataReceiver::stop() {
  if (udp_socket_) {
    udp_socket_->close();
    udp_socket_->deleteLater();
    udp_socket_ = nullptr;
  }

  if (ws_server_) {
    keep_alive_timer_->stop();
    for (auto* client : ws_clients_) {
      client->close();
      client->deleteLater();
    }
    ws_clients_.clear();

    ws_server_->close();
    ws_server_->deleteLater();
    ws_server_ = nullptr;
  }

  if (running_) {
    running_ = false;
    emit status_changed(false, QStringLiteral("Receiver stopped"));
  }
}

DataReceiver::Mode DataReceiver::mode() const { return mode_; }

quint16 DataReceiver::port() const { return port_; }

bool DataReceiver::running() const { return running_; }

void DataReceiver::on_udp_ready_read() {
  while (udp_socket_ && udp_socket_->hasPendingDatagrams()) {
    QByteArray payload;
    payload.resize(static_cast<int>(udp_socket_->pendingDatagramSize()));
    udp_socket_->readDatagram(payload.data(), payload.size());
    handle_payload(payload);
  }
}

void DataReceiver::on_web_socket_connected() {
  if (!ws_server_) {
    return;
  }

  QWebSocket* socket = ws_server_->nextPendingConnection();
  if (!socket) {
    return;
  }

  ws_clients_.append(socket);
  connect(socket, &QWebSocket::textMessageReceived, this, &DataReceiver::on_web_socket_text);
  connect(socket, &QWebSocket::binaryMessageReceived, this, &DataReceiver::on_web_socket_binary);
  connect(socket, &QWebSocket::disconnected, this, &DataReceiver::on_web_socket_disconnected);
}

void DataReceiver::on_web_socket_text(const QString& message) { handle_payload(message.toUtf8()); }

void DataReceiver::on_web_socket_binary(const QByteArray& payload) { handle_payload(payload); }

void DataReceiver::on_web_socket_disconnected() {
  auto* socket = qobject_cast<QWebSocket*>(sender());
  if (!socket) {
    return;
  }

  ws_clients_.removeAll(socket);
  socket->deleteLater();
}

void DataReceiver::send_web_socket_keep_alive() {
  for (auto* client : ws_clients_) {
    if (client->state() == QAbstractSocket::ConnectedState) {
      client->ping();
    }
  }
}

bool DataReceiver::handle_payload(const QByteArray& payload) {
  double value = 0.0;
  if (!parse_double(payload, &value)) {
    return false;
  }
  emit sample_received(value);
  return true;
}

bool DataReceiver::parse_double(const QByteArray& payload, double* out_value) const {
  bool ok = false;
  const double as_text = QString::fromUtf8(payload).trimmed().toDouble(&ok);
  if (ok) {
    *out_value = as_text;
    return true;
  }

  if (payload.size() == static_cast<int>(sizeof(double))) {
    QDataStream stream(payload);
    stream.setByteOrder(QDataStream::LittleEndian);
    double as_binary = 0.0;
    stream >> as_binary;
    if (stream.status() == QDataStream::Ok) {
      *out_value = as_binary;
      return true;
    }
  }

  return false;
}

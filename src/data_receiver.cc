#include "data_receiver.h"

#include <QByteArray>
#include <QDataStream>
#include <QTimer>
#include <QUdpSocket>
#include <QWebSocket>
#include <QWebSocketServer>

DataReceiver::DataReceiver(QObject* parent) : QObject(parent), keep_alive_timer_(new QTimer(this)) {
  keep_alive_timer_->setInterval(5000);
  connect(keep_alive_timer_, &QTimer::timeout, this, &DataReceiver::sendWebSocketKeepAlive);
}

DataReceiver::~DataReceiver() { stop(); }

bool DataReceiver::start(Mode mode, quint16 port) {
  stop();

  mode_ = mode;
  port_ = port;

  if (mode == Mode::Udp) {
    udp_socket_ = new QUdpSocket(this);
    connect(udp_socket_, &QUdpSocket::readyRead, this, &DataReceiver::onUdpReadyRead);
    if (!udp_socket_->bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress)) {
      emit statusChanged(false, QStringLiteral("Failed to bind UDP port %1").arg(port));
      udp_socket_->deleteLater();
      udp_socket_ = nullptr;
      return false;
    }
    running_ = true;
    emit statusChanged(true, QStringLiteral("Listening UDP on %1").arg(port));
    return true;
  }

  ws_server_ =
      new QWebSocketServer(QStringLiteral("qtconsole"), QWebSocketServer::NonSecureMode, this);
  connect(ws_server_, &QWebSocketServer::newConnection, this, &DataReceiver::onWebSocketConnected);

  if (!ws_server_->listen(QHostAddress::AnyIPv4, port)) {
    emit statusChanged(false, QStringLiteral("Failed to listen WebSocket on %1").arg(port));
    ws_server_->deleteLater();
    ws_server_ = nullptr;
    return false;
  }

  keep_alive_timer_->start();
  running_ = true;
  emit statusChanged(true, QStringLiteral("Listening WebSocket on %1").arg(port));
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
    emit statusChanged(false, QStringLiteral("Receiver stopped"));
  }
}

DataReceiver::Mode DataReceiver::mode() const { return mode_; }

quint16 DataReceiver::port() const { return port_; }

bool DataReceiver::running() const { return running_; }

void DataReceiver::onUdpReadyRead() {
  while (udp_socket_ && udp_socket_->hasPendingDatagrams()) {
    QByteArray payload;
    payload.resize(static_cast<int>(udp_socket_->pendingDatagramSize()));
    udp_socket_->readDatagram(payload.data(), payload.size());
    handlePayload(payload);
  }
}

void DataReceiver::onWebSocketConnected() {
  if (!ws_server_) {
    return;
  }

  QWebSocket* socket = ws_server_->nextPendingConnection();
  if (!socket) {
    return;
  }

  ws_clients_.append(socket);
  connect(socket, &QWebSocket::textMessageReceived, this, &DataReceiver::onWebSocketText);
  connect(socket, &QWebSocket::binaryMessageReceived, this, &DataReceiver::onWebSocketBinary);
  connect(socket, &QWebSocket::disconnected, this, &DataReceiver::onWebSocketDisconnected);
}

void DataReceiver::onWebSocketText(const QString& message) { handlePayload(message.toUtf8()); }

void DataReceiver::onWebSocketBinary(const QByteArray& payload) { handlePayload(payload); }

void DataReceiver::onWebSocketDisconnected() {
  auto* socket = qobject_cast<QWebSocket*>(sender());
  if (!socket) {
    return;
  }

  ws_clients_.removeAll(socket);
  socket->deleteLater();
}

void DataReceiver::sendWebSocketKeepAlive() {
  for (auto* client : ws_clients_) {
    if (client->state() == QAbstractSocket::ConnectedState) {
      client->ping();
    }
  }
}

bool DataReceiver::handlePayload(const QByteArray& payload) {
  double value = 0.0;
  if (!parseDouble(payload, &value)) {
    return false;
  }
  emit sampleReceived(value);
  return true;
}

bool DataReceiver::parseDouble(const QByteArray& payload, double* out_value) const {
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

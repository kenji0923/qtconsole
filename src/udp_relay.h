#pragma once

#include <QHostAddress>
#include <QObject>
#include <QVector>

class DataReceiver;
class QUdpSocket;

class UdpRelay : public QObject {
  Q_OBJECT
 public:
  struct Destination {
    QString host;
    quint16 port = 9001;
    bool enabled = true;
  };

  explicit UdpRelay(DataReceiver* receiver, QObject* parent = nullptr);
  void setEnabled(bool enabled);
  void setDestinations(const QVector<Destination>& destinations);

 signals:
  void destinationStatusChanged(int index, const QString& status);

 private:
  struct Endpoint {
    Destination destination;
    QHostAddress address;
    QString status;
  };
  void forwardDatagram(const QByteArray& payload);
  void reportStatus(int index, const QString& status);
  bool isReceiverAddress(const QHostAddress& address, quint16 port) const;

  DataReceiver* receiver_;
  QUdpSocket* socket_;
  QVector<Endpoint> endpoints_;
  QVector<int> lookups_;
  quint64 generation_ = 0;
  bool enabled_ = false;
};

#include "udp_relay.h"

#include <QHostInfo>
#include <QNetworkInterface>
#include <QUdpSocket>

#include "data_receiver.h"

UdpRelay::UdpRelay(DataReceiver* receiver, QObject* parent)
    : QObject(parent), receiver_(receiver), socket_(new QUdpSocket(this)) {
  connect(receiver_, &DataReceiver::udpDatagramReceived, this, &UdpRelay::forwardDatagram);
}

void UdpRelay::setEnabled(bool enabled) { enabled_ = enabled; }

void UdpRelay::setDestinations(const QVector<Destination>& destinations) {
  ++generation_;
  for (int lookup : lookups_) {
    QHostInfo::abortHostLookup(lookup);
  }
  lookups_.clear();
  endpoints_.clear();
  for (const auto& destination : destinations) {
    endpoints_.append(Endpoint{destination, QHostAddress(), QString()});
  }
  const quint64 generation = generation_;
  for (int i = 0; i < endpoints_.size(); ++i) {
    auto& endpoint = endpoints_[i];
    if (!endpoint.destination.enabled) {
      reportStatus(i, "Disabled");
      continue;
    }
    const QString host = endpoint.destination.host.trimmed();
    if (host.isEmpty() || endpoint.destination.port == 0) {
      reportStatus(i, "Enter a host and port (1-65535)");
      continue;
    }
    if (endpoint.address.setAddress(host)) {
      if (endpoint.address == QHostAddress::Any ||
          endpoint.address == QHostAddress::AnyIPv4 ||
          endpoint.address == QHostAddress::AnyIPv6) {
        endpoint.address.clear();
        reportStatus(i, "Use a destination address, not a wildcard");
      } else {
        reportStatus(i, "Ready");
      }
      continue;
    }
    reportStatus(i, "Resolving...");
    lookups_.append(QHostInfo::lookupHost(host, this, [this, i, generation](const QHostInfo& info) {
      if (generation != generation_) {
        return;
      }
      if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
        reportStatus(i, "Lookup failed: " + info.errorString());
        return;
      }
      // Prefer IPv4 to match the receiver; accept IPv6-only hosts.
      QHostAddress address = info.addresses().first();
      for (const auto& candidate : info.addresses()) {
        if (candidate.protocol() == QAbstractSocket::IPv4Protocol) {
          address = candidate;
          break;
        }
      }
      endpoints_[i].address = address;
      reportStatus(i, "Ready");
    }));
  }
}

bool UdpRelay::isReceiverAddress(const QHostAddress& address, quint16 port) const {
  if (!receiver_->running() || receiver_->mode() != DataReceiver::Mode::Udp ||
      receiver_->port() != port) {
    return false;
  }
  if (address.isLoopback()) {
    return true;
  }
  for (const auto& local : QNetworkInterface::allAddresses()) {
    if (address.isEqual(local, QHostAddress::ConvertV4MappedToIPv4)) {
      return true;
    }
  }
  return false;
}

void UdpRelay::forwardDatagram(const QByteArray& payload) {
  if (!enabled_) {
    return;
  }
  for (int i = 0; i < endpoints_.size(); ++i) {
    const auto& endpoint = endpoints_[i];
    if (!endpoint.destination.enabled || endpoint.address.isNull()) {
      continue;
    }
    if (isReceiverAddress(endpoint.address, endpoint.destination.port)) {
      reportStatus(i, "Blocked: destination is this receiver");
      continue;
    }
    const qint64 written =
        socket_->writeDatagram(payload, endpoint.address, endpoint.destination.port);
    reportStatus(i, written == payload.size() ? QStringLiteral("Sending")
                                              : "Send failed: " + socket_->errorString());
  }
}

void UdpRelay::reportStatus(int index, const QString& status) {
  if (endpoints_[index].status == status) {
    return;
  }
  endpoints_[index].status = status;
  emit destinationStatusChanged(index, status);
}

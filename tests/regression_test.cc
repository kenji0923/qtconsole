#include <QCheckBox>
#include <QDataStream>
#include <QNetworkDatagram>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QUdpSocket>

#include "data_receiver.h"
#include "measurement_model.h"
#include "relay_widget.h"
#include "udp_relay.h"

class RegressionTest : public QObject {
  Q_OBJECT
 private slots:
  void equationRecoversAfterSyntaxErrors() {
    MeasurementModel model;
    MeasurementSample latest;
    connect(&model, &MeasurementModel::sampleUpdated, this,
            [&](const MeasurementSample& sample, double) { latest = sample; });
    const QStringList invalid = {"x +", "sin(", "x + * 2", "unknown(x)"};
    for (const auto& equation : invalid) {
      model.setEquation(equation);
      QVERIFY(!model.equationValid());
      QVERIFY(!model.equationError().isEmpty());
      model.pushRawSample(3);
      QCOMPARE(latest.processed_value, 3.0);
      model.setEquation("2*x + sin(pi/2)");
      QVERIFY2(model.equationValid(), qPrintable(model.equationError()));
      QVERIFY(model.equationError().isEmpty());
      model.pushRawSample(3);
      QCOMPARE(latest.processed_value, 7.0);
    }
    model.setEquation("x +");
    model.setEquation("");
    QVERIFY(model.equationValid());
    model.pushRawSample(5);
    QCOMPARE(latest.processed_value, 5.0);
  }

  void equationRecoversAfterDomainErrors() {
    MeasurementModel model;
    MeasurementSample latest;
    connect(&model, &MeasurementModel::sampleUpdated, this,
            [&](const MeasurementSample& sample, double) { latest = sample; });
    model.setEquation("sqrt(x)");
    model.pushRawSample(-1);
    QCOMPARE(latest.processed_value, -1.0);
    model.pushRawSample(9);
    QCOMPARE(latest.processed_value, 3.0);
    QCOMPARE(latest.averaged_value, 3.0);
  }

  void relayPreservesDatagramsAndFansOut() {
    QUdpSocket first, second, disabled, sender, port_reservation;
    QVERIFY(first.bind(QHostAddress(QHostAddress::LocalHost), 0));
    QVERIFY(second.bind(QHostAddress(QHostAddress::LocalHost), 0));
    QVERIFY(disabled.bind(QHostAddress(QHostAddress::LocalHost), 0));
    QVERIFY(port_reservation.bind(QHostAddress(QHostAddress::LocalHost), 0));
    const quint16 port = port_reservation.localPort();
    port_reservation.close();
    DataReceiver receiver;
    QVERIFY(receiver.start(DataReceiver::Mode::Udp, port));
    UdpRelay relay(&receiver);
    relay.setDestinations({{"127.0.0.1", first.localPort(), true},
                           {"127.0.0.1", second.localPort(), true},
                           {"127.0.0.1", disabled.localPort(), false}});
    relay.setEnabled(true);
    QSignalSpy samples(&receiver, &DataReceiver::sampleReceived);
    QByteArray binary;
    QDataStream stream(&binary, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << 12.5;
    const QList<QByteArray> payloads = {" 123.45\n", binary, QByteArray("\0bad\xff", 5), {}};
    for (const auto& payload : payloads) {
      QCOMPARE(sender.writeDatagram(payload, QHostAddress::LocalHost, port),
               static_cast<qint64>(payload.size()));
      QTRY_VERIFY_WITH_TIMEOUT(first.hasPendingDatagrams(), 2000);
      QTRY_VERIFY_WITH_TIMEOUT(second.hasPendingDatagrams(), 2000);
      QCOMPARE(first.receiveDatagram().data(), payload);
      QCOMPARE(second.receiveDatagram().data(), payload);
    }
    QCOMPARE(samples.count(), 2);
    QVERIFY(!disabled.hasPendingDatagrams());
    relay.setEnabled(false);
    sender.writeDatagram("1", QHostAddress::LocalHost, port);
    QTRY_COMPARE(samples.count(), 3);
    QVERIFY(!first.hasPendingDatagrams());
    QVERIFY(!second.hasPendingDatagrams());
    relay.setEnabled(true);
    relay.setDestinations({});
    sender.writeDatagram("2", QHostAddress::LocalHost, port);
    QTRY_COMPARE(samples.count(), 4);
    QVERIFY(!first.hasPendingDatagrams());
  }

  void relayResolvesHostsAndBlocksSelf() {
    QUdpSocket destination, sender, reservation;
    QVERIFY(destination.bind(QHostAddress(QHostAddress::LocalHost), 0));
    QVERIFY(reservation.bind(QHostAddress(QHostAddress::LocalHost), 0));
    const quint16 port = reservation.localPort();
    reservation.close();
    DataReceiver receiver;
    QVERIFY(receiver.start(DataReceiver::Mode::Udp, port));
    UdpRelay relay(&receiver);
    QString host_status, self_status;
    connect(&relay, &UdpRelay::destinationStatusChanged, this,
            [&](int row, const QString& status) {
      if (row == 0) host_status = status;
      if (row == 1) self_status = status;
    });
    relay.setDestinations({{"localhost", destination.localPort(), true},
                           {"127.0.0.1", port, true}});
    QTRY_COMPARE_WITH_TIMEOUT(host_status, QString("Ready"), 5000);
    relay.setEnabled(true);
    QSignalSpy received(&receiver, &DataReceiver::udpDatagramReceived);
    sender.writeDatagram("42", QHostAddress::LocalHost, port);
    QTRY_VERIFY(destination.hasPendingDatagrams());
    QCOMPARE(destination.receiveDatagram().data(), QByteArray("42"));
    QVERIFY(self_status.startsWith("Blocked:"));
    QTest::qWait(30);
    QCOMPARE(received.count(), 1);
  }

  void relaySettingsAndPanel() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath("relay.ini"), QSettings::IniFormat);
    settings.setValue("relay/enabled", true);
    settings.beginWriteArray("relay/destinations", 2);
    settings.setArrayIndex(0);
    settings.setValue("host", "127.0.0.1");
    settings.setValue("port", 12345);
    settings.setValue("enabled", true);
    settings.setArrayIndex(1);
    settings.setValue("host", "localhost");
    settings.setValue("port", 23456);
    settings.setValue("enabled", false);
    settings.endArray();
    DataReceiver receiver;
    UdpRelay relay(&receiver);
    RelayWidget widget(&relay);
    widget.loadSettings(&settings);
    auto* table = widget.findChild<QTableWidget*>();
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->item(1, 0)->checkState(), Qt::Unchecked);
    QVERIFY(widget.findChild<QCheckBox*>()->isChecked());
    QSettings output(directory.filePath("export.ini"), QSettings::IniFormat);
    widget.saveSettings(&output);
    for (const auto& key : settings.allKeys()) {
      QCOMPARE(output.value(key), settings.value(key));
    }
    table->selectRow(0);
    for (auto* button : widget.findChildren<QPushButton*>()) {
      if (button->text() == "Remove selected") button->click();
    }
    QCOMPARE(table->rowCount(), 1);
    widget.saveSettings(&output);
    QCOMPARE(output.value("relay/destinations/size").toInt(), 1);
    QCOMPARE(output.value("relay/destinations/1/host").toString(), QString("localhost"));
    QSettings empty(directory.filePath("old-config.ini"), QSettings::IniFormat);
    widget.loadSettings(&empty);
    QCOMPARE(table->rowCount(), 0);
    QVERIFY(!widget.findChild<QCheckBox*>()->isChecked());
  }
};

QTEST_MAIN(RegressionTest)
#include "regression_test.moc"

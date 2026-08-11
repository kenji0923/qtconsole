#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSet>
#include <QTextStream>

#include <cstdio>

#include "history_paths.h"

namespace {
QByteArray csvNumber(double value) { return QByteArray::number(value, 'g', 17); }

QByteArray csvField(const QString& value) {
  QByteArray utf8 = value.toUtf8();
  utf8.replace("\"", "\"\"");
  return QByteArray("\"") + utf8 + QByteArray("\"");
}

QString ledgerPath(const QString& archive_root, const QString& batch_id) {
  const QString instance_id = batch_id.section(':', 0, 0);
  return QDir(archive_root)
      .filePath(QStringLiteral(".qtconsole_state/") +
                sanitizeHistoryComponent(instance_id) + QStringLiteral(".batches"));
}

void loadLedger(const QString& path, QSet<QString>* loaded_ledgers,
                QSet<QString>* completed_batches) {
  if (loaded_ledgers->contains(path)) {
    return;
  }
  loaded_ledgers->insert(path);
  QFile ledger(path);
  if (!ledger.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return;
  }
  while (!ledger.atEnd()) {
    const QString batch_id = QString::fromUtf8(ledger.readLine()).trimmed();
    if (!batch_id.isEmpty()) {
      completed_batches->insert(batch_id);
    }
  }
}

void recordCompletedBatch(const QString& path, const QString& batch_id) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile ledger(path);
  if (ledger.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    ledger.write(batch_id.toUtf8() + '\n');
    ledger.flush();
  }
}

QJsonObject writeBatch(const QJsonObject& batch, QSet<QString>* loaded_ledgers,
                       QSet<QString>* completed_batches) {
  QJsonObject response;
  const QString batch_id = batch.value(QStringLiteral("batch_id")).toString();
  response.insert(QStringLiteral("batch_id"), batch_id);

  if (batch.value(QStringLiteral("version")).toInt() != 1 || batch_id.isEmpty()) {
    response.insert(QStringLiteral("ok"), false);
    response.insert(QStringLiteral("error"), QStringLiteral("unsupported writer message"));
    return response;
  }
  const QString archive_root = batch.value(QStringLiteral("archive_root")).toString();
  const QString measurement_id = batch.value(QStringLiteral("measurement_id")).toString();
  const QString session_id = batch.value(QStringLiteral("session_id")).toString();
  const QDate date = QDate::fromString(batch.value(QStringLiteral("date")).toString(), Qt::ISODate);
  const QJsonArray records = batch.value(QStringLiteral("records")).toArray();
  if (archive_root.isEmpty() || measurement_id.isEmpty() || session_id.isEmpty() ||
      !date.isValid() || records.isEmpty()) {
    response.insert(QStringLiteral("ok"), false);
    response.insert(QStringLiteral("error"), QStringLiteral("incomplete writer message"));
    return response;
  }

  const QString ledger_path = ledgerPath(archive_root, batch_id);
  loadLedger(ledger_path, loaded_ledgers, completed_batches);
  if (completed_batches->contains(batch_id)) {
    response.insert(QStringLiteral("ok"), true);
    return response;
  }

  const QString directory = historyDirectoryPath(archive_root, measurement_id, date);
  if (!QDir().mkpath(directory)) {
    response.insert(QStringLiteral("ok"), false);
    response.insert(QStringLiteral("error"),
                    QStringLiteral("unable to create history directory: %1").arg(directory));
    return response;
  }

  const QString path = historyFilePath(archive_root, measurement_id, date);
  QLockFile lock(path + QStringLiteral(".lock"));
  lock.setStaleLockTime(30000);
  if (!lock.tryLock(5000)) {
    response.insert(QStringLiteral("ok"), false);
    response.insert(QStringLiteral("error"),
                    QStringLiteral("unable to lock history file: %1").arg(path));
    return response;
  }

  QFile file(path);
  const bool needs_header = !file.exists() || file.size() == 0;
  if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    response.insert(QStringLiteral("ok"), false);
    response.insert(QStringLiteral("error"), file.errorString());
    return response;
  }

  QByteArray output;
  if (needs_header) {
    output += "timestamp_iso8601,epoch_ms,session_id,raw_value,processed_value,averaged_value\n";
  }
  for (const QJsonValue& value : records) {
    const QJsonArray record = value.toArray();
    if (record.size() < 5) {
      continue;
    }
    const qint64 timestamp_ms = static_cast<qint64>(record.at(1).toDouble());
    const QString timestamp = QDateTime::fromMSecsSinceEpoch(timestamp_ms)
                                  .toLocalTime()
                                  .toString(Qt::ISODateWithMs);
    output += csvField(timestamp) + ',' + QByteArray::number(timestamp_ms) + ',' +
              csvField(session_id) + ',' + csvNumber(record.at(2).toDouble()) + ',' +
              csvNumber(record.at(3).toDouble()) + ',' + csvNumber(record.at(4).toDouble()) + '\n';
  }

  if (file.write(output) != output.size() || !file.flush()) {
    response.insert(QStringLiteral("ok"), false);
    response.insert(QStringLiteral("error"), file.errorString());
    return response;
  }
  recordCompletedBatch(ledger_path, batch_id);
  completed_batches->insert(batch_id);
  response.insert(QStringLiteral("ok"), true);
  response.insert(QStringLiteral("path"), path);
  return response;
}
}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  QFile input;
  input.open(stdin, QIODevice::ReadOnly | QIODevice::Text);
  QFile output;
  output.open(stdout, QIODevice::WriteOnly | QIODevice::Text);
  QSet<QString> completed_batches;
  QSet<QString> loaded_ledgers;

  while (true) {
    const QByteArray line = input.readLine();
    if (line.isEmpty() && input.atEnd()) {
      break;
    }
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parse_error);
    QJsonObject response;
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
      response.insert(QStringLiteral("batch_id"), QString());
      response.insert(QStringLiteral("ok"), false);
      response.insert(QStringLiteral("error"), QStringLiteral("invalid JSON message"));
    } else {
      response = writeBatch(document.object(), &loaded_ledgers, &completed_batches);
    }
    QByteArray response_line = QJsonDocument(response).toJson(QJsonDocument::Compact);
    response_line.append('\n');
    output.write(response_line);
    output.flush();
  }
  for (const QString& ledger_path : loaded_ledgers) {
    QFile::remove(ledger_path);
  }
  return 0;
}

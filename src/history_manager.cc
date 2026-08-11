#include "history_manager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QUuid>
#include <QtMath>
#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>

namespace {
constexpr int kMaxBatchSamples = 20000;

QDate localDateForTimestamp(qint64 timestamp_ms) {
  return QDateTime::fromMSecsSinceEpoch(timestamp_ms).toLocalTime().date();
}
}  // namespace

HistoryManager::HistoryManager(QObject* parent)
    : QObject(parent),
      config_{defaultHistoryRoot(), 30, 500000},
      writer_(new QProcess(this)),
      writer_instance_id_(QUuid::createUuid().toString(QUuid::WithoutBraces)) {
  flush_timer_.setInterval(config_.flush_interval_sec * 1000);
  restart_timer_.setSingleShot(true);

  connect(&flush_timer_, &QTimer::timeout, this, &HistoryManager::onFlushTimer);
  connect(&restart_timer_, &QTimer::timeout, this, &HistoryManager::onRestartTimer);
  connect(writer_, &QProcess::started, this, &HistoryManager::onWriterStarted);
  connect(writer_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this](int exit_code, QProcess::ExitStatus) { onWriterFinished(exit_code); });
  connect(writer_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
    onWriterError();
  });
  connect(writer_, &QProcess::readyReadStandardOutput, this,
          &HistoryManager::onWriterReadyRead);

  flush_timer_.start();
}

HistoryManager::~HistoryManager() { shutdown(); }

void HistoryManager::applyConfig(const HistoryConfig& config) {
  HistoryConfig normalized = config;
  if (normalized.archive_root.trimmed().isEmpty()) {
    normalized.archive_root = defaultHistoryRoot();
  }
  normalized.archive_root = QDir::cleanPath(normalized.archive_root);
  normalized.flush_interval_sec = qBound(1, normalized.flush_interval_sec, 3600);
  normalized.max_samples = qBound(10000, normalized.max_samples, 10000000);

  const bool root_changed = normalized.archive_root != config_.archive_root;
  config_ = normalized;
  flush_timer_.setInterval(config_.flush_interval_sec * 1000);
  enforceSampleLimit();

  if (root_changed && session_active_) {
    session_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
  }
  emitStatus();
}

HistoryConfig HistoryManager::config() const { return config_; }

void HistoryManager::startSession(const QString& measurement_id) {
  flushNow();
  active_measurement_id_ = measurement_id;
  session_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
  session_active_ = true;
  ensureWriterRunning();
  emitStatus();
}

void HistoryManager::stopSession() {
  session_active_ = false;
  flushNow();
  emitStatus();
}

QString HistoryManager::activeMeasurementId() const { return active_measurement_id_; }

QString HistoryManager::sessionId() const { return session_id_; }

void HistoryManager::appendSample(const MeasurementSample& incoming) {
  if (!session_active_) {
    return;
  }

  MeasurementSample sample = incoming;
  sample.sequence = next_sequence_++;
  sample.session_id = session_id_;
  sample.measurement_id = active_measurement_id_;
  sample.archive_root = config_.archive_root;
  samples_.push_back(std::move(sample));
  enforceSampleLimit();
  emit historyChanged();
}

void HistoryManager::resetDisplayHistory() {
  display_start_sequence_ = next_sequence_;
  emit displayHistoryReset();
}

bool HistoryManager::hasDisplaySamples() const {
  return firstDisplayIterator() != samples_.cend();
}

qint64 HistoryManager::latestDisplayTimestampMs() const {
  if (samples_.empty() || samples_.back().sequence < display_start_sequence_) {
    return 0;
  }
  return samples_.back().timestamp_ms;
}

QVector<QPointF> HistoryManager::buildDisplayPoints(qint64 left_ms, qint64 right_ms,
                                                    int max_display_points) const {
  auto begin = firstDisplayIterator();
  begin = std::lower_bound(begin, samples_.cend(), left_ms,
                           [](const MeasurementSample& sample, qint64 timestamp) {
                             return sample.timestamp_ms < timestamp;
                           });
  auto end = std::upper_bound(begin, samples_.cend(), right_ms,
                              [](qint64 timestamp, const MeasurementSample& sample) {
                                return timestamp < sample.timestamp_ms;
                              });
  const int sample_count = static_cast<int>(std::distance(begin, end));
  if (sample_count <= 0) {
    return {};
  }

  max_display_points = qMax(2, max_display_points);
  QVector<QPointF> points;
  if (sample_count <= max_display_points) {
    points.reserve(sample_count);
    for (auto it = begin; it != end; ++it) {
      points.append(QPointF(static_cast<qreal>(it->timestamp_ms), it->averaged_value));
    }
    return points;
  }

  const int target_buckets = qMax(1, max_display_points / 2);
  const int bucket_size = qMax(1, (sample_count + target_buckets - 1) / target_buckets);
  points.reserve(max_display_points + 2);
  auto bucket_begin = begin;
  while (bucket_begin != end) {
    auto bucket_end = bucket_begin;
    int count = 0;
    while (bucket_end != end && count < bucket_size) {
      ++bucket_end;
      ++count;
    }

    auto min_it = bucket_begin;
    auto max_it = bucket_begin;
    for (auto it = bucket_begin; it != bucket_end; ++it) {
      if (it->averaged_value < min_it->averaged_value) {
        min_it = it;
      }
      if (it->averaged_value > max_it->averaged_value) {
        max_it = it;
      }
    }
    const auto append = [&points](const MeasurementSample& sample) {
      points.append(QPointF(static_cast<qreal>(sample.timestamp_ms), sample.averaged_value));
    };
    if (min_it->timestamp_ms <= max_it->timestamp_ms) {
      append(*min_it);
      if (max_it != min_it) {
        append(*max_it);
      }
    } else {
      append(*max_it);
      append(*min_it);
    }
    bucket_begin = bucket_end;
  }
  return points;
}

QVector<MeasurementSample> HistoryManager::displaySamples(qint64 left_ms,
                                                          qint64 right_ms) const {
  QVector<MeasurementSample> result;
  auto begin = firstDisplayIterator();
  begin = std::lower_bound(begin, samples_.cend(), left_ms,
                           [](const MeasurementSample& sample, qint64 timestamp) {
                             return sample.timestamp_ms < timestamp;
                           });
  auto end = std::upper_bound(begin, samples_.cend(), right_ms,
                              [](qint64 timestamp, const MeasurementSample& sample) {
                                return timestamp < sample.timestamp_ms;
                              });
  result.reserve(static_cast<int>(std::distance(begin, end)));
  for (auto it = begin; it != end; ++it) {
    result.append(*it);
  }
  return result;
}

int HistoryManager::sampleCount() const { return static_cast<int>(samples_.size()); }

int HistoryManager::pendingSampleCount() const {
  return static_cast<int>(std::count_if(samples_.cbegin(), samples_.cend(), [this](const auto& s) {
    return s.sequence > last_acked_sequence_;
  }));
}

quint64 HistoryManager::droppedSampleCount() const { return dropped_samples_; }

QString HistoryManager::currentHistoryFile() const {
  if (active_measurement_id_.isEmpty()) {
    return QString();
  }
  const qint64 timestamp = samples_.empty() ? QDateTime::currentMSecsSinceEpoch()
                                            : samples_.back().timestamp_ms;
  return historyFilePath(config_.archive_root, active_measurement_id_,
                         localDateForTimestamp(timestamp));
}

QString HistoryManager::statusText() const {
  if (!warning_message_.isEmpty() || dropped_samples_ > 0) {
    QString text = QStringLiteral("History warning: %1").arg(warning_message_);
    if (dropped_samples_ > 0) {
      text += QStringLiteral(" | dropped %1").arg(dropped_samples_);
    }
    text += QStringLiteral(" | pending %1").arg(pendingSampleCount());
    return text;
  }
  if (writer_->state() == QProcess::Running) {
    return QStringLiteral("History: saving | pending %1").arg(pendingSampleCount());
  }
  if (!session_active_ && pendingSampleCount() == 0) {
    return QStringLiteral("History: idle");
  }
  return QStringLiteral("History: writer starting | pending %1").arg(pendingSampleCount());
}

bool HistoryManager::hasWarning() const {
  return !warning_message_.isEmpty() || dropped_samples_ > 0;
}

void HistoryManager::flushNow() {
  if (samples_.empty() || pendingSampleCount() == 0) {
    return;
  }
  flush_target_sequence_ = qMax(flush_target_sequence_, samples_.back().sequence);
  ensureWriterRunning();
  if (writer_->state() == QProcess::Running) {
    sendNextBatch();
  }
}

void HistoryManager::clearWarning() {
  warning_message_.clear();
  dropped_samples_ = 0;
  emitStatus();
}

void HistoryManager::shutdown(int timeout_ms) {
  if (shutting_down_) {
    return;
  }
  flushNow();
  if (writer_->state() == QProcess::Starting) {
    writer_->waitForStarted(qMax(0, timeout_ms));
  }
  shutting_down_ = true;
  if (writer_->state() == QProcess::Running && in_flight_.id.isEmpty()) {
    sendNextBatch();
  }
  if (writer_->state() == QProcess::Running) {
    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + qMax(0, timeout_ms);
    while ((!in_flight_.id.isEmpty() || pendingSampleCount() > 0) &&
           QDateTime::currentMSecsSinceEpoch() < deadline) {
      writer_->waitForReadyRead(50);
      onWriterReadyRead();
      if (in_flight_.id.isEmpty()) {
        sendNextBatch();
      }
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);
    }
    writer_->closeWriteChannel();
    if (!writer_->waitForFinished(qMax(0, timeout_ms))) {
      writer_->terminate();
      writer_->waitForFinished(1000);
    }
  }
}

void HistoryManager::onWriterStarted() {
  restart_attempt_ = 0;
  emitStatus();
  sendNextBatch();
}

void HistoryManager::onWriterFinished(int exit_code) {
  if (shutting_down_) {
    return;
  }
  if (!in_flight_.id.isEmpty()) {
    retry_batch_ = in_flight_;
    in_flight_ = {};
  }
  if (exit_code != 0 || pendingSampleCount() > 0) {
    setWarning(QStringLiteral("writer exited (%1)").arg(exit_code));
    scheduleRestart();
  }
}

void HistoryManager::onWriterError() {
  if (shutting_down_) {
    return;
  }
  if (!in_flight_.id.isEmpty()) {
    retry_batch_ = in_flight_;
    in_flight_ = {};
  }
  setWarning(writer_->errorString());
  scheduleRestart();
}

void HistoryManager::onWriterReadyRead() {
  writer_output_buffer_ += writer_->readAllStandardOutput();
  qsizetype newline = -1;
  while ((newline = writer_output_buffer_.indexOf('\n')) >= 0) {
    const QByteArray line = writer_output_buffer_.left(newline).trimmed();
    writer_output_buffer_.remove(0, newline + 1);
    if (!line.isEmpty()) {
      handleAckLine(line);
    }
  }
}

void HistoryManager::onFlushTimer() { flushNow(); }

void HistoryManager::onRestartTimer() { ensureWriterRunning(); }

void HistoryManager::ensureWriterRunning() {
  if (shutting_down_ || writer_->state() != QProcess::NotRunning) {
    return;
  }
  writer_->setProgram(writerProgramPath());
  writer_->start();
}

void HistoryManager::scheduleRestart() {
  if (shutting_down_ || restart_timer_.isActive()) {
    return;
  }
  static const int delays_ms[] = {1000, 2000, 5000, 10000, 30000};
  const int index = qMin(restart_attempt_, 4);
  ++restart_attempt_;
  restart_timer_.start(delays_ms[index]);
}

void HistoryManager::sendNextBatch() {
  if (!in_flight_.id.isEmpty() || writer_->state() != QProcess::Running) {
    return;
  }

  auto begin = std::find_if(samples_.cbegin(), samples_.cend(), [this](const auto& sample) {
    return sample.sequence > last_acked_sequence_;
  });
  if (begin == samples_.cend() || begin->sequence > flush_target_sequence_) {
    emitStatus();
    return;
  }

  const QDate batch_date = localDateForTimestamp(begin->timestamp_ms);
  const QString archive_root = begin->archive_root;
  const QString measurement_id = begin->measurement_id;
  const QString session_id = begin->session_id;
  QJsonArray records;
  auto end = begin;
  const bool retrying =
      !retry_batch_.id.isEmpty() && retry_batch_.first_sequence == begin->sequence;
  if (!retrying) {
    retry_batch_ = {};
  }
  const quint64 retry_final_sequence =
      retrying ? retry_batch_.last_sequence : std::numeric_limits<quint64>::max();
  const quint64 final_sequence = qMin(retry_final_sequence, flush_target_sequence_);
  while (end != samples_.cend() && records.size() < kMaxBatchSamples &&
         end->sequence <= final_sequence &&
         end->archive_root == archive_root && end->measurement_id == measurement_id &&
         end->session_id == session_id && localDateForTimestamp(end->timestamp_ms) == batch_date) {
    QJsonArray record;
    record.append(static_cast<double>(end->sequence));
    record.append(static_cast<double>(end->timestamp_ms));
    record.append(end->raw_value);
    record.append(end->processed_value);
    record.append(end->averaged_value);
    records.append(record);
    ++end;
  }

  in_flight_.id = retrying ? retry_batch_.id
                           : writer_instance_id_ + ':' + QString::number(next_batch_number_++);
  in_flight_.first_sequence = begin->sequence;
  in_flight_.last_sequence = std::prev(end)->sequence;

  QJsonObject message;
  message.insert(QStringLiteral("version"), 1);
  message.insert(QStringLiteral("batch_id"), in_flight_.id);
  message.insert(QStringLiteral("archive_root"), archive_root);
  message.insert(QStringLiteral("measurement_id"), measurement_id);
  message.insert(QStringLiteral("session_id"), session_id);
  message.insert(QStringLiteral("date"), batch_date.toString(Qt::ISODate));
  message.insert(QStringLiteral("records"), records);
  QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
  payload.append('\n');
  if (writer_->write(payload) < 0) {
    retry_batch_ = in_flight_;
    in_flight_ = {};
    setWarning(writer_->errorString());
    scheduleRestart();
  }
}

void HistoryManager::handleAckLine(const QByteArray& line) {
  QJsonParseError parse_error;
  const QJsonDocument document = QJsonDocument::fromJson(line, &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
    setWarning(QStringLiteral("invalid writer response"));
    return;
  }
  const QJsonObject object = document.object();
  const QString batch_id = object.value(QStringLiteral("batch_id")).toString();
  if (batch_id != in_flight_.id) {
    return;
  }
  if (!object.value(QStringLiteral("ok")).toBool()) {
    const QString error = object.value(QStringLiteral("error")).toString();
    retry_batch_ = in_flight_;
    in_flight_ = {};
    setWarning(error.isEmpty() ? QStringLiteral("history write failed") : error);
    scheduleRestart();
    return;
  }

  last_acked_sequence_ = qMax(last_acked_sequence_, in_flight_.last_sequence);
  retry_batch_ = {};
  in_flight_ = {};
  emitStatus();
  if (last_acked_sequence_ < flush_target_sequence_) {
    sendNextBatch();
  }
}

void HistoryManager::enforceSampleLimit() {
  const int maximum = config_.max_samples;
  if (static_cast<int>(samples_.size()) < maximum) {
    return;
  }
  quint64 removed_unacked = 0;
  while (static_cast<int>(samples_.size()) >= maximum) {
    const int remove_count = maximum / 2;
    for (int i = 0; i < remove_count && !samples_.empty(); ++i) {
      if (samples_.front().sequence > last_acked_sequence_) {
        ++removed_unacked;
      }
      samples_.pop_front();
    }
  }
  if (removed_unacked > 0) {
    dropped_samples_ += removed_unacked;
    setWarning(QStringLiteral("history buffer overflow"));
  }
  flushNow();
}

void HistoryManager::setWarning(const QString& message) {
  warning_message_ = message;
  emitStatus();
}

void HistoryManager::emitStatus() { emit statusChanged(statusText(), hasWarning()); }

QString HistoryManager::writerProgramPath() const {
#ifdef Q_OS_WIN
  const QString executable = QStringLiteral("qtconsole-history-writer.exe");
#else
  const QString executable = QStringLiteral("qtconsole-history-writer");
#endif
  return QDir(QCoreApplication::applicationDirPath()).filePath(executable);
}

std::deque<MeasurementSample>::const_iterator HistoryManager::firstDisplayIterator() const {
  return std::lower_bound(samples_.cbegin(), samples_.cend(), display_start_sequence_,
                          [](const MeasurementSample& sample, quint64 sequence) {
                            return sample.sequence < sequence;
                          });
}

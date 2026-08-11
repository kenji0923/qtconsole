#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointF>
#include <QTimer>
#include <QVector>
#include <deque>

#include "history_paths.h"
#include "measurement_sample.h"

QT_BEGIN_NAMESPACE
class QProcess;
QT_END_NAMESPACE

class HistoryManager : public QObject {
  Q_OBJECT
 public:
  explicit HistoryManager(QObject* parent = nullptr);
  ~HistoryManager() override;

  void applyConfig(const HistoryConfig& config);
  HistoryConfig config() const;

  void startSession(const QString& measurement_id);
  void stopSession();
  QString activeMeasurementId() const;
  QString sessionId() const;

  void appendSample(const MeasurementSample& sample);
  void resetDisplayHistory();

  bool hasDisplaySamples() const;
  qint64 latestDisplayTimestampMs() const;
  QVector<QPointF> buildDisplayPoints(qint64 left_ms, qint64 right_ms,
                                      int max_display_points) const;
  QVector<MeasurementSample> displaySamples(qint64 left_ms, qint64 right_ms) const;

  int sampleCount() const;
  int pendingSampleCount() const;
  quint64 droppedSampleCount() const;
  QString currentHistoryFile() const;
  QString statusText() const;
  bool hasWarning() const;

  void flushNow();
  void clearWarning();
  void shutdown(int timeout_ms = 5000);

 signals:
  void historyChanged();
  void displayHistoryReset();
  void statusChanged(const QString& text, bool warning);

 private slots:
  void onWriterStarted();
  void onWriterFinished(int exit_code);
  void onWriterError();
  void onWriterReadyRead();
  void onFlushTimer();
  void onRestartTimer();

 private:
  struct BatchState {
    QString id;
    quint64 first_sequence = 0;
    quint64 last_sequence = 0;
  };

  void ensureWriterRunning();
  void scheduleRestart();
  void sendNextBatch();
  void handleAckLine(const QByteArray& line);
  void enforceSampleLimit();
  void setWarning(const QString& message);
  void emitStatus();
  QString writerProgramPath() const;
  std::deque<MeasurementSample>::const_iterator firstDisplayIterator() const;

  HistoryConfig config_;
  std::deque<MeasurementSample> samples_;
  quint64 next_sequence_ = 1;
  quint64 display_start_sequence_ = 1;
  quint64 last_acked_sequence_ = 0;
  quint64 flush_target_sequence_ = 0;
  quint64 dropped_samples_ = 0;

  QString active_measurement_id_;
  QString session_id_;
  bool session_active_ = false;

  QProcess* writer_;
  QTimer flush_timer_;
  QTimer restart_timer_;
  QByteArray writer_output_buffer_;
  BatchState in_flight_;
  BatchState retry_batch_;
  QString writer_instance_id_;
  quint64 next_batch_number_ = 1;
  int restart_attempt_ = 0;
  QString warning_message_;
  bool shutting_down_ = false;
};

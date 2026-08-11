#include "time_series_widget.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGraphicsLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPdfWriter>
#include <QPushButton>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLegend>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include "history_manager.h"
#include "measurement_model.h"

namespace {
QString csvField(const QString& value) {
  QString escaped = value;
  escaped.replace('"', QStringLiteral("\"\""));
  return QStringLiteral("\"") + escaped + QStringLiteral("\"");
}
}  // namespace

TimeSeriesWidget::TimeSeriesWidget(MeasurementModel* model, HistoryManager* history,
                                   QWidget* parent)
    : QWidget(parent),
      model_(model),
      history_(history),
      chart_(new QChart),
      chart_view_(new QChartView(chart_, this)),
      series_(new QLineSeries),
      axis_x_(new QDateTimeAxis),
      axis_y_(new QValueAxis),
      auto_scale_check_(new QCheckBox("Auto scale Y", this)),
      min_spin_(new QDoubleSpinBox(this)),
      max_spin_(new QDoubleSpinBox(this)),
      duration_spin_(new QDoubleSpinBox(this)),
      reset_button_(new QPushButton("Reset view", this)),
      pause_button_(new QPushButton("Pause", this)),
      export_data_button_(new QPushButton("Export data", this)),
      export_image_button_(new QPushButton("Export image", this)),
      render_timer_(new QTimer(this)) {
  chart_->addSeries(series_);
  chart_->legend()->hide();
  chart_->setMargins(QMargins(0, 0, 0, 0));
  chart_->layout()->setContentsMargins(0, 0, 0, 0);
  chart_->setBackgroundRoundness(0);

  axis_x_->setTitleText("Local time");
  axis_y_->setTitleText("Value");
  axis_y_->setRange(0.0, model_->referenceMax());

  chart_->addAxis(axis_x_, Qt::AlignBottom);
  chart_->addAxis(axis_y_, Qt::AlignLeft);
  series_->attachAxis(axis_x_);
  series_->attachAxis(axis_y_);
  updateXAxisRangeFor(QDateTime::currentMSecsSinceEpoch());

  chart_view_->setRenderHint(QPainter::Antialiasing, false);

  auto_scale_check_->setChecked(true);
  min_spin_->setEnabled(false);
  max_spin_->setEnabled(false);

  min_spin_->setRange(-1e9, 1e9);
  min_spin_->setDecimals(3);
  min_spin_->setValue(0.0);

  max_spin_->setRange(-1e9, 1e9);
  max_spin_->setDecimals(3);
  max_spin_->setValue(model_->referenceMax());

  duration_spin_->setRange(0.1, 3600.0);
  duration_spin_->setDecimals(2);
  duration_spin_->setSingleStep(0.5);
  duration_spin_->setValue(static_cast<double>(window_ms_) / 1000.0);

  auto* form = new QFormLayout;
  form->addRow("Y min", min_spin_);
  form->addRow("Y max", max_spin_);
  form->addRow("Duration (s)", duration_spin_);

  auto* controls_buttons = new QGridLayout;
  controls_buttons->addWidget(auto_scale_check_, 0, 0);
  controls_buttons->addWidget(pause_button_, 0, 1);
  controls_buttons->addWidget(export_data_button_, 1, 0);
  controls_buttons->addWidget(export_image_button_, 1, 1);
  controls_buttons->addWidget(reset_button_, 1, 2);

  auto* controls = new QHBoxLayout;
  controls->addLayout(form);
  controls->addLayout(controls_buttons);
  controls->addStretch(1);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(chart_view_, 1);
  layout->addLayout(controls);

  connect(history_, &HistoryManager::displayHistoryReset, this,
          &TimeSeriesWidget::onHistoryReset);
  connect(auto_scale_check_, &QCheckBox::toggled, this, &TimeSeriesWidget::onAutoScaleToggled);
  connect(min_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &TimeSeriesWidget::onRangeEdited);
  connect(max_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &TimeSeriesWidget::onRangeEdited);
  connect(duration_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &TimeSeriesWidget::onDurationChanged);
  connect(reset_button_, &QPushButton::clicked, history_,
          &HistoryManager::resetDisplayHistory);
  connect(pause_button_, &QPushButton::clicked, this, &TimeSeriesWidget::onPauseToggled);
  connect(export_data_button_, &QPushButton::clicked, this,
          &TimeSeriesWidget::onExportDataClicked);
  connect(export_image_button_, &QPushButton::clicked, this,
          &TimeSeriesWidget::onExportImageClicked);
  connect(render_timer_, &QTimer::timeout, this, &TimeSeriesWidget::renderFrame);

  render_timer_->setInterval(16);
  render_timer_->start();
}

void TimeSeriesWidget::loadSettings(QSettings* settings) {
  settings->beginGroup("time_series");

  auto_scale_check_->setChecked(settings->value("auto_scale", true).toBool());
  min_spin_->setValue(settings->value("y_min", 0.0).toDouble());
  max_spin_->setValue(settings->value("y_max", model_->referenceMax()).toDouble());
  duration_spin_->setValue(settings->value("duration_sec", 10.0).toDouble());
  const bool paused = settings->value("paused", false).toBool();
  if (paused_ != paused) {
    onPauseToggled();
  }

  settings->endGroup();
}

void TimeSeriesWidget::saveSettings(QSettings* settings) const {
  settings->beginGroup("time_series");
  settings->setValue("auto_scale", auto_scale_check_->isChecked());
  settings->setValue("y_min", min_spin_->value());
  settings->setValue("y_max", max_spin_->value());
  settings->setValue("duration_sec", duration_spin_->value());
  settings->setValue("paused", paused_);
  settings->endGroup();
}

void TimeSeriesWidget::onHistoryReset() {
  series_->clear();
  paused_right_ms_ = paused_ ? QDateTime::currentMSecsSinceEpoch() : 0;
  updateXAxisRangeFor(currentRightTimestampMs());
  applyAxisRange();
}

void TimeSeriesWidget::onAutoScaleToggled(bool checked) {
  min_spin_->setEnabled(!checked);
  max_spin_->setEnabled(!checked);
  if (!checked) {
    applyAxisRange();
  }
}

void TimeSeriesWidget::onRangeEdited() {
  if (!auto_scale_check_->isChecked()) {
    applyAxisRange();
  }
}

void TimeSeriesWidget::onDurationChanged(double duration_sec) {
  window_ms_ = static_cast<qint64>(qMax(0.1, duration_sec) * 1000.0);
  updateXAxisRangeFor(currentRightTimestampMs());
  renderFrame();
}

void TimeSeriesWidget::onPauseToggled() {
  paused_ = !paused_;
  if (paused_) {
    paused_right_ms_ = currentRightTimestampMs();
  } else {
    paused_right_ms_ = 0;
  }
  pause_button_->setText(paused_ ? "Resume" : "Pause");
  if (!paused_) {
    renderFrame();
  }
}

void TimeSeriesWidget::onExportDataClicked() {
  const QString path = QFileDialog::getSaveFileName(this, "Export visible time series data",
                                                    "time_series.csv", "CSV files (*.csv)");
  if (path.isEmpty()) {
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, "Export failed", "Unable to open file for writing.");
    return;
  }

  const qint64 right_ms = currentRightTimestampMs();
  const QVector<MeasurementSample> samples =
      history_->displaySamples(right_ms - window_ms_, right_ms);
  QTextStream stream(&file);
  stream.setLocale(QLocale::c());
  stream << "timestamp_iso8601,epoch_ms,session_id,raw_value,processed_value,averaged_value\n";
  for (const auto& sample : samples) {
    const QString timestamp = QDateTime::fromMSecsSinceEpoch(sample.timestamp_ms)
                                  .toLocalTime()
                                  .toString(Qt::ISODateWithMs);
    stream << csvField(timestamp) << ',' << sample.timestamp_ms << ','
           << csvField(sample.session_id) << ',' << sample.raw_value << ','
           << sample.processed_value << ',' << sample.averaged_value << '\n';
  }
}

void TimeSeriesWidget::onExportImageClicked() {
  const QString path = QFileDialog::getSaveFileName(this, "Export chart image", "time_series.png",
                                                    "PNG image (*.png);;PDF document (*.pdf)");
  if (path.isEmpty()) {
    return;
  }

  const QString suffix = QFileInfo(path).suffix().toLower();
  if (suffix == "pdf") {
    QPdfWriter pdf_writer(path);
    pdf_writer.setResolution(300);
    const QSize content_px = chart_view_->size();
    const qreal width_pt = content_px.width() * 72.0 / pdf_writer.resolution();
    const qreal height_pt = content_px.height() * 72.0 / pdf_writer.resolution();
    QPageSize page_size(QSizeF(width_pt, height_pt), QPageSize::Point, "content_size");
    pdf_writer.setPageSize(page_size);
    pdf_writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter painter(&pdf_writer);
    chart_view_->render(&painter);
    painter.end();
    return;
  }

  if (!chart_view_->grab().save(path, "PNG")) {
    QMessageBox::warning(this, "Export failed", "Unable to save PNG image.");
  }
}

void TimeSeriesWidget::applyAxisRange() {
  double ymin = min_spin_->value();
  double ymax = max_spin_->value();
  if (ymin >= ymax) {
    ymax = ymin + 1.0;
    max_spin_->setValue(ymax);
  }
  axis_y_->setRange(ymin, ymax);
}

void TimeSeriesWidget::updateXAxisRangeFor(qint64 right_ms) {
  const qint64 left_ms = right_ms - window_ms_;
  const QDateTime left = QDateTime::fromMSecsSinceEpoch(left_ms).toLocalTime();
  const QDateTime right = QDateTime::fromMSecsSinceEpoch(right_ms).toLocalTime();
  axis_x_->setRange(left, right);
  updateXAxisAppearance(left_ms, right_ms);
}

void TimeSeriesWidget::updateXAxisAppearance(qint64 left_ms, qint64 right_ms) {
  const QDateTime left = QDateTime::fromMSecsSinceEpoch(left_ms).toLocalTime();
  const QDateTime right = QDateTime::fromMSecsSinceEpoch(right_ms).toLocalTime();
  if (left.date().year() != right.date().year()) {
    axis_x_->setFormat(QStringLiteral("yyyy/MM/dd\nHH:mm:ss"));
  } else if (left.date() != right.date()) {
    axis_x_->setFormat(QStringLiteral("MM/dd\nHH:mm:ss"));
  } else {
    axis_x_->setFormat(QStringLiteral("HH:mm:ss"));
  }
  const int viewport_width = qMax(240, chart_view_->viewport()->width());
  axis_x_->setTickCount(qBound(2, viewport_width / 120, 8));
}

qint64 TimeSeriesWidget::currentRightTimestampMs() const {
  if (paused_ && paused_right_ms_ > 0) {
    return paused_right_ms_;
  }
  const qint64 latest = history_->latestDisplayTimestampMs();
  return latest > 0 ? latest : QDateTime::currentMSecsSinceEpoch();
}

void TimeSeriesWidget::renderFrame() {
  if (paused_) {
    return;
  }

  const qint64 right_ms = currentRightTimestampMs();
  updateXAxisRangeFor(right_ms);
  const int max_display_points = qMax(64, chart_view_->viewport()->width()) * 2;
  const QVector<QPointF> points =
      history_->buildDisplayPoints(right_ms - window_ms_, right_ms, max_display_points);
  series_->replace(points);
  if (points.isEmpty() || !auto_scale_check_->isChecked()) {
    return;
  }

  qreal ymin = points.front().y();
  qreal ymax = ymin;
  for (const auto& point : points) {
    ymin = qMin(ymin, point.y());
    ymax = qMax(ymax, point.y());
  }
  if (qFuzzyCompare(ymin, ymax)) {
    ymin -= 1.0;
    ymax += 1.0;
  }
  axis_y_->setRange(ymin, ymax);
}

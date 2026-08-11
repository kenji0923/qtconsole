#include "main_window.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDockWidget>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QUrl>
#include <QWidget>

#include "data_receiver.h"
#include "history_manager.h"
#include "history_paths.h"
#include "history_settings_dialog.h"
#include "measurement_model.h"
#include "numeric_ratio_widget.h"
#include "receiver_control_widget.h"
#include "statistics_widget.h"
#include "time_series_widget.h"
#include "title_line_widget.h"

namespace {
QSettings createBootstrapSettings() {
#ifdef Q_OS_WIN
  return QSettings(QSettings::IniFormat, QSettings::UserScope, "kshu", "qtconsole");
#else
  return QSettings(QSettings::NativeFormat, QSettings::UserScope, "kshu", "qtconsole");
#endif
}
}  // namespace

MainWindow::MainWindow(QWidget* parent) : MainWindow(StartupOptions(), parent) {}

MainWindow::MainWindow(const StartupOptions& startup_options, QWidget* parent)
    : QMainWindow(parent),
      model_(new MeasurementModel(this)),
      history_(new HistoryManager(this)),
      receiver_(new DataReceiver(this)),
      title_line_widget_(nullptr),
      numeric_ratio_widget_(nullptr),
      receiver_control_widget_(nullptr),
      time_series_widget_(nullptr),
      input_dock_(nullptr),
      title_line_dock_(nullptr),
      always_on_top_action_(nullptr),
      history_status_label_(new QLabel(this)),
      clear_history_warning_button_(new QPushButton(QStringLiteral("Clear"), this)),
      startup_options_(startup_options) {
  setWindowTitle("qtconsole_UDP9000");
  resize(1100, 700);
  setDockNestingEnabled(true);

  connect(receiver_, &DataReceiver::sampleReceived, model_, &MeasurementModel::pushRawSample);
  connect(model_, &MeasurementModel::sampleUpdated, history_, &HistoryManager::appendSample);
  connect(receiver_, &DataReceiver::statusChanged, this,
          &MainWindow::onReceiverStatusChanged);
  connect(history_, &HistoryManager::statusChanged, this,
          &MainWindow::onHistoryStatusChanged);

  receiver_control_widget_ = new ReceiverControlWidget(receiver_, model_, this);
  input_dock_ = new QDockWidget("Input", this);
  input_dock_->setObjectName("input_dock");
  input_dock_->setWidget(receiver_control_widget_);
  input_dock_->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
  addDockWidget(Qt::TopDockWidgetArea, input_dock_);

  title_line_widget_ = new TitleLineWidget(this);
  title_line_dock_ = new QDockWidget("Title line", this);
  title_line_dock_->setObjectName("title_line_dock");
  title_line_dock_->setWidget(title_line_widget_);
  title_line_dock_->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
  addDockWidget(Qt::TopDockWidgetArea, title_line_dock_);

  numeric_ratio_widget_ = new NumericRatioWidget(model_, this);
  auto* numeric_dock = new QDockWidget("Numeric / Ratio", this);
  numeric_dock->setObjectName("numeric_ratio_dock");
  numeric_dock->setWidget(numeric_ratio_widget_);
  numeric_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
  addDockWidget(Qt::LeftDockWidgetArea, numeric_dock);

  time_series_widget_ = new TimeSeriesWidget(model_, history_, this);
  auto* series_dock = new QDockWidget("Time Series", this);
  series_dock->setObjectName("time_series_dock");
  series_dock->setWidget(time_series_widget_);
  series_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
  addDockWidget(Qt::LeftDockWidgetArea, series_dock);

  auto* stats_widget = new StatisticsWidget(model_, this);
  auto* stats_dock = new QDockWidget("Statistics", this);
  stats_dock->setObjectName("statistics_dock");
  stats_dock->setWidget(stats_widget);
  stats_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
  addDockWidget(Qt::LeftDockWidgetArea, stats_dock);

  tabifyDockWidget(numeric_dock, series_dock);
  tabifyDockWidget(series_dock, stats_dock);
  numeric_dock->raise();

  auto* file_menu = menuBar()->addMenu("File");
  file_menu->addAction("Save Config As...", this, &MainWindow::onSaveConfigAs);
  file_menu->addAction("Load Config...", this, &MainWindow::onLoadConfig);

  auto* view_menu = menuBar()->addMenu("View");
  view_menu->addAction(input_dock_->toggleViewAction());
  view_menu->addAction(title_line_dock_->toggleViewAction());
  view_menu->addAction(numeric_dock->toggleViewAction());
  view_menu->addAction(series_dock->toggleViewAction());
  view_menu->addAction(stats_dock->toggleViewAction());
  always_on_top_action_ = view_menu->addAction("Always on top");
  always_on_top_action_->setCheckable(true);
  connect(always_on_top_action_, &QAction::toggled, this, &MainWindow::onToggleAlwaysOnTop);

  auto* config_menu = menuBar()->addMenu("Config");
  config_menu->addAction("History Settings...", this, &MainWindow::onHistorySettings);
  config_menu->addAction("Open Current History Folder", this,
                         &MainWindow::onOpenHistoryFolder);

  history_status_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  clear_history_warning_button_->setFlat(true);
  clear_history_warning_button_->setVisible(false);
  connect(clear_history_warning_button_, &QPushButton::clicked, this,
          &MainWindow::onClearHistoryWarning);
  statusBar()->addPermanentWidget(history_status_label_, 1);
  statusBar()->addPermanentWidget(clear_history_warning_button_);

  setCentralWidget(new QWidget());

  connect(receiver_control_widget_, &ReceiverControlWidget::configurationChanged, this,
          &MainWindow::updateWindowIdentity);

  loadSettings();
  applyStartupOverrides();
  updateWindowIdentity();
  receiver_control_widget_->startReceiving();
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* event) {
  saveSettings();
  receiver_->stop();
  history_->shutdown();
  QMainWindow::closeEvent(event);
}

void MainWindow::updateWindowIdentity() {
  const QString identity = buildWindowIdentity();
  setWindowTitle(identity);
  QCoreApplication::setApplicationName(identity);
  const QString prefix = "qtconsole_";
  const QString display_title =
      identity.startsWith(prefix) ? identity.mid(prefix.size()) : identity;
  // Show the identity in the title-line body; the dock keeps its static
  // "Title line" name so the View-menu toggle stays labelled and the dock
  // has real content (a zero-height body failed to reappear when re-checked).
  title_line_widget_->setTitle(display_title);
}

void MainWindow::loadSettings() {
  QSettings bootstrap_settings = createBootstrapSettings();
  const QString last_identity = bootstrap_settings.value("last_identity", windowTitle()).toString();
  if (!last_identity.isEmpty()) {
    setWindowTitle(last_identity);
    QCoreApplication::setApplicationName(last_identity);
  }

  QSettings settings = createSettings();

  settings.beginGroup("main_window");
  const bool always_on_top = settings.value("always_on_top", false).toBool();
  const QByteArray geometry = settings.value("geometry").toByteArray();
  const QByteArray state = settings.value("state").toByteArray();
  settings.endGroup();

  if (always_on_top_action_->isChecked() != always_on_top) {
    always_on_top_action_->setChecked(always_on_top);
  } else {
    onToggleAlwaysOnTop(always_on_top);
  }

  numeric_ratio_widget_->loadSettings(&settings);
  receiver_control_widget_->loadSettings(&settings);
  time_series_widget_->loadSettings(&settings);
  history_->applyConfig(loadHistoryConfig(buildWindowIdentity()));
  settings.beginGroup("statistics");
  const bool statistics_running = settings.value("running", true).toBool();
  if (statistics_running) {
    model_->startStatistics();
  } else {
    model_->stopStatistics();
  }
  settings.endGroup();

  if (!state.isEmpty()) {
    restoreState(state);
  }
  if (!geometry.isEmpty()) {
    restoreGeometry(geometry);
    // Re-apply once after state restore to avoid size-hint growth on tiny windows.
    restoreGeometry(geometry);
  }
}

void MainWindow::saveSettings() const {
  QSettings settings = createSettings();

  settings.beginGroup("main_window");
  settings.setValue("always_on_top", always_on_top_action_->isChecked());
  settings.setValue("geometry", saveGeometry());
  settings.setValue("state", saveState());
  settings.endGroup();

  numeric_ratio_widget_->saveSettings(&settings);
  receiver_control_widget_->saveSettings(&settings);
  time_series_widget_->saveSettings(&settings);
  const QString history_id = history_->activeMeasurementId().isEmpty()
                                 ? buildWindowIdentity()
                                 : history_->activeMeasurementId();
  saveHistoryConfig(history_id, history_->config());
  settings.beginGroup("statistics");
  settings.setValue("running", model_->statisticsRunning());
  settings.endGroup();

  QSettings bootstrap_settings = createBootstrapSettings();
  bootstrap_settings.setValue("last_identity", windowTitle());
}

void MainWindow::applyStartupOverrides() {
  if (startup_options_.has_protocol) {
    receiver_control_widget_->setProtocol(startup_options_.protocol);
  }
  if (startup_options_.has_port) {
    receiver_control_widget_->setPort(startup_options_.port);
  }
  if (startup_options_.has_measurement_title) {
    receiver_control_widget_->setMeasurementTitle(startup_options_.measurementTitle);
  }
}

QString MainWindow::buildWindowIdentity() const {
  const QString protocol = receiver_control_widget_->receiverProtocolAbbrev();
  const int receiver_port = receiver_control_widget_->port();
  const QString measurementTitle = receiver_control_widget_->measurementTitle();
  if (measurementTitle.isEmpty()) {
    return QString("qtconsole_%1%2").arg(protocol).arg(receiver_port);
  }
  return QString("qtconsole_%1%2_%3").arg(protocol).arg(receiver_port).arg(measurementTitle);
}

QSettings MainWindow::createSettings() const {
#ifdef Q_OS_WIN
  return QSettings(QSettings::IniFormat, QSettings::UserScope, "kshu", windowTitle());
#else
  return QSettings(QSettings::NativeFormat, QSettings::UserScope, "kshu", windowTitle());
#endif
}

HistoryConfig MainWindow::loadHistoryConfig(const QString& measurement_id) const {
  HistoryConfig config{defaultHistoryRoot(), 30, 500000};
  QSettings settings = createBootstrapSettings();
  settings.beginGroup(QStringLiteral("history_profiles"));
  settings.beginGroup(historyProfileKey(measurement_id));
  config.archive_root =
      settings.value(QStringLiteral("archive_root"), config.archive_root).toString();
  config.flush_interval_sec =
      settings.value(QStringLiteral("flush_interval_sec"), config.flush_interval_sec).toInt();
  config.max_samples =
      settings.value(QStringLiteral("max_samples"), config.max_samples).toInt();
  settings.endGroup();
  settings.endGroup();
  return config;
}

void MainWindow::saveHistoryConfig(const QString& measurement_id,
                                   const HistoryConfig& config) const {
  QSettings settings = createBootstrapSettings();
  settings.beginGroup(QStringLiteral("history_profiles"));
  settings.beginGroup(historyProfileKey(measurement_id));
  settings.setValue(QStringLiteral("measurement_id"), measurement_id);
  settings.setValue(QStringLiteral("archive_root"), config.archive_root);
  settings.setValue(QStringLiteral("flush_interval_sec"), config.flush_interval_sec);
  settings.setValue(QStringLiteral("max_samples"), config.max_samples);
  settings.endGroup();
  settings.endGroup();
}

void MainWindow::onToggleAlwaysOnTop(bool enabled) {
  setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
  show();
}

void MainWindow::onHistorySettings() {
  const QString measurement_id = history_->activeMeasurementId().isEmpty()
                                     ? buildWindowIdentity()
                                     : history_->activeMeasurementId();
  const HistoryConfig config = history_->activeMeasurementId().isEmpty()
                                   ? loadHistoryConfig(measurement_id)
                                   : history_->config();
  HistorySettingsDialog dialog(measurement_id, config, history_, this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }
  const HistoryConfig updated = dialog.historyConfig();
  saveHistoryConfig(measurement_id, updated);
  if (measurement_id == history_->activeMeasurementId()) {
    history_->applyConfig(updated);
  }
}

void MainWindow::onOpenHistoryFolder() {
  QString directory;
  const QString current_file = history_->currentHistoryFile();
  if (!current_file.isEmpty()) {
    directory = QFileInfo(current_file).absolutePath();
  } else {
    const QString measurement_id = buildWindowIdentity();
    const HistoryConfig config = loadHistoryConfig(measurement_id);
    directory = config.archive_root;
  }
  QDir().mkpath(directory);
  QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
}

void MainWindow::onClearHistoryWarning() { history_->clearWarning(); }

void MainWindow::onHistoryStatusChanged(const QString& text, bool warning) {
  history_status_label_->setText(text);
  history_status_label_->setStyleSheet(
      warning ? QStringLiteral("QLabel { color: #d08020; }") : QString());
  clear_history_warning_button_->setVisible(warning);
}

void MainWindow::onReceiverStatusChanged(bool running, const QString&) {
  if (running) {
    const QString measurement_id = buildWindowIdentity();
    history_->applyConfig(loadHistoryConfig(measurement_id));
    history_->startSession(measurement_id);
  } else {
    history_->stopSession();
  }
}

void MainWindow::onSaveConfigAs() {
  const QString path = QFileDialog::getSaveFileName(this, "Save configuration",
                                                    windowTitle() + ".ini", "INI files (*.ini)");
  if (path.isEmpty()) {
    return;
  }

  QSettings file_settings(path, QSettings::IniFormat);
  file_settings.beginGroup("main_window");
  file_settings.setValue("geometry", saveGeometry());
  file_settings.setValue("state", saveState());
  file_settings.endGroup();

  numeric_ratio_widget_->saveSettings(&file_settings);
  receiver_control_widget_->saveSettings(&file_settings);
  time_series_widget_->saveSettings(&file_settings);
  file_settings.beginGroup("statistics");
  file_settings.setValue("running", model_->statisticsRunning());
  file_settings.endGroup();

  const QString measurement_id = buildWindowIdentity();
  const HistoryConfig history_config =
      measurement_id == history_->activeMeasurementId() ? history_->config()
                                                        : loadHistoryConfig(measurement_id);
  file_settings.beginGroup("history");
  file_settings.setValue("archive_root", history_config.archive_root);
  file_settings.setValue("flush_interval_sec", history_config.flush_interval_sec);
  file_settings.setValue("max_samples", history_config.max_samples);
  file_settings.endGroup();
}

void MainWindow::onLoadConfig() {
  const QString path =
      QFileDialog::getOpenFileName(this, "Load configuration", QString(), "INI files (*.ini)");
  if (path.isEmpty()) {
    return;
  }

  QSettings file_settings(path, QSettings::IniFormat);
  numeric_ratio_widget_->loadSettings(&file_settings);
  receiver_control_widget_->loadSettings(&file_settings);
  time_series_widget_->loadSettings(&file_settings);
  file_settings.beginGroup("statistics");
  const bool statistics_running = file_settings.value("running", true).toBool();
  if (statistics_running) {
    model_->startStatistics();
  } else {
    model_->stopStatistics();
  }
  file_settings.endGroup();
  updateWindowIdentity();

  const QString measurement_id = buildWindowIdentity();
  HistoryConfig history_config = loadHistoryConfig(measurement_id);
  file_settings.beginGroup("history");
  history_config.archive_root =
      file_settings.value("archive_root", history_config.archive_root).toString();
  history_config.flush_interval_sec =
      file_settings.value("flush_interval_sec", history_config.flush_interval_sec).toInt();
  history_config.max_samples =
      file_settings.value("max_samples", history_config.max_samples).toInt();
  file_settings.endGroup();
  saveHistoryConfig(measurement_id, history_config);
  if (measurement_id == history_->activeMeasurementId()) {
    history_->applyConfig(history_config);
  }

  file_settings.beginGroup("main_window");
  const QByteArray geometry = file_settings.value("geometry").toByteArray();
  if (!geometry.isEmpty()) {
    restoreGeometry(geometry);
  }
  const QByteArray state = file_settings.value("state").toByteArray();
  if (!state.isEmpty()) {
    restoreState(state);
  }
  file_settings.endGroup();
}

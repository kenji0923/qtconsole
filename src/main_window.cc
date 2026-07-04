#include "main_window.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDockWidget>
#include <QFileDialog>
#include <QMenuBar>
#include <QSettings>
#include <QWidget>

#include "data_receiver.h"
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
      receiver_(new DataReceiver(this)),
      title_line_widget_(nullptr),
      numeric_ratio_widget_(nullptr),
      receiver_control_widget_(nullptr),
      time_series_widget_(nullptr),
      input_dock_(nullptr),
      title_line_dock_(nullptr),
      always_on_top_action_(nullptr),
      startup_options_(startup_options) {
  setWindowTitle("qtconsole_UDP9000");
  resize(1100, 700);
  setDockNestingEnabled(true);

  connect(receiver_, &DataReceiver::sampleReceived, model_, &MeasurementModel::pushRawSample);

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

  time_series_widget_ = new TimeSeriesWidget(model_, this);
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

void MainWindow::onToggleAlwaysOnTop(bool enabled) {
  setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
  show();
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

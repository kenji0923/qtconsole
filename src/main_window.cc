#include "main_window.h"

#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>

#include "data_receiver.h"
#include "measurement_model.h"
#include "numeric_ratio_widget.h"
#include "receiver_control_widget.h"
#include "statistics_widget.h"
#include "time_series_widget.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), model_(new MeasurementModel(this)), receiver_(new DataReceiver(this)) {
  setWindowTitle("qtconsole");
  resize(1100, 700);
  setDockNestingEnabled(true);

  connect(receiver_, &DataReceiver::sample_received, model_, &MeasurementModel::push_sample);

  auto* receiver_control = new ReceiverControlWidget(receiver_, this);
  auto* control_dock = new QDockWidget("Input", this);
  control_dock->setWidget(receiver_control);
  control_dock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
  addDockWidget(Qt::TopDockWidgetArea, control_dock);

  auto* numeric_widget = new NumericRatioWidget(model_, this);
  auto* numeric_dock = new QDockWidget("Numeric / Ratio", this);
  numeric_dock->setWidget(numeric_widget);
  numeric_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
  addDockWidget(Qt::LeftDockWidgetArea, numeric_dock);

  auto* series_widget = new TimeSeriesWidget(model_, this);
  auto* series_dock = new QDockWidget("Time Series", this);
  series_dock->setWidget(series_widget);
  series_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
  addDockWidget(Qt::LeftDockWidgetArea, series_dock);

  auto* stats_widget = new StatisticsWidget(model_, this);
  auto* stats_dock = new QDockWidget("Statistics", this);
  stats_dock->setWidget(stats_widget);
  stats_dock->setAllowedAreas(Qt::AllDockWidgetAreas);
  addDockWidget(Qt::LeftDockWidgetArea, stats_dock);

  tabifyDockWidget(numeric_dock, series_dock);
  tabifyDockWidget(series_dock, stats_dock);
  numeric_dock->raise();

  auto* view_menu = menuBar()->addMenu("View");
  view_menu->addAction(numeric_dock->toggleViewAction());
  view_menu->addAction(series_dock->toggleViewAction());
  view_menu->addAction(stats_dock->toggleViewAction());

  auto* center_placeholder =
      new QLabel("Use the dock tabs to switch views. Drag a tab to undock or re-dock.", this);
  center_placeholder->setAlignment(Qt::AlignCenter);
  setCentralWidget(center_placeholder);
}

MainWindow::~MainWindow() = default;

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <QPainter>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QStringConverter>
#include <QTableWidgetItem>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QLinearGradient>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QValueAxis>
#include <QtDataVisualization/Q3DSurface>
#include <QtDataVisualization/QSurface3DSeries>
#include <QtDataVisualization/QSurfaceDataProxy>
#include <QtDataVisualization/Q3DTheme>
#include <QtConcurrent>
#include <QApplication>
#include <QtCharts/QDateTimeAxis>

double minX;
double maxX;
double minY;
double maxY;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 强行清除设计器自带的布局干扰
    if (ui->centralwidget->layout()) {
        delete ui->centralwidget->layout();
    }

    // 手动创建左 2 右 3 结构
    // 创建左侧垂直区域 (按钮 + 表格 + 2D图)
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(ui->btnImport3D);
    leftLayout->addWidget(ui->btnImport2D);
    leftLayout->addWidget(ui->btnExportData);
    leftLayout->addWidget(ui->btnDraw3D);
    leftLayout->addWidget(ui->btnDrawChart);
    leftLayout->addWidget(ui->tableWidget);
    leftLayout->addWidget(ui->chartContainer);
    
    // 设置最小高度
    ui->tableWidget->setMinimumHeight(250);
    ui->chartContainer->setMinimumHeight(250);

    // 创建右侧垂直区域 (只放 3D 容器)
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->addWidget(ui->view3DContainer);

    // 创建全局水平布局并绑定到 centralwidget
    QHBoxLayout *mainLayout = new QHBoxLayout(ui->centralwidget);
    mainLayout->addLayout(leftLayout, 2);  // 左侧占 2 份宽度
    mainLayout->addLayout(rightLayout, 3); // 右侧占 3 份宽度
    
    // 增加间距
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(20);

    // 表格与样式修复
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget->verticalHeader()->setVisible(false);
    ui->tableWidget->setFrameShape(QFrame::NoFrame);

    // 皮肤
    this->setStyleSheet(
        "QMainWindow, QWidget#centralwidget { background-color: #1e1e24; }"
        "QWidget#chartContainer, QWidget#view3DContainer { background-color: #1e1e24; border: 1px solid #3f4254; border-radius: 4px; }"
        "QWidget { color: #e0e0e0; font-family: 'Segoe UI', Arial; }"
        "QPushButton { background-color: #3a3f58; border: 1px solid #5c6380; border-radius: 6px; padding: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #4b5275; }"
        "QPushButton:pressed { background-color: #2b2f42; }"
        "QTableWidget { background-color: #2b2d3a; alternate-background-color: #323544; gridline-color: #3f4254; border: none; }"
        "QHeaderView::section { background-color: #3a3f58; border: 1px solid #2b2d3a; padding: 6px; font-weight: bold; }"
    );

    // 绑定信号
    connect(&m_watcher3D, &QFutureWatcher<GridResult>::finished, this, &MainWindow::on_Draw3D_finished);
}

MainWindow::~MainWindow()
{
    if (m_watcher3D.isRunning()) {
        m_watcher3D.waitForFinished();
    }
    
    delete ui;
}

// 刷新监测报表（仅显示时间、水位、瓦斯、应力）
void MainWindow::refreshTable() {
    ui->tableWidget->clear();
    ui->tableWidget->setRowCount(static_cast<int>(m_monitoringData.size()));
    ui->tableWidget->setColumnCount(4);
    ui->tableWidget->setHorizontalHeaderLabels({"时间", "钻孔水位(m)", "瓦斯浓度(%)", "岩层应力(MPa)"});

    for (int i = 0; i < static_cast<int>(m_monitoringData.size()); ++i) {
        ui->tableWidget->setItem(i, 0, new QTableWidgetItem(m_monitoringData[i].time));
        ui->tableWidget->setItem(i, 1, new QTableWidgetItem(QString::number(m_monitoringData[i].waterLevel, 'f', 1)));
        ui->tableWidget->setItem(i, 2, new QTableWidgetItem(QString::number(m_monitoringData[i].gas, 'f', 2)));
        ui->tableWidget->setItem(i, 3, new QTableWidgetItem(QString::number(m_monitoringData[i].stress, 'f', 1)));
    }
}

// 导入 2D 监测数据
void MainWindow::on_btnImport2D_clicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "选择监测数据", "", "CSV (*.csv)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "错误", "无法打开文件，请检查文件是否被其他程序占用。");
        return;
    }

    m_monitoringData.clear();
    QTextStream in(&file);
    // 自动检测编码，防止中文乱码
    in.setEncoding(QStringConverter::Utf8); 

    QString header = in.readLine(); 
    int lineNum = 1;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList cols = line.split(',');
    
        if (cols.size() >= 4) {
            bool ok1;
            double wl = cols[1].toDouble(&ok1);
             
            QDateTime dt = QDateTime::fromString(cols[0], "yyyy/M/d H:mm");

            if (!dt.isValid()) {
                dt = QDateTime::fromString(cols[0], "yyyy-MM-dd HH:mm");
            }
    
            if (ok1 && dt.isValid()) { // 只要水位和时间有效就保留 
                MonitoringData data;
                data.time = cols[0];
                data.dateTime = dt;
                data.waterLevel = wl;
                
                // 容错处理缺失值 
                bool okG, okS;
                data.gas = cols[2].toDouble(&okG) ? cols[2].toDouble() : std::numeric_limits<double>::quiet_NaN();
                data.stress = cols[3].toDouble(&okS) ? cols[3].toDouble() : std::numeric_limits<double>::quiet_NaN();
    
                m_monitoringData.push_back(data);
            }
        }
    }
    
    if (m_monitoringData.empty()) {
        QMessageBox::warning(this, "导入失败", "未从文件中识别到有效数据。");
    } else {
        refreshTable(); 
    }
}

// 绘制水位折线图
void MainWindow::on_btnDrawChart_clicked() {
    if (m_monitoringData.empty()) return;

    QLineSeries *series = new QLineSeries();
    series->setName("水位 (m)");

    for (const auto& data : m_monitoringData) {
        // 将 QDateTime 转换为毫秒时间戳作为 X 轴坐标 
        series->append(data.dateTime.toMSecsSinceEpoch(), data.waterLevel);
    }

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTheme(QChart::ChartThemeDark);
    chart->setBackgroundBrush(QColor("#1e1e24"));

    // 配置时间轴 X
    QDateTimeAxis *axisX = new QDateTimeAxis();
    axisX->setFormat("MM-dd HH:mm");
    axisX->setTitleText("监测时间");
    axisX->setTickCount(5);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    // 配置水位轴 Y
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("水位 (m)");
    axisY->setLabelFormat("%.2f");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    // 自动计算时间范围
    axisX->setRange(m_monitoringData.front().dateTime, m_monitoringData.back().dateTime);

    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    // 渲染到容器 
    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(ui->chartContainer->layout());
    if (!layout) layout = new QVBoxLayout(ui->chartContainer);
    clearLayout(layout);
    layout->addWidget(chartView);
}

// 导入 3D 空间数据
void MainWindow::on_btnImport3D_clicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "选择勘察数据", "", "CSV (*.csv)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;
    QTextStream in(&file);

    m_spatialData.clear();
    in.readLine(); // 跳过表头

    while (!in.atEnd()) {
        QStringList cols = in.readLine().split(',');
        if (cols.size() >= 4) {
            bool okX, okY, okZ;
            double x = cols[1].toDouble(&okX);
            double y = cols[2].toDouble(&okY);
            double z = cols[3].toDouble(&okZ);

            // 过滤掉经纬度或坐标为 0 的异常点
            if (okX && okY && okZ) {
                m_spatialData.push_back({x, y, z});
            }
        }
    }

    // 检查数据量是否满足插值要求(至少三组)
    if (m_spatialData.size() < 3) {
        QMessageBox::critical(this, "数据不足", "空间数据点少于 3 个，无法进行面状建模。");
        return;
    }

    try {
        m_adaptor = std::make_unique<PointCloudAdaptor>(m_spatialData);
        m_kdTree = std::make_unique<KDTree2D>(2, *m_adaptor, nanoflann::KDTreeSingleIndexAdaptorParams(10));
        m_kdTree->buildIndex(); 
        QMessageBox::information(this, "成功", QString("已成功构建 KD-Tree，加载点数：%1").arg(m_spatialData.size()));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "算法异常", QString("KD-Tree 构建失败: %1").arg(e.what()));
    }
}

// IDW 算法
double MainWindow::calculateIDW(double queryX, double queryY) const {
    if (!m_kdTree || m_spatialData.empty()) return 0.0;

    const double S2 = 0.25;          // 平滑系数
    const size_t num_results = 15;   // K值：只找最近的 15 个钻孔
    
    // 如果导入的数据总共都不够 15 个，就按实际数量取
    size_t actual_results = std::min(num_results, m_spatialData.size());
    
    // 准备容器接收查询结果：一个是点的索引，一个是距离的平方
    std::vector<uint32_t> ret_index(actual_results);
    std::vector<double> out_dist_sqr(actual_results);

    // 将要查询的坐标点打包成数组
    double query_pt[2] = {queryX, queryY};
    
    actual_results = m_kdTree->knnSearch(&query_pt[0], actual_results, &ret_index[0], &out_dist_sqr[0]);

    double weightedSum = 0.0, weightTotal = 0.0;

    // 遍历15个最近的点
    for (size_t i = 0; i < actual_results; ++i) {
        double d2 = out_dist_sqr[i];
        
        // 网格刚好落在真实钻孔上，距离极小，直接返回真实值
        if (d2 < 1e-6) return m_spatialData[ret_index[i]].z; 
        
        double w = 1.0 / (d2 + S2);
        weightedSum += w * m_spatialData[ret_index[i]].z;
        weightTotal += w;
    }
    
    return (weightTotal < 1e-12) ? 0.0 : (weightedSum / weightTotal);
}

// 渲染3D地形
void MainWindow::on_btnDraw3D_clicked() {
    if (m_spatialData.empty()) {
        QMessageBox::warning(this, "提示", "请先导入 3D 空间坐标数据！");
        return;
    }
    setUIEnabled(false);

    auto minXIt = std::min_element(m_spatialData.begin(), m_spatialData.end(),
    [](const SpatialData &p1, const SpatialData &p2) { return p1.x < p2.x; });
    auto maxXIt = std::max_element(m_spatialData.begin(), m_spatialData.end(),
    [](const SpatialData &p1, const SpatialData &p2) { return p1.x < p2.x; });
    auto minYIt = std::min_element(m_spatialData.begin(), m_spatialData.end(),
    [](const SpatialData &p1, const SpatialData &p2) { return p1.y < p2.y; });
    auto maxYIt = std::max_element(m_spatialData.begin(), m_spatialData.end(),
    [](const SpatialData &p1, const SpatialData &p2) { return p1.y < p2.y; });

    minX = minXIt->x;
    maxX = maxXIt->x;
    minY = minYIt->y;
    maxY = maxYIt->y;

    const double step = 0.5;

    QFuture<GridResult> future = QtConcurrent::run([this, step]() -> GridResult {
        int countX = static_cast<int>((maxX - minX) / step) + 1;
        int countY = static_cast<int>((maxY - minY) / step) + 1;

        auto *dataArray = new QSurfaceDataArray();
        double minZ = 1e10, maxZ = -1e10;

        for (int yi = 0; yi < countY; ++yi) {
            auto *row = new QSurfaceDataRow(countX);
            double gy = minY + yi * step;
            for (int xi = 0; xi < countX; ++xi) {
                double gx = minX + xi * step;
                double gz = this->calculateIDW(gx, gy);
                minZ = std::min(minZ, gz);
                maxZ = std::max(maxZ, gz);
                (*row)[xi].setPosition(QVector3D(gx, gz, gy));
            }
            dataArray->append(row);
        }
        return {dataArray, minZ, maxZ};
    });
    m_watcher3D.setFuture(future);
}

void MainWindow::on_Draw3D_finished() {
    setUIEnabled(true);
    GridResult res = m_watcher3D.result();
    Q3DSurface *surface = new Q3DSurface();
    QSurface3DSeries *series = new QSurface3DSeries();
    series->dataProxy()->resetArray(res.dataArray);
    
    QLinearGradient gradient;
    gradient.setColorAt(0.0, Qt::blue); gradient.setColorAt(0.5, Qt::green); gradient.setColorAt(1.0, Qt::red);
    series->setBaseGradient(gradient);
    series->setColorStyle(Q3DTheme::ColorStyleRangeGradient);
    surface->addSeries(series);
    surface->activeTheme()->setType(Q3DTheme::ThemeArmyBlue);

    QVBoxLayout *layout3D = qobject_cast<QVBoxLayout *>(ui->view3DContainer->layout());
    if (!layout3D) {
        layout3D = new QVBoxLayout(ui->view3DContainer);
        layout3D->setContentsMargins(0, 0, 0, 0);
    }
    clearLayout(layout3D);

    QWidget *container = QWidget::createWindowContainer(surface);
    layout3D->addWidget(container);
}

// 导出网格数据
void MainWindow::on_btnExportData_clicked() {
    if (m_spatialData.empty()) return;
    QString path = QFileDialog::getSaveFileName(this, "导出网格", "", "CSV (*.csv)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream out(&file);
        out << "X,Y,Z\n";
        for(double y = minY; y <= maxY; y += 5) {
            for(double x = minX; x <= maxX; x += 5) {
                out << x << "," << y << "," << calculateIDW(x,y) << "\n";
            }
        }
    }
}

// 辅助工具函数
void MainWindow::clearLayout(QLayout* layout) {
    if (!layout) return;
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

void MainWindow::setUIEnabled(bool e) {
    ui->btnDraw3D->setEnabled(e);
    ui->btnDraw3D->setText(e ? "渲染 3D 地形" : "正在推算中...");
}
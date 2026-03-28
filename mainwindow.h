#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <vector>
#include <QString>
#include <QFutureWatcher>
#include <QtDataVisualization/QSurfaceDataProxy>
#include <QDateTime>

#include <memory>
#include "nanoflann.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// 2D 监测数据
struct MonitoringData {
    QString time;
    QDateTime dateTime;
    double waterLevel; // 钻孔水位(m)
    double gas;        // 瓦斯浓度(%)
    double stress;     // 岩层应力(MPa)
};

// 3D 建模的空间数据
struct SpatialData {
    double x;
    double y;
    double z; // 高程/水位值
};

// 用于多线程返回 3D 渲染所需的数据阵列和极值
struct GridResult {
    QSurfaceDataArray* dataArray;
    double minZ;
    double maxZ;
};

struct PointCloudAdaptor {
    const std::vector<SpatialData>& pts;
    PointCloudAdaptor(const std::vector<SpatialData>& pts) : pts(pts) { }

    // 返回数据量
    inline size_t kdtree_get_point_count() const { return pts.size(); }

    // 返回特定索引下的 X (dim=0) 或 Y (dim=1) 坐标
    inline double kdtree_get_pt(const size_t idx, const size_t dim) const {
        if (dim == 0) return pts[idx].x;
        else return pts[idx].y;
    }

    // BBox 边界框计算（不需要，直接返回 false）
    template <class BBOX>
    bool kdtree_get_bbox(BBOX& /*bb*/) const { return false; }
};

// 2D KD-Tree
typedef nanoflann::KDTreeSingleIndexAdaptor<
    nanoflann::L2_Simple_Adaptor<double, PointCloudAdaptor>,
    PointCloudAdaptor,
    2
> KDTree2D;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_btnImport2D_clicked();
    void on_btnImport3D_clicked();
    void on_btnDrawChart_clicked();
    void on_btnDraw3D_clicked();
    void on_btnExportData_clicked();
    void on_Draw3D_finished();

private:
    Ui::MainWindow *ui;

    // 分离的数据存储容器
    std::vector<MonitoringData> m_monitoringData; 
    std::vector<SpatialData> m_spatialData;    

    std::unique_ptr<PointCloudAdaptor> m_adaptor;
    std::unique_ptr<KDTree2D> m_kdTree;

    QFutureWatcher<GridResult> m_watcher3D;

    void clearLayout(QLayout* layout);
    double calculateIDW(double queryX, double queryY) const;
    void setUIEnabled(bool enabled);
    void refreshTable();
};
#endif // MAINWINDOW_H
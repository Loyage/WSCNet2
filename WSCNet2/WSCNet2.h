#pragma once

#include "ui_WSCNet2.h"

#include <QStringList>
#include <QtWidgets/QWidget>
#include <opencv2/opencv.hpp>
#include "myqlabel.h"
#include "countDropletsThread.h"
#include "includes/nlohmann/json.hpp"

typedef std::pair<cv::Point2f, float> CIRCLE; //标注圆数据类型，包括圆心，半径
typedef std::pair<CIRCLE, int> dropType;  //液滴数据类型：包括标注圆和类别

class dropInfo
{
public:
    std::string imgName;
    int dropNumber;
    int nonYeast;
    int singleYeast;
    int multipleYeast;
    float singleRate;
    float singleAll;
    float meanDiameter;
    float cvValue;
};

class WSCNet2 : public QWidget
{
    Q_OBJECT

public:
    WSCNet2(QWidget *parent = nullptr);
    ~WSCNet2();

private slots:
    void on_pushButton_chooseWorkPlace_clicked();

    void on_pushButton_chooseImage_clicked();

    void on_lineEdit_imageName_textChanged(const QString &text);

    void on_pushButton_chooseLabelText_clicked();

    void on_lineEdit_labelText_textChanged(const QString &text);

    void on_pushButton_countDroplets_clicked();

    void on_pushButton_clearMessage_clicked();

    void on_pushButton_saveParams_clicked();

    void on_pushButton_saveResult_clicked();

    void on_pushButton_quit_clicked();

    void on_toolButton_lastImg_clicked();

    void on_toolButton_nextImg_clicked();

    void on_toolButton_zoomIn_clicked();

    void on_toolButton_zoomReset_clicked();

    void on_toolButton_zoomOut_clicked();

    void on_checkBox_editImage_clicked();

    void on_checkBox_manual_clicked();

    void on_checkBox_deleteDrops_clicked();

    void on_checkBox_light_clicked();

    void on_comboBox_function_currentIndexChanged(int index);

    void radiusModify(int value);  //半径偏差显示

    void filterByMinRadius(int value); //最小半径过滤

    void filterByMaxRadius(int value);//最大半径过滤

    void paintCircle(float centerX, float centerY, float radius); //手动画圆后实时更新槽函数

    void deleteCircle(float x, float y);

    void onThreadFinished(); //线程结束

private:
    Ui::WSCNet2Class ui;
    countDropletsThread* count_thread = nullptr; // 液滴计数线程
    Myqlabel* myqlabel_showImg; // 用于显示图片的label

    int m_edit_flag;            // 编辑模式下不同参数修改的m_edit_flag
    int m_radius_modify_record; // 半径偏差记录
    float m_zoom;               // 用来存储图像缩放值
    bool m_is_contrast;         // 对比度调节标志位

    cv::Mat m_cur_img;          // 当前处理的图像mat
    cv::Mat m_cur_img_display;  // 用于显示标注的图像

    std::vector<dropType> m_circle_result; // 存储在识别模式下的circle结果和编辑模式下读取txt的结果
    std::vector<std::vector<dropType>> m_accu_circle_results; // 存储在编辑模式下的circle暂时结果std::vector
    std::vector<dropType> m_edit_circle_result; // 存储在编辑模式下的circle修改后的结果
    std::vector<dropInfo> m_droplet_info;

    QString m_workplace_path;       // 工作地址
    QString m_file_chosen_path;     // 图片地址
    QString m_label_chosen_path;    // 标注地址
    QString m_label_save_file_path; // 标注文件所在文件夹地址
    QString m_img_save_file_path;   // 图片结果保存文件夹地址
    QString m_default_message;      // 默认信息
    QStringList m_img_name_list;    // 图片名列表
    const QString mk_label_res_folder_name; // 标注存储文件夹名
    const QString mk_img_res_folder_name;   // 图片存储文件夹名
    const cv::Scalar mk_color_single_drop;  // 单包液滴circle颜色
    const cv::Scalar mk_color_multi_drop;   // 多包液滴circle颜色
    const cv::Scalar mk_color_empty_drop;   // 非包液滴circle颜色

    cv::Mat QImageToMat(QImage image);
    QImage MatToQImage(const cv::Mat& mat);
    auto checkFileType(const QString& file_path) -> countDropletsThread::ECountMode; // 检查文件类型
    void imgDisplay(std::vector<dropType> circles); // 标注修改后更新图像显示
    void loadParamsFromJson(); // 从json文件中读取参数
    void savePramsToJson(); // 保存参数到json文件
    void updateComponentAvailability(); // 更新组件可用性
};

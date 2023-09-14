#pragma once

#include "ui_WSCNet2.h"

#include <QStringList>
#include <QtWidgets/QWidget>
#include <opencv2/opencv.hpp>
#include "myqlabel.h"
#include "countDropletsThread.h"
#include "includes/nlohmann/json.hpp"

typedef std::pair<cv::Point2f, float> CIRCLE; //��עԲ�������ͣ�����Բ�ģ��뾶
typedef std::pair<CIRCLE, int> dropType;  //Һ���������ͣ�������עԲ�����

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

    void radiusModify(int value);  //�뾶ƫ����ʾ

    void filterByMinRadius(int value); //��С�뾶����

    void filterByMaxRadius(int value);//���뾶����

    void paintCircle(float centerX, float centerY, float radius); //�ֶ���Բ��ʵʱ���²ۺ���

    void deleteCircle(float x, float y);

    void onThreadFinished(); //�߳̽���

private:
    Ui::WSCNet2Class ui;
    countDropletsThread* count_thread = nullptr; // Һ�μ����߳�
    Myqlabel* myqlabel_showImg; // ������ʾͼƬ��label

    int m_edit_flag;            // �༭ģʽ�²�ͬ�����޸ĵ�m_edit_flag
    int m_radius_modify_record; // �뾶ƫ���¼
    float m_zoom;               // �����洢ͼ������ֵ
    bool m_is_contrast;         // �Աȶȵ��ڱ�־λ

    cv::Mat m_cur_img;          // ��ǰ�����ͼ��mat
    cv::Mat m_cur_img_display;  // ������ʾ��ע��ͼ��

    std::vector<dropType> m_circle_result; // �洢��ʶ��ģʽ�µ�circle����ͱ༭ģʽ�¶�ȡtxt�Ľ��
    std::vector<std::vector<dropType>> m_accu_circle_results; // �洢�ڱ༭ģʽ�µ�circle��ʱ���std::vector
    std::vector<dropType> m_edit_circle_result; // �洢�ڱ༭ģʽ�µ�circle�޸ĺ�Ľ��
    std::vector<dropInfo> m_droplet_info;

    QString m_workplace_path;       // ������ַ
    QString m_file_chosen_path;     // ͼƬ��ַ
    QString m_label_chosen_path;    // ��ע��ַ
    QString m_label_save_file_path; // ��ע�ļ������ļ��е�ַ
    QString m_img_save_file_path;   // ͼƬ��������ļ��е�ַ
    QString m_default_message;      // Ĭ����Ϣ
    QStringList m_img_name_list;    // ͼƬ���б�
    const QString mk_label_res_folder_name; // ��ע�洢�ļ�����
    const QString mk_img_res_folder_name;   // ͼƬ�洢�ļ�����
    const cv::Scalar mk_color_single_drop;  // ����Һ��circle��ɫ
    const cv::Scalar mk_color_multi_drop;   // ���Һ��circle��ɫ
    const cv::Scalar mk_color_empty_drop;   // �ǰ�Һ��circle��ɫ

    cv::Mat QImageToMat(QImage image);
    QImage MatToQImage(const cv::Mat& mat);
    auto checkFileType(const QString& file_path) -> countDropletsThread::ECountMode; // ����ļ�����
    void imgDisplay(std::vector<dropType> circles); // ��ע�޸ĺ����ͼ����ʾ
    void loadParamsFromJson(); // ��json�ļ��ж�ȡ����
    void savePramsToJson(); // ���������json�ļ�
    void updateComponentAvailability(); // �������������
};

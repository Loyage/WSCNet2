#include "WSCNet2.h"

#ifdef WIN32
#pragma execution_character_set("utf-8")
#endif

#include <direct.h>
#include <fstream>
#include <QDebug>
#include <QAction>
#include <QDateTime>
#include <QValidator>
#include <QSettings>
#include <QTextCodec>
#include <QMessageBox>
#include <QFileDialog>
#include "includes/nlohmann/json.hpp"

using namespace cv;
using namespace std;
using json = nlohmann::json;

WSCNet2::WSCNet2(QWidget *parent)
    : QWidget(parent),
    mk_label_res_folder_name("textResult"),
    mk_img_res_folder_name("imgResult"),
    mk_color_empty_drop(Scalar(0, 0, 255)),
    mk_color_single_drop(Scalar(255, 0, 0)),
    mk_color_multi_drop(Scalar(0, 255, 0))
{
    ui.setupUi(this);

    m_edit_flag = 0;
    m_zoom = 1.0;  //显示图像缩放初始化
    m_is_contrast = 0; //对比度调节初始化
    m_file_chosen_path = "";
    m_default_message = ui.textEdit_informationOutput->toHtml(); // Message初始信息

    myqlabel_showImg = new Myqlabel();

    ui.spinBox_kernelSize->setRange(3, 11);
    ui.groupBox_params->setEnabled(false);
    ui.pushButton_chooseImage->setEnabled(false);
    ui.pushButton_chooseLabelText->setEnabled(false);
    ui.pushButton_countDroplets->setEnabled(false);
    ui.pushButton_chooseModule->setEnabled(false);
    ui.horizontalSlider_maxRadius->setEnabled(false);
    ui.horizontalSlider_minRadius->setEnabled(false);
    ui.horizontalSlider_radiusModify->setEnabled(false);
    ui.checkBox_editImage->setEnabled(false);

    // 编辑模式下参数修改滚轮与按钮同步
    connect(ui.horizontalSlider_radiusModify, SIGNAL(valueChanged(int)), ui.spinBox_radiusModify, SLOT(setValue(int)));
    connect(ui.horizontalSlider_minRadius, SIGNAL(valueChanged(int)), ui.spinBox_minRadius, SLOT(setValue(int)));
    connect(ui.horizontalSlider_maxRadius, SIGNAL(valueChanged(int)), ui.spinBox_maxRadius, SLOT(setValue(int)));

    // 编辑模式下修改参数实时显示图像
    connect(ui.horizontalSlider_radiusModify, SIGNAL(valueChanged(int)), this, SLOT(radiusModify(int)));
    connect(ui.horizontalSlider_minRadius, SIGNAL(valueChanged(int)), this, SLOT(filterByMinRadius(int)));
    connect(ui.horizontalSlider_maxRadius, SIGNAL(valueChanged(int)), this, SLOT(filterByMaxRadius(int)));

    // 手动画圆后实时显示图像
    connect(myqlabel_showImg, SIGNAL(outputCircle(float, float, float, int)), this, SLOT(paintCircle(float, float, float, int)));
    connect(myqlabel_showImg, SIGNAL(outputDrag(float, float)), this, SLOT(dragCircle(float, float)));

    // 添加“清空按钮”
    QLineEdit* pLineEdit = ui.lineEdit_imageName;
    auto clearActionImage = new QAction;
    clearActionImage->setIcon(QApplication::style()->standardIcon(QStyle::SP_LineEditClearButton));
    pLineEdit->addAction(clearActionImage, QLineEdit::TrailingPosition);
    connect(clearActionImage, &QAction::triggered, pLineEdit, [pLineEdit] { pLineEdit->setText(""); });

    auto clearActionLabel = new QAction;
    clearActionLabel->setIcon(QApplication::style()->standardIcon(QStyle::SP_LineEditClearButton));
    pLineEdit = ui.lineEdit_labelText;
    pLineEdit->addAction(clearActionLabel, QLineEdit::TrailingPosition);
    connect(clearActionLabel, &QAction::triggered, pLineEdit, [pLineEdit] { pLineEdit->setText(""); });

    auto clearActionModule = new QAction;
    clearActionModule->setIcon(QApplication::style()->standardIcon(QStyle::SP_LineEditClearButton));
    pLineEdit = ui.lineEdit_moduleName;
    pLineEdit->addAction(clearActionModule, QLineEdit::TrailingPosition);
    connect(clearActionModule, &QAction::triggered, pLineEdit, [pLineEdit] { pLineEdit->setText(""); });

    // 如果没有配置文件，则创建配置文件
    if (!QFile::exists("./WSCNet2.ini"))
    {
        QSettings settings("./WSCNet2.ini", QSettings::IniFormat);
        settings.setIniCodec(QTextCodec::codecForName("UTF-8"));
        settings.setValue("PATH/last_workplace", "C:/");
        settings.setValue("General/language", "zh-cn");
        settings.sync();
    }
    
    // 读取上次软件关闭时工作地址
    QSettings settings("./WSCNet2.ini", QSettings::IniFormat);
    m_workplace_path = settings.value("PATH/last_workplace").toString();
    // 加载默认语言配置
    m_translator = new QTranslator;
    if (settings.value("General/language").toString() == "en")
    {
        m_translator->load("./Translate_EN.qm");
		qApp->installTranslator(m_translator);
        ui.comboBox_language->setCurrentIndex(1);
	}
}

WSCNet2::~WSCNet2()
{
    delete myqlabel_showImg;
    delete m_translator;
}

void WSCNet2::on_pushButton_chooseWorkPlace_clicked()
{
    QString workPlaceAddress = QFileDialog::getExistingDirectory(this, "workPlace", m_workplace_path);
    if (!workPlaceAddress.size()) // 防止取消选择
        return;

    ui.lineEdit_workPlace->setText(workPlaceAddress + "/");
    ui.groupBox_params->setEnabled(true);

    if (workPlaceAddress != m_workplace_path)  //如果地址跟上次不一样，则清除图名，label名，重置工作空间地址
    {
        ui.lineEdit_imageName->clear();
        ui.lineEdit_labelText->clear();
        m_workplace_path = workPlaceAddress + "/";
        m_label_save_file_path = m_workplace_path + mk_label_res_folder_name; //标注文件文件夹地址
        m_img_save_file_path = m_workplace_path + mk_img_res_folder_name; //图像结果存储文件夹地址
        ui.textEdit_informationOutput->append("The work place is set to " + m_workplace_path + "\n");//输出
        m_circle_result.clear();
        imgDisplay(m_circle_result);

        // 保存工作地址
        QSettings settings("./WSCNet2.ini", QSettings::IniFormat);
        settings.setValue("PATH/last_workplace", m_workplace_path);
        settings.sync();
    }

    // 读取文件夹下所有图片
    QDir dir(m_workplace_path);
    QStringList nameFilters;
    nameFilters << "*.bmp" << "*.jpg" << "*.png" << "*.jpeg";
    m_img_name_list = dir.entryList(nameFilters, QDir::Files | QDir::Readable, QDir::Name);
    if (m_img_name_list.size())
    {
        ui.textEdit_informationOutput->append("The work place has " + QString::number(m_img_name_list.size()) + " images.");//输出
    }
    else
    {
        ui.textEdit_informationOutput->append("<font color=\"#FF8000\">WARNING</font> The work place has no images.");//输出
    }

    loadParamsFromJson();
    updateComponentAvailability(); // 更新组件可用性
}

void WSCNet2::on_pushButton_chooseImage_clicked()
{
    QString workPlaceAddress = ui.lineEdit_workPlace->text();
    m_file_chosen_path = QFileDialog::getOpenFileName(this, "File Dialog", workPlaceAddress, "image file(*bmp *jpg *png *)");
    QString file_path = QFileInfo(m_file_chosen_path).path() + "/";

    if (m_file_chosen_path.size()) // 防止取消选择
    {
        if (file_path != workPlaceAddress) // 防止图片不在当前文件夹
        {
            QMessageBox::warning(this, "Warning", tr("图像不在当前文件夹，请重新选择！"), QMessageBox::Ok);
            return;
        }

        if (checkFileType(m_file_chosen_path) == dropRecgThread::ECountMode::COUNT_UNKNOWN)
        {
            QMessageBox::warning(this, "ERROR", tr("未知的图片类型"), QMessageBox::Ok);
            m_file_chosen_path = "";
        }
        else
        {
            string imageNameS = m_file_chosen_path.toStdString().substr(workPlaceAddress.size(), m_file_chosen_path.size() - workPlaceAddress.size());
            QString imageName = QString::fromStdString(imageNameS);
            ui.textEdit_informationOutput->append("The image/video is set to " + imageName);//输出
            ui.lineEdit_imageName->setText(imageName);
        }
    }
}

void WSCNet2::on_lineEdit_imageName_textChanged(const QString& text)
{
    m_file_chosen_path = ui.lineEdit_workPlace->text() + text;
    if (!text.size()) // 清空
    {
        ui.textEdit_informationOutput->append("Clear chosen image.");//输出
        ui.lineEdit_labelText->clear();
        m_cur_img.release();
    }
    else
    {
        auto file_type = checkFileType(m_file_chosen_path);
        // 未知文件类型
        if (file_type == dropRecgThread::ECountMode::COUNT_UNKNOWN)
        {
            ui.textEdit_informationOutput->append("<font color=\"#FF0000\">ERROR</font> Unkown file type！");
            return;
        }
        // 图片地址
        else if (file_type == dropRecgThread::ECountMode::COUNT_IMG)
        {
            QImage currentImage;
            currentImage.load(m_file_chosen_path);

            m_cur_img = QImageToMat(currentImage);  //传递给mat
            if (1 == m_cur_img.channels())  //保证mat中存储3通道图
                cvtColor(m_cur_img, m_cur_img, COLOR_GRAY2BGR);
            if (4 == m_cur_img.channels())
                cvtColor(m_cur_img, m_cur_img, COLOR_BGRA2BGR);

            m_circle_result.clear();
            m_accu_circle_results.clear();
            m_edit_circle_result.clear();
            ui.checkBox_manual->setChecked(false);
            ui.checkBox_deleteDrops->setChecked(false);

            // try load modified label file
            QString img_name = QString::fromStdString(text.toStdString().substr(0, text.toStdString().find_last_of(".")));
            QString modified_label_add = m_label_save_file_path + "/" + img_name + "_modified.txt";
            QString label_add = m_label_save_file_path + "/" + img_name + "_drops.txt";
            ifstream modified_label_file(modified_label_add.toStdString());
            ifstream label_file(label_add.toStdString());
            if (modified_label_file.good())
            {
                modified_label_file.close();
                ui.lineEdit_labelText->setText(img_name + "_modified.txt");
                ui.textEdit_informationOutput->append("Load modified label file.");
            }
            else if (label_file.good())// else load generated label file
            {
                label_file.close();
                ui.lineEdit_labelText->setText(img_name + "_drops.txt");
                ui.textEdit_informationOutput->append("Load generated label file.");
            }
            else
            {
                ui.textEdit_informationOutput->append("<font color=\"#FF8000\">WARNING</font> No label file found. \n");
                imgDisplay(m_circle_result);
            }
        }
        // 视频地址
        else if (file_type == dropRecgThread::ECountMode::COUNT_VIDEO)
        {
            m_circle_result.clear();
            m_accu_circle_results.clear();
            m_edit_circle_result.clear();
            ui.checkBox_manual->setChecked(false);
            ui.checkBox_deleteDrops->setChecked(false);
        }
    }

    updateComponentAvailability(); // 更新组件可用性
}

void WSCNet2::imgDisplay(vector<dropType> circles)
{
    if (m_cur_img.empty())
    {
        ui.textEdit_informationOutput->append("current image is empty!");
        myqlabel_showImg->clear();
        return;
    }

    m_cur_img.copyTo(m_cur_img_display);
    Mat showImg;
    m_cur_img_display.copyTo(showImg);

    if (m_is_contrast)
    {
        cvtColor(showImg, showImg, COLOR_BGR2GRAY);
        equalizeHist(showImg, showImg);
        cvtColor(showImg, showImg, COLOR_GRAY2BGR);
    }

    Scalar showColor;

    for (int i = 0; i != circles.size(); i++)
    {
        if (0 == circles[i].second)
        {
            showColor = mk_color_empty_drop;
        }
        else if (1 == circles[i].second)
        {
            showColor = mk_color_single_drop;
        }
        else if (2 == circles[i].second)
        {
            showColor = mk_color_multi_drop;
        }
        else if (-1 == circles[i].second)//非液滴
        {
            continue;
        }

        circle(m_cur_img_display, circles[i].first.first, circles[i].first.second, showColor, 1);
        circle(showImg, circles[i].first.first, circles[i].first.second, showColor, 1);
    }

    cv::resize(showImg, showImg, Size(showImg.cols * m_zoom, showImg.rows * m_zoom));


    myqlabel_showImg->clear();
    QImage currentImage = MatToQImage(showImg);
    myqlabel_showImg->setAlignment(Qt::AlignCenter);
    myqlabel_showImg->setPixmap(QPixmap::fromImage(currentImage));
    myqlabel_showImg->resize(QSize(currentImage.width(), currentImage.height()));
    ui.scrollArea->setWidget(myqlabel_showImg);  //图片过大加上滚轮
}

void WSCNet2::on_pushButton_chooseLabelText_clicked()
{
    m_cur_img.copyTo(m_cur_img_display);

    QString labelFileAddress = ui.lineEdit_workPlace->text() + mk_label_res_folder_name + "/";
    m_label_chosen_path = QFileDialog::getOpenFileName(this, "File Dialog", labelFileAddress, "Label file(*txt)");
    if (m_label_chosen_path.size()) // 防止用户取消选择
    {
        string textNameS = m_label_chosen_path.toStdString().substr(labelFileAddress.size(), m_label_chosen_path.size() - labelFileAddress.size());
        QString textName = QString::fromStdString(textNameS);
        ui.lineEdit_labelText->setText(textName);
    }
}

void WSCNet2::on_lineEdit_labelText_textChanged(const QString& text)
{
    QString labelFileAddress = ui.lineEdit_workPlace->text() + mk_label_res_folder_name + "/" + text;
    m_circle_result.clear();
    m_accu_circle_results.clear();
    m_edit_circle_result.clear();
    ui.checkBox_manual->setChecked(false);
    if (text.size())
    {
        //读取txt
        ifstream infile;
        dropType circles_txt;
        infile.open(labelFileAddress.toStdString(), ios::in);
        if (!infile.is_open())
        {
            ui.textEdit_informationOutput->append("<font color=\"#FF0000\">ERROR</font> loading label file: " + labelFileAddress);//输出
            return;
        }
        while (!infile.eof())
        {
            infile >> circles_txt.first.first.x >> circles_txt.first.first.y >> circles_txt.first.second >> circles_txt.second;
            if (infile.good())
            {
                m_circle_result.push_back(circles_txt);
            }
        }
    }
    else
    {
        ui.textEdit_informationOutput->append("Clear chosen label.");//输出
    }
    imgDisplay(m_circle_result); //标注改变，发生信号

    printDropMessages();
    updateComponentAvailability(); // 更新组件可用性
}

void WSCNet2::on_pushButton_chooseModule_clicked()
{
    QString work_place = ui.lineEdit_workPlace->text();
    QString module_chosen = QFileDialog::getOpenFileName(this, "File Dialog", work_place, "module file(*pt *pth)");
    QString module_chosen_path = QFileInfo(module_chosen).path() + "/";

    if (module_chosen_path != work_place)
    {
        QMessageBox::warning(this, "Warning", tr("Please choose file in the workplace!"), QMessageBox::Ok);
        return;
    }

    if (!module_chosen.contains("traced"))
    {
        ui.textEdit_informationOutput->append("<font color=\"#FF8000\">WARNING</font> You might have chosen a untraced module!");
    }

    QString module_chosen_name = QFileInfo(module_chosen).fileName();
    ui.lineEdit_moduleName->setText(module_chosen_name);
}

void WSCNet2::on_pushButton_countDroplets_clicked()
{
    bool is_bright_field = ui.radioButton_bright->isChecked();
    int kernel_size = ui.spinBox_kernelSize->value();
    int min_radius = ui.spinBox_minRadius->value();
    int max_radius = ui.spinBox_maxRadius->value();
    int radius_modify = ui.spinBox_radiusModify->value();
    QString module_path = m_workplace_path + ui.lineEdit_moduleName->text();
    if (ui.lineEdit_moduleName->text().isEmpty()) // 模型地址为空，直接将空字符串传递给cout_thread
    {
        module_path = "";
        ui.textEdit_informationOutput->append("<font color=\"#FF8000\">WARNING</font> Module path is none, will only do segmentation. \n");
    }

    count_thread = new dropRecgThread();
    count_thread->setPathToSaveResult(mk_img_res_folder_name, mk_label_res_folder_name);
    count_thread->setParams(is_bright_field, kernel_size, min_radius, max_radius, radius_modify, module_path);
    connect(count_thread, &dropRecgThread::reportToMain, this, [=](QString message) {
        ui.textEdit_informationOutput->append(message);
        });
    connect(count_thread, SIGNAL(done()), this, SLOT(onThreadFinished()));

    if (ui.lineEdit_imageName->text() == "" || ui.lineEdit_imageName->text() == "\t") // 识别整个文件夹
    {
        ui.textEdit_informationOutput->append(tr("开始识别当前文件夹下所有图像，请耐心等待..."));
        count_thread->setCountObject(dropRecgThread::ECountMode::COUNT_FOLDER, m_workplace_path);
    }
    else // 识别单张图片或视频
    {
        auto file_type = checkFileType(m_workplace_path + m_file_chosen_path);
        if (file_type == dropRecgThread::ECountMode::COUNT_IMG)
        {
            ui.textEdit_informationOutput->append(tr("开始识别当前图像."));
            count_thread->setCountObject(dropRecgThread::ECountMode::COUNT_IMG, m_workplace_path + ui.lineEdit_imageName->text());
        }
        else if (file_type == dropRecgThread::ECountMode::COUNT_VIDEO)
        {
            ui.textEdit_informationOutput->append(tr("开始识别当前视频."));
            count_thread->setCountObject(dropRecgThread::ECountMode::COUNT_VIDEO, m_workplace_path + ui.lineEdit_imageName->text());
        }
        else
        {
            ui.textEdit_informationOutput->append("<font color=\"#FF0000\">ERROR</font> Unkown file type！");
            return;
        }
    }
    updateComponentAvailability(); // 更新组件可用性
    count_thread->start();
}

void WSCNet2::on_pushButton_clearMessage_clicked()
{
    ui.textEdit_informationOutput->setHtml(m_default_message);
}

void WSCNet2::on_pushButton_saveParams_clicked()
{
    savePramsToJson();
    ui.textEdit_informationOutput->append("Save params to param.json.");
}

void WSCNet2::on_checkBox_editImage_clicked()
{
    if (ui.checkBox_editImage->isChecked()) // 参数修正模式
    {
        m_radius_modify_record = ui.spinBox_radiusModify->value();

        // 设置滑动条范围
        int radius_modify = ui.spinBox_radiusModify->value();
        int minRadius = ui.spinBox_minRadius->value();
        int maxRadius = ui.spinBox_maxRadius->value();

        ui.horizontalSlider_radiusModify->setRange(radius_modify - 10, radius_modify + 10);
        ui.horizontalSlider_radiusModify->setValue(radius_modify);
        ui.horizontalSlider_minRadius->setRange(minRadius - 20, minRadius + 20);
        ui.horizontalSlider_minRadius->setValue(minRadius);
        ui.horizontalSlider_maxRadius->setRange(maxRadius - 20, maxRadius + 20);
        ui.horizontalSlider_maxRadius->setValue(maxRadius);
    }
    else  // 识别模式
    {
    }

    updateComponentAvailability(); // 更新组件可用性
}

void WSCNet2::radiusModify(int value)  //半径偏差显示
{
    if (m_cur_img.empty() || m_circle_result.empty())
        return;

    if (m_accu_circle_results.empty())
        m_accu_circle_results.push_back(m_circle_result);

    if (!m_edit_circle_result.empty() && m_edit_flag != 1)
        m_accu_circle_results.push_back(m_edit_circle_result);

    m_edit_circle_result = m_accu_circle_results.back();

    m_cur_img.copyTo(m_cur_img_display);
    vector<dropType> curCircles;
    curCircles = m_accu_circle_results.back();
    for (int i = 0; i != curCircles.size(); i++)
    {
        m_edit_circle_result[i].first.second = curCircles[i].first.second + (float)value - m_radius_modify_record;
        m_edit_circle_result[i].first.second = m_edit_circle_result[i].first.second > 1 ? m_edit_circle_result[i].first.second : 1;
    }
    imgDisplay(m_edit_circle_result);
    m_edit_flag = 1;
}

void WSCNet2::filterByMinRadius(int value) //最小半径过滤
{
    if (m_cur_img.empty() || m_circle_result.empty())
        return;

    if (m_accu_circle_results.empty())
        m_accu_circle_results.push_back(m_circle_result);

    if (!m_edit_circle_result.empty() && m_edit_flag != 2)
    {
        m_accu_circle_results.push_back(m_edit_circle_result);
        //ui.spinBox_radiusModify->setValue(0);
    }

    m_edit_circle_result.clear();

    m_cur_img.copyTo(m_cur_img_display);

    vector<dropType> curCircles;
    curCircles = m_accu_circle_results.back();
    for (int i = 0; i != curCircles.size(); i++)
    {
        if (curCircles[i].first.second > (double)value)
        {
            m_edit_circle_result.push_back(curCircles[i]);
        }
    }
    imgDisplay(m_edit_circle_result);
    m_edit_flag = 2;
}

void WSCNet2::filterByMaxRadius(int value) //最大半径过滤
{
    if (m_cur_img.empty() || m_circle_result.empty())
        return;
    if (m_accu_circle_results.empty())
        m_accu_circle_results.push_back(m_circle_result);

    if (!m_edit_circle_result.empty() && m_edit_flag != 3)
    {
        m_accu_circle_results.push_back(m_edit_circle_result);
        //ui.spinBox_radiusModify->setValue(0);
    }
    m_edit_circle_result.clear();

    m_cur_img.copyTo(m_cur_img_display);

    vector<dropType> curCircles;
    curCircles = m_accu_circle_results.back();
    for (int i = 0; i != curCircles.size(); i++)
    {
        if (curCircles[i].first.second < (double)value)
        {
            m_edit_circle_result.push_back(curCircles[i]);
        }
    }
    imgDisplay(m_edit_circle_result);
    m_edit_flag = 3;

}

//手动修正模式
void WSCNet2::on_checkBox_manual_clicked()
{
    ui.checkBox_deleteDrops->setChecked(false);
    if (ui.checkBox_manual->isChecked())
    {
        if (m_accu_circle_results.empty())
            m_accu_circle_results.push_back(m_circle_result);
        if (!m_edit_circle_result.empty())
            m_accu_circle_results.push_back(m_edit_circle_result);
        myqlabel_showImg->setPaintable(true); //label绘制使能信号为true
    }
    else
    {
        myqlabel_showImg->setPaintable(false); //label绘制使能信号为false
    }

    updateComponentAvailability(); // 更新组件可用性
}

void WSCNet2::paintCircle(float centerX, float centerY, float radius, int drop_state)
{
    if (radius > 0) //增加液滴
    {
        m_edit_circle_result = m_accu_circle_results.back();
        float diftx = myqlabel_showImg->width() / 2 - m_cur_img.cols / 2 * m_zoom;
        float difty = myqlabel_showImg->height() / 2 - m_cur_img.rows / 2 * m_zoom;

        //修正坐标
        float diftCenterX = (centerX - diftx) / m_zoom;
        float diftCenterY = (centerY - difty) / m_zoom;

        radius = radius / m_zoom + 0.5;

        if (diftCenterX >= 0 && diftCenterX < m_cur_img.cols && diftCenterY >= 0 && diftCenterY < m_cur_img.rows)
        {
            if (drop_state == -1)
            {
                if (ui.comboBox_function->currentIndex() == 0)
                    drop_state = 0;
                else if (ui.comboBox_function->currentIndex() == 1)
                    drop_state = 1;
                else if (ui.comboBox_function->currentIndex() == 2)
                    drop_state = 2;
            }

            dropType aCircle(dropType(CIRCLE(Point2f(diftCenterX, diftCenterY), radius), drop_state));
            m_edit_circle_result.push_back(aCircle);
            m_accu_circle_results.back() = m_edit_circle_result;
            imgDisplay(m_edit_circle_result);
        }
    }
    else if (radius == 0)//执行删除液滴、标注液滴等
    {
        m_edit_circle_result = m_accu_circle_results.back();

        float diftx = myqlabel_showImg->width() / 2 - m_cur_img.cols / 2 * m_zoom;
        float difty = myqlabel_showImg->height() / 2 - m_cur_img.rows / 2 * m_zoom;

        //修正圆心
        float diftCenterX = (centerX - diftx) / m_zoom;
        float diftCenterY = (centerY - difty) / m_zoom;

        if (diftCenterX < 0 || diftCenterX >= m_cur_img.cols || diftCenterY < 0 || diftCenterY >= m_cur_img.rows)//点击位置超出图像
            return;

        //计算所选中的液滴
        vector<pair<float, int>> disWithRadius;
        for (int i = 0; i != m_edit_circle_result.size(); i++)
        {
            float dis = sqrt((m_edit_circle_result[i].first.first.x - diftCenterX) * (m_edit_circle_result[i].first.first.x - diftCenterX) +
                (m_edit_circle_result[i].first.first.y - diftCenterY) * (m_edit_circle_result[i].first.first.y - diftCenterY));
            disWithRadius.push_back(pair<float, int>(dis, i));
        }
        sort(disWithRadius.begin(), disWithRadius.end());
        if (disWithRadius[0].first > m_edit_circle_result[disWithRadius[0].second].first.second) //未选中任何液滴
            return;

        if (ui.checkBox_deleteDrops->isChecked())//删除液滴
        {
            m_edit_circle_result.erase(m_edit_circle_result.begin() + disWithRadius[0].second);
            imgDisplay(m_edit_circle_result);
        }
        else if (ui.comboBox_function->currentIndex() == 0)//标注空包
        {
            m_edit_circle_result[disWithRadius[0].second].second = 0;
            imgDisplay(m_edit_circle_result);
        }
        else if (ui.comboBox_function->currentIndex() == 1)//标注单包
        {
            m_edit_circle_result[disWithRadius[0].second].second = 1;
            imgDisplay(m_edit_circle_result);
        }
        else if (ui.comboBox_function->currentIndex() == 2)//标注多包
        {
            m_edit_circle_result[disWithRadius[0].second].second = 2;
            imgDisplay(m_edit_circle_result);
        }
        m_accu_circle_results.back() = m_edit_circle_result;
    }

}

void WSCNet2::dragCircle(float centerX, float centerY)
{
    m_edit_circle_result = m_accu_circle_results.back();

    float diftx = myqlabel_showImg->width() / 2 - m_cur_img.cols / 2 * m_zoom;
    float difty = myqlabel_showImg->height() / 2 - m_cur_img.rows / 2 * m_zoom;

    //修正圆心
    float diftCenterX = (centerX - diftx) / m_zoom;
    float diftCenterY = (centerY - difty) / m_zoom;

    if (diftCenterX < 0 || diftCenterX >= m_cur_img.cols || diftCenterY < 0 || diftCenterY >= m_cur_img.rows)//点击位置超出图像
    {
        myqlabel_showImg->setCircle(-1, -1, -1, -1);
        return;
    }

    //计算所选中的液滴
    vector<pair<float, int>> disWithRadius;
    for (int i = 0; i != m_edit_circle_result.size(); i++)
    {
        float dis = sqrt((m_edit_circle_result[i].first.first.x - diftCenterX) * (m_edit_circle_result[i].first.first.x - diftCenterX) +
            (m_edit_circle_result[i].first.first.y - diftCenterY) * (m_edit_circle_result[i].first.first.y - diftCenterY));
        disWithRadius.push_back(pair<float, int>(dis, i));
    }
    sort(disWithRadius.begin(), disWithRadius.end());
    if (disWithRadius[0].first > m_edit_circle_result[disWithRadius[0].second].first.second) //未选中任何液滴
    {
        myqlabel_showImg->setCircle(-1, -1, -1, -1);
        return;
    }

    // 删除液滴
    const int delete_circle_id = disWithRadius[0].second;
    dropType aCircle = m_edit_circle_result[delete_circle_id];
    float centerX_new = aCircle.first.first.x * m_zoom + diftx;
    float centerY_new = aCircle.first.first.y * m_zoom + difty;
    myqlabel_showImg->setCircle(centerX_new, centerY_new, aCircle.first.second, aCircle.second);
    m_edit_circle_result.erase(m_edit_circle_result.begin() + delete_circle_id);
    m_accu_circle_results.back() = m_edit_circle_result;
    imgDisplay(m_edit_circle_result);
}

void WSCNet2::deleteCircle(float x, float y)
{
    m_edit_circle_result = m_accu_circle_results.back();

    float diftx = myqlabel_showImg->width() / 2 - m_cur_img.cols / 2 * m_zoom;
    float difty = myqlabel_showImg->height() / 2 - m_cur_img.rows / 2 * m_zoom;

    //修正圆心
    float diftCenterX = (x - diftx) / m_zoom;
    float diftCenterY = (y - difty) / m_zoom;

    if (diftCenterX < 0 || diftCenterX >= m_cur_img.cols || diftCenterY < 0 || diftCenterY >= m_cur_img.rows)//点击位置超出图像
        return;

    //计算所选中的液滴
    vector<pair<float, int>> disWithRadius;
    for (int i = 0; i != m_edit_circle_result.size(); i++)
    {
        float dis = sqrt((m_edit_circle_result[i].first.first.x - diftCenterX) * (m_edit_circle_result[i].first.first.x - diftCenterX) +
            (m_edit_circle_result[i].first.first.y - diftCenterY) * (m_edit_circle_result[i].first.first.y - diftCenterY));
        disWithRadius.push_back(pair<float, int>(dis, i));
    }
    sort(disWithRadius.begin(), disWithRadius.end());
    if (disWithRadius[0].first > m_edit_circle_result[disWithRadius[0].second].first.second) //未选中任何液滴
        return;


    if (ui.checkBox_deleteDrops->isChecked())
    {
        int r = QMessageBox::question(this, tr("标注包菌液滴"), tr("当前液滴只包含几个菌？"), "0", "1", ("2个及以上"));

        if (r == 0)//0包
        {
            m_edit_circle_result[disWithRadius[0].second].second = 0;
            imgDisplay(m_edit_circle_result);
        }
        else if (r == 1)//单包
        {
            m_edit_circle_result[disWithRadius[0].second].second = 1;
            imgDisplay(m_edit_circle_result);
        }
        else if (r == 2)//多包
        {
            m_edit_circle_result[disWithRadius[0].second].second = 2;
            imgDisplay(m_edit_circle_result);
        }
        m_accu_circle_results.back() = m_edit_circle_result;
    }
    else //非标注情况：双击表示删除液滴
    {
        m_edit_circle_result.erase(m_edit_circle_result.begin() + disWithRadius[0].second);
        imgDisplay(m_edit_circle_result);
        m_accu_circle_results.back() = m_edit_circle_result;
    }
}

void WSCNet2::on_pushButton_saveResult_clicked()
{
    bool cover(0);
    if (m_circle_result.empty() && m_accu_circle_results.empty() && m_edit_circle_result.empty())
    {
        QMessageBox::information(NULL, "WARNING", tr("当前没有标注结果可供保存！"));
        return;
    }
    if (m_cur_img_display.empty())
    {
        QMessageBox::information(NULL, "WARNING", tr("当前没有图像结果可供保存！"));
        return;
    }

    if (m_edit_circle_result.empty())
    {
        if (!m_accu_circle_results.empty())
            m_edit_circle_result = m_accu_circle_results.back();
        else if (!m_circle_result.empty())
        {
            m_edit_circle_result = m_circle_result;
        }
    }

    QString imgName = ui.lineEdit_imageName->text();

    int nonCount(0);
    int singleCount(0);
    int multiCount(0);
    vector<double> dropDiam; //液滴直径

    float mean(0.0); //平均直径
    float cvValue(0.0); //cv值
    float singleRate(0.0);//单包率
    float singleAll(0.0);//单包/（单包+多包）

    for (int i = 0; i != m_edit_circle_result.size(); i++)
    {
        if (0 == m_edit_circle_result[i].second)
        {
            nonCount++;
        }
        else if (1 == m_edit_circle_result[i].second)
        {
            singleCount++;
        }
        else if (2 == m_edit_circle_result[i].second)
        {
            multiCount++;
        }
        dropDiam.push_back(m_edit_circle_result[i].first.second * 2);
    }

    //计算液滴平均半径和cv
    if (!dropDiam.empty())
    {
        float sum = accumulate(begin(dropDiam), end(dropDiam), 0.0);
        mean = sum / dropDiam.size(); //均值

        float accum = 0.0;
        for_each(begin(dropDiam), end(dropDiam), [&](const double d) {
            accum += (d - mean) * (d - mean);
            });

        float stdev = sqrt(accum / (dropDiam.size() - 1)); //方差
        cvValue = stdev / mean;

        singleRate = (float)singleCount / (float)m_edit_circle_result.size();
    }
    if ((singleCount + multiCount) != 0)
    {
        singleAll = (float)singleCount / ((float)singleCount + (float)multiCount);
    }

    dropInfo new_dropinfo;

    new_dropinfo.imgName = imgName.toStdString();
    new_dropinfo.dropNumber = m_edit_circle_result.size();
    new_dropinfo.nonYeast = nonCount;
    new_dropinfo.singleYeast = singleCount;
    new_dropinfo.multipleYeast = multiCount;
    new_dropinfo.singleRate = singleRate;
    new_dropinfo.singleAll = singleAll;
    new_dropinfo.meanDiameter = mean;
    new_dropinfo.cvValue = cvValue;

    String dropInfoSaveName(m_workplace_path.toStdString() + "droplet_information.txt");

    //判断是否存在，不存在就新建一个
    ofstream tryfile(dropInfoSaveName, ios::app);
    tryfile.close();

    m_droplet_info.clear();
    ifstream infile;  //读取文件
    dropInfo dropinfo;
    infile.open(dropInfoSaveName, ios::in);

    if (!infile.is_open())
    {
        ui.textEdit_informationOutput->append("Open file failure");//输出
        return;
    }
    while (!infile.eof())
    {
        infile >> dropinfo.imgName >> dropinfo.dropNumber >> dropinfo.nonYeast
            >> dropinfo.singleYeast >> dropinfo.multipleYeast >> dropinfo.singleRate
            >> dropinfo.singleAll >> dropinfo.meanDiameter >> dropinfo.cvValue;
        if (infile.good())
        {
            if (imgName.toStdString() == dropinfo.imgName)//是否已经处理过这张图片
            {
                int r = QMessageBox::question(this, tr("重复写入"), tr("存在当前图像的处理结果，是否覆盖？"), "Yes", "No");
                if (0 == r)
                {
                    cover = 1;  //选择覆盖
                    dropinfo = new_dropinfo;
                }
                else
                {
                    infile.close();
                    return;  //选择不覆盖，直接结束
                }
            }
            m_droplet_info.push_back(dropinfo);
        }
    }
    infile.close();

    if (!cover)  //未处理过这张图像,跟新m_droplet_info
    {
        m_droplet_info.push_back(new_dropinfo);
    }

    //更新后的液滴信息重新写入dropInformation_txt
    ofstream outfile(dropInfoSaveName);

    for (int i = 0; i != m_droplet_info.size(); i++)
    {
        dropInfo dropinfo = m_droplet_info[i];
        outfile << dropinfo.imgName << "\t" << dropinfo.dropNumber << "\t" << dropinfo.nonYeast << "\t"
            << dropinfo.singleYeast << "\t" << dropinfo.multipleYeast << "\t" << dropinfo.singleRate << "\t"
            << dropinfo.singleAll << "\t" << dropinfo.meanDiameter << "\t" << dropinfo.cvValue << endl;
    }
    outfile.close();

    //覆盖原来的标注结果
    String imgNamePure = imgName.toStdString().substr(0, imgName.toStdString().find_last_of("."));
    String labelSaveName(m_label_save_file_path.toStdString() + "/" + imgNamePure + "_modified.txt");
    
    ofstream fout(labelSaveName);

    for (int i = 0; i != m_edit_circle_result.size(); i++)
    {
        fout << m_edit_circle_result[i].first.first.x << "\t" << m_edit_circle_result[i].first.first.y << "\t" << m_edit_circle_result[i].first.second << "\t" << m_edit_circle_result[i].second << "\t" << endl;
    }
    fout.close();  //存储标注结果

    //覆盖原来的标注图像结果
    String imageSaveName(m_img_save_file_path.toStdString() + "/" + imgNamePure + "_modified.bmp");
    imwrite(imageSaveName, m_cur_img_display);  //存储图像结果

    QDateTime current_date_time = QDateTime::currentDateTime();
    QString current_date = current_date_time.toString("yyyy.MM.dd hh:mm:ss");
    ui.textEdit_informationOutput->append(tr("保存成功！ ") + current_date);
}

void WSCNet2::on_pushButton_quit_clicked()
{
    this->close();
}

void WSCNet2::on_checkBox_deleteDrops_clicked()
{
    if (m_cur_img_display.empty())
    {
        QMessageBox::information(NULL, "WARNING", tr("当前没有图像可供删除标注！"));
        ui.checkBox_deleteDrops->setChecked(false);
        return;
    }
    if (m_circle_result.empty() && m_accu_circle_results.empty() && m_edit_circle_result.empty())
    {
        QMessageBox::information(NULL, "WARNING", tr("当前没有标注结果可供删除！"));
        ui.checkBox_deleteDrops->setChecked(false);
        return;
    }

    if (m_edit_circle_result.empty())
    {
        if (!m_accu_circle_results.empty())
            m_edit_circle_result = m_accu_circle_results.back();
        else if (!m_circle_result.empty())
        {
            m_edit_circle_result = m_circle_result;
        }
    }
    imgDisplay(m_edit_circle_result);

    updateComponentAvailability(); // 更新组件可用性
}


void WSCNet2::on_toolButton_lastImg_clicked()
{
    if (m_img_name_list.size() == 0) // 当前文件夹没有图像
    {
        QMessageBox::information(NULL, "ERROR", tr("当前文件夹没有图像，请重新设置！"));
        return;
    }

    QString img_text = ui.lineEdit_imageName->text();
    if (img_text.size() == 0) // 当前未设置图像，直接显示最后一张图像
    {
        ui.lineEdit_imageName->setText(m_img_name_list.last());
        return;
    }

    int img_index = m_img_name_list.indexOf(img_text);
    if (img_index == -1)
    {
        QMessageBox::information(NULL, "WARNING", tr("未找到当前图像！"));
        ui.lineEdit_imageName->setText(m_img_name_list[0]);
    }
    else if (img_index == 0)
    {
        QMessageBox::information(NULL, "WARNING", tr("当前已经是第一张图像！"));
        return;
    }
    else
    {
        img_index--;
        ui.lineEdit_imageName->setText(m_img_name_list[img_index]);
    }
}

void WSCNet2::on_toolButton_nextImg_clicked()
{
    if (m_img_name_list.size() == 0) // 当前文件夹没有图像
    {
        QMessageBox::information(NULL, "ERROR", tr("当前文件夹没有图像，请重新设置！"));
        return;
    }

    QString img_text = ui.lineEdit_imageName->text();
    if (img_text.size() == 0) // 当前未设置图像，直接显示第一张图像
    {
        ui.lineEdit_imageName->setText(m_img_name_list[0]);
        return;
    }

    int img_index = m_img_name_list.indexOf(ui.lineEdit_imageName->text());
    if (img_index == -1)
    {
        QMessageBox::information(NULL, "WARNING", tr("未找到当前图像！"));
        ui.lineEdit_imageName->setText(m_img_name_list[m_img_name_list.size() - 1]);
    }
    else if (img_index == m_img_name_list.size()-1)
    {
        QMessageBox::information(NULL, "WARNING", tr("当前已经是最后一张图像！"));
        return;
    }
    else
    {
        img_index++;
        ui.lineEdit_imageName->setText(m_img_name_list[img_index]);
    }
}

void WSCNet2::on_toolButton_zoomIn_clicked()
{
    if (m_cur_img.empty())
        return;
    m_zoom += 0.1;
    imgDisplay(m_circle_result);
}

void WSCNet2::on_toolButton_zoomReset_clicked()
{
    if (m_cur_img.empty())
        return;
    m_zoom = 1.0;
    imgDisplay(m_circle_result);
}

void WSCNet2::on_toolButton_zoomOut_clicked()
{
    if (m_cur_img.empty())
        return;
    m_zoom -= 0.1;
    imgDisplay(m_circle_result);
}

void WSCNet2::on_comboBox_language_currentIndexChanged(int index)
{
    QSettings settings("./WSCNet2.ini", QSettings::IniFormat);

    m_translator->load("./Translate_EN.qm");
    if (index == 0)
    {
        qApp->removeTranslator(m_translator);
        settings.setValue("General/language", "zh-cn");
    }
    else
    {
        qApp->installTranslator(m_translator);
        settings.setValue("General/language", "en");
    }
	ui.retranslateUi(this);

    settings.sync();
}

void WSCNet2::on_comboBox_function_currentIndexChanged(int index)
{
    myqlabel_showImg->m_state = index;
}

void WSCNet2::on_checkBox_light_clicked()
{
    if (ui.checkBox_light->isChecked())
        m_is_contrast = true;
    else
        m_is_contrast = false;

    if (m_edit_circle_result.empty())
    {
        if (!m_accu_circle_results.empty())
            m_edit_circle_result = m_accu_circle_results.back();
        else if (!m_circle_result.empty())
        {
            m_edit_circle_result = m_circle_result;
        }
    }
    imgDisplay(m_edit_circle_result);
}

void WSCNet2::onThreadFinished()
{
    ui.textEdit_informationOutput->append(tr("识别完成！"));

    count_thread->quit();
    count_thread->wait();
    delete count_thread;
    count_thread = nullptr;

    if (ui.lineEdit_imageName->text().size()) // 将最新的图像识别结果进行展示
    {
        auto file_type = checkFileType(ui.lineEdit_imageName->text());
        if (file_type == dropRecgThread::ECountMode::COUNT_IMG)
        {
            QString text = ui.lineEdit_imageName->text();
            QString img_name = QString::fromStdString(text.toStdString().substr(0, text.toStdString().find_last_of(".")));
            ui.lineEdit_labelText->clear();
            ui.lineEdit_labelText->setText(img_name + "_drops.txt");
        }
    }

    updateComponentAvailability(); // 更新组件可用性
}

//QImage转Mat
Mat WSCNet2::QImageToMat(QImage image)
{
    cv::Mat mat;
    switch (image.format())
    {
    case QImage::Format_ARGB32:
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32_Premultiplied:
        mat = cv::Mat(image.height(), image.width(), CV_8UC4, (void*)image.constBits(), image.bytesPerLine());
        break;
    case QImage::Format_RGB888:
        mat = cv::Mat(image.height(), image.width(), CV_8UC3, (void*)image.constBits(), image.bytesPerLine());
        cv::cvtColor(mat, mat, COLOR_BGR2RGB);
        break;
    case QImage::Format_Indexed8:
        mat = cv::Mat(image.height(), image.width(), CV_8UC1, (void*)image.constBits(), image.bytesPerLine());
        break;
    }
    return mat;
}

//Mat转QImage
QImage WSCNet2::MatToQImage(const Mat& mat)
{
    // 8-bits unsigned, NO. OF CHANNELS = 1
    if (mat.type() == CV_8UC1)
    {
        QImage image(mat.cols, mat.rows, QImage::Format_Indexed8);
        // Set the color table (used to translate colour indexes to qRgb values)
        image.setColorCount(256);
        for (int i = 0; i < 256; i++)
        {
            image.setColor(i, qRgb(i, i, i));
        }
        // Copy input Mat
        uchar* pSrc = mat.data;
        for (int row = 0; row < mat.rows; row++)
        {
            uchar* pDest = image.scanLine(row);
            memcpy(pDest, pSrc, mat.cols);
            pSrc += mat.step;
        }
        return image;
    }
    // 8-bits unsigned, NO. OF CHANNELS = 3
    else if (mat.type() == CV_8UC3)
    {
        // Copy input Mat
        const uchar* pSrc = (const uchar*)mat.data;
        // Create QImage with same dimensions as input Mat
        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        return image.rgbSwapped();
    }
    else if (mat.type() == CV_8UC4)
    {
        // Copy input Mat
        const uchar* pSrc = (const uchar*)mat.data;
        // Create QImage with same dimensions as input Mat
        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        return image.copy();
    }
    else
    {
        return QImage();
    }
}

void WSCNet2::loadParamsFromJson()
{
    QString path_json = m_workplace_path + "param.json";
    if (!QFile::exists(path_json))
    {
        ui.textEdit_informationOutput->append("<font color=\"#FF8000\">WARNING</font> " + tr("当前目录下未找到param.json，使用默认参数代替！"));
        savePramsToJson();
        return;
    }

    ifstream f_json(path_json.toStdString());
    try
    {
        json data_json = json::parse(f_json);

        if (data_json["is_bright_field"].get<bool>())
            ui.radioButton_bright->setChecked(true);
        else
            ui.radioButton_dark->setChecked(true);

        ui.spinBox_kernelSize->setValue(data_json["kernel_size"].get<int>());
        ui.spinBox_radiusModify->setValue(data_json["dev_bright"].get<int>());
        ui.spinBox_minRadius->setValue(data_json["min_radius"].get<int>());
        ui.spinBox_maxRadius->setValue(data_json["max_radius"].get<int>());
        ui.lineEdit_moduleName->setText(QString::fromStdString(data_json["module_name"].get<string>()));
    }
    catch (const exception&)
    {
        ui.textEdit_informationOutput->append(QString::fromStdString("<font color=\"#FF0000\">ERROR</font> ") + tr("读取param.json失败！"));
        return;
    }
    f_json.close();
}

void WSCNet2::savePramsToJson()
{
    QString path_json = m_workplace_path + "param.json";
    ofstream f_json(path_json.toStdString());
    json data_json;
    data_json["is_bright_field"] = ui.radioButton_bright->isChecked();
    data_json["kernel_size"] = ui.spinBox_kernelSize->value();
    data_json["dev_bright"] = ui.spinBox_radiusModify->value();
    data_json["min_radius"] = ui.spinBox_minRadius->value();
    data_json["max_radius"] = ui.spinBox_maxRadius->value();
    data_json["module_name"] = ui.lineEdit_moduleName->text().toStdString();
    f_json << data_json << endl;
    f_json.close();
}

auto WSCNet2::checkFileType(const QString& file_path) -> dropRecgThread::ECountMode
{
    if (
        file_path.endsWith(".bmp") ||
        file_path.endsWith(".jpg") ||
        file_path.endsWith(".png") ||
        file_path.endsWith(".jpeg") ||
        file_path.endsWith(".tif")
        )
    {
        return dropRecgThread::ECountMode::COUNT_IMG;
    }
    else if (
        file_path.endsWith(".mp4") ||
        file_path.endsWith(".avi") ||
        file_path.endsWith(".mov") ||
        file_path.endsWith(".wmv") ||
        file_path.endsWith(".flv") ||
        file_path.endsWith(".mkv") ||
        file_path.endsWith(".webm")
        )
    {
        return dropRecgThread::ECountMode::COUNT_VIDEO;
    }
    else
    {
        return dropRecgThread::ECountMode::COUNT_UNKNOWN;
    }
}

void WSCNet2::updateComponentAvailability()
{
    const bool is_counting_running = (count_thread != nullptr);
    const bool is_file_chosen = !ui.lineEdit_imageName->text().isEmpty();
    const bool is_label_chosen = !ui.lineEdit_labelText->text().isEmpty();
    const bool is_module_chosen = !ui.lineEdit_moduleName->text().isEmpty();
    const bool is_editing_mode = ui.checkBox_editImage->isChecked();
    const bool is_manual_mode = ui.checkBox_manual->isChecked();
    const bool is_video = dropRecgThread::ECountMode::COUNT_VIDEO == 
        checkFileType(ui.lineEdit_workPlace->text() + ui.lineEdit_imageName->text());
    const bool is_delete_drops = ui.checkBox_deleteDrops->isChecked();

    ui.pushButton_chooseImage->setEnabled(!is_counting_running && !is_manual_mode);
    ui.pushButton_chooseLabelText->setEnabled(!is_counting_running && is_file_chosen && !is_video);
    ui.pushButton_chooseModule->setEnabled(!is_counting_running);
    ui.pushButton_countDroplets->setEnabled(!is_counting_running && !is_editing_mode && !is_manual_mode);
    ui.pushButton_saveResult->setEnabled(!is_counting_running && is_manual_mode);
    ui.comboBox_function->setEnabled(!is_counting_running && is_manual_mode && !is_delete_drops);
    ui.checkBox_deleteDrops->setEnabled(!is_counting_running && is_manual_mode);
    ui.checkBox_manual->setEnabled(!is_counting_running && is_file_chosen && !is_video);
    ui.checkBox_editImage->setEnabled(!is_counting_running && is_file_chosen && !is_video);
    ui.pushButton_quit->setEnabled(!is_counting_running);
    ui.pushButton_saveParams->setEnabled(!is_counting_running);

    const bool is_params_adjust_available = !is_counting_running && !is_editing_mode && !is_manual_mode;
    ui.radioButton_bright->setEnabled(is_params_adjust_available);
    ui.radioButton_dark->setEnabled(is_params_adjust_available);
    ui.spinBox_kernelSize->setEnabled(is_params_adjust_available);
    ui.spinBox_radiusModify->setEnabled(is_params_adjust_available);
    ui.spinBox_minRadius->setEnabled(is_params_adjust_available);
    ui.spinBox_maxRadius->setEnabled(is_params_adjust_available);

    const bool is_horizontal_slider_available = !is_counting_running && is_editing_mode && !is_manual_mode;
    ui.horizontalSlider_radiusModify->setEnabled(is_horizontal_slider_available);
    ui.horizontalSlider_minRadius->setEnabled(is_horizontal_slider_available);
    ui.horizontalSlider_maxRadius->setEnabled(is_horizontal_slider_available);


    if (is_counting_running)
    {
        ui.pushButton_countDroplets->setText(tr("识别中..."));
    }
    else if (!is_file_chosen)
    {
        ui.pushButton_countDroplets->setText(tr("识别当前文件夹"));
        ui.pushButton_countDroplets->setStyleSheet("background: rgb(173,255,47)");
    }
    else if (!is_video)
    {
        ui.pushButton_countDroplets->setText(tr("识别当前图像"));
        ui.pushButton_countDroplets->setStyleSheet("background: rgb(192,192,192)");
    }
    else
    {
        ui.pushButton_countDroplets->setText(tr("识别当前图像"));
        ui.pushButton_countDroplets->setStyleSheet("background: rgb(0,206,209)");
    }
}

void WSCNet2::printDropMessages()
{
    if (m_circle_result.empty())
        return;

    Scalar showColor;
    int allCount = m_circle_result.size();
    int nonCount = 0;
    int singleCount = 0;
    int multiCount = 0;
    int wrongCount = 0;
    vector<double> dropDiam; //液滴半径

    float mean = .0; //平均直径
    float cvVelue = .0; //cv值
    float singleRate = .0;//单包率
    float singleAll = .0;

    for (const auto& circle_result : m_circle_result)
    {
        switch (circle_result.second)
        {
            case -1:
                wrongCount++;
                continue;
                break;
            case 0:
                nonCount++;
                break;
            case 1:
                singleCount++;
                break;
            case 2:
                multiCount++;
                break;
            default:
                break;
        }

        dropDiam.push_back(circle_result.first.second);
    }
    //计算液滴平均半径和cv
    if (!dropDiam.empty())
    {
        float sum = accumulate(begin(dropDiam), end(dropDiam), 0.0);
        mean = sum / dropDiam.size(); //均值

        float accum = 0.0;
        for_each(begin(dropDiam), end(dropDiam), [&](const double d) {
            accum += (d - mean) * (d - mean);
            });

        float stdev = sqrt(accum / (dropDiam.size() - 1)); //方差
        cvVelue = stdev / mean;

        singleRate = (float)singleCount / (float)m_circle_result.size();

        if ((singleCount + multiCount) != 0)
        {
            singleAll = (float)singleCount / (float)(singleCount + multiCount);
        }
    }

    ui.textEdit_informationOutput->append("-------------------------------------------------");
    ui.textEdit_informationOutput->append(tr("总液滴数:\t") + QString::number(allCount - wrongCount));
    ui.textEdit_informationOutput->append(tr("空包液滴:\t") + QString::number(nonCount));
    ui.textEdit_informationOutput->append(tr("单包液滴:\t") + QString::number(singleCount));
    ui.textEdit_informationOutput->append(tr("多包液滴:\t") + QString::number(multiCount));
    ui.textEdit_informationOutput->append(tr("总单包率:\t") + QString("%1%").arg(100 * singleRate));
    ui.textEdit_informationOutput->append(tr("单/(单+多):\t") + QString("%1%").arg(100 * singleAll));
    ui.textEdit_informationOutput->append(tr("平均半径:\t") + QString::number(mean, 'f', 1));
    ui.textEdit_informationOutput->append(tr("半径CV值:\t") + QString::number(cvVelue, 'f', 3));
    ui.textEdit_informationOutput->append("-------------------------------------------------");
}

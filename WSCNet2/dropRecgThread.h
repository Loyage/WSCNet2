#pragma once

#include <QThread>
#include <QString>

class dropRecgThread :
    public QThread
{
    Q_OBJECT

public:
    explicit dropRecgThread(QObject *parent = nullptr);
    ~dropRecgThread();

    enum class ECountMode { COUNT_IMG, COUNT_FOLDER, COUNT_VIDEO, COUNT_UNKNOWN };

    void setPathToSaveResult(QString img_save_folder, QString text_save_folder); // 接收保存路径

    void setCountObject(ECountMode count_mode, QString path);

    void setParams(bool is_bright_field, int kernel_size, int min_radius, int max_radius, int dev_bright, QString module_path); // 接收参数(识别文件夹下所有图片

protected:
    void run() override;

signals:
    void done();

    void reportToMain(QString); // 报告完成进度

private:
    ECountMode m_count_mode;
    QString m_folder_path;
    QString m_file_name;
    QString m_img_save_folder;
    QString m_text_save_folder;
    QString m_module_path;
    
    bool m_is_bright_field;
    int m_dev_bright;
    int m_kernel_size;
    float m_min_radius;
    float m_max_radius;

    void videoToFrames(std::string& img_folder);
    void framesToVideo();
};


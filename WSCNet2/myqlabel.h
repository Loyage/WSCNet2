#pragma once

#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>

class Myqlabel:
    public QLabel
{
    Q_OBJECT

public:
    Myqlabel(QWidget* parent = nullptr);
    ~Myqlabel();

    void paintEvent(QPaintEvent* event);

    void mousePressEvent(QMouseEvent* event);

    void mouseReleaseEvent(QMouseEvent* event);

    void mouseMoveEvent(QMouseEvent* event);

    void mouseDoubleClickEvent(QMouseEvent* event);

    void setPaintable(bool value); //使能信号

    bool isPaintMode;
    float centerX,centerY,radius;
    int state;  //当前是空包，单包，多包状态

signals:
    void outputCircle(float,float,float);

    void outputDelete(float,float);
};

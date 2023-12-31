﻿#pragma once

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

    void setCircle(float x, float y, float r, int drop_state);

    int m_state;  //当前是空包，单包，多包状态

signals:
    void outputCircle(float, float, float, int);

    void outputDelete(float, float);

    void outputDrag(float, float);

private:
    float m_x1, m_y1; // 鼠标按下时的坐标
    float m_centerX_old, m_centerY_old; // 拖动前的圆心坐标
    float m_centerX, m_centerY, m_radius;
    bool m_is_paintable, m_is_deleteMode;
    Qt::MouseButton m_button;
};

#include "myqlabel.h"

#include <QPen>
#include <QCursor>
#include <QPainter>

Myqlabel::Myqlabel(QWidget* parent): QLabel(parent)
{
    this->isPaintMode = false;
    this->centerX = -1.0;
    this->centerY = -1.0;
    this->radius = -1.0;
    this->state = 0;
}

Myqlabel::~Myqlabel()
{
}

void Myqlabel::setPaintable(bool value)
{
    isPaintMode = value;
}

void Myqlabel::paintEvent(QPaintEvent *event)
{
    //comment before
    QLabel::paintEvent(event); //绘制背景的图片

    if (!isPaintMode)
        return;

    this->setCursor(Qt::CrossCursor);

    QPainter painter(this);

    QPen qpen;
    if(0==state)//空包
    {
        qpen= QPen(Qt::red, 1);
    }
    else if(1==state)//单包
    {
        qpen= QPen(Qt::blue, 1);
    }
    else if(2==state)//多包
    {
        qpen= QPen(Qt::green, 1);
    }
    painter.setPen(qpen);
    painter.drawEllipse(QPoint((int)centerX,(int)centerY),(int)radius,(int)radius);
    //painter.drawRect(QRect(x1, y1, x2 - x1, y2 - y1));
}

void Myqlabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (!isPaintMode)
            return;
        centerX = event->pos().x();
        centerY = event->pos().y();
        QCursor cursor;
        cursor.setShape(Qt::CrossCursor);
        //QApplication::setOverrideCursor(cursor);
    }
}

void Myqlabel::mouseReleaseEvent(QMouseEvent *event)
{
    if(!isPaintMode)
        return;
    int x2 = event->pos().x(); //鼠标相对于所在控件的位置
    int y2 = event->pos().y();
    radius = sqrt(((float)centerX-(float)x2)*((float)centerX-(float)x2)+((float)centerY-(float)y2)*((float)centerY-(float)y2));

    if(centerX>-1&&centerY>-1&&radius>=0)
        emit outputCircle(centerX,centerY,radius);

    update();
    //QApplication::restoreOverrideCursor();

}

void Myqlabel::mouseMoveEvent(QMouseEvent *event)
{
    if(!isPaintMode)
        return;
    if (event->buttons() & Qt::LeftButton)
    {
        int x2 = event->pos().x(); //鼠标相对于所在控件的位置
        int y2 = event->pos().y();
        radius = (int)sqrt(((float)centerX-(float)x2)*((float)centerX-(float)x2)+((float)centerY-(float)y2)*((float)centerY-(float)y2));
        update();
    }
}

void Myqlabel::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(!isPaintMode)
        return;
    int x2 = event->pos().x(); //鼠标相对于所在控件的位置
    int y2 = event->pos().y();
    //emit outputDelete(x2,y2);
}

#include "myqlabel.h"

#include <QPen>
#include <QCursor>
#include <QPainter>

Myqlabel::Myqlabel(QWidget* parent): QLabel(parent)
{
    this->isPaintable = false;
    this->centerX = -1.0;
    this->centerY = -1.0;
    this->radius = -1.0;
    this->state = 0;
    this->m_x1 = -1.0;
    this->m_y1 = -1.0;
    this->m_button = Qt::NoButton;
    this->isDeleteMode = false;
    this->centerX_old = -1.0;
    this->centerY_old = -1.0;
}

Myqlabel::~Myqlabel()
{
}

void Myqlabel::setPaintable(bool value)
{
    isPaintable = value;
    if (isPaintable)
    {
		this->setCursor(Qt::CrossCursor);
	}
    else
    {
		this->setCursor(Qt::ArrowCursor);
	}
}

void Myqlabel::setCircle(float x , float y, float r, int drop_state)
{
    centerX = x;
    centerY = y;
    radius = r;
    state = drop_state;

    centerX_old = x;
    centerY_old = y;
}

void Myqlabel::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event); //绘制背景的图片

    if (!isPaintable)
        return;

    if (radius < 0)
        return;

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
    if (!isPaintable)
        return;
    if (m_button == Qt::NoButton && event->button() == Qt::LeftButton) //左键按下，开始画圆
    {
        centerX = event->pos().x();
        centerY = event->pos().y();
        m_button = Qt::LeftButton;
    }
    else if (m_button == Qt::NoButton && event->button() == Qt::RightButton) //右键按下，开始拖动
    {
        m_x1 = event->pos().x();
        m_y1 = event->pos().y();
        // 发出信号，获取对应圆位置信息
        emit outputDrag(m_x1, m_y1);
        m_button = Qt::RightButton;
        update();
	}
    else
    {
        return;
    }
}

void Myqlabel::mouseReleaseEvent(QMouseEvent *event)
{
    if(!isPaintable)
        return;
    if (m_button == Qt::LeftButton && event->button() == Qt::LeftButton)
    {
        m_button = Qt::NoButton;
        int x2 = event->pos().x();
        int y2 = event->pos().y();
        radius = sqrt(((float)centerX - (float)x2) * ((float)centerX - (float)x2) + ((float)centerY - (float)y2) * ((float)centerY - (float)y2));

        if (centerX > -1 && centerY > -1 && radius >= 0)
            emit outputCircle(centerX, centerY, radius, -1);

        update();
    }
    else if (m_button == Qt::RightButton && event->button() == Qt::RightButton)
    {
        m_button = Qt::NoButton;
		emit outputCircle(centerX, centerY, radius, state);
        radius = -1.0;
        state = 0;
		update();
	}
    else
    {
		return;
	}
}

void Myqlabel::mouseMoveEvent(QMouseEvent *event)
{
    if(!isPaintable)
        return;
    if (event->buttons() & Qt::LeftButton)
    {
        int x2 = event->pos().x(); //鼠标相对于所在控件的位置
        int y2 = event->pos().y();
        radius = (int)sqrt(((float)centerX-(float)x2)*((float)centerX-(float)x2)+((float)centerY-(float)y2)*((float)centerY-(float)y2));
        update();
    }
    else if (event->buttons() & Qt::RightButton)
    {
		centerX = centerX_old + event->pos().x() - m_x1;
        centerY = centerY_old + event->pos().y() - m_y1;
		update();
	}
    else
    {
		return;
	}
}

void Myqlabel::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(!isPaintable)
        return;
    int x2 = event->pos().x(); //鼠标相对于所在控件的位置
    int y2 = event->pos().y();
    //emit outputDelete(x2,y2);
}

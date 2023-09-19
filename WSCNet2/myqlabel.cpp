#include "myqlabel.h"

#include <QPen>
#include <QCursor>
#include <QPainter>

Myqlabel::Myqlabel(QWidget* parent): QLabel(parent)
{
    this->m_is_paintable = false;
    this->m_centerX = -1.0;
    this->m_centerY = -1.0;
    this->m_radius = -1.0;
    this->m_state = 0;
    this->m_x1 = -1.0;
    this->m_y1 = -1.0;
    this->m_button = Qt::NoButton;
    this->m_is_deleteMode = false;
    this->m_centerX_old = -1.0;
    this->m_centerY_old = -1.0;
}

Myqlabel::~Myqlabel()
{
}

void Myqlabel::setPaintable(bool value)
{
    m_is_paintable = value;
    if (m_is_paintable)
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
    m_centerX = x;
    m_centerY = y;
    m_radius = r;
    m_state = drop_state;

    m_centerX_old = x;
    m_centerY_old = y;
}

void Myqlabel::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event); //绘制背景的图片

    if (!m_is_paintable)
        return;

    if (m_radius < 0)
        return;

    QPainter painter(this);
    QPen qpen;
    if(0==m_state)//空包
    {
        qpen= QPen(Qt::red, 1);
    }
    else if(1==m_state)//单包
    {
        qpen= QPen(Qt::blue, 1);
    }
    else if(2==m_state)//多包
    {
        qpen= QPen(Qt::green, 1);
    }
    painter.setPen(qpen);
    painter.drawEllipse(QPoint((int)m_centerX,(int)m_centerY),(int)m_radius,(int)m_radius);
    //painter.drawRect(QRect(x1, y1, x2 - x1, y2 - y1));
}

void Myqlabel::mousePressEvent(QMouseEvent *event)
{
    if (!m_is_paintable)
        return;
    if (m_button == Qt::NoButton && event->button() == Qt::LeftButton) //左键按下，开始画圆
    {
        m_centerX = event->pos().x();
        m_centerY = event->pos().y();
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
    if(!m_is_paintable)
        return;
    if (m_button == Qt::LeftButton && event->button() == Qt::LeftButton)
    {
        m_button = Qt::NoButton;
        int x2 = event->pos().x();
        int y2 = event->pos().y();
        m_radius = sqrt(pow((m_centerX - x2), 2) + pow((m_centerY - y2), 2));

        if (m_centerX > -1 && m_centerY > -1 && m_radius >= 0)
            emit outputCircle(m_centerX, m_centerY, m_radius, -1);

        update();
    }
    else if (m_button == Qt::RightButton && event->button() == Qt::RightButton)
    {
        m_button = Qt::NoButton;
		emit outputCircle(m_centerX, m_centerY, m_radius, m_state);
        m_radius = -1.0;
        m_state = 0;
		update();
	}
    else
    {
		return;
	}
}

void Myqlabel::mouseMoveEvent(QMouseEvent *event)
{
    if(!m_is_paintable)
        return;
    if (event->buttons() & Qt::LeftButton)
    {
        int x2 = event->pos().x(); //鼠标相对于所在控件的位置
        int y2 = event->pos().y();
        m_radius = (int)sqrt(((float)m_centerX-(float)x2)*((float)m_centerX-(float)x2)+((float)m_centerY-(float)y2)*((float)m_centerY-(float)y2));
        update();
    }
    else if (event->buttons() & Qt::RightButton)
    {
		m_centerX = m_centerX_old + event->pos().x() - m_x1;
        m_centerY = m_centerY_old + event->pos().y() - m_y1;
		update();
	}
    else
    {
		return;
	}
}

void Myqlabel::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(!m_is_paintable)
        return;
    int x2 = event->pos().x(); //鼠标相对于所在控件的位置
    int y2 = event->pos().y();
    //emit outputDelete(x2,y2);
}

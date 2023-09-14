#include "WSCNet2.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    //qRegisterMetaType<std::string>("std::string");
    WSCNet2 w;
    w.show();
    return a.exec();
}

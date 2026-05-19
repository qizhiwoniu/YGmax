#include "stdafx.h"
#include "YGmax.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    YGmax window;
    window.show();
    return app.exec();
}

/*!
 * \file
 * \brief Qt integration example.
 */

#include "ExampleQtStore.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSocketNotifier>
#include <QUrl>
#include <iostream>
#include <stored>

int main(int argc, char** argv)
{
	std::cout << stored::banner() << std::endl;

	// Initialize Qt.
	QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QGuiApplication app(argc, argv);

	// Initialize the store.
	stored::QExampleQtStore store;
	stored::Debugger debugger("qt");
	debugger.map(store);

	stored::DebugZmqLayer zmqLayer;
	zmqLayer.wrap(debugger);

	// Prepare polling the socket from Qt's event loop.
	QSocketNotifier socketNotifier(zmqLayer.fd(), QSocketNotifier::Read);

	QObject::connect(&socketNotifier, &QSocketNotifier::activated, [&]() {
		switch((errno = zmqLayer.recv())) {
		case 0:
		case EINTR:
		case EAGAIN:
			break; // Ok.
		default:
			perror("Cannot recv");
			app.exit(1);
		}
	});

	std::cout << "Connect via ZMQ to debug this application." << std::endl;

	// Initialize QML
	QQmlApplicationEngine engine;

	// Pass the store to QML.
	engine.rootContext()->setContextProperty("store", &store);

	// Load the main window.
	engine.load(QUrl("qrc:/main.qml"));

	// There we go!
	return app.exec();
}

#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <QScreen>
#include <QFont>
#include <QIcon>
#include <QDebug>
#include <QSettings>
#include "mainwindow.h"

// Custom message handler to filter debug output based on settings
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Check if debug mode is enabled
    QSettings settings("LianLi", "LConnect3");
    bool debugEnabled = settings.value("Debug/Enabled", false).toBool();
    
    // Always show warnings, critical messages, and fatal errors
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        QByteArray localMsg = msg.toLocal8Bit();
        const char *file = context.file ? context.file : "";
        const char *function = context.function ? context.function : "";
        switch (type) {
        case QtWarningMsg:
            fprintf(stderr, "Warning: %s\n", localMsg.constData());
            break;
        case QtCriticalMsg:
            fprintf(stderr, "Critical: %s\n", localMsg.constData());
            break;
        case QtFatalMsg:
            fprintf(stderr, "Fatal: %s\n", localMsg.constData());
            abort();
        default:
            break;
        }
        return;
    }
    
    // Only show debug/info messages if debug mode is enabled
    if (debugEnabled) {
        QByteArray localMsg = msg.toLocal8Bit();
        fprintf(stdout, "%s\n", localMsg.constData());
        fflush(stdout);
    }
}

int main(int argc, char *argv[])
{
    // High DPI scaling is enabled by default in Qt6
    QApplication app(argc, argv);
    
    // Install custom message handler to control debug output
    qInstallMessageHandler(customMessageHandler);
    
    // Set application properties
    app.setApplicationName("LL-Connect 3");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("L-Connect Linux");
    app.setWindowIcon(QIcon(":/icons/resources/logo.png"));
    
    // Set dark theme
    app.setStyle(QStyleFactory::create("Fusion"));
    
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    
    app.setPalette(darkPalette);
    
    // Set font scaling for High DPI - adjust for device pixel ratio
    QFont appFont = app.font();
    appFont.setStyleStrategy(QFont::PreferAntialias);
    
    // Debug DPI information and adjust font size
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        qDebug() << "Primary screen DPI:" << screen->logicalDotsPerInch();
        qDebug() << "Device pixel ratio:" << screen->devicePixelRatio();
        qDebug() << "Physical DPI:" << screen->physicalDotsPerInch();
        qDebug() << "Available geometry:" << screen->availableGeometry();
        qDebug() << "Screen geometry:" << screen->geometry();
        
        // Adjust font size based on device pixel ratio to prevent oversizing
        qreal dpr = screen->devicePixelRatio();
        if (dpr > 1.0) {
            // Scale down font size to compensate for DPI scaling
            int baseSize = appFont.pointSize();
            int adjustedSize = static_cast<int>(baseSize * 0.8); // Reduce by 20%
            appFont.setPointSize(qMax(8, adjustedSize)); // Minimum 8pt
            qDebug() << "Adjusted font size from" << baseSize << "to" << appFont.pointSize();
        }
    }
    
    app.setFont(appFont);
    
    // Create and show main window
    MainWindow window;
    bool minimizeOnStartup = false;
    {
        QSettings settings("LianLi", "LConnect3");
        minimizeOnStartup = settings.value("Startup/MinimizeOnStartup", false).toBool();
    }

    if (minimizeOnStartup) {
        window.showMinimized();
    } else {
        window.show();
    }
    
    return app.exec();
}

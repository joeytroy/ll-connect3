#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "pages/systeminfopage.h"
#include "pages/fanprofilepage.h"
#include "pages/lightingpage.h"
#include "pages/settingspage.h"
#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <QFont>
#include <QIcon>
#include <QTabBar>
#include <QList>
#include <QSettings>
#include <QCloseEvent>
#include <QTimer>
#include "version.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_currentPage(0)
{
    // Enable High DPI scaling for this window
    setAttribute(Qt::WA_NoSystemBackground, false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    
    // Setup UI from .ui file
    ui->setupUi(this);
    
    setupUI();
    applyStyles();
    
    // Set window properties with proper size policies
    setWindowTitle("LL-Connect 3");
    setMinimumSize(900, 510);
    
    // Restore window geometry from settings
    QSettings settings("LianLi", "LConnect3");
    QByteArray geometry = settings.value("windowGeometry").toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    // Window will use minimum size (900x510) if no saved geometry exists
    
    // Set window icon
    setWindowIcon(QIcon(":/icons/resources/logo.png"));
    
    // Set proper size policy for the main window
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

MainWindow::~MainWindow()
{
    // Save window geometry before destruction
    QSettings settings("LianLi", "LConnect3");
    settings.setValue("windowGeometry", saveGeometry());
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Save window geometry when closing
    QSettings settings("LianLi", "LConnect3");
    settings.setValue("windowGeometry", saveGeometry());
    event->accept();
}

void MainWindow::setupUI()
{
    // UI is already set up by ui->setupUi(this)
    // Now we need to connect signals and set up additional functionality
    
    // Set version label from generated version header (VERSION.md)
    if (ui->versionLabel) {
        ui->versionLabel->setText(QStringLiteral("v") + QString::fromUtf8(APP_VERSION_STRING));
    }
    
    // Connect navigation buttons
    connect(ui->systemInfoBtn, &QPushButton::clicked, this, &MainWindow::onNavigationClicked);
    connect(ui->fanProfileBtn, &QPushButton::clicked, this, &MainWindow::onNavigationClicked);
    connect(ui->lightingBtn, &QPushButton::clicked, this, &MainWindow::onNavigationClicked);
    connect(ui->settingsBtn, &QPushButton::clicked, this, &MainWindow::onNavigationClicked);
    
    // Create page instances
    m_systemInfoPage = new SystemInfoPage();
    m_fanProfilePage = new FanProfilePage();
    m_lightingPage = new LightingPage();
    m_settingsPage = new SettingsPage();
    
    // Set up cross-page references
    m_settingsPage->setLightingPage(m_lightingPage);
    
    // Add pages to content stack
    ui->contentStack->addWidget(m_systemInfoPage);
    ui->contentStack->addWidget(m_fanProfilePage);
    ui->contentStack->addWidget(m_lightingPage);
    ui->contentStack->addWidget(m_settingsPage);
    
    // Set initial page
    ui->contentStack->setCurrentIndex(0);
    ui->systemInfoBtn->setChecked(true);

    // Check for updates after the window is fully shown
    QTimer::singleShot(2000, m_settingsPage, &SettingsPage::checkForUpdatesOnStartup);
}

void MainWindow::setupSidebar()
{
    // Create sidebar with proper size policy
    m_sidebar = new QFrame();
    m_sidebar->setFixedWidth(210);
    m_sidebar->setObjectName("sidebar");
    m_sidebar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    m_sidebarLayout = new QVBoxLayout(m_sidebar);
    m_sidebarLayout->setContentsMargins(24, 28, 24, 28);
    m_sidebarLayout->setSpacing(18);

    // Logo + wordmark
    QHBoxLayout *logoLayout = new QHBoxLayout();
    logoLayout->setContentsMargins(0, 0, 0, 0);
    logoLayout->setSpacing(12);

    m_logoBadge = new QFrame();
    m_logoBadge->setObjectName("logoBadge");
    m_logoBadge->setFixedSize(42, 42);

    QVBoxLayout *badgeLayout = new QVBoxLayout(m_logoBadge);
    badgeLayout->setContentsMargins(0, 0, 0, 0);

    m_logoInitialLabel = new QLabel("L");
    m_logoInitialLabel->setObjectName("logoInitial");
    m_logoInitialLabel->setAlignment(Qt::AlignCenter);
    badgeLayout->addWidget(m_logoInitialLabel);

    m_logoLabel = new QLabel("LL-CONNECT 3");
    m_logoLabel->setObjectName("logoLabel");

    logoLayout->addWidget(m_logoBadge);
    logoLayout->addWidget(m_logoLabel);
    logoLayout->addStretch();

    m_sidebarLayout->addLayout(logoLayout);

    QFrame *sidebarDivider = new QFrame();
    sidebarDivider->setObjectName("sidebarDivider");
    sidebarDivider->setFixedHeight(1);
    sidebarDivider->setFrameShape(QFrame::HLine);
    sidebarDivider->setFrameShadow(QFrame::Plain);
    m_sidebarLayout->addWidget(sidebarDivider);

    // Navigation buttons
    m_systemInfoBtn = new QPushButton("System Info");
    m_systemInfoBtn->setObjectName("navButton");
    m_systemInfoBtn->setCheckable(true);
    m_systemInfoBtn->setChecked(true);
    connect(m_systemInfoBtn, &QPushButton::clicked, this, &MainWindow::onNavigationClicked);

    m_fanProfileBtn = new QPushButton("Fan/Pump Profile");
    m_fanProfileBtn->setObjectName("navButton");
    m_fanProfileBtn->setCheckable(true);
    connect(m_fanProfileBtn, &QPushButton::clicked, this, &MainWindow::onNavigationClicked);

    m_lightingBtn = new QPushButton("Quick/Sync Lighting");
    m_lightingBtn->setObjectName("navButton");
    m_lightingBtn->setCheckable(true);
    connect(m_lightingBtn, &QPushButton::clicked, this, &MainWindow::onNavigationClicked);


    m_settingsBtn = new QPushButton("Settings");
    m_settingsBtn->setObjectName("navButton");
    m_settingsBtn->setCheckable(true);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onNavigationClicked);

    const QList<QPushButton*> navButtons {
        m_systemInfoBtn,
        m_fanProfileBtn,
        m_lightingBtn,
        m_settingsBtn
    };

    for (QPushButton *button : navButtons) {
        button->setCursor(Qt::PointingHandCursor);
    }
    
    m_sidebarLayout->addWidget(m_systemInfoBtn);
    m_sidebarLayout->addWidget(m_fanProfileBtn);
    m_sidebarLayout->addWidget(m_lightingBtn);
    m_sidebarLayout->addWidget(m_settingsBtn);

    m_sidebarLayout->addStretch();

    // Version label (from VERSION.md at build time)
    m_versionLabel = new QLabel(QStringLiteral("v") + QString::fromUtf8(APP_VERSION_STRING));
    m_versionLabel->setObjectName("versionLabel");
    m_sidebarLayout->addWidget(m_versionLabel);
    
    m_mainLayout->addWidget(m_sidebar);
}

void MainWindow::setupTopTabs()
{
    // Create top widget with proper size policy
    m_topWidget = new QWidget();
    m_topWidget->setObjectName("topWidget");
    m_topWidget->setFixedHeight(72);
    m_topWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_topLayout = new QHBoxLayout(m_topWidget);
    m_topLayout->setContentsMargins(28, 18, 28, 18);
    m_topLayout->setSpacing(16);

    // Tab widget
    m_tabWidget = new QTabWidget();
    m_tabWidget->setObjectName("tabWidget");
    m_tabWidget->addTab(new QWidget(), "System Resource");
    m_tabWidget->addTab(new QWidget(), "System Specs");

    m_tabWidget->tabBar()->setExpanding(false);
    m_tabWidget->tabBar()->setDocumentMode(true);

    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    m_topLayout->addWidget(m_tabWidget);
    m_topLayout->addStretch();

    // Floating window toggle
    m_toggleLabel = new QLabel("Displays a floating system information window");
    m_toggleLabel->setObjectName("toggleLabel");
    m_topLayout->addWidget(m_toggleLabel);

    m_floatingToggle = new QCheckBox();
    m_floatingToggle->setObjectName("floatingToggle");
    m_floatingToggle->setChecked(true);
    m_topLayout->addWidget(m_floatingToggle);
    
    // Action buttons
    m_exportBtn = new QPushButton("Export");
    m_exportBtn->setObjectName("actionButton");

    m_importBtn = new QPushButton("Import");
    m_importBtn->setObjectName("actionButton");

    m_topLayout->addWidget(m_exportBtn);
    m_topLayout->addWidget(m_importBtn);
}

void MainWindow::setupMainContent()
{
    // Create content stack with proper size policy
    m_contentStack = new QStackedWidget();
    m_contentStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Create pages
    m_systemInfoPage = new SystemInfoPage();
    m_fanProfilePage = new FanProfilePage();
    m_lightingPage = new LightingPage();
    m_settingsPage = new SettingsPage();
    
    // Add pages to stack
    m_contentStack->addWidget(m_systemInfoPage);
    m_contentStack->addWidget(m_fanProfilePage);
    m_contentStack->addWidget(m_lightingPage);
    m_contentStack->addWidget(m_settingsPage);
    
    // Create main content widget
    QWidget *mainContentWidget = new QWidget();
    QVBoxLayout *mainContentLayout = new QVBoxLayout(mainContentWidget);
    mainContentLayout->setContentsMargins(0, 0, 0, 0);
    mainContentLayout->setSpacing(0);
    
    // mainContentLayout->addWidget(m_topWidget); // Removed top bar section
    mainContentLayout->addWidget(m_contentStack);
    
    m_mainLayout->addWidget(mainContentWidget);
}

void MainWindow::applyStyles()
{
    setStyleSheet(R"(
        QMainWindow {
            background-color: #050c1f;
        }

        #centralWidget {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #0b1c3f,
                                        stop:1 #09152c);
        }

        #sidebar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 #0d1734,
                                        stop:1 #081022);
            border-right: 1px solid rgba(255, 255, 255, 0.05);
        }

        #logoBadge {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #1c4ad6,
                                        stop:1 #2da6ff);
            border-radius: 21px;
        }

        #logoInitial {
            color: #ffffff;
            font-size: 14px;
            font-weight: 600;
            letter-spacing: 1px;
        }

        #logoLabel {
            color: #ffffff;
            font-size: 13px;
            font-weight: 600;
        }

        #sidebarDivider {
            background-color: rgba(255, 255, 255, 0.08);
            border: none;
            margin-top: 6px;
        }

        #navButton {
            background-color: transparent;
            color: rgba(255, 255, 255, 0.68);
            border: none;
            padding: 10px 16px;
            text-align: left;
            border-radius: 12px;
            font-size: 12px;
            font-weight: 500;
        }

        #navButton:hover {
            background: rgba(45, 166, 255, 0.14);
            color: #ffffff;
        }

        #navButton:checked {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #2464ff,
                                        stop:1 #2da6ff);
            color: #ffffff;
            font-weight: 600;
        }

        #versionLabel {
            color: rgba(255, 255, 255, 0.35);
            font-size: 12px;
            letter-spacing: 1px;
        }

        #topWidget {
            background: rgba(8, 18, 42, 0.82);
            border-bottom: 1px solid rgba(255, 255, 255, 0.04);
        }

        #tabWidget {
            background: transparent;
        }

        #tabWidget::pane {
            border: none;
            background: transparent;
        }

        #tabWidget QTabBar::tab {
            background: transparent;
            color: rgba(255, 255, 255, 0.56);
            padding: 8px 20px;
            margin-right: 16px;
            border: none;
            border-bottom: 2px solid transparent;
            font-size: 12px;
            font-weight: 500;
        }

        #tabWidget QTabBar::tab:selected {
            color: #2da6ff;
            border-bottom: 3px solid #2da6ff;
        }

        #tabWidget QTabBar::tab:hover {
            color: #ffffff;
        }

        #toggleLabel {
            color: rgba(255, 255, 255, 0.66);
            font-size: 11px;
        }

        #floatingToggle {
            spacing: 0px;
        }

        #floatingToggle::indicator {
            width: 54px;
            height: 28px;
            border-radius: 14px;
            background: rgba(20, 34, 66, 0.9);
            border: 1px solid rgba(255, 255, 255, 0.05);
            margin: 0px;
        }

        #floatingToggle::indicator:checked {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #2da6ff,
                                        stop:1 #1dd5ff);
            border: none;
        }

        #actionButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #1d5dff,
                                        stop:1 #2da6ff);
            color: #ffffff;
            border: none;
            padding: 8px 16px;
            border-radius: 14px;
            font-size: 11px;
            font-weight: 600;
        }

        #actionButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #3875ff,
                                        stop:1 #4dbeff);
        }
    )");
}

void MainWindow::onNavigationClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    // Uncheck all buttons
    ui->systemInfoBtn->setChecked(false);
    ui->fanProfileBtn->setChecked(false);
    ui->lightingBtn->setChecked(false);
    ui->settingsBtn->setChecked(false);
    
    // Check clicked button
    button->setChecked(true);
    
    // Switch to corresponding page
    if (button == ui->systemInfoBtn) {
        ui->contentStack->setCurrentWidget(m_systemInfoPage);
        m_currentPage = 0;
    } else if (button == ui->fanProfileBtn) {
        ui->contentStack->setCurrentWidget(m_fanProfilePage);
        m_currentPage = 1;
    } else if (button == ui->lightingBtn) {
        ui->contentStack->setCurrentWidget(m_lightingPage);
        m_currentPage = 2;
    } else if (button == ui->settingsBtn) {
        ui->contentStack->setCurrentWidget(m_settingsPage);
        m_currentPage = 3;
    }
}

void MainWindow::onTabChanged(int index)
{
    // Handle tab changes if needed
    Q_UNUSED(index)
}

/***********************************************************************
 * Copyright 2005  Max Howell <max.howell@methylblue.com>
 *           2007  Ian Monroe <ian@monroe.nu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ***********************************************************************/

#include "mainWindow.h"
#include "timeLabel.h"

#include <QActionGroup>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>        //::stateChanged()
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>      //ctor
#include <QMouseEvent>
#include <QObject>
#include <QTimer>
#include <QMimeData>
#include <QFileDialog>
#include <QApplication>
#include <QDebug>
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QDBusReply>
#include <QDBusInterface>
#include <QDBusUnixFileDescriptor>

#include <KAboutData>
#include <KNotificationRestrictions>
#include <KSqueezedTextLabel>
#include <KToggleFullScreenAction>
#include <KToolBar>
#include <KXMLGUIFactory>
#include <KProtocolInfo>
#include <KIO/UDSEntry>
#include <KSharedConfig>
#include <KJobWidgets>
#include <KCursor>
#include <KIO/StatJob>
#include <KLocalizedString>
#include <KActionMenu>

#include <Phonon/VideoWidget>
#include <Phonon/BackendCapabilities>

#include <Solid/Device>
#include <Solid/OpticalDisc>

#include "actions.h"
#include "discSelectionDialog.h"
#include "mpris2/mpris2.h"
#include "fullScreenToolBarHandler.h"
#include "messageBox.h"
#include "playDialog.h"  //::play()
#include "playlistFile.h"
#include "theStream.h"
#include "ui_videoSettingsWidget.h"
#include "videoWindow.h"
#include "audioView2.h"
#include "loadView.h"


namespace Dragon {

MainWindow *MainWindow::s_instance = 0;
/// @see codeine.h
QWidget* mainWindow() { return MainWindow::s_instance; }

MainWindow::MainWindow()
    : KXmlGuiWindow()
    , m_mainView( 0 )
    , m_audioView( 0 )
    , m_loadView( new LoadView(this) )
    , m_currentWidget( nullptr )
    , m_leftDock( 0 )
    , m_positionSlider( 0 )
    , m_volumeSlider( 0 )
    , m_timeLabel( 0 )
    , m_titleLabel( new QLabel( this ) )
    , m_playDialog( 0 )
    , m_menuToggleAction( 0 )
    , m_stopScreenSaver( 0 )
    , m_stopSleepCookie( -1 )
    , m_stopScreenPowerMgmtCookie( -1 )
    , m_profileMaxDays(30)
    , m_toolbarIsHidden(false)
    , m_statusbarIsHidden(false)
    , m_menuBarIsHidden(false)
    , m_FullScreenHandler( 0 )
{
    s_instance = this;
    setMouseTracking( true );

    m_mainView = new QStackedWidget(this);
    m_mainView->setMouseTracking( true );

    new VideoWindow( this );
    videoWindow()->setMouseTracking( true );

    m_positionSlider = videoWindow()->newPositionSlider();

    m_mainView->addWidget(m_loadView);
    m_audioView = new AudioView2(this);
    m_mainView->addWidget(m_audioView);
    m_mainView->addWidget(videoWindow());
    m_mainView->setCurrentWidget(m_loadView);

    setCentralWidget( m_mainView );

    setFocusProxy( videoWindow() ); // essential! See VideoWindow::event(), QEvent::FocusOut

    m_titleLabel->setMargin( 2 );
    m_titleLabel->setSizePolicy(QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed ));

    // FIXME work around a bug in KStatusBar
    // sizeHint width of statusbar seems to get stupidly large quickly
    statusBar()->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Maximum );

    setupActions();

    //setStandardToolBarMenuEnabled( false ); //bah to setupGUI()!
    //toolBar()->show(); //it's possible it would be hidden, but we don't want that as no UI way to show it!

    {
        KActionCollection* ac = actionCollection();
        KActionMenu *menuAction = 0;
#define make_menu( name, text ) \
        menuAction = new KActionMenu( text, this ); \
        menuAction->setObjectName( name ); \
        menuAction->setEnabled( false ); \
        connect( menuAction->menu(), SIGNAL(aboutToShow()), SLOT(aboutToShowMenu()) ); \
        ac->addAction( menuAction->objectName(), menuAction );
        make_menu( QLatin1String( "aspect_ratio_menu" ), i18n( "Aspect &Ratio" ) );
        make_menu( QLatin1String( "audio_channels_menu" ), i18n( "&Audio Channels" ) );
        make_menu( QLatin1String( "subtitle_channels_menu" ), i18n( "&Subtitles" ) );
#undef make_menu
        {
            m_aspectRatios = new QActionGroup( this );
            m_aspectRatios->setExclusive( true );
#define make_ratio_action( text, objectname, aspectEnum ) \
{ \
    QAction* ratioAction = new QAction( this ); \
    ratioAction->setText( text ); \
    ratioAction->setCheckable( true ); \
    m_aspectRatios->addAction( ratioAction ); \
    TheStream::addRatio( aspectEnum, ratioAction ); \
    ac->addAction( objectname, ratioAction ); \
    connect( ratioAction, SIGNAL(triggered()), this, SLOT(streamSettingChange()) ); \
}
            make_ratio_action( i18n( "Determine &Automatically" ), QLatin1String( "ratio_auto" ),  Phonon::VideoWidget::AspectRatioAuto );
            make_ratio_action( i18n( "&4:3" ), QLatin1String( "ratio_golden" ), Phonon::VideoWidget::AspectRatio4_3 );
            make_ratio_action( i18n( "Ana&morphic (16:9)" ), QLatin1String( "ratio_anamorphic" ), Phonon::VideoWidget::AspectRatio16_9 );
            make_ratio_action( i18n( "&Window Size" ), QLatin1String( "ratio_window" ), Phonon::VideoWidget::AspectRatioWidget );
#undef make_ratio_action
            ac->action( QLatin1String( "ratio_auto" ) )->setChecked( true );
            ac->action( QLatin1String( "aspect_ratio_menu" ) )->menu()->addActions( m_aspectRatios->actions() );
        }
    }

    setupGUI(); //load xml dragonplayerui.rc file
    //must be done after setupGUI:
    {
        toolBar()->setAllowedAreas( Qt::TopToolBarArea | Qt::BottomToolBarArea );
        toolBar()->setFloatable( false );
    }
    KXMLGUIClient::stateChanged( QLatin1String( "empty" ) );

    //"faster" startup
    //TODO if we have a size stored for this video, do the "faster" route
    QTimer::singleShot( 0, this, SLOT(init()) );
    QApplication::setOverrideCursor( Qt::WaitCursor );
}

void
MainWindow::init()
{
    //connect the stuff in loadView
    connect( m_loadView, SIGNAL(openDVDPressed()), this, SLOT(playDisc()) );
    connect( m_loadView, SIGNAL(openFilePressed()), this, SLOT(openFileDialog()) );
    connect( m_loadView, SIGNAL(openStreamPressed()), this, SLOT(openStreamDialog()) );
    connect( m_loadView, SIGNAL(loadUrl(QUrl)), this, SLOT(open(QUrl)) );

    //connect the video player
    connect( engine(), SIGNAL(stateUpdated(Phonon::State,Phonon::State)), this, SLOT(engineStateChanged(Phonon::State)) );
    connect( engine(), SIGNAL(currentSourceChanged(Phonon::MediaSource)), this, SLOT(engineMediaChanged(Phonon::MediaSource)) );
    connect( engine(), SIGNAL(seekableChanged(bool)), this, SLOT(engineSeekableChanged(bool)) );
    connect( engine(), SIGNAL(metaDataChanged()), this, SLOT(engineMetaDataChanged()) );
    connect( engine(), SIGNAL(hasVideoChanged(bool)), this, SLOT(engineHasVideoChanged(bool)) );

    connect( engine(), SIGNAL(subChannelsChanged(QList<QAction*>)), this, SLOT(subChannelsChanged(QList<QAction*>)) );
    connect( engine(), SIGNAL(audioChannelsChanged(QList<QAction*>)), this, SLOT(audioChannelsChanged(QList<QAction*>)) );
    connect( engine(), SIGNAL(mutedChanged(bool)), this, SLOT(mutedChanged(bool)) );

    connect( engine(), &VideoWindow::finished, this, &MainWindow::toggleLoadView );

    if( !engine()->init() ) {
        KMessageBox::error( this, i18n(
                                "<qt>Phonon could not be successfully initialized. Dragon Player will now exit.</qt>") );
        QApplication::exit( 2 );
    }

    //would be dangerous for these to happen before the videoWindow() is initialised
    setAcceptDrops( true );
    connect( statusBar(), SIGNAL(messageChanged(QString)), engine(), SLOT(showOSD(QString)) );
    //statusBar()->insertPermanentItem( "hello world", 0, 0 );
    m_timeLabel = new TimeLabel( statusBar() );
    connect( videoWindow(), SIGNAL(tick(qint64)), m_timeLabel, SLOT(setCurrentTime(qint64)) );
    connect( videoWindow(), SIGNAL(totalTimeChanged(qint64)), m_timeLabel, SLOT(setTotalTime(qint64)) );
    statusBar()->addPermanentWidget( m_titleLabel, 100 );
    statusBar()->addPermanentWidget( m_timeLabel );

    new Mpris2(this);

    QApplication::restoreOverrideCursor();
    engineStateChanged(Phonon::StoppedState);//set everything as it would be in stopped state
    engineSeekableChanged(false);
}

MainWindow::~MainWindow()
{
    hide(); //so we appear to have quit, and then sound fades out below
    releasePowerSave();
    delete videoWindow(); //fades out sound in dtor
}

void MainWindow::closeEvent (QCloseEvent *event)
{
    mainWindow()->setWindowState( Qt::WindowNoState );
    // restore the following user ui settings which are changed in full screen mode
    if (mainWindow()->isFullScreen()) {
        statusBar()->setHidden( m_statusbarIsHidden );
        toolBar()->setHidden( m_toolbarIsHidden );
        menuBar()->setHidden( m_menuBarIsHidden );
    }

    KMainWindow::closeEvent( event );
}

void MainWindow::wheelEvent (QWheelEvent *event)
{
    if (event->delta() > 0)
        engine()->increaseVolume();
    else
        engine()->decreaseVolume();
    event->accept();
}

void
MainWindow::setupActions()
{
    KActionCollection * const ac = actionCollection();

    KStandardAction::quit( qApp, SLOT(closeAllWindows()), ac );

    KStandardAction::open( this, SLOT(toggleLoadView()), ac )->setText( i18n("Play &Media...") );

#define addToAc( X ) ac->addAction( X->objectName(), X );

    KToggleFullScreenAction* toggleFullScreen = new KToggleFullScreenAction( this, ac );
    toggleFullScreen->setObjectName( QLatin1String( "fullscreen" ) );
    ac->setDefaultShortcuts(toggleFullScreen, QList<QKeySequence>() << Qt::Key_F << KStandardShortcut::fullScreen());
    toggleFullScreen->setAutoRepeat( false );
    connect( toggleFullScreen, SIGNAL(toggled(bool)), Dragon::mainWindow(), SLOT(setFullScreen(bool)) );
    addToAc( toggleFullScreen );

    new PlayAction( this, SLOT(play()), ac );
    new VolumeAction( this, SLOT(toggleVolumeSlider(bool)), ac );

    m_menuToggleAction =
            static_cast<KToggleAction*>(ac->addAction(KStandardAction::ShowMenubar,
                                                      menuBar(),
                                                      SLOT(setVisible(bool))));

    QAction *action = new QAction(i18nc("@action", "Increase Volume"), ac);
    action->setObjectName(QLatin1String("volume_inc"));
    connect(action, SIGNAL(triggered()), engine(), SLOT(increaseVolume()));
    addToAc(action);

    action = new QAction(i18nc("@action", "Decrease Volume"), ac);
    action->setObjectName(QLatin1String("volume_dec"));
    connect(action, SIGNAL(triggered()), engine(), SLOT(decreaseVolume()));
    addToAc(action);

    QAction* playerStop = new QAction( QIcon::fromTheme(QLatin1String( "media-playback-stop" )), i18n("Stop"), ac );
    playerStop->setObjectName( QLatin1String( "stop" ) );
    ac->setDefaultShortcuts(playerStop, QList<QKeySequence>() << Qt::Key_S << Qt::Key_MediaStop);
    connect( playerStop, SIGNAL(triggered()), this, SLOT(stop()) );
    addToAc( playerStop )

    KToggleAction* mute = new KToggleAction( QIcon::fromTheme(QLatin1String( "player-volume-muted" )), i18nc( "Mute the sound output", "Mute"), ac );
    mute->setObjectName( QLatin1String( "mute" ) );
    ac->setDefaultShortcut(mute, Qt::Key_M);
    connect( mute, SIGNAL(toggled(bool)), videoWindow(), SLOT(mute(bool)) );
    addToAc( mute )

    QAction* resetZoom = new QAction( QIcon::fromTheme(QLatin1String( "zoom-fit-best" )), i18n("Reset Video Scale"), ac );
    resetZoom->setObjectName( QLatin1String( "reset_zoom" ) );
    ac->setDefaultShortcut(resetZoom, Qt::Key_Equal);
    connect( resetZoom, SIGNAL(triggered()), videoWindow(), SLOT(resetZoom()) );
    addToAc( resetZoom )

    QAction* dvdMenu = new QAction( QIcon::fromTheme(QLatin1String( "media-optical-video" )), i18n("Menu Toggle"), ac );
    dvdMenu->setObjectName( QLatin1String( "toggle_dvd_menu" ) );
    ac->setDefaultShortcut(dvdMenu, Qt::Key_R);
    connect( dvdMenu, SIGNAL(triggered()), engine(), SLOT(toggleDVDMenu()) );
    addToAc( dvdMenu )

    QWidgetAction* positionSlider = new QWidgetAction( ac );
    positionSlider->setObjectName( QLatin1String( "position_slider" ) );
    positionSlider->setText(i18n("Position Slider"));
    positionSlider->setDefaultWidget( m_positionSlider );
    addToAc( positionSlider )

    QAction* videoSettings = new QAction( i18n("Video Settings"), ac );
    videoSettings->setObjectName( QLatin1String( "video_settings" ) );
    videoSettings->setCheckable( true );
    connect( videoSettings, SIGNAL(toggled(bool)), this, SLOT(toggleVideoSettings(bool)) );
    addToAc( videoSettings )

    QAction* uniqueToggle =
            new QAction( i18nc("@action:inmenu Whether only one instance of dragon can be started"
                               " and will be reused when the user tries to play another file.",
                               "One Instance Only"), ac );
    uniqueToggle->setObjectName( QLatin1String( "unique" ) );
    uniqueToggle->setCheckable( true );
    uniqueToggle->setChecked( !KSharedConfig::openConfig()->group("KDE").readEntry("MultipleInstances", QVariant(false)).toBool() );
    connect( uniqueToggle, SIGNAL(toggled(bool)), this, SLOT(toggleUnique(bool)) );
    addToAc( uniqueToggle )

    QAction* prev_chapter = new QAction( QIcon::fromTheme(QLatin1String( "media-skip-backward" )), i18n("Previous"), ac );
    prev_chapter->setObjectName( QLatin1String( "prev_chapter" ) );
    ac->setDefaultShortcuts(prev_chapter, QList<QKeySequence>() << Qt::Key_Comma << Qt::Key_MediaPrevious);
    connect( prev_chapter, SIGNAL(triggered()), engine(), SLOT(prevChapter()) );
    addToAc( prev_chapter )

    QAction* next_chapter = new QAction( QIcon::fromTheme(QLatin1String( "media-skip-forward" )), i18n("Next"), ac );
    next_chapter->setObjectName( QLatin1String( "next_chapter" ) );
    ac->setDefaultShortcuts(next_chapter, QList<QKeySequence>() << Qt::Key_Period << Qt::Key_MediaNext);
    connect( next_chapter, SIGNAL(triggered()), engine(), SLOT(nextChapter()) );
    addToAc( next_chapter )

    // xgettext: no-c-format
    QAction* tenPercentBack = new QAction( QIcon::fromTheme(QLatin1String( "media-seek-backward" )), i18n("Return 10% Back"), ac );
    tenPercentBack->setObjectName( QLatin1String( "ten_percent_back" ) );
    ac->setDefaultShortcut(tenPercentBack, Qt::Key_PageUp);
    connect( tenPercentBack, SIGNAL(triggered()), engine(), SLOT(tenPercentBack()) );
    addToAc( tenPercentBack )

    // xgettext: no-c-format
    QAction* tenPercentForward = new QAction( QIcon::fromTheme(QLatin1String( "media-seek-forward" )), i18n("Go 10% Forward"), ac );
    tenPercentForward->setObjectName( QLatin1String( "ten_percent_forward" ) );
    ac->setDefaultShortcut(tenPercentForward, Qt::Key_PageDown);
    connect( tenPercentForward, SIGNAL(triggered()), engine(), SLOT(tenPercentForward()) );
    addToAc( tenPercentForward )

    QAction* tenSecondsBack = new QAction( QIcon::fromTheme(QLatin1String( "media-seek-backward" )), i18n("Return 10 Seconds Back"), ac );
    tenSecondsBack->setObjectName( QLatin1String( "ten_seconds_back" ) );
    ac->setDefaultShortcut(tenSecondsBack, Qt::Key_Minus);
    connect( tenSecondsBack, SIGNAL(triggered()), engine(), SLOT(tenSecondsBack()) );
    addToAc( tenSecondsBack )

    QAction* tenSecondsForward = new QAction( QIcon::fromTheme(QLatin1String( "media-seek-forward" )), i18n("Go 10 Seconds Forward"), ac );
    tenSecondsForward->setObjectName( QLatin1String( "ten_seconds_forward" ) );
    ac->setDefaultShortcut(tenSecondsForward, Qt::Key_Plus);
    connect( tenSecondsForward, SIGNAL(triggered()), engine(), SLOT(tenSecondsForward()) );
    addToAc( tenSecondsForward )
    #undef addToAc
}

void
MainWindow::toggleUnique( bool unique )
{
    KSharedConfig::Ptr cfg = KSharedConfig::openConfig(); //kf5 FIXME? this might not work w/o KUniqueApplication
    cfg->group("KDE").writeEntry("MultipleInstances", !unique);
    cfg->sync();
}

void
MainWindow::toggleVideoSettings( bool show )
{
    if( show ) {
        m_leftDock = new QDockWidget( this );
        m_leftDock->setObjectName( QLatin1String("left_dock" ));
        m_leftDock->setFeatures( QDockWidget::NoDockWidgetFeatures );
        QWidget* videoSettingsWidget = new QWidget( m_leftDock );
        m_leftDock->setWidget( videoSettingsWidget );
        Ui::VideoSettingsWidget ui;
        ui.setupUi( videoSettingsWidget );
        KGuiItem::assign(ui.defaultsButton, KStandardGuiItem::defaults());
        KGuiItem::assign(ui.closeButton, KStandardGuiItem::closeWindow());
        videoSettingsWidget->adjustSize();
        addDockWidget( Qt::LeftDockWidgetArea, m_leftDock );
        m_sliders.clear();
        m_sliders << ui.brightnessSlider << ui.contrastSlider << ui.hueSlider <<  ui.saturationSlider;
        updateSliders();
        foreach( QSlider* slider, m_sliders )
            connect( slider, SIGNAL(valueChanged(int)), engine(), SLOT(settingChanged(int)) );

        connect( ui.defaultsButton, SIGNAL(clicked(bool)), this, SLOT(restoreDefaultVideoSettings()) );
        connect( ui.closeButton, SIGNAL(clicked(bool)), action( "video_settings" ), SLOT(setChecked(bool)) );
        connect( ui.closeButton, SIGNAL(clicked(bool)), m_leftDock, SLOT(deleteLater()) );
    } else {
        m_sliders.clear();
        delete m_leftDock;
    }
}

void
MainWindow::restoreDefaultVideoSettings()
{
    foreach( QSlider* slider, m_sliders )
        slider->setValue(0);
}

void
MainWindow::toggleLoadView()
{
    if( m_mainView->currentWidget() == m_loadView ) {
        if (m_currentWidget && engine()->state() != Phonon::StoppedState) {
            if( m_mainView->indexOf(m_currentWidget) == -1 ) {
                m_mainView->addWidget(m_currentWidget);
            }
            m_mainView->setCurrentWidget(m_currentWidget);
        }
        if (engine()->state() == Phonon::StoppedState) {
            m_loadView->setThumbnail(nullptr);
        }
        engine()->isPreview(false);
    } else if( m_currentWidget != m_audioView ) {
        m_mainView->setCurrentWidget( m_loadView );
        if (m_currentWidget && engine()->state() != Phonon::StoppedState) {
            m_mainView->removeWidget(m_currentWidget);
            engine()->isPreview(true);
            m_loadView->setThumbnail(m_currentWidget);
        }
    } else {
        m_mainView->setCurrentWidget(m_loadView);
    }
}

void
MainWindow::toggleVolumeSlider( bool show )
{
    if( show ) {
        m_volumeSlider = engine()->newVolumeSlider();
        m_volumeSlider->setDisabled ( engine()->isMuted() );
        m_volumeSlider->setFocus(Qt::PopupFocusReason);

        m_muteCheckBox = new QCheckBox();
        m_muteCheckBox->setText( i18nc( "Mute the sound output", "Mute" ) );
        m_muteCheckBox->setChecked ( engine()->isMuted() );
        connect( m_muteCheckBox, SIGNAL(toggled(bool)), videoWindow(), SLOT(mute(bool)) );

        QVBoxLayout *layout = new QVBoxLayout();
        layout->addWidget(m_volumeSlider);
        layout->addWidget(m_muteCheckBox);

        QWidget *dock = new QWidget;
        dock->setLayout(layout);

        m_rightDock = new QDockWidget( this );
        m_rightDock->setFeatures( QDockWidget::NoDockWidgetFeatures );
        m_rightDock->setObjectName("volume_dock");
        dock->setParent( m_rightDock );
        m_rightDock->setWidget( dock );
        addDockWidget( Qt::RightDockWidgetArea, m_rightDock );
    } else {
        disconnect( m_muteCheckBox, SIGNAL(toggled(bool)), videoWindow(), SLOT(mute(bool)) );
        delete m_rightDock; // it's a QPointer, it will 0 itself
    }
}

void
MainWindow::mutedChanged( bool mute )
{
    if( m_rightDock ) {
        m_volumeSlider->setDisabled ( mute );
        m_muteCheckBox->setChecked ( mute );
    }
}

void MainWindow::stop()
{
    engine()->stop();
    toggleLoadView();
}

void
MainWindow::updateSliders()
{
    foreach( QSlider* slider, m_sliders )
        slider->setValue( engine()->videoSetting( slider->objectName() ) );
}

void
MainWindow::engineMessage( const QString &message )
{
    statusBar()->showMessage( message, 3500 );
}

bool
MainWindow::open( const QUrl &url )
{
    qDebug() << "Opening" << url;

    if( load( url ) ) {
        const int offset = (TheStream::hasProfile() && isFresh())
                           // adjust offset if we have session history for this video
                           ? TheStream::profile().readEntry<int>( "Position", 0 )
                           : 0;
        qDebug() << "Initial offset is "<< offset;
        engine()->loadSettings();
        updateSliders();
        return engine()->play( offset );
    }

    return false;
}

bool
MainWindow::load( const QUrl &url )
{
    //FileWatch the file that is opened

    if( url.isEmpty() ) {
        MessageBox::sorry( i18n( "Dragon Player was asked to open an empty URL; it cannot." ) );
        return false;
    }

    bool ret = false;

    PlaylistFile playlist( url );
    if( playlist.isPlaylist() ) {
        //TODO: problem is we return out of the function
        //statusBar()->message( i18n("Parsing playlist file...") );

        if( playlist.isValid() )
            ret = engine()->load( playlist.contents() );
        else {
            MessageBox::sorry( playlist.error() );
            return false;
        }
    }

    // local protocols like nepomuksearch:/ are not supported by xine
    // check if an UDS_LOCAL_PATH is defined.
    if (!ret && KProtocolInfo::protocolClass(url.scheme()) == QLatin1String(":local")) {
        //#define UDS_LOCAL_PATH (7 | KIO::UDS_STRING)
        KIO::StatJob * job = KIO::stat(url, KIO::StatJob::SourceSide, 2);
        KJobWidgets::setWindow(job, this);
        if (job->exec()) {
            KIO::UDSEntry e = job->statResult();
            const QString path = e.stringValue( KIO::UDSEntry::UDS_LOCAL_PATH );
            if( !path.isEmpty() )
                ret = engine()->load( QUrl::fromLocalFile( path ) );
        }
        job->deleteLater();
    }

    //let xine handle invalid, etc, QUrlS
    //TODO it handles non-existing files with bad error message
    if (!ret)
        ret = engine()->load( url );

    if( ret ) {
        m_currentWidget = nullptr;
        m_loadView->setThumbnail(nullptr);
    }
    return ret;
}

void
MainWindow::play()
{
    switch( engine()->state() ) {
    case Phonon::PlayingState:
        engine()->pause();
        break;
    case Phonon::PausedState:
        engine()->resume();
        if( m_mainView->currentWidget() == m_loadView )
            toggleLoadView();
        break;
    case Phonon::StoppedState:
        engine()->play();
        m_currentWidget = nullptr;
        m_loadView->setThumbnail(nullptr);
        break;
    default:
        break;
    }
}

void
MainWindow::openFileDialog()
{
    QStringList mimeFilter = Phonon::BackendCapabilities::availableMimeTypes();
    //temporary fixes for MimeTypes that Xine does support but it doesn't return - this is a Xine bug.
    mimeFilter << QLatin1String( "audio/x-flac");
    mimeFilter << QLatin1String( "video/mp4" );
    mimeFilter << QLatin1String( "application/x-cd-image" ); // added for *.iso images

    static QUrl lastDirectory;

    QFileDialog dlg(this, i18n("Select File to Play"));
    dlg.setAcceptMode(QFileDialog::AcceptOpen);
    dlg.setFileMode(QFileDialog::ExistingFile);
    dlg.setMimeTypeFilters(mimeFilter);
    dlg.selectMimeTypeFilter(QStringLiteral("application/octet-stream")); // by default don't restrict

    if (lastDirectory.isValid()) {
        dlg.setDirectoryUrl(lastDirectory);
    } else {
        dlg.setDirectory(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
    }

    dlg.exec();

    lastDirectory = dlg.directoryUrl();
    const QList<QUrl> urls = dlg.selectedUrls();

    if( urls.isEmpty() ) {
        qDebug() << Q_FUNC_INFO << "URL empty";
        return;
    } else {
        open( urls.first() );
    }
}

void
MainWindow::openStreamDialog()
{
    QUrl url = QUrl::fromUserInput(QInputDialog::getText( this, i18nc("@title:window", "Stream to Play"), i18n("Stream:") ));

    if( url.isEmpty() ) {
        qDebug() << "URL empty in MainWindow::openStreamDialog()";
        return;
    } else {
        open( url );
    }
}

void
MainWindow::playDisc()
{
    QList< Solid::Device > playableDiscs;
    {
        const QList< Solid::Device > deviceList = Solid::Device::listFromType( Solid::DeviceInterface::OpticalDisc );

        foreach( const Solid::Device &device, deviceList ) {
            const Solid::OpticalDisc* disc = device.as<const Solid::OpticalDisc>();
            if( disc ) {
                if( disc->availableContent() & ( Solid::OpticalDisc::VideoDvd | Solid::OpticalDisc::VideoCd | Solid::OpticalDisc::SuperVideoCd |  Solid::OpticalDisc::Audio ) )
                    playableDiscs << device;
            }
        }
    }
    if( !playableDiscs.isEmpty() ) {
        if( playableDiscs.size() > 1 ) { //more than one disc, show user a selection box
            qDebug() << "> 1 possible discs, showing dialog";
            new DiscSelectionDialog( this, playableDiscs );
        } else { //only one optical disc inserted, play whatever it is
            bool status = engine()->playDisc( playableDiscs.first() );
            qDebug() << "playing disc" << status ;
        }
    } else {
        engine()->playDvd();
        m_loadView->setThumbnail(nullptr);
        toggleLoadView();
        qDebug() << "no disc in drive or Solid isn't working";
    }
}

void
MainWindow::openRecentFile(const QUrl &url )
{
    m_playDialog->deleteLater();
    m_playDialog = 0;
    this->open( url );
}

void
MainWindow::setFullScreen( bool isFullScreen )
{
    qDebug() << "Setting full screen to " << isFullScreen;
    mainWindow()->setWindowState( (isFullScreen ? Qt::WindowFullScreen : Qt::WindowNoState ));

    if(isFullScreen) {
        m_statusbarIsHidden=statusBar()->isHidden();
        m_toolbarIsHidden=toolBar()->isHidden();
        m_menuBarIsHidden=menuBar()->isHidden();
        toolBar()->setHidden( false );
        statusBar()->setHidden( true );
        menuBar()->setHidden(true);
    } else {
        statusBar()->setHidden(m_statusbarIsHidden);
        toolBar()->setHidden(m_toolbarIsHidden);
        menuBar()->setHidden(m_menuBarIsHidden);
        // In case someone hit the shortcut while being in fullscreen, the action
        // would be out of sync.
        m_menuToggleAction->setChecked(!m_menuBarIsHidden);
    }
    if( m_leftDock )
        m_leftDock->setHidden( isFullScreen );
    // the right dock is handled by the tool bar handler

    if( isFullScreen ) {
        if (!m_FullScreenHandler)
            m_FullScreenHandler = new FullScreenToolBarHandler( this );
    } else {
        action( "fullscreen" )->setEnabled( videoWindow()->state() == Phonon::PlayingState
                                            || videoWindow()->state() ==  Phonon::PausedState);
        delete m_FullScreenHandler;
        m_FullScreenHandler = 0;
    }
}

void
MainWindow::showVolume( bool visible)
{
    if( m_rightDock )
        m_rightDock->setVisible( visible );
}

bool
MainWindow::volumeContains( const QPoint &mousePos )
{
    if ( m_rightDock )
        return m_rightDock->geometry().contains(mousePos);
    return false;
}

void
MainWindow::aboutToShowMenu()
{
    TheStream::aspectRatioAction()->setChecked( true );
    {
        const int subId = TheStream::subtitleChannel();
        const QList< QAction* > subs = action("subtitle_channels_menu")->menu()->actions();
        qDebug() << "subtitle #" << subId << " is going to be checked";
        foreach( QAction* subAction, subs ) {
            if( subAction->property( TheStream::CHANNEL_PROPERTY ).toInt() == subId ) {
                subAction->setChecked( true );
                break;
            }
            qDebug() << subAction->property( TheStream::CHANNEL_PROPERTY ).toInt() << " not checked.";
        }
    }
    {
        const int audioId = TheStream::audioChannel();
        const QList< QAction* > audios = action("audio_channels_menu")->menu()->actions();
        qDebug() << "audio #" << audioId << " is going to be checked";
        foreach( QAction* audioAction, audios ) {
            if( audioAction->property( TheStream::CHANNEL_PROPERTY ).toInt() == audioId ) {
                audioAction->setChecked( true );
                break;
            }
        }
    }
}

void
MainWindow::dragEnterEvent( QDragEnterEvent *e )
{
    e->setAccepted( e->mimeData()->hasUrls() );
}

void
MainWindow::dropEvent( QDropEvent *e )
{
    if( e->mimeData()->hasUrls() )
        this->open( e->mimeData()->urls().first() );
    else
        engineMessage( i18n("Sorry, no media was found in the drop") );
}

void
MainWindow::keyPressEvent( QKeyEvent *e )
{
    switch( e->key() ) {
    case Qt::Key_Left:
        engine()->relativeSeek( -5000 );
        break;
    case Qt::Key_Right:
        engine()->relativeSeek( 5000 );
        break;
    case Qt::Key_Escape:
        action("fullscreen")->setChecked( false );
    default: ;
    }
}

void
MainWindow::inhibitPowerSave()
{
    if (m_stopSleepCookie == -1) {
        QDBusInterface iface(QStringLiteral("org.freedesktop.login1"),
                             QStringLiteral("/org/freedesktop/login1"),
                             QStringLiteral("org.freedesktop.login1.Manager"),
                             QDBusConnection::systemBus());
        if (iface.isValid()) {
            QDBusReply<QDBusUnixFileDescriptor> reply;
            if (TheStream::hasVideo()) {
                reply = iface.call(QStringLiteral("Inhibit"),
                                   QStringLiteral("sleep:idle"),
                                   KAboutData::applicationData().componentName(),
                                   QStringLiteral("playing a video"),
                                   QStringLiteral("block"));
            } else {
                reply = iface.call(QStringLiteral("Inhibit"),
                                   QStringLiteral("sleep"),
                                   KAboutData::applicationData().componentName(),
                                   QStringLiteral("playing an audio"),
                                   QStringLiteral("block"));
            }
            if (reply.isValid()) {
                m_stopSleepCookie = reply.value().fileDescriptor();
            }
        }
    }
    // TODO: inhibit screen sleep. No viable API found.
    // https://git.reviewboard.kde.org/r/129651
    if (!m_stopScreenSaver && TheStream::hasVideo())
        m_stopScreenSaver = new KNotificationRestrictions(KNotificationRestrictions::ScreenSaver, i18nc("Notification inhibition reason", "playing a video"));
}

void
MainWindow::releasePowerSave()
{
    //stop suppressing sleep
    if (m_stopSleepCookie != -1) {
        ::close(m_stopSleepCookie);
        m_stopSleepCookie = -1;
    }

    //stop disabling screensaver
    delete m_stopScreenSaver; // It is always 0, I have been careful.
    m_stopScreenSaver = 0;
}

QMenu*
MainWindow::menu( const char *name )
{
    // KXMLGUI is "really good".
    return static_cast<QMenu*>(factory()->container( QLatin1String( name ), this ));
}

void
MainWindow::streamSettingChange()
{
    if( sender()->objectName().left( 5 ) == QLatin1String( "ratio" ) ) {
        TheStream::setRatio( dynamic_cast< QAction* > ( sender() ) );
    }
}

void
MainWindow::updateTitleBarText()
{
    if( !TheStream::hasMedia() ) {
        m_titleLabel->setText( i18n("No media loaded") );
    } else if( engine()->state() == Phonon::PausedState ) {
        m_titleLabel->setText( i18n("Paused") );
    } else {
        m_titleLabel->setText( TheStream::prettyTitle() );
    }
    qDebug() << "set titles ";
}

#define CHANNELS_CHANGED( function, actionName ) \
    void \
    MainWindow::function( QList< QAction* > subActions ) \
{ \
    if( subActions.size() <= 2 ) \
    action( actionName )->setEnabled( false ); \
    else \
{ \
    action( actionName )->menu()->addActions( subActions ); \
    action( actionName )->setEnabled( true ); \
} \
}

CHANNELS_CHANGED( subChannelsChanged  , "subtitle_channels_menu" )
CHANNELS_CHANGED( audioChannelsChanged, "audio_channels_menu" )
#undef CHANNELS_CHANGED

/// Convenience class for other classes that need access to the actionCollection
KActionCollection*
actionCollection()
{
    return static_cast<MainWindow*>(mainWindow())->actionCollection();
}

/// Convenience class for other classes that need access to the actions
QAction*
action( const char *name )
{
    KActionCollection *actionCollection = 0;
    QAction *action = 0;

    if( mainWindow() )
        if( ( actionCollection = ((MainWindow*)mainWindow() )->actionCollection() ) )
            action = actionCollection->action(QLatin1String( name ) );
    if( !action )
        qDebug() << name;
    Q_ASSERT( mainWindow() );
    Q_ASSERT( actionCollection );
    Q_ASSERT( action );

    return action;
}

bool MainWindow::isFresh()
{
    QDate date = TheStream::profile().readEntry<QDate>("Date", QDate::currentDate());

    return (date.daysTo(QDate::currentDate()) < m_profileMaxDays) ? true : false;
}


} //namespace Dragon

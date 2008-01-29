/**
 * Copyright (C) 2002-2003 by Koos Vriezen <koos.vriezen@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#ifndef KMPLAYERVIEW_H
#define KMPLAYERVIEW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <qwidget.h>
#include <qimage.h>
#include <qtextedit.h>

#include <kdockwidget.h>
#include <kurl.h>
#include <kmediaplayer/view.h>

#include "kmplayersource.h"

#define MOUSE_INVISIBLE_DELAY 2000

class QPaintDevice;
class QPainter;
class QSlider;
class QLabel;
class KActionCollection;
class KAction;
class KShortcut;
class KStatusBar;
class KFindDialog;

namespace KMPlayer {

class View;
class ViewArea;
class ControlPanel;
class VolumeBar;
class Console;
class PlayListView;
class PlayListView;
class RootPlayListItem;
class PictureWidget;

typedef KStatusBar StatusBar;

/*
 * The console GUI
 */
class TextEdit : public QTextEdit {
public:
    TextEdit (QWidget * parent, View * view);
protected:
    void contextMenuEvent (QContextMenuEvent * e);
private:
    View * m_view;
};

/*
 * The infowindow GUI
 */
class InfoWindow : public QTextEdit {
public:
    InfoWindow (QWidget * parent, View * view);
    KDE_NO_EXPORT View * view () const { return m_view; }
protected:
    void contextMenuEvent (QContextMenuEvent * e);
private:
    View * m_view;
};

class KMPLAYER_NO_EXPORT PictureWidget : public QWidget {
    View *m_view;
public:
    PictureWidget (QWidget *parent, View *view);
    KDE_NO_CDTOR_EXPORT ~PictureWidget () {}
protected:
    void mousePressEvent (QMouseEvent *);
    void mouseMoveEvent (QMouseEvent *);
};

/*
 * The view containing ViewArea and playlist
 */
class KMPLAYER_EXPORT View : public KMediaPlayer::View {
    Q_OBJECT
public:
    enum ControlPanelMode {
        CP_Hide, CP_AutoHide, CP_Show, CP_Only /* no video widget */
    };
    enum StatusBarMode {
        SB_Hide, SB_Show, SB_Only /* no video widget */
    };

    View (QWidget *parent, const char *);
    ~View();

    void addText (const QString &, bool eol=false);
    void init (KActionCollection * ac);
    void reset ();
    //void print(QPrinter *pPrinter);

    TextEdit * console () const { return m_multiedit; }
    PictureWidget *picture () const KMPLAYER_NO_MBR_EXPORT { return m_picture; }
    KDE_NO_EXPORT ControlPanel * controlPanel () const {return m_control_panel;}
    KDE_NO_EXPORT StatusBar * statusBar () const {return m_status_bar;}
    KDE_NO_EXPORT PlayListView * playList () const { return m_playlist; }
    KDE_NO_EXPORT InfoWindow * infoPanel () const { return m_infopanel; }
    KDE_NO_EXPORT KDockArea * docArea () const { return m_dockarea; }
    KDE_NO_EXPORT ViewArea * viewArea () const { return m_view_area; }
    KDE_NO_EXPORT bool keepSizeRatio () const { return m_keepsizeratio; }
    void setKeepSizeRatio (bool b);
    void setControlPanelMode (ControlPanelMode m);
    void setStatusBarMode (StatusBarMode m);
    void setEraseColor (const QColor &);
    KDE_NO_EXPORT ControlPanelMode controlPanelMode () const { return m_controlpanel_mode; }
    KDE_NO_EXPORT StatusBarMode statusBarMode () const { return m_statusbar_mode; }
    void delayedShowButtons (bool show);
    bool isFullScreen () const;
    int statusBarHeight () const;
    KDE_NO_EXPORT bool editMode () const { return m_edit_mode; }
    bool setPicture (const QString & path);
    void setNoInfoMessages (bool b) { m_no_info = b; }
    void setViewOnly ();
    void setInfoPanelOnly ();
    void setPlaylistOnly ();
    void setEditMode (RootPlayListItem *, bool enable=true);
    void dragEnterEvent (QDragEnterEvent *);
    void dropEvent (QDropEvent *);
    KDE_NO_EXPORT void emitPictureClicked () { emit pictureClicked (); }
    /* raise video widget, might (auto) hides panel */
    void videoStart ();
    void playingStart ();
    /* shows panel */
    void playingStop ();
    void mouseMoved (int x, int y) KMPLAYER_NO_MBR_EXPORT;
public slots:
    void setVolume (int);
    void updateVolume ();
    void fullScreen ();
    void updateLayout ();
    void toggleShowPlaylist ();
    void toggleVideoConsoleWindow ();
    void setInfoMessage (const QString & msg);
    void setStatusMessage (const QString & msg);
signals:
    void urlDropped (const KURL::List & urls);
    void pictureClicked ();
    void fullScreenChanged ();
    void windowVideoConsoleToggled (bool show);
protected:
    void leaveEvent (QEvent *) KDE_NO_EXPORT;
    void timerEvent (QTimerEvent *) KDE_NO_EXPORT;
private:
    // console output
    TextEdit * m_multiedit;
    PictureWidget *m_picture;
    // widget that layouts m_widgetstack for ratio setting and m_control_panel
    ViewArea * m_view_area;
    // playlist widget
    PlayListView * m_playlist;
    // infopanel widget
    InfoWindow * m_infopanel;
    // all widget types
    KDockArea * m_dockarea;
    KDockWidget * m_dock_video;
    KDockWidget * m_dock_playlist;
    KDockWidget * m_dock_infopanel;
    QString tmplog;
    QImage m_image;
    ControlPanel * m_control_panel;
    StatusBar * m_status_bar;
    QSlider * m_volume_slider;
    const char * m_mixer_object;
    ControlPanelMode m_controlpanel_mode;
    ControlPanelMode m_old_controlpanel_mode;
    StatusBarMode m_statusbar_mode;
    int controlbar_timer;
    int infopanel_timer;
    bool m_keepsizeratio;
    bool m_playing;
    bool m_mixer_init;
    bool m_inVolumeUpdate;
    bool m_sreensaver_disabled;
    bool m_tmplog_needs_eol;
    bool m_revert_fullscreen;
    bool m_no_info;
    bool m_edit_mode;
};


} // namespace

#endif // KMPLAYERVIEW_H

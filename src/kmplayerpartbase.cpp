/**
 * Copyright (C) 2002-2003 by Koos Vriezen <koos ! vriezen ? xs4all ! nl>
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

#ifdef KDE_USE_FINAL
#undef Always
#endif

#include <config.h>
#include <qapplication.h>
#include <qcstring.h>
#include <qcursor.h>
#include <qtimer.h>
#include <qpair.h>
#include <qpushbutton.h>
#include <qpopupmenu.h>
#include <qslider.h>
#include <qvaluelist.h>
#include <qfile.h>
#include <qregexp.h>
#include <qtextstream.h>

#include <kmessagebox.h>
#include <kaboutdata.h>
#include <kdebug.h>
#include <kbookmarkmenu.h>
#include <kbookmarkmanager.h>
#include <kconfig.h>
#include <ksimpleconfig.h>
#include <kaction.h>
#include <kprocess.h>
#include <kstandarddirs.h>
#include <kmimetype.h>
#include <kprotocolinfo.h>
#include <kapplication.h>
#include <kio/job.h>
#include <kio/jobclasses.h>

#include "kmplayerpartbase.h"
#include "kmplayerview.h"
#include "kmplayerconfig.h"
#include "kmplayerprocess.h"
#include "kmplayer_smil.h"

namespace KMPlayer {
    
class BookmarkOwner : public KBookmarkOwner {
public:
    BookmarkOwner (PartBase *);
    KDE_NO_CDTOR_EXPORT virtual ~BookmarkOwner () {}
    void openBookmarkURL(const QString& _url);
    QString currentTitle() const;
    QString currentURL() const;
private:
    PartBase * m_player;
};

class BookmarkManager : public KBookmarkManager {
public:
    BookmarkManager (const QString &);
};

} // namespace

using namespace KMPlayer;

KDE_NO_CDTOR_EXPORT BookmarkOwner::BookmarkOwner (PartBase * player)
    : m_player (player) {}

KDE_NO_EXPORT void BookmarkOwner::openBookmarkURL (const QString & url) {
    m_player->openURL (KURL (url));
}

KDE_NO_EXPORT QString BookmarkOwner::currentTitle () const {
    return m_player->source ()->prettyName ();
}

KDE_NO_EXPORT QString BookmarkOwner::currentURL () const {
    return m_player->source ()->url ().url ();
}

inline BookmarkManager::BookmarkManager(const QString & bmfile)
  : KBookmarkManager (bmfile, false) {
}

//-----------------------------------------------------------------------------

PartBase::PartBase (QWidget * wparent, const char *wname,
                    QObject * parent, const char *name, KConfig * config)
 : KMediaPlayer::Player (wparent, wname ? wname : "kde_kmplayer_view", parent, name ? name : "kde_kmplayer_part"),
   m_config (config),
   m_view (new View (wparent, wname ? wname : "kde_kmplayer_view")),
   m_settings (new Settings (this, config)),
   m_process (0L),
   m_recorder (0L),
   m_source (0L),
   m_bookmark_menu (0L),
   m_record_timer (0),
   m_noresize (false),
   m_in_update_tree (false)
{
    m_players ["mplayer"] = new MPlayer (this, m_settings);
    m_players ["xine"] = new Xine (this, m_settings);
    m_players ["gst"] = new GStreamer (this, m_settings);
    m_recorders ["mencoder"] = new MEncoder (this, m_settings);
    m_recorders ["mplayerdumpstream"] = new MPlayerDumpstream(this, m_settings);
    m_recorders ["ffmpeg"] = new FFMpeg (this, m_settings);
    m_sources ["urlsource"] = new URLSource (this);

    QString bmfile = locate ("data", "kmplayer/bookmarks.xml");
    QString localbmfile = locateLocal ("data", "kmplayer/bookmarks.xml");
    if (localbmfile != bmfile) {
        kdDebug () << "cp " << bmfile << " " << localbmfile << endl;
        KProcess p;
        p << "/bin/cp" << QFile::encodeName (bmfile) << QFile::encodeName (localbmfile);
        p.start (KProcess::Block);
    }
    m_bookmark_manager = new BookmarkManager (localbmfile);
    m_bookmark_owner = new BookmarkOwner (this);
}

void PartBase::showConfigDialog () {
    m_settings->show ("URLPage");
}

KDE_NO_EXPORT void PartBase::showPlayListWindow () {
    m_view->toggleShowPlaylist ();
}

KDE_NO_EXPORT void PartBase::addBookMark (const QString & t, const QString & url) {
    KBookmarkGroup b = m_bookmark_manager->root ();
    b.addBookmark (m_bookmark_manager, t, KURL (url));
    m_bookmark_manager->emitChanged (b);
}

void PartBase::init (KActionCollection * action_collection) {
    KParts::Part::setWidget (m_view);
    m_view->init ();
    connect(m_settings, SIGNAL(configChanged()), this, SLOT(settingsChanged()));
    m_settings->readConfig ();
    m_settings->applyColorSetting (false);
    m_bPosSliderPressed = false;
    m_bookmark_menu = new KBookmarkMenu (m_bookmark_manager, m_bookmark_owner, m_view->controlPanel ()->bookmarkMenu (), action_collection, true, true);
    connect (m_view, SIGNAL (urlDropped (const KURL::List &)), this, SLOT (openURL (const KURL::List &)));
    connect (m_view->playList (), SIGNAL (addBookMark (const QString &, const QString &)), this, SLOT (addBookMark (const QString &, const QString &)));
    connect (m_view->playList (), SIGNAL (executed (QListViewItem *)), this, SLOT (playListItemSelected (QListViewItem *)));
    new KAction (i18n ("Edit playlist item"), 0, 0, m_view->playList (), SLOT (editCurrent ()), action_collection, "edit_playlist_item");
}

void PartBase::connectPanel (ControlPanel * panel) {
    panel->contrastSlider ()->setValue (m_settings->contrast);
    panel->brightnessSlider ()->setValue (m_settings->brightness);
    panel->hueSlider ()->setValue (m_settings->hue);
    panel->saturationSlider ()->setValue (m_settings->saturation);
    panel->volumeBar ()->setValue (m_settings->volume);
    connect (panel->button (ControlPanel::button_playlist), SIGNAL (clicked ()), this, SLOT (showPlayListWindow ()));
    connect (panel->button (ControlPanel::button_back), SIGNAL (clicked ()), this, SLOT (back ()));
    connect (panel->button (ControlPanel::button_play), SIGNAL (clicked ()), this, SLOT (play ()));
    connect (panel->button (ControlPanel::button_forward), SIGNAL (clicked ()), this, SLOT (forward ()));
    connect (panel->button (ControlPanel::button_pause), SIGNAL (clicked ()), this, SLOT (pause ()));
    connect (panel->button (ControlPanel::button_stop), SIGNAL (clicked ()), this, SLOT (stop ()));
    connect (panel->button (ControlPanel::button_record), SIGNAL (clicked()), this, SLOT (record()));
    connect (panel->volumeBar (), SIGNAL (volumeChanged (int)), this, SLOT (volumeChanged (int)));
    connect (panel->positionSlider (), SIGNAL (valueChanged (int)), this, SLOT (positionValueChanged (int)));
    connect (panel->positionSlider (), SIGNAL (sliderPressed()), this, SLOT (posSliderPressed()));
    connect (panel->positionSlider (), SIGNAL (sliderReleased()), this, SLOT (posSliderReleased()));
    connect (panel->contrastSlider (), SIGNAL (valueChanged(int)), this, SLOT (contrastValueChanged(int)));
    connect (panel->brightnessSlider (), SIGNAL (valueChanged(int)), this, SLOT (brightnessValueChanged(int)));
    connect (panel->hueSlider (), SIGNAL (valueChanged(int)), this, SLOT (hueValueChanged(int)));
    connect (panel->saturationSlider (), SIGNAL (valueChanged(int)), this, SLOT (saturationValueChanged(int)));
    panel->popupMenu()->connectItem (ControlPanel::menu_fullscreen, this, SLOT (fullScreen ()));
    panel->popupMenu ()->connectItem (ControlPanel::menu_config,
                                       this, SLOT (showConfigDialog ()));
    panel->popupMenu ()->connectItem (ControlPanel::menu_video,
                                      m_view, SLOT(toggleVideoConsoleWindow()));
    //connect (panel (), SIGNAL (clicked ()), m_settings, SLOT (show ()));
}

PartBase::~PartBase () {
    kdDebug() << "PartBase::~PartBase" << endl;
    m_view = (View*) 0;
    stop ();
    if (m_source)
        m_source->deactivate ();
    delete m_settings;
    delete m_bookmark_menu;
    delete m_bookmark_manager;
    delete m_bookmark_owner;
}

void PartBase::settingsChanged () {
    if (!m_view)
        return;
    if (m_settings->showcnfbutton)
        m_view->controlPanel()->button (ControlPanel::button_config)->show();
    else
        m_view->controlPanel()->button (ControlPanel::button_config)->hide();
    m_view->controlPanel()->enableRecordButtons (m_settings->showrecordbutton);
    if (m_settings->showplaylistbutton)
        m_view->controlPanel()->button (ControlPanel::button_playlist)->show();
    else
        m_view->controlPanel()->button (ControlPanel::button_playlist)->hide();
    if (!m_settings->showbroadcastbutton)
        m_view->controlPanel ()->broadcastButton ()->hide ();
    keepMovieAspect (m_settings->sizeratio);
    m_settings->applyColorSetting (true);
}

KMediaPlayer::View* PartBase::view () {
    return m_view;
}

void PartBase::setProcess (const char * name) {
    Process * process = name ? m_players [name] : 0L;
    if (m_process == process)
        return;
    if (!m_source)
        m_source = m_sources ["urlsource"];
    if (m_process)
        m_process->quit ();
    m_process = process;
    if (!process)
        return;
    if (m_process->playing ()) {
        m_view->controlPanel ()->setPlaying (true);
        m_view->controlPanel ()->showPositionSlider (!!m_source->length ());
        m_view->controlPanel ()->enableSeekButtons (m_source->isSeekable ());
    }
    emit processChanged (name);
}

void PartBase::setRecorder (const char * name) {
    Process * recorder = name ? m_recorders [name] : 0L;
    if (m_recorder == recorder)
        return;
    if (m_recorder)
        m_recorder->quit ();
    m_recorder = recorder;
}

extern const char * strGeneralGroup;

KDE_NO_EXPORT void PartBase::slotPlayerMenu (int id) {
    bool playing = m_process->playing ();
    const char * srcname = m_source->name ();
    QPopupMenu * menu = m_view->controlPanel ()->playerMenu ();
    ProcessMap::const_iterator pi = m_players.begin(), e = m_players.end();
    unsigned i = 0;
    for (; pi != e && i < menu->count(); ++pi) {
        Process * proc = pi.data ();
        if (!proc->supports (srcname))
            continue;
        int menuid = menu->idAt (i);
        menu->setItemChecked (menuid, menuid == id);
        if (menuid == id) {
            m_settings->backends [srcname] = proc->name ();
            if (playing && strcmp (m_process->name (), proc->name ()))
                m_process->quit ();
            setProcess (proc->name ());
        }
        ++i;
    }
    if (playing)
        setSource (m_source); // re-activate
}

void PartBase::updatePlayerMenu (ControlPanel * panel) {
    if (!m_view || !m_process)
        return;
    QPopupMenu * menu = panel->playerMenu ();
    menu->clear ();
    if (!m_source)
        return;
    const ProcessMap::const_iterator e = m_players.end();
    int id = 0; // if multible parts, id's should be the same for all menu's
    for (ProcessMap::const_iterator i = m_players.begin(); i != e; ++i) {
        Process * p = i.data ();
        if (p->supports (m_source->name ())) {
            menu->insertItem (p->menuName (), this, SLOT (slotPlayerMenu (int)), 0, id++);
            if (i.data() == m_process)
                menu->setItemChecked (id-1, true);
        }
    }
}

void PartBase::connectSource (Source * old_source, Source * source) {
    if (old_source) {
        disconnect (old_source, SIGNAL(endOfPlayItems ()), this, SLOT(stop ()));
        disconnect (old_source, SIGNAL (dimensionsChanged ()),
                    this, SLOT (sourceHasChangedDimensions ()));
        disconnect (old_source, SIGNAL (positioned (int)),
                    this, SLOT (positioned (int)));
        disconnect (old_source, SIGNAL (lengthFound (int)),
                    this, SLOT (lengthFound (int)));
        disconnect (old_source, SIGNAL (startPlaying ()),
                    this, SLOT (playingStarted ()));
        disconnect (old_source, SIGNAL (stopPlaying ()),
                    this, SLOT (playingStopped ()));
        disconnect (old_source, SIGNAL (loaded (int)),
                    this, SLOT (loaded (int)));
    }
    if (source) {
        connect (source, SIGNAL (endOfPlayItems ()), this, SLOT (stop ()));
        connect (source, SIGNAL (dimensionsChanged ()),
                this, SLOT (sourceHasChangedDimensions ()));
        connect (source, SIGNAL (positioned(int)), this, SLOT(positioned(int)));
        connect (source, SIGNAL (lengthFound(int)),this,SLOT(lengthFound(int)));
        connect (source, SIGNAL (startPlaying()), this, SLOT(playingStarted()));
        connect (source, SIGNAL (stopPlaying ()), this, SLOT(playingStopped()));
        connect (source, SIGNAL (loaded (int)), this, SLOT (loaded (int)));
    }
}

void PartBase::setSource (Source * _source) {
    Source * old_source = m_source;
    if (m_source) {
        m_source->deactivate ();
        stop ();
        if (m_view) {
            m_view->reset ();
            m_view->setInfoMessage (QString::null);
        }
        disconnect (m_source, SIGNAL (startRecording ()),
                    this, SLOT (recordingStarted ()));
    }
    if (m_view) {
        m_view->controlPanel ()->setAutoControls (true);
        m_view->controlPanel ()->enableRecordButtons (m_settings->showrecordbutton);
        if (!m_settings->showcnfbutton)
            m_view->controlPanel()->button(ControlPanel::button_config)->hide();
        if (!m_settings->showplaylistbutton)
          m_view->controlPanel()->button(ControlPanel::button_playlist)->hide();
    }
    QString p = m_settings->backends [_source->name()];
    if (p.isEmpty ()) {
        m_config->setGroup (strGeneralGroup);
        p = m_config->readEntry (_source->name (), "");
    }
    if (p.isEmpty () || !m_players.contains (p) || !m_players [p]->supports (_source->name ())) {
        p.truncate (0);
        if (!m_process->supports (_source->name ())) {
            ProcessMap::const_iterator i, e = m_players.end();
            for (i = m_players.begin(); i != e; ++i)
                if (i.data ()->supports (_source->name ())) {
                    p = QString (i.data ()->name ());
                    break;
                }
        } else
            p = QString (m_process->name ());
    }
    if (!p.isEmpty ()) {
        if (!m_process || p != m_process->name ())
            setProcess (p.ascii ());
        m_settings->backends [_source->name()] = m_process->name ();
    }
    m_source = _source;
    connectSource (old_source, m_source);
    m_process->setSource (m_source);
    connect (m_source, SIGNAL(startRecording()), this,SLOT(recordingStarted()));
    m_source->init ();
    if (m_view && m_view->viewer ()) {
        updatePlayerMenu (m_view->controlPanel ());
        m_view->viewer ()->setAspect (0.0);
    }
    if (m_source) QTimer::singleShot (0, m_source, SLOT (activate ()));
    emit sourceChanged (old_source, m_source);
}

KDE_NO_EXPORT void PartBase::changeURL (const QString & url) {
    emit urlChanged (url);
}

bool PartBase::isSeekable (void) const {
    return m_source ? m_source->isSeekable () : false;
}

bool PartBase::hasLength () const {
    return m_source ? m_source->hasLength () : false; 
}

unsigned long PartBase::length () const {
    return m_source ? m_source->length () : 0;
}

bool PartBase::openURL (const KURL & url) {
    kdDebug () << "PartBase::openURL " << url.url() << url.isValid () << endl;
    if (!m_view) return false;
    stop ();
    Source * src = (url.isEmpty () ? m_sources ["urlsource"] : (!url.protocol ().compare ("kmplayer") && m_sources.contains (url.host ()) ? m_sources [url.host ()] : m_sources ["urlsource"]));
    src->setSubURL (KURL ());
    src->setURL (url);
    src->setIdentified (false);
    setSource (src);
    return true;
}

bool PartBase::openURL (const KURL::List & urls) {
    if (urls.size () == 1) {
        openURL (urls[0]);
    } else {
        openURL (KURL ());
        NodePtr d = m_source->document ();
        if (d)
            for (int i = 0; i < urls.size (); i++)
                d->appendChild ((new GenericURL (d, urls [i].url ()))->self ());
    }
    return true;
}

bool PartBase::closeURL () {
    stop ();
    if (m_view) {
        m_view->viewer ()->setAspect (0.0);
        m_view->reset ();
    }
    return true;
}

bool PartBase::openFile () {
    return false;
}

void PartBase::keepMovieAspect (bool b) {
    if (m_view) {
        m_view->setKeepSizeRatio (b);
        if (m_source)
            m_view->viewer ()->setAspect (b ? m_source->aspect () : 0.0);
    }
}

static const char * statemap [] = {
    "NotRunning", "Ready", "Buffering", "Playing"
};

void PartBase::recordingStarted () {
    if (m_settings->replayoption == Settings::ReplayAfter)
        m_record_timer = startTimer (1000 * m_settings->replaytime);
}

void PartBase::recordingStopped () {
    killTimer (m_record_timer);
    m_record_timer = 0;
    if (m_settings->replayoption == Settings::ReplayFinished ||
            (m_settings->replayoption == Settings::ReplayAfter && !playing ())) {
        Recorder * rec = dynamic_cast <Recorder*> (m_recorder);
        if (rec)
            openURL (rec->recordURL ());
    }
}

void PartBase::timerEvent (QTimerEvent * e) {
    kdDebug () << "record timer event" << (m_recorder->playing () && !playing ()) << endl;
    killTimer (e->timerId ());
    m_record_timer = 0;
    if (m_recorder->playing () && !playing ()) {
        Recorder * rec = dynamic_cast <Recorder*> (m_recorder);
        if (rec)
            openURL (rec->recordURL ());
    }
}

void PartBase::playingStarted () {
    //m_view->viewer ()->setAspect (m_source->aspect ());
    if (m_view) {
        m_view->controlPanel ()->setPlaying (true);
        m_view->controlPanel ()->showPositionSlider (!!m_source->length ());
        m_view->controlPanel ()->enableSeekButtons (m_source->isSeekable ());
        if (m_settings->autoadjustvolume && m_process)
           m_process->volume(m_view->controlPanel()->volumeBar()->value(),true);
    }
    emit loading (100);
}

void PartBase::playingStopped () {
    kdDebug () << "playingStopped " << this << endl;
    if (m_view) {
        m_view->controlPanel ()->setPlaying (false);
        m_view->reset ();
    }
    m_bPosSliderPressed = false;
}

KDE_NO_EXPORT void PartBase::positioned (int pos) {
    if (m_view && !m_bPosSliderPressed)
        m_view->controlPanel ()->setPlayingProgress (pos);
}

void PartBase::loaded (int percentage) {
    if (m_view && !m_bPosSliderPressed)
        m_view->controlPanel ()->setLoadingProgress (percentage);
    emit loading (percentage);
}

void PartBase::lengthFound (int len) {
    if (!m_view) return;
    m_view->controlPanel ()->setPlayingLength (len);
}

unsigned long PartBase::position () const {
    return m_source ? 100 * m_source->position () : 0;
}

void PartBase::pause () {
    if (m_process)
        m_process->pause ();
}

void PartBase::back () {
    m_source->backward ();
}

void PartBase::forward () {
    m_source->forward ();
}

static NodePtr depthFirstFindMrl (NodePtr e) {
    if (e->expose ()) {
        if (e->isMrl ())
            return e;
        for (NodePtr c = e->firstChild (); c; c = c->nextSibling ()) {
            NodePtr m = depthFirstFindMrl (c);
            if (m)
                return m;
        }
    }
    return NodePtr ();
}

void PartBase::playListItemSelected (QListViewItem * item) {
    if (m_in_update_tree) return;
    ListViewItem * vi = static_cast <ListViewItem *> (item);
    if (vi->m_elm) {
        m_view->setInfoMessage (vi->m_elm->innerText ());
        NodePtr mrl = depthFirstFindMrl (vi->m_elm);
        if (mrl)
            m_source->jump (mrl);
    } else if (vi->m_attr) {
        if (!strcasecmp (vi->m_attr->nodeName (), "src") ||
                !strcasecmp (vi->m_attr->nodeName (), "href") ||
                !strcasecmp (vi->m_attr->nodeName (), "data") ||
                !strcasecmp (vi->m_attr->nodeName (), "url") ||
                !strcasecmp (vi->m_attr->nodeName (), "value")) {
            QString src (vi->m_attr->nodeValue ());
            if (!src.isEmpty ()) {
                ListViewItem * pi = static_cast <ListViewItem*>(item->parent());
                if (pi) {
                    for (NodePtr e = pi->m_elm; e; e = e->parentNode ()) {
                        Mrl * mrl = e->mrl ();
                        if (mrl)
                            src = KURL (mrl->src, src).url ();
                    }
                    KURL url (src);
                    if (url.isValid ())
                        openURL (url);
                }
            }
        }
    } else
        updateTree (); // items already deleted
}

void PartBase::updateTree () {
    m_in_update_tree = true;
    if (m_view && m_source)
        m_view->playList ()->updateTree (m_source->document (), m_source->current ());
    m_in_update_tree = false;
}

void PartBase::record () {
    if (m_view) m_view->setCursor (QCursor (Qt::WaitCursor));
    if (m_recorder->playing ()) {
        m_recorder->stop ();
    } else {
        m_settings->show  ("RecordPage");
        m_view->controlPanel ()->setRecording (false);
    }
    if (m_view) m_view->setCursor (QCursor (Qt::ArrowCursor));
}

void PartBase::play () {
    if (!m_process || !m_view) return;
    if (m_process->state () == Process::NotRunning) {
        m_process->ready (m_view->viewer ());
    } else if (m_process->state () == Process::Ready) {
        m_source->playCurrent ();
    } else
        m_process->play (m_source, m_source->current ());
}

bool PartBase::playing () const {
    return m_process && m_process->state () > Process::Ready;
}

void PartBase::stop () {
    QPushButton * b = m_view ? m_view->controlPanel ()->button (ControlPanel::button_stop) : 0L;
    if (b) {
        if (!b->isOn ())
            b->toggle ();
        m_view->setCursor (QCursor (Qt::WaitCursor));
    }
    if (m_process)
        m_process->quit ();
    if (m_source)
        m_source->reset ();
    if (m_view) {
        m_view->setCursor (QCursor (Qt::ArrowCursor));
        if (b->isOn ())
            b->toggle ();
    }
}

void PartBase::seek (unsigned long msec) {
    if (m_process)
        m_process->seek (msec/100, true);
}

void PartBase::adjustVolume (int incdec) {
    m_process->volume (incdec, false);
}

void PartBase::increaseVolume () {
    if (m_view)
        m_view->controlPanel ()->volumeBar ()->setValue (m_view->controlPanel ()->volumeBar ()->value () + 2);
}

void PartBase::decreaseVolume () {
    if (m_view)
        m_view->controlPanel ()->volumeBar ()->setValue (m_view->controlPanel ()->volumeBar ()->value () - 2);
}

KDE_NO_EXPORT void PartBase::posSliderPressed () {
    m_bPosSliderPressed=true;
}

KDE_NO_EXPORT void PartBase::posSliderReleased () {
    m_bPosSliderPressed=false;
#if (QT_VERSION < 0x030200)
    const QSlider * posSlider = dynamic_cast <const QSlider *> (sender ());
#else
    const QSlider * posSlider = ::qt_cast<const QSlider *> (sender ());
#endif
    if (posSlider)
        m_process->seek (posSlider->value(), true);
}

KDE_NO_EXPORT void PartBase::volumeChanged (int val) {
    if (m_process) {
        m_settings->volume = val;
        m_process->volume (val, true);
    }
}

KDE_NO_EXPORT void PartBase::contrastValueChanged (int val) {
    m_settings->contrast = val;
    m_process->contrast (val, true);
}

KDE_NO_EXPORT void PartBase::brightnessValueChanged (int val) {
    m_settings->brightness = val;
    m_process->brightness (val, true);
}

KDE_NO_EXPORT void PartBase::hueValueChanged (int val) {
    m_settings->hue = val;
    m_process->hue (val, true);
}

KDE_NO_EXPORT void PartBase::saturationValueChanged (int val) {
    m_settings->saturation = val;
    m_process->saturation (val, true);
}

KDE_NO_EXPORT void PartBase::sourceHasChangedDimensions () {
    if (m_view && m_source) {
        m_view->viewer ()->setAspect (m_source->aspect ());
        m_view->updateLayout ();
    }
    emit sourceDimensionChanged ();
}

KDE_NO_EXPORT void PartBase::positionValueChanged (int pos) {
    QSlider * slider = ::qt_cast <QSlider *> (sender ());
    if (slider && slider->isEnabled ())
        m_process->seek (pos, true);
}

KDE_NO_EXPORT void PartBase::fullScreen () {
    if (m_view)
        m_view->fullScreen ();
}

KDE_NO_EXPORT void PartBase::toggleFullScreen () {
    m_view->fullScreen ();
}

KAboutData* PartBase::createAboutData () {
    KMessageBox::error(0L, "createAboutData", "KMPlayer");
    return 0;
}

//-----------------------------------------------------------------------------

Source::Source (const QString & name, PartBase * player, const char * n)
 : QObject (player, n),
   m_name (name), m_player (player), m_auto_play (true),
   m_frequency (0), m_xvport (0), m_xvencoding (-1) {
    init ();
}

Source::~Source () {
    if (m_document)
        m_document->document ()->dispose ();
    m_document = NodePtr ();
    Q_ASSERT (m_current.ptr () == 0L);
}

void Source::init () {
    //setDimensions (320, 240);
    m_width = 0;
    m_height = 0;
    m_aspect = 0.0;
    setLength (0);
    m_position = 0;
    m_identified = false;
    m_recordcmd.truncate (0);
}

void Source::setDimensions (int w, int h) {
    if (m_width != w || m_height != h) {
        m_width = w;
        m_height = h;
        setAspect (h > 0 ? 1.0 * w / h : 0.0);
        emit dimensionsChanged ();
    }
}

void Source::setLength (int len) {
    m_length = len;
    emit lengthFound (len);
}

KDE_NO_EXPORT void Source::setPosition (int pos) {
    m_position = pos;
    emit positioned (pos);
}

/*
static void printTree (NodePtr root, QString off=QString()) {
    if (!root) {
        kdDebug() << off << "[null]" << endl;
        return;
    }
    kdDebug() << off << root->nodeName() << " " << (Element*)root << (root->isMrl() ? root->mrl ()->src : QString ("-")) << endl;
    off += QString ("  ");
    for (NodePtr e = root->firstChild(); e; e = e->nextSibling())
        printTree(e, off);
}*/

void Source::setURL (const KURL & url) {
    m_url = url;
    m_back_request = 0L;
    if (m_document && !m_document->hasChildNodes () &&
            (m_document->mrl()->src.isEmpty () || m_document->mrl()->src == url.url ()))
        // special case, mime is set first by plugin FIXME v
        m_document->mrl()->src = url.url ();
    else {
        if (m_document)
            m_document->document ()->dispose ();
        m_document = (new Document (url.url (), this))->self ();
    }
    if (m_player->process () && m_player->source () == this)
        m_player->updateTree ();
    m_current = m_document;
}

void Source::setTitle (const QString & title) {
    emit titleChanged (title);
}

void Source::reset () {
    if (m_document) {
        //kdDebug() << "Source::first" << endl;
        m_current = NodePtr ();
        m_document->reset ();
        m_player->updateTree ();
    }
}

QString Source::currentMrl () {
    Mrl * mrl = m_current ? m_current->mrl () : 0L;
    kdDebug() << "Source::currentMrl " << (m_current ? m_current->nodeName():"") << " src:" << (mrl ? mrl->src  : QString ()) << endl;
    return mrl ? mrl->src : QString ();
}

void Source::playCurrent () {
    QString url = currentMrl ();
    m_player->changeURL (url);
    if (m_player->view ())
        static_cast <View *> (m_player->view ())->videoStop (); //show buttonbar
    if (m_document && !m_document->active ()) {
        if (!m_current)
            m_document->activate ();
        else { // ugly code duplicate w/ back_request
            for (NodePtr p = m_current->parentNode(); p; p = p->parentNode())
                p->setState (Element::state_activated);
            m_current->activate ();
        }
    } else if (!m_current) {
        emit endOfPlayItems ();
    } else if (m_current->state == Element::state_deferred) {
        m_current->undefer ();
    } else if (m_player->process ()->state () == Process::NotRunning) {
        m_player->process ()->ready (static_cast <View *> (m_player->view ())->viewer ());
    } else if (m_player->process ()) {
        m_player->process ()->play (this, m_current->mrl ()->realMrl ());
    }
    //kdDebug () << "Source::playCurrent " << (m_current ? m_current->nodeName():" doc act:") <<  (m_document && !m_document->active ()) << " cur:" << (!m_current)  << " cur act:" << (m_current && !m_current->active ()) <<  endl;
    m_player->updateTree ();
}

static NodePtr findDepthFirst (NodePtr elm) {
    if (!elm)
        return NodePtr ();
    NodePtr tmp = elm;
    for ( ; tmp; tmp = tmp->nextSibling ()) {
        if (tmp->isMrl ())
            return tmp;
        NodePtr tmp2 = findDepthFirst (tmp->firstChild ());
        if (tmp2)
            return tmp2;
    }
    return NodePtr ();
}

bool Source::requestPlayURL (NodePtr mrl) {
    //kdDebug() << "Source::requestPlayURL " << mrl->mrl ()->src << endl;
    if (m_player->process ()->state () > Process::Ready) {
        m_back_request = mrl; // still playing, schedule it
        m_player->process ()->stop ();
    } else {
        m_current = mrl;
        m_player->updateTree ();
        QTimer::singleShot (0, this, SLOT (playCurrent ()));
    }
    return true;
}

bool Source::setCurrent (NodePtr mrl) {
    m_current = mrl;
    return true;
}

void Source::stateElementChanged (NodePtr elm) {
    //kdDebug() << "[01;31mSource::stateElementChanged[00m " << elm->nodeName () << " state:" << (int) elm->state << " cur isMrl:" << (m_current && m_current->isMrl ()) << " elm==realMrl:" << (m_current && elm == m_current->mrl ()->realMrl ()) << " p state:" << m_player->process ()->state () << endl;
    if (elm->state == Element::state_deactivated) {
        if (elm == m_document && !m_back_request)
            emit endOfPlayItems (); // played all items
        else if (m_current && m_current->isMrl () &&
                 elm == m_current->mrl ()->realMrl () &&
                 m_player->process ()->state () > Process::Ready)
            //a SMIL movies stopped by SMIL events rather than movie just ending
            m_player->process ()->stop ();
    }
    if (elm->expose () && (elm->state == Element::state_activated ||
                           elm->state == Element::state_deactivated))
        m_player->updateTree ();
    else if (m_player->view ()) // only updates the state_began changes (if any)
        static_cast <View*> (m_player->view())->playList ()->triggerUpdate ();
}

void Source::setEventDispatcher (NodePtr e) {
    if (m_player->view ())
        static_cast <View*> (m_player->view())->viewArea()->setEventListener(e);
}

void Source::setInfoMessage (const QString & msg) {
    if (m_player->view ())
        static_cast <View*> (m_player->view())->setInfoMessage (msg);
}

void Source::repaintRect (int x, int y, int w, int h) {
    //kdDebug () << "repaint " << x << "," << y << " " << w << "x" << h << endl;
    if (m_player->view ())
     static_cast<View*>(m_player->view())->viewArea()->scheduleRepaint(x,y,w,h);
}

void Source::moveRect (int x, int y, int w, int h, int x1, int y1) {
    if (m_player->view ())
      static_cast<View*>(m_player->view())->viewArea()->moveRect(x,y,w,h,x1,y1);
}

void Source::avWidgetSizes (int x, int y, int w, int h, unsigned int * bg) {
    if (m_player->view ())
      static_cast<View*>(m_player->view())->viewArea ()->setAudioVideoGeometry (x, y, w, h, bg);
}

void Source::insertURL (const QString & mrl) {
    kdDebug() << "Source::insertURL " << m_current.ptr () << mrl << endl;
    KURL url (currentMrl (), mrl);
    if (!url.isValid ())
        kdError () << "try to append non-valid url" << endl;
    else if (KURL (currentMrl ()) == url)
        kdError () << "try to append url to itself" << endl;
    else if (m_current) {
        int depth = 0;
        for (NodePtr e = m_current; e->parentNode (); e = e->parentNode ())
            ++depth;
        if (depth < 40) {
            NodePtr e = m_current;
            if (m_current->isMrl ())
                e = m_current->mrl ()->realMrl ();
            e->appendChild ((new GenericURL (m_document, KURL::decode_string (url.url ()), KURL::decode_string (mrl)))->self ());
        } else
            kdError () << "insertURL exceeds limit" << endl;
    }
}

void Source::play () {
    m_player->updateTree ();
    QTimer::singleShot (0, m_player, SLOT (play ()));
    //printTree (m_document);
}

void Source::backward () {
    if (m_document->hasChildNodes ()) {
        m_back_request = m_current;
        if (!m_back_request || m_back_request == m_document) {
            m_back_request = m_document->lastChild ();
            while (m_back_request->lastChild () && !m_back_request->isMrl ())
                m_back_request = m_back_request->lastChild ();
            if (m_back_request->isMrl ())
                return;
        }
        while (m_back_request && m_back_request != m_document) {
            if (m_back_request->previousSibling ()) {
                m_back_request = m_back_request->previousSibling ();
                NodePtr e = findDepthFirst (m_back_request); // lastDepth..
                if (e) {
                    m_back_request = e;
                    if (m_player->playing ())
                        m_player->process ()->stop ();
                    else if (m_current) {
                        m_document->reset ();
                        m_current = e;
                        QTimer::singleShot (0, this, SLOT (playCurrent ()));
                    }
                    return;
                }
            } else
                m_back_request = m_back_request->parentNode ();
        }
        m_back_request = 0L;
    } else
        m_player->process ()->seek (-1 * m_player->settings ()->seektime * 10, false);
}

void Source::forward () {
    if (m_document->hasChildNodes ()) {
        if (m_player->playing ())
            m_player->process ()->stop ();
        else if (m_current)
            m_current->reset ();
    } else
        m_player->process ()->seek (m_player->settings()->seektime * 10, false);
}

void Source::jump (NodePtr e) {
    if (e->isMrl ()) {
        if (m_player->playing ()) {
            m_back_request = e;
            m_player->process ()->stop ();
        } else {
            if (m_current)
                m_document->reset ();
            m_current = e;
            QTimer::singleShot (0, this, SLOT (playCurrent ()));
        }
    } else
        m_player->updateTree ();
}

QString Source::mime () const {
    return m_current ? m_current->mrl ()->mimetype : (m_document ? m_document->mrl ()->mimetype : QString ());
}

void Source::setMime (const QString & m) {
    QString mimestr (m);
    int plugin_pos = mimestr.find ("-plugin");
    if (plugin_pos > 0)
        mimestr.truncate (plugin_pos);
    kdDebug () << "setMime " << mimestr << endl;
    if (m_current)
        m_current->mrl ()->mimetype = mimestr;
    else {
        if (m_document)
            m_document->document ()->dispose ();
        m_document = (new Document (QString (), this))->self ();
        m_document->mrl ()->mimetype = mimestr;
    }
}

bool Source::processOutput (const QString & str) {
    if (str.startsWith ("ID_VIDEO_WIDTH")) {
        int pos = str.find ('=');
        if (pos > 0) {
            int w = str.mid (pos + 1).toInt ();
            if (height () > 0)
                setDimensions (w, height ());
            else
                setWidth (w);
        }
    } else if (str.startsWith ("ID_VIDEO_HEIGHT")) {
        int pos = str.find ('=');
        if (pos > 0) {
            int h = str.mid (pos + 1).toInt ();
            if (width () > 0)
                setDimensions (width (), h);
            else
                setHeight (h);
        }
    } else if (str.startsWith ("ID_VIDEO_ASPECT")) {
        int pos = str.find ('=');
        if (pos > 0)
            setAspect (str.mid (pos + 1).replace (',', '.').toFloat ());
    } else
        return false;
    return true;
}

QString Source::filterOptions () {
    Settings* m_settings = m_player->settings ();
    QString PPargs ("");
    if (m_settings->postprocessing)
    {
        if (m_settings->pp_default)
            PPargs = "-vop pp=de";
        else if (m_settings->pp_fast)
            PPargs = "-vop pp=fa";
        else if (m_settings->pp_custom) {
            PPargs = "-vop pp=";
            if (m_settings->pp_custom_hz) {
                PPargs += "hb";
                if (m_settings->pp_custom_hz_aq && \
                        m_settings->pp_custom_hz_ch)
                    PPargs += ":ac";
                else if (m_settings->pp_custom_hz_aq)
                    PPargs += ":a";
                else if (m_settings->pp_custom_hz_ch)
                    PPargs += ":c";
                PPargs += "/";
            }
            if (m_settings->pp_custom_vt) {
                PPargs += "vb";
                if (m_settings->pp_custom_vt_aq && \
                        m_settings->pp_custom_vt_ch)
                    PPargs += ":ac";
                else if (m_settings->pp_custom_vt_aq)
                    PPargs += ":a";
                else if (m_settings->pp_custom_vt_ch)
                    PPargs += ":c";
                PPargs += "/";
            }
            if (m_settings->pp_custom_dr) {
                PPargs += "dr";
                if (m_settings->pp_custom_dr_aq && \
                        m_settings->pp_custom_dr_ch)
                    PPargs += ":ac";
                else if (m_settings->pp_custom_dr_aq)
                    PPargs += ":a";
                else if (m_settings->pp_custom_dr_ch)
                    PPargs += ":c";
                PPargs += "/";
            }
            if (m_settings->pp_custom_al) {
                PPargs += "al";
                if (m_settings->pp_custom_al_f)
                    PPargs += ":f";
                PPargs += "/";
            }
            if (m_settings->pp_custom_tn) {
                PPargs += "tn";
                /*if (1 <= m_settings->pp_custom_tn_s <= 3){
                    PPargs += ":";
                    PPargs += m_settings->pp_custom_tn_s;
                    }*/ //disabled 'cos this is wrong
                PPargs += "/";
            }
            if (m_settings->pp_lin_blend_int) {
                PPargs += "lb";
                PPargs += "/";
            }
            if (m_settings->pp_lin_int) {
                PPargs += "li";
                PPargs += "/";
            }
            if (m_settings->pp_cub_int) {
                PPargs += "ci";
                PPargs += "/";
            }
            if (m_settings->pp_med_int) {
                PPargs += "md";
                PPargs += "/";
            }
            if (m_settings->pp_ffmpeg_int) {
                PPargs += "fd";
                PPargs += "/";
            }
        }
        if (PPargs.endsWith("/"))
            PPargs.truncate(PPargs.length()-1);
    }
    return PPargs;
}

bool Source::hasLength () {
    return true;
}

bool Source::isSeekable () {
    return true;
}

void Source::setIdentified (bool b) {
    //kdDebug () << "Source::setIdentified " << m_identified << b <<endl;
    m_identified = b;
}

void Source::stateChange(Process *p, Process::State olds, Process::State news) {
    if (!p || !p->viewer ()) return;
    if (dynamic_cast <Recorder *> (p)) {
        kdDebug () << "recordState " << statemap[olds] << " -> " << statemap[news] << endl;
        p->viewer ()->view ()->controlPanel ()->setRecording (news > Process::Ready);
        if (news == Process::Ready && olds > Process::Ready)
            p->quit ();
        else if (news >= Process::Ready)
            emit startRecording ();
        else if (news == Process::NotRunning)
            emit stopRecording ();
    } else {
        p->viewer()->view()->controlPanel()->setPlaying(news > Process::Ready);
        kdDebug () << "processState " << statemap[olds] << " -> " << statemap[news] << endl;
        if (news == Process::Playing) {
            p->viewer ()->view ()->videoStart ();
            emit startPlaying ();
        } else if (news == Process::NotRunning) {
            if (hasLength () && position () > length ())
                setLength (position ());
            setPosition (0);
            emit stopPlaying ();
        } else if (news == Process::Ready) {
            if (olds > Process::Ready) {
                if (p->mrl ()->active ()) // if cause is eof
                    p->mrl ()->finish (); // set node to finished
                if (m_back_request && m_back_request->isMrl ()) { // jump in pl
                    m_current = m_back_request;
                    if (m_current->id > SMIL::id_node_first &&
                            m_current->id < SMIL::id_node_last) {
                        // don't mess with SMIL, just play the damn link
                        playCurrent ();
                    } else {
                        // sanitize pl having all parents of current activated
                        m_document->reset (); // deactivate everything
                        for (NodePtr p = m_current->parentNode(); p; p = p->parentNode())
                            p->setState (Element::state_activated);
                        m_current->activate (); // calls requestPlayUrl
                    }
                    m_back_request = 0L;
                }
                if (m_player->view ())
                    static_cast<View*>(m_player->view())->viewArea()->repaint();
            } else
                QTimer::singleShot (0, this, SLOT (playCurrent ()));
        }
    }
}

QString Source::prettyName () {
    return QString (i18n ("Unknown"));
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT URLSource::URLSource (PartBase * player, const KURL & url)
    : Source (i18n ("URL"), player, "urlsource"), m_job (0L) {
    setURL (url);
    //kdDebug () << "URLSource::URLSource" << endl;
}

KDE_NO_CDTOR_EXPORT URLSource::~URLSource () {
    //kdDebug () << "URLSource::~URLSource" << endl;
}

KDE_NO_EXPORT void URLSource::init () {
    Source::init ();
}

int URLSource::width () {
    Mrl * mrl = m_current ? m_current->mrl () : 0L;
    if (mrl && mrl->id > SMIL::id_node_first && mrl->id < SMIL::id_node_last)
        return mrl->width;
    return Source::width ();
}

int URLSource::height () {
    Mrl * mrl = m_current ? m_current->mrl () : 0L;
    if (mrl && mrl->id > SMIL::id_node_first && mrl->id < SMIL::id_node_last)
        return mrl->height;
    return Source::height ();
}

float URLSource::aspect () {
    Mrl * mrl = m_current ? m_current->mrl () : 0L;
    if (mrl && mrl->id > SMIL::id_node_first && mrl->id < SMIL::id_node_last)
        return mrl->aspect;
    return Source::aspect ();
}

void URLSource::dimensions (int & w, int & h) {
    if (!m_player->mayResize () && m_player->view ()) {
        w = static_cast <View *> (m_player->view ())->viewer ()->width ();
        h = static_cast <View *> (m_player->view ())->viewer ()->height ();
    } else if (m_document && m_document->firstChild () &&
            !strcmp (m_document->firstChild ()->nodeName (), "smil")) {
        Mrl * mrl = m_document->firstChild ()->mrl ();
        w = mrl->width;
        h = mrl->height;
        if (w && h)
            return;
    } else
        Source::dimensions (w, h);
    
}

void URLSource::setWidth (int w) {
    Mrl * mrl = m_current ? m_current->mrl () : 0L;
    if (mrl && mrl->id > SMIL::id_node_first && mrl->id < SMIL::id_node_last)
        mrl->width = w;
    else
        Source::setWidth (w);
}

void URLSource::setHeight (int w) {
    Mrl * mrl = m_current ? m_current->mrl () : 0L;
    if (mrl && mrl->id > SMIL::id_node_first && mrl->id < SMIL::id_node_last)
        mrl->height = w;
    else
        Source::setHeight (w);
}

void URLSource::setDimensions (int w, int h) {
    Mrl * mrl = m_current ? m_current->mrl () : 0L;
    if (mrl && mrl->id > SMIL::id_node_first && mrl->id < SMIL::id_node_last) {
        if (m_player->view ()) {
            mrl = mrl->realMrl ()->mrl ();
            mrl->width = w;
            mrl->height = h;
            if (h > 0) {
                float a = 1.0 * w / h;
                mrl->aspect = a;
                static_cast <View *> (m_player->view())->viewer()->setAspect(a);
                static_cast <View *> (m_player->view ())->updateLayout ();
            }
        }
    } else
        Source::setDimensions (w, h);
}

void URLSource::setAspect (float w) {
    Mrl * mrl = m_current ? m_current->mrl () : 0L;
    if (mrl && mrl->id > SMIL::id_node_first && mrl->id < SMIL::id_node_last)
        mrl->aspect = w;
    else
        Source::setAspect (w);
}

KDE_NO_EXPORT bool URLSource::hasLength () {
    return !!length ();
}

KDE_NO_EXPORT void URLSource::activate () {
    if (url ().isEmpty () && (!m_document || !m_document->hasChildNodes ())) {
        m_player->updateTree ();
        return;
    }
    if (m_auto_play)
        play ();
}

KDE_NO_EXPORT void URLSource::terminateJob () {
    if (m_job) {
        m_job->kill (); // silent, no kioResult signal
        if (m_player->view ())
            static_cast <View *> (m_player->view ())->controlPanel ()->setPlaying (m_player->playing ());
    }
    m_job = 0L;
}

KDE_NO_EXPORT void URLSource::deactivate () {
    terminateJob ();
    setEventDispatcher (NodePtr ());
}

KDE_NO_EXPORT QString URLSource::prettyName () {
    if (m_url.isEmpty ())
        return QString (i18n ("URL"));
    if (m_url.url ().length () > 50) {
        QString newurl = m_url.protocol () + QString ("://");
        if (m_url.hasHost ())
            newurl += m_url.host ();
        if (m_url.port ())
            newurl += QString (":%1").arg (m_url.port ());
        QString file = m_url.fileName ();
        int len = newurl.length () + file.length ();
        KURL path = KURL (m_url.directory ());
        bool modified = false;
        while (path.url ().length () + len > 50 && path != path.upURL ()) {
            path = path.upURL ();
            modified = true;
        }
        QString dir = path.directory ();
        if (!dir.endsWith (QString ("/")))
            dir += '/';
        if (modified)
            dir += QString (".../");
        newurl += dir + file;
        return QString (i18n ("URL - %1").arg (newurl));
    }
    return QString (i18n ("URL - %1").arg (m_url.prettyURL ()));
}

static bool isPlayListMime (const QString & mime) {
    const char * mimestr = mime.ascii ();
    return mimestr && (!strcmp (mimestr, "audio/mpegurl") ||
            !strcmp (mimestr, "audio/x-mpegurl") ||
            !strcmp (mimestr, "video/x-ms-wmp") ||
            !strcmp (mimestr, "video/x-ms-asf") ||
            !strcmp (mimestr, "video/x-ms-wmv") ||
            !strcmp (mimestr, "video/x-ms-wvx") ||
            !strcmp (mimestr, "audio/x-scpls") ||
            !strcmp (mimestr, "audio/x-pn-realaudio") ||
            !strcmp (mimestr, "audio/vnd.rn-realaudio") ||
            !strcmp (mimestr, "audio/m3u") ||
            !strcmp (mimestr, "audio/x-m3u") ||
            !strncmp (mimestr, "text/", 5) ||
            (!strncmp (mimestr, "application/", 12) &&
             strstr (mimestr + 12,"+xml")) ||
            !strncasecmp (mimestr, "application/smil", 16) ||
            !strncasecmp (mimestr, "application/xml", 15) ||
            //!strcmp (mimestr, "application/rss+xml") ||
            //!strcmp (mimestr, "application/atom+xml") ||
            !strcmp (mimestr, "application/x-mplayer2"));
}

KDE_NO_EXPORT void URLSource::read (QTextStream & textstream) {
    QString line;
    do {
        line = textstream.readLine ();
    } while (!line.isNull () && line.stripWhiteSpace ().isEmpty ());
    if (!line.isNull ()) {
        NodePtr cur_elm = m_current;
        if (m_current->isMrl ())
            cur_elm = m_current->mrl ()->realMrl ();
        if (mime () == QString ("audio/x-scpls")) {
            bool groupfound = false;
            int nr = -1;
            struct Entry {
                QString url, title;
            } * entries = 0L;
            do {
                line = line.stripWhiteSpace ();
                if (!line.isEmpty ()) {
                    if (line.startsWith (QString ("[")) && line.endsWith (QString ("]"))) {
                        if (!groupfound && line.mid (1, line.length () - 2).stripWhiteSpace () == QString ("playlist"))
                            groupfound = true;
                        else
                            break;
                        kdDebug () << "Group found: " << line << endl;
                    } else if (groupfound) {
                        int eq_pos = line.find (QChar ('='));
                        if (eq_pos > 0) {
                            if (line.lower ().startsWith (QString ("numberofentries"))) {
                                nr = line.mid (eq_pos + 1).stripWhiteSpace ().toInt ();
                                kdDebug () << "numberofentries : " << nr << endl;
                                if (nr > 0 && nr < 1024)
                                    entries = new Entry[nr];
                                else
                                    nr = 0;
                            } else if (nr > 0) {
                                QString ll = line.lower ();
                                if (ll.startsWith (QString ("file"))) {
                                    int i = line.mid (4, eq_pos-4).toInt ();
                                    if (i > 0 && i <= nr)
                                        entries[i-1].url = line.mid (eq_pos + 1).stripWhiteSpace ();
                                } else if (ll.startsWith (QString ("title"))) {
                                    int i = line.mid (5, eq_pos-5).toInt ();
                                    if (i > 0 && i <= nr)
                                        entries[i-1].title = line.mid (eq_pos + 1).stripWhiteSpace ();
                                }
                            }
                        }
                    }
                }
                line = textstream.readLine ();
            } while (!line.isNull ());
            for (int i = 0; i < nr; i++)
                if (!entries[i].url.isEmpty ())
                    cur_elm->appendChild ((new GenericURL (m_document, KURL::decode_string (entries[i].url), entries[i].title))->self ());
            delete [] entries;
        } else if (line.stripWhiteSpace ().startsWith (QChar ('<'))) {
            readXML (cur_elm, textstream, line);
            cur_elm->normalize ();
            if (m_document && m_document->firstChild ()) {
                // SMIL documents have set its size of root-layout
                Mrl * mrl = m_document->firstChild ()->mrl ();
                if (mrl)
                    Source::setDimensions (mrl->width, mrl->height);
            }
        } else if (line.lower () != QString ("[reference]")) do {
            QString mrl = line.stripWhiteSpace ();
            if (mrl.lower ().startsWith (QString ("asf ")))
                mrl = mrl.mid (4).stripWhiteSpace ();
            if (!mrl.isEmpty () && !mrl.startsWith (QChar ('#')))
                cur_elm->appendChild((new GenericURL(m_document, mrl))->self());
            line = textstream.readLine ();
        } while (!line.isNull ()); /* TODO && m_document.size () < 1024 / * support 1k entries * /);*/
    }
;
}

KDE_NO_EXPORT void URLSource::kioData (KIO::Job *, const QByteArray & d) {
    int size = m_data.size ();
    int newsize = size + d.size ();
    if (!size) { // first data
        int accuraty = 0;
        KMimeType::Ptr mime = KMimeType::findByContent (d, &accuraty);
        if (mime) {
            if (!mime->name ().startsWith (QString ("text/")))
                newsize = 0;
            kdDebug () << "URLSource::kioData: " << mime->name () << accuraty << endl;
        }
    }
    //kdDebug () << "URLSource::kioData: " << newsize << endl;
    if (newsize <= 0 || newsize > 200000) {
        m_data.resize (0);
        m_job->kill (false);
    } else  {
        m_data.resize (newsize);
        memcpy (m_data.data () + size, d.data (), newsize - size);
    }
}

KDE_NO_EXPORT void URLSource::kioMimetype (KIO::Job * job, const QString & mimestr) {
    setMime (mimestr);
    if (job && !isPlayListMime (mime ())) // Note: -plugin might be stripped
        job->kill (false);
}

KDE_NO_EXPORT void URLSource::kioResult (KIO::Job *) {
    m_job = 0L; // KIO::Job::kill deletes itself
    QTextStream textstream (m_data, IO_ReadOnly);
    if (isPlayListMime (mime ()))
        read (textstream);
    if (m_current) {
        m_current->mrl ()->parsed = true;
        QTimer::singleShot (0, this, SLOT (playCurrent ()));
    }
    static_cast <View *> (m_player->view())->controlPanel()->setPlaying (false);
}

void URLSource::playCurrent () {
    terminateJob ();
    if (!m_current || !m_current->active ()) {
        // run m_document->activate() first
        Source::playCurrent ();
        return;
    }
    KURL url (currentMrl ());
    if (url.isEmpty () || m_current->mrl ()->parsed) {
        Source::playCurrent ();
        return;
    }
    int depth = 0;
    if (m_current)
        for (NodePtr e = m_current; e->parentNode (); e = e->parentNode ())
            ++depth;
    if (depth > 40) {
        Source::playCurrent ();
    } else {
        QString mimestr = mime ();
        bool maybe_playlist = isPlayListMime (mimestr);
        m_current->defer (); // may need a reactivate
        //kdDebug () << "URLSource::playCurrent " << mimestr << maybe_playlist << endl;
        if (url.isLocalFile ()) {
            QFile file (url.path ());
            if (!file.exists ()) {
                kdDebug () << "URLSource::playCurrent not found " << url.path () << " " << currentMrl () << endl;
                m_current->mrl ()->parsed = true;
                Source::playCurrent ();
                return;
            }
            if (mimestr.isEmpty ()) {
                KMimeType::Ptr mimeptr = KMimeType::findByURL (url);
                if (mimeptr) {
                    setMime (mimeptr->name ());
                    maybe_playlist = isPlayListMime (mime ()); // get new mime
                }
            }
            if (maybe_playlist && file.size () < 200000 && file.open (IO_ReadOnly)) {
                QTextStream textstream (&file);
                read (textstream);
            }
            m_current->mrl ()->parsed = true;
            QTimer::singleShot (0, this, SLOT (playCurrent ()));
        } else if ((maybe_playlist &&
                    url.protocol ().compare (QString ("mms")) &&
                    url.protocol ().compare (QString ("rtsp")) &&
                    url.protocol ().compare (QString ("rtp"))) ||
                (mimestr.isEmpty () &&
                 url.protocol ().startsWith (QString ("http")))) {
            m_data.truncate (0);
            m_job = KIO::get (url, false, false);
            m_job->addMetaData ("PropagateHttpHeader", "true");
            connect (m_job, SIGNAL (data (KIO::Job *, const QByteArray &)),
                    this, SLOT (kioData (KIO::Job *, const QByteArray &)));
            //connect( m_job, SIGNAL(connected(KIO::Job*)),
            //         this, SLOT(slotConnected(KIO::Job*)));
            connect (m_job, SIGNAL (mimetype (KIO::Job *, const QString &)),
                    this, SLOT (kioMimetype (KIO::Job *, const QString &)));
            connect (m_job, SIGNAL (result (KIO::Job *)),
                    this, SLOT (kioResult (KIO::Job *)));
            static_cast <View *> (m_player->view ())->controlPanel ()->setPlaying (true);
        } else {
            m_current->mrl ()->parsed = true;
            QTimer::singleShot (0, this, SLOT (playCurrent ()));
        }
    }
}

KDE_NO_EXPORT void URLSource::play () {
    Source::play ();
}

bool URLSource::requestPlayURL (NodePtr mrl) {
    if (m_document != mrl->mrl ()->realMrl ()) {
        KURL base = m_document->mrl ()->src;
        KURL dest = mrl->mrl ()->realMrl ()->mrl ()->src;
        // check if some remote playlist tries to open something local, but
        // do ignore unknown protocols because there are so many and we only
        // want to cache local ones.
        if (
#if 0
            !KProtocolInfo::protocolClass (dest.protocol ()).isEmpty () &&
#else
            dest.isLocalFile () &&
#endif
                !kapp->authorizeURLAction ("redirect", base, dest)) {
            kdWarning () << "requestPlayURL from document " << base << " to play " << dest << " is not allowed" << endl;
            return false;
        }
    }
    return Source::requestPlayURL (mrl);
}

//-----------------------------------------------------------------------------

#include "kmplayerpartbase.moc"
#include "kmplayersource.moc"

/* This file is part of the KDE project
 *
 * Copyright (C) 2003 Koos Vriezen <koos.vriezen@xs4all.nl>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef KMPLAYERSOURCE_H
#define KMPLAYERSOURCE_H

#include <qobject.h>
#include <qstring.h>
#include <kurl.h>

#include "kmplayerplaylist.h"

class KMPlayer;
class KConfig;
class QWidget;
class QFrame;
class QListViewItem;
class KMPlayerSourcePrivate;

class KMPLAYER_EXPORT KMPlayerSource : public QObject {
    Q_OBJECT
public:
    KMPlayerSource (const QString & name, KMPlayer * player, const char * src);
    virtual ~KMPlayerSource ();
    virtual void init ();
    virtual bool processOutput (const QString & line);

    bool identified () const { return m_identified; }
    virtual bool hasLength ();
    virtual bool isSeekable ();

    KDE_NO_EXPORT int width () const { return m_width; }
    KDE_NO_EXPORT int height () const { return m_height; }
    /* length () returns length in deci-seconds */
    KDE_NO_EXPORT int length () const { return m_length; }
    /* position () returns position in deci-seconds */
    KDE_NO_EXPORT int position () const { return m_position; }
    KDE_NO_EXPORT float aspect () const { return m_aspect > 0.01 ? m_aspect : (m_height > 0 ? (1.0*m_width)/m_height: 0.0); }
    KDE_NO_EXPORT const KURL & url () const { return m_url; }
    KDE_NO_EXPORT const KURL & subUrl () const { return m_sub_url; }
    QString first ();
    QString current ();
    QString next ();
    virtual void getCurrent (); // will emit currentURL
    QString mime () const;
    KDE_NO_EXPORT const QString & audioDevice () const { return m_audiodevice; }
    KDE_NO_EXPORT const QString & videoDevice () const { return m_videodevice; }
    KDE_NO_EXPORT const QString & videoNorm () const { return m_videonorm; }
    KDE_NO_EXPORT const int frequency () const { return m_frequency; }
    KDE_NO_EXPORT const QString & pipeCmd () const { return m_pipecmd; }
    KDE_NO_EXPORT const QString & options () const { return m_options; }
    KDE_NO_EXPORT const QString & recordCmd () const { return m_recordcmd; }
    virtual QString filterOptions ();

    void setURL (const KURL & url);
    void insertURL (const QString & url);
    KDE_NO_EXPORT void setSubURL (const KURL & url) { m_sub_url = url; }
    void setMime (const QString & m);
    KDE_NO_EXPORT void setWidth (int w) { m_width = w; }
    KDE_NO_EXPORT void setHeight (int h) { m_height = h; }
    KDE_NO_EXPORT void setAspect (float a) { m_aspect = a; }
    /* setLength (len) set length in deci-seconds */
    void setLength (int len);
    /* setPosition (pos) set position in deci-seconds */
    KDE_NO_EXPORT void setPosition (int pos) { m_position = pos; }
    virtual void setIdentified (bool b = true);
    KDE_NO_EXPORT void setAutoPlay (bool b) { m_auto_play = b; }

    virtual QString prettyName ();
signals:
    // in respond to 'first'/'current'/'next'/'previous' call
    void currentURL (const QString &);
public slots:
    virtual void activate () = 0;
    virtual void deactivate () = 0;
    virtual void forward ();
    virtual void backward ();
    virtual void play ();
    virtual void jump (ElementPtr e);
protected:
    ElementPtr m_document;
    ElementPtr m_current;
    ElementPtrW m_back_request;
    QString m_name;
    KMPlayer * m_player;
    QString m_recordcmd;
    bool m_identified;
    bool m_auto_play;
    KURL m_url;
    KURL m_sub_url;
    QString m_audiodevice;
    QString m_videodevice;
    QString m_videonorm;
    int m_frequency;
    QString m_pipecmd;
    QString m_options;
private:
    int m_width;
    int m_height;
    float m_aspect;
    int m_length;
    int m_position;
};

#endif

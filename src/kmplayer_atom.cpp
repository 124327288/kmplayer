/**
 * Copyright (C) 2005 by Koos Vriezen <koos ! vriezen ? xs4all ! nl>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 **/

#include <config.h>
#include <kdebug.h>
#include "kmplayer_atom.h"

using namespace KMPlayer;

NodePtr ATOM::Feed::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "entry"))
        return (new ATOM::Entry (m_doc))->self ();
    else if (!strcmp (tag.latin1 (), "link"))
        return (new ATOM::Link (m_doc))->self ();
    return NodePtr ();
}

NodePtr ATOM::Entry::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "link"))
        return (new ATOM::Link (m_doc))->self ();
    else if (!strcmp (tag.latin1 (), "title"))
        return (new DarkNode (m_doc, tag, id_node_title))->self ();
    else if (!strcmp (tag.latin1 (), "summary"))
        return (new DarkNode (m_doc, tag, id_node_summary))->self ();
    return NodePtr ();
}

void ATOM::Entry::closed () {
    for (NodePtr c = firstChild (); c; c = c->nextSibling ()) {
        if (c->id == id_node_title) {
            QString str = c->innerText ();
            pretty_name = str.left (str.find (QChar ('\n')));
        }
    }
}

void ATOM::Link::closed () {
    QString href;
    QString rel;
    for (AttributePtr a = attributes ()->first (); a; a = a->nextSibling ()) {
        if (!strcasecmp (a->nodeName (), "rel"))
            rel = a->nodeValue ();
        else if (!strcasecmp (a->nodeName (), "href"))
            href = a->nodeValue ();
        else if (!strcasecmp (a->nodeName (), "title"))
            pretty_name = a->nodeValue ();
    }
    if (!href.isEmpty () && rel == QString::fromLatin1 ("enclosure"))
        src = href;
    else if (pretty_name.isEmpty ())
        pretty_name = href;
}

void ATOM::Content::closed () {
    //QString type = getAttribute (QString::fromLatin1 ("type"));
}

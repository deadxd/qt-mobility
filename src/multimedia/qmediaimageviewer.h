/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Mobility Components.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QMEDIAIMAGEVIEWER_H
#define QMEDIAIMAGEVIEWER_H

#include "qmediaobject.h"
#include "qmediacontent.h"
#include <qmediaenumdebug.h>

QT_BEGIN_NAMESPACE

class QAbstractVideoSurface;
class QGraphicsVideoItem;
class QMediaPlaylist;
class QVideoWidget;

class QMediaImageViewerPrivate;
class Q_MULTIMEDIA_EXPORT QMediaImageViewer : public QMediaObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(MediaStatus mediaStatus READ mediaStatus NOTIFY mediaStatusChanged)
    Q_PROPERTY(QMediaContent media READ media WRITE setMedia NOTIFY mediaChanged)
    Q_PROPERTY(int timeout READ timeout WRITE setTimeout)
    Q_PROPERTY(int elapsedTime READ elapsedTime NOTIFY elapsedTimeChanged)
    Q_ENUMS(State MediaStatus)

public:
    enum State
    {
        StoppedState,
        PlayingState,
        PausedState
    };

    enum MediaStatus
    {
        NoMedia,
        LoadingMedia,
        LoadedMedia,
        InvalidMedia
    };

    explicit QMediaImageViewer(QObject *parent = 0);
    ~QMediaImageViewer();

    State state() const;
    MediaStatus mediaStatus() const;

    QMediaContent media() const;
    QMediaPlaylist *playlist() const;

    int timeout() const;
    int elapsedTime() const;

    void setVideoOutput(QVideoWidget *widget);
    void setVideoOutput(QGraphicsVideoItem *item);
    void setVideoOutput(QAbstractVideoSurface *surface);

    bool bind(QObject *);
    void unbind(QObject *);

public Q_SLOTS:
    void setMedia(const QMediaContent &media);
    void setPlaylist(QMediaPlaylist *playlist);

    void play();
    void pause();
    void stop();

    void setTimeout(int timeout);

Q_SIGNALS:
    void stateChanged(QMediaImageViewer::State state);
    void mediaStatusChanged(QMediaImageViewer::MediaStatus status);
    void mediaChanged(const QMediaContent &media);
    void elapsedTimeChanged(int time);
protected:
    void timerEvent(QTimerEvent *event);

private:
    Q_DECLARE_PRIVATE(QMediaImageViewer)
    Q_PRIVATE_SLOT(d_func(), void _q_mediaStatusChanged(QMediaImageViewer::MediaStatus))
    Q_PRIVATE_SLOT(d_func(), void _q_playlistMediaChanged(const QMediaContent &))
    Q_PRIVATE_SLOT(d_func(), void _q_playlistDestroyed())
};

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QMediaImageViewer::State)
Q_DECLARE_METATYPE(QMediaImageViewer::MediaStatus)

Q_MEDIA_ENUM_DEBUG(QMediaImageViewer, State)
Q_MEDIA_ENUM_DEBUG(QMediaImageViewer, MediaStatus)

#endif

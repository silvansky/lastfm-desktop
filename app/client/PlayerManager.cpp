/***************************************************************************
 *   Copyright 2005-2008 Last.fm Ltd                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *    This program is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301, USA.          *
 ***************************************************************************/

#include "PlayerManager.h"
#include "PlayerEvent.h"
#include "Settings.h"


PlayerManager::PlayerManager( PlayerListener* listener )
             : QObject( (QObject*)listener ),
               m_state( PlayerState::Stopped )
{
    QObject* o = (QObject*)listener;
    connect( o, SIGNAL(trackStarted( Track )), SLOT(onTrackStarted( Track )) );
    connect( o, SIGNAL(playbackEnded( QString )), SLOT(onPlaybackEnded( QString )) );
    connect( o, SIGNAL(playbackPaused( QString )), SLOT(onPlaybackPaused( QString )) );
    connect( o, SIGNAL(playbackResumed( QString )), SLOT(onPlaybackResumed( QString )) );
    connect( o, SIGNAL(playerConnected( QString )), SLOT(onPlayerConnected( QString )) );
    connect( o, SIGNAL(playerDisconnected( QString)), SLOT(onPlayerDisconnected( QString )) );
}


#define ONE_PLAYER_HACK( id ) if (m_playerId.size() && m_playerId != id ) return;

void
PlayerManager::onPlaybackSessionStarted( const QString& id )
{
	Q_ASSERT( id.size() );
	Q_ASSERT( m_track.isNull() );
	ONE_PLAYER_HACK( id )
	using namespace PlayerState;

	switch (m_state)
	{
		case Stopped:
			break;
		default:
			qWarning() << "Ignoring request by connected player to start already started session";
			return;
	}
	
	m_state = Playing;
	emit event( PlayerEvent::PlaybackSessionStarted, id );
}


void
PlayerManager::onPreparingTrack( const Track& t )
{
	ONE_PLAYER_HACK( t.playerId() );
	using namespace PlayerState;
	// if t is null, we continue to say we are preparing anyway

	m_state = Loading;
	
	switch (m_state)
	{
		case Stopped:
			onPlaybackSessionStarted( t.playerId() );
			break;
			
		case Loading:
			// valid, though bad user experience
			// fall through
		case Paused:
		case Stalled:
		case Playing:
			if (!m_track.isNull())
				onTrackEnded( t.playerId() );
			break;
	}
	
	m_track = t;
	
	emit event( PlayerEvent::PreparingTrack, QVariant::fromValue( t ) );
}


void
PlayerManager::onTrackStarted( const Track& t )
{   
	ONE_PLAYER_HACK( t.playerId() )
	using namespace PlayerState;
	
	if (t.isNull() && (m_state == Stopped || m_state == Playing))
	{
		qWarning() << "Empty TrackInfo object presented for TrackStarted notification, this is wrong!";
		emit event( PlayerEvent::PlaybackSessionEnded );
		return;
	}
	
	switch (m_state)
	{
		case Stopped:
			onPlaybackSessionStarted( t.playerId() );
			break;
		case Loading:
		case Paused:
		case Stalled:
			m_state = Playing;
			// fall through
		case Playing:
			if (!m_track.isNull())
				onTrackEnded( t.playerId() );
			break;
	}
	
	m_track = t;
	m_track.m_watch = new StopWatch( t.duration() * The::settings().scrobblePoint() / 100 ); 
	connect( m_track.m_watch, SIGNAL(timeout()), SLOT(onStopWatchTimedOut()) );

	emit event( PlayerEvent::TrackStarted, QVariant::fromValue( m_track ) );
}


void
PlayerManager::onTrackEnded( const QString& id )
{
	ONE_PLAYER_HACK( id )
	using namespace PlayerState;

	if (m_track.isNull()) {
		qWarning() << "Ignoring request by connected player to end null track";
		return;
	}

	switch (m_state)
	{
		case Stopped:
			qWarning() << "Programmer Error: EndTrack requested for non-null track but state is stopped";
			break;

		case Loading:
		case Playing:
		case Paused:
		case Stalled:
			break;
	}

	ObservedTrack const was = m_track;
	
	delete m_track.m_watch;
	m_track = ObservedTrack();	
	
	emit event( PlayerEvent::TrackEnded, QVariant::fromValue( was ) );
	
	// indeed state remains playing/loading, as playing refers to a session,
	// where we are actively trying to play something. We are no in a kind of
	// loading state
}


void
PlayerManager::onPlaybackEnded( const QString& id )
{
	ONE_PLAYER_HACK( id )
	using namespace PlayerState;

	onTrackEnded( id );
	
	switch (m_state)
	{
		case Stopped:
			qWarning() << "Ignoring request by connected player to set Stopped state again";
			return;
		
		case Loading:
		case Playing:
		case Paused:
		case Stalled:
			break;
	}
		
	m_state = Stopped;
	emit event( PlayerEvent::PlaybackSessionEnded );
}


void
PlayerManager::onPlaybackPaused( const QString& id )
{
	Q_ASSERT( !m_track.isNull() );
	Q_ASSERT( m_track.m_watch );
	ONE_PLAYER_HACK( id )
	using namespace PlayerState;
	
	switch (m_state)
	{
		case Paused:
			qWarning() << "Ignoring request by connected player to set Paused state again";
		case Stopped:
			qWarning() << "Ignoring request to pause when in stopped state";
			return;

		case Loading:
		case Playing:
		case Stalled:
			if (m_track.isNull() || !m_track.m_watch)
			{
				qWarning() << "Programmer error, m_track is broken";
				return;
			}
			break;
	}

	if (m_track.isNull() || !m_track.m_watch)
		return;
	
	m_track.m_watch->pause();
	m_state = Paused;

	emit event( PlayerEvent::PlaybackPaused, QVariant::fromValue( m_track ) );
}


void
PlayerManager::onPlaybackResumed( const QString& id )
{
	Q_ASSERT( !m_track.isNull() );
	Q_ASSERT( m_track.m_watch );
	ONE_PLAYER_HACK( id )
	using namespace PlayerState;
	
	switch (m_state)
	{
		case Loading:
		case Playing:
			// no point as nothing would change
			qWarning() << "Ignoring request by connected player to resume playing track";
			return;
			
		case Stopped:
			qWarning() << "Ignoring request by connected player to resume null track";
			return;

		case Stalled:
		case Paused:
			if (m_track.isNull() || !m_track.m_watch)
			{
				qWarning() << "Programmer error, m_track is broken";
				return;
			}
			break;
	}
	
	m_state = Playing;
	m_track.m_watch->resume();

	emit event( PlayerEvent::PlaybackUnpaused, QVariant::fromValue( m_track ) );
}


void
PlayerManager::onPlayerConnected( const QString &id )
{
	if (m_playerId.size()) return;
	
	m_playerId = id;
    emit event( PlayerEvent::PlayerConnected, QVariant::fromValue( id ) );
}


void
PlayerManager::onPlayerDisconnected( const QString &id )
{
	ONE_PLAYER_HACK( id )
	
    emit event( PlayerEvent::PlayerDisconnected, QVariant::fromValue( id ) );

    // Implicit PlayerEvent::PlaybackEnded to avoid crashing / buggy media
    // players leaving the scrobbler in a playing state
    onPlaybackEnded( id );
}


void
PlayerManager::onStopWatchTimedOut()
{
    MutableTrack( track() ).upgradeRating( Track::Scrobbled );
    emit event( PlayerEvent::ScrobblePointReached, QVariant::fromValue( track() ) );
}

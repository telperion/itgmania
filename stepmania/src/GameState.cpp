#include "global.h"
/*
-----------------------------------------------------------------------------
 Class: GameState

 Desc: See Header.

 Copyright (c) 2001-2003 by the person(s) listed below.  All rights reserved.
	Chris Danford
	Chris Gomez
-----------------------------------------------------------------------------
*/

#include "GameState.h"
#include "IniFile.h"
#include "GameManager.h"
#include "PrefsManager.h"
#include "InputMapper.h"
#include "song.h"
#include "Course.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "SongManager.h"
#include "Steps.h"
#include "NoteSkinManager.h"
#include "ModeChoice.h"
#include "NoteFieldPositioning.h"
#include "Character.h"
#include "UnlockSystem.h"
#include "AnnouncerManager.h"
#include "ProfileManager.h"
#include "arch/arch.h"
#include "ThemeManager.h"
#include "fstream"
#include "Bookkeeper.h"


GameState*	GAMESTATE = NULL;	// global and accessable from anywhere in our program

#define CHARACTERS_DIR BASE_PATH "Characters" SLASH
#define NAMES_BLACKLIST_FILE BASE_PATH "Data" SLASH "NamesBlacklist.dat"

GameState::GameState()
{
	m_CurGame = GAME_DANCE;
	m_iCoins = 0;
	/* Don't reset yet; let the first screen do it, so we can
	 * use PREFSMAN. */
//	Reset();
	m_pPosition = NULL;

	m_pUnlockingSys = new UnlockSystem;
	ReloadCharacters();
}

GameState::~GameState()
{
	delete m_pUnlockingSys;
	delete m_pPosition;
}

void GameState::Reset()
{
	ASSERT( THEME );

	int p;

	m_CurStyle = STYLE_INVALID;
	m_bPlayersCanJoin = false;
	for( p=0; p<NUM_PLAYERS; p++ )
		m_bSideIsJoined[p] = false;
//	m_iCoins = 0;	// don't reset coin count!
	m_MasterPlayerNumber = PLAYER_INVALID;
	m_sPreferredGroup	= GROUP_ALL_MUSIC;
	m_bChangedFailType = false;
	for( p=0; p<NUM_PLAYERS; p++ )
		m_PreferredDifficulty[p] = DIFFICULTY_INVALID;
	m_SongSortOrder = SORT_INVALID;
	m_PlayMode = PLAY_MODE_INVALID;
	m_bEditing = false;
	m_bDemonstrationOrJukebox = false;
	m_bJukeboxUsesModifiers = false;
	m_iCurrentStageIndex = 0;
	m_bAllow2ndExtraStage = true;
	m_bDifficultCourses = false;

	NOTESKIN->RefreshNoteSkinData( GAMESTATE->m_CurGame );

	m_iGameSeed = rand();
	m_iRoundSeed = rand();

	m_pCurSong = NULL;
	for( p=0; p<NUM_PLAYERS; p++ )
		m_pCurNotes[p] = NULL;
	m_pCurCourse = NULL;

	SAFE_DELETE( m_pPosition );
	m_pPosition = new NoteFieldPositioning("Positioning.ini");

	ResetMusicStatistics();
	ResetStageStatistics();
	SONGMAN->UpdateBest();

	m_vPlayedStageStats.clear();

	for( p=0; p<NUM_PLAYERS; p++ )
	{	
		m_CurrentPlayerOptions[p].Init();
		m_PlayerOptions[p].Init();
		m_StoredPlayerOptions[p].Init();
	}
	m_SongOptions.Init();
	
	for( p=0; p<NUM_PLAYERS; p++ )
	{
		ApplyModifiers( (PlayerNumber)p, THEME->GetMetric( "Common","DefaultModifiers" ) );
		ApplyModifiers( (PlayerNumber)p, PREFSMAN->m_sDefaultModifiers );
	}

	for( p=0; p<NUM_PLAYERS; p++ )
	{
		if( PREFSMAN->m_ShowDancingCharacters == PrefsManager::CO_RANDOM)
			m_pCurCharacters[p] = GetRandomCharacter();
		else
			m_pCurCharacters[p] = GetDefaultCharacter();
	}

	for( p=0; p<NUM_PLAYERS; p++ )
	{
		m_fSuperMeterGrowthScale[p] = 1;
		m_iCpuSkill[p] = 5;
	}

	for( p=0; p<NUM_PLAYERS; p++ )
		PROFILEMAN->UnloadProfile( (PlayerNumber)p );

	// save stats info intermitently in case of crash
	BOOKKEEPER->WriteToDisk();
	SONGMAN->SaveMachineScoresToDisk();
}

void GameState::Update( float fDelta )
{
	for( int p=0; p<NUM_PLAYERS; p++ )
	{
		m_CurrentPlayerOptions[p].Approach( m_PlayerOptions[p], fDelta );

		bool RebuildPlayerOptions = false;

		/* See if any delayed attacks are starting. */
		for( int s=0; s<MAX_SIMULTANEOUS_ATTACKS; s++ )
		{
			if( m_ActiveAttacks[p][s].fStartSecond < 0 )
				continue; /* already started */
			if( m_ActiveAttacks[p][s].fStartSecond > this->m_fMusicSeconds )
				continue; /* not yet */

			m_ActiveAttacks[p][s].fStartSecond = -1;

			ActivateAttack( (PlayerNumber)p, s, true );
			RebuildPlayerOptions = true;
		}

		/* See if any attacks are ending. */
		m_bActiveAttackEndedThisUpdate[p] = false;

		for( int s=0; s<MAX_SIMULTANEOUS_ATTACKS; s++ )
		{
			if( m_ActiveAttacks[p][s].fStartSecond >= 0 )
				continue; /* hasn't started yet */

			if( m_ActiveAttacks[p][s].fSecsRemaining <= 0 )
				continue; /* ended already */

			m_ActiveAttacks[p][s].fSecsRemaining -= fDelta;

			if( m_ActiveAttacks[p][s].fSecsRemaining > 0 )
				continue; /* continuing */

			/* ending */
			m_ActiveAttacks[p][s].fSecsRemaining = 0;
			m_ActiveAttacks[p][s].sModifier = "";
			m_bActiveAttackEndedThisUpdate[p] = true;
			RebuildPlayerOptions = true;
		}

		if( RebuildPlayerOptions )
			RebuildPlayerOptionsFromActiveAttacks( (PlayerNumber)p );
	}
}

void GameState::ReloadCharacters()
{
	unsigned i;

	for( i=0; i<m_pCharacters.size(); i++ )
		delete m_pCharacters[i];
	m_pCharacters.clear();

	for( int p=0; p<NUM_PLAYERS; p++ )
		m_pCurCharacters[p] = NULL;

	CStringArray as;
	GetDirListing( CHARACTERS_DIR "*", as, true, true );
	bool FoundDefault = false;
	for( i=0; i<as.size(); i++ )
	{
		CString sCharName, sDummy;
		splitpath(as[i], sDummy, sCharName, sDummy);
		sCharName.MakeLower();

		if( sCharName == "cvs" )	// the directory called "CVS"
			continue;		// ignore it

		if( sCharName.CompareNoCase("default")==0 )
			FoundDefault = true;

		Character* pChar = new Character;
		if( pChar->Load( as[i] ) )
			m_pCharacters.push_back( pChar );
		else
			delete pChar;
	}
	
	if( !FoundDefault )
		RageException::Throw( "Couldn't find \"Characters" SLASH "default\"" );
	if( !m_pCharacters.size() )
		RageException::Throw( "Couldn't find any character definitions" );
}

const float GameState::MUSIC_SECONDS_INVALID = -5000.0f;

void GameState::ResetMusicStatistics()
{	
	m_fMusicSeconds = MUSIC_SECONDS_INVALID;
	m_fSongBeat = 0;
	m_fCurBPS = 10;
	m_bFreeze = false;
	m_bPastHereWeGo = false;
}

void GameState::ResetStageStatistics()
{
	StageStats OldStats = GAMESTATE->m_CurStageStats;
	m_CurStageStats = StageStats();
	if( GetStageIndex() > 0 && PREFSMAN->m_bComboContinuesBetweenSongs )
		memcpy( GAMESTATE->m_CurStageStats.iCurCombo, OldStats .iCurCombo,  sizeof(OldStats.iCurCombo) );

	RemoveAllActiveAttacks();
	RemoveAllInventory();
	m_fOpponentHealthPercent = 1;
	m_fTugLifePercentP1 = 0.5f;
	for( int p=0; p<NUM_PLAYERS; p++ )
		m_fSuperMeter[p] = 0;
}

void GameState::UpdateSongPosition(float fPositionSeconds)
{
	ASSERT(m_pCurSong);

	m_fMusicSeconds = fPositionSeconds;
	m_pCurSong->GetBeatAndBPSFromElapsedTime( m_fMusicSeconds, m_fSongBeat, m_fCurBPS, m_bFreeze );
//	LOG->Trace( "m_fMusicSeconds = %f, m_fSongBeat = %f, m_fCurBPS = %f, m_bFreeze = %f", m_fMusicSeconds, m_fSongBeat, m_fCurBPS, m_bFreeze );
}

float GameState::GetSongPercent( float beat ) const
{
	/* 0 = first step; 1 = last step */
	return (beat - m_pCurSong->m_fFirstBeat) / m_pCurSong->m_fLastBeat;
}

int GameState::GetStageIndex()
{
	return m_iCurrentStageIndex;
}

int GameState::GetNumStagesLeft()
{
	if(GAMESTATE->IsExtraStage() || GAMESTATE->IsExtraStage2())
		return 1;
	if( PREFSMAN->m_bEventMode )
		return 999;
	return PREFSMAN->m_iNumArcadeStages - m_iCurrentStageIndex;
}

bool GameState::IsFinalStage()
{
	if( PREFSMAN->m_bEventMode )
		return false;
	int iPredictedStageForCurSong = 1;
	if( m_pCurSong != NULL )
		iPredictedStageForCurSong = SongManager::GetNumStagesForSong( m_pCurSong );
	
	return m_iCurrentStageIndex + iPredictedStageForCurSong == PREFSMAN->m_iNumArcadeStages;
}

bool GameState::IsExtraStage()
{
	if( PREFSMAN->m_bEventMode )
		return false;
	return m_iCurrentStageIndex == PREFSMAN->m_iNumArcadeStages;
}

bool GameState::IsExtraStage2()
{
	if( PREFSMAN->m_bEventMode )
		return false;
	return m_iCurrentStageIndex == PREFSMAN->m_iNumArcadeStages+1;
}

CString GameState::GetStageText()
{
	if( m_bDemonstrationOrJukebox )				return "demo";
	else if( m_PlayMode == PLAY_MODE_ONI )		return "oni";
	else if( m_PlayMode == PLAY_MODE_NONSTOP )	return "nonstop";
	else if( m_PlayMode == PLAY_MODE_ENDLESS )	return "endless";
	else if( PREFSMAN->m_bEventMode )			return "event";
	else if( IsFinalStage() )					return "final";
	else if( IsExtraStage() )					return "extra1";
	else if( IsExtraStage2() )					return "extra2";
	else										return ssprintf("%d",m_iCurrentStageIndex+1);
}

void GameState::GetAllStageTexts( CStringArray &out )
{
	out.clear();
	out.push_back( "demo" );
	out.push_back( "oni" );
	out.push_back( "nonstop" );
	out.push_back( "endless" );
	out.push_back( "event" );
	out.push_back( "final" );
	out.push_back( "extra1" );
	out.push_back( "extra2" );
	for( int stage = 0; stage < PREFSMAN->m_iNumArcadeStages; ++stage )
		out.push_back( ssprintf("%d",stage+1) );
}

int GameState::GetCourseSongIndex()
{
	int iSongIndex = 0;
	/* iSongsPlayed includes the current song, so it's 1-based; subtract one. */
	for( int p=0; p<NUM_PLAYERS; p++ )
		if( IsPlayerEnabled(p) )
			iSongIndex = max( iSongIndex, m_CurStageStats.iSongsPlayed[p]-1 );
	return iSongIndex;
}

GameDef* GameState::GetCurrentGameDef()
{
	ASSERT( m_CurGame != GAME_INVALID );	// the game must be set before calling this
	return GAMEMAN->GetGameDefForGame( m_CurGame );
}

const StyleDef* GameState::GetCurrentStyleDef()
{

	ASSERT( m_CurStyle != STYLE_INVALID );	// the style must be set before calling this
	return GAMEMAN->GetStyleDefForStyle( m_CurStyle );
}


bool GameState::IsPlayerEnabled( PlayerNumber pn )
{
	// In rave, all players are present.  Non-human players are CPU controlled.
	switch( m_PlayMode )
	{
	case PLAY_MODE_BATTLE:
	case PLAY_MODE_RAVE:
		return true;
	}

	return IsHumanPlayer( pn );
}

bool GameState::IsHumanPlayer( PlayerNumber pn )
{
	if( m_CurStyle == STYLE_INVALID )	// no style chosen
		if( this->m_bPlayersCanJoin )	
			return m_bSideIsJoined[pn];	// only allow input from sides that have already joined
		else
			return true;	// if we can't join, then we're on a screen like MusicScroll or GameOver

	switch( GetCurrentStyleDef()->m_StyleType )
	{
	case StyleDef::TWO_PLAYERS_TWO_CREDITS:
		return true;
	case StyleDef::ONE_PLAYER_ONE_CREDIT:
	case StyleDef::ONE_PLAYER_TWO_CREDITS:
		return pn == m_MasterPlayerNumber;
	default:
		ASSERT(0);		// invalid style type
		return false;
	}
}

PlayerNumber GameState::GetFirstHumanPlayer()
{
	for( int p=0; p<NUM_PLAYERS; p++ )
		if( IsHumanPlayer(p) )
			return (PlayerNumber)p;
	ASSERT(0);	// there must be at least 1 human player
	return PLAYER_INVALID;
}

bool GameState::IsCpuPlayer( PlayerNumber pn )
{
	return IsPlayerEnabled(pn) && !IsHumanPlayer(pn);
}


bool GameState::IsCourseMode() const
{
	switch(m_PlayMode)
	{
	case PLAY_MODE_ONI:
	case PLAY_MODE_NONSTOP:
	case PLAY_MODE_ENDLESS:
		return true;
	default:
		return false;
	}
}

bool GameState::IsBattleMode() const
{
	switch( GAMESTATE->m_PlayMode )
	{
	case PLAY_MODE_BATTLE:
		return true;
	default:
		return false;
	}
}

bool GameState::HasEarnedExtraStage()
{
	if( PREFSMAN->m_bEventMode )
		return false;

	if( !PREFSMAN->m_bAllowExtraStage )
		return false;

	if( GAMESTATE->m_PlayMode != PLAY_MODE_ARCADE )
		return false;

	if( (GAMESTATE->IsFinalStage() || GAMESTATE->IsExtraStage()) )
	{
		for( int p=0; p<NUM_PLAYERS; p++ )
		{
			if( !GAMESTATE->IsPlayerEnabled(p) )
				continue;	// skip

			if( GAMESTATE->m_pCurNotes[p]->GetDifficulty() != DIFFICULTY_HARD && 
				GAMESTATE->m_pCurNotes[p]->GetDifficulty() != DIFFICULTY_CHALLENGE )
				continue; /* not hard enough! */

			/* If "choose EX" is enabled, then we should only grant EX2 if the chosen
			 * stage was the EX we would have chosen (m_bAllow2ndExtraStage is true). */
			if( PREFSMAN->m_bPickExtraStage && GAMESTATE->IsExtraStage() && !GAMESTATE->m_bAllow2ndExtraStage )
				continue;

			if( m_CurStageStats.GetGrade((PlayerNumber)p) >= GRADE_AA )
				return true;
		}
	}
	return false;
}

PlayerNumber GameState::GetBestPlayer()
{
	PlayerNumber winner = PLAYER_1;
	for( int p=PLAYER_1+1; p<NUM_PLAYERS; p++ )
	{
		if( GAMESTATE->m_CurStageStats.iActualDancePoints[p] == GAMESTATE->m_CurStageStats.iActualDancePoints[winner] )
			return PLAYER_INVALID;	// draw
		else if( GAMESTATE->m_CurStageStats.iActualDancePoints[p] > GAMESTATE->m_CurStageStats.iActualDancePoints[winner] )
			winner = (PlayerNumber)p;
	}
	return winner;
}

StageResult GameState::GetStageResult( PlayerNumber pn )
{
	switch( GAMESTATE->m_PlayMode )
	{
	case PLAY_MODE_BATTLE:
	case PLAY_MODE_RAVE:
		switch( pn )
		{
		case PLAYER_1:	return (m_fTugLifePercentP1>=0.5f)?RESULT_WIN:RESULT_LOSE;
		case PLAYER_2:	return (m_fTugLifePercentP1<0.5f)?RESULT_WIN:RESULT_LOSE;
		default:	ASSERT(0); return RESULT_LOSE;
		}
	default:
		return (GetBestPlayer()==pn)?RESULT_WIN:RESULT_LOSE;
	}
}

void GameState::GetFinalEvalStatsAndSongs( StageStats& statsOut, vector<Song*>& vSongsOut )
{
	statsOut = StageStats();

	// Show stats only for the latest 3 normal songs + passed extra stages
	int PassedRegularSongsLeft = 3;
	for( int i = (int)GAMESTATE->m_vPlayedStageStats.size()-1; i >= 0; --i )
	{
		const StageStats &s = GAMESTATE->m_vPlayedStageStats[i];

		if( !s.OnePassed() )
			continue;

		if( s.StageType == StageStats::STAGE_NORMAL )
		{
			if( PassedRegularSongsLeft == 0 )
				break;

			--PassedRegularSongsLeft;
		}

		statsOut.AddStats( s );

		vSongsOut.insert( vSongsOut.begin(), s.pSong );
	}

	if(!vSongsOut.size()) return;

	/* XXX: I have no idea if this is correct--but it's better than overflowing,
	 * anyway. -glenn */
	for( int p=0; p<NUM_PLAYERS; p++ )
	{
		if( !IsPlayerEnabled(p) )
			continue;

		for( int r = 0; r < NUM_RADAR_CATEGORIES; r++)
		{
			statsOut.fRadarPossible[p][r] /= vSongsOut.size();
			statsOut.fRadarActual[p][r] /= vSongsOut.size();
		}
	}
}


void GameState::ApplyModifiers( PlayerNumber pn, CString sModifiers )
{
	const SongOptions::FailType ft = GAMESTATE->m_SongOptions.m_FailType;

	m_PlayerOptions[pn].FromString( sModifiers );
	m_SongOptions.FromString( sModifiers );

	if( ft != GAMESTATE->m_SongOptions.m_FailType )
		GAMESTATE->m_bChangedFailType = true;
}

/* Store the player's preferred options.  This is called at the very beginning
 * of gameplay. */
void GameState::StoreSelectedOptions()
{
	for( int p=0; p<NUM_PLAYERS; p++ )
		GAMESTATE->m_StoredPlayerOptions[p] = GAMESTATE->m_PlayerOptions[p];
	m_StoredSongOptions = m_SongOptions;
}

/* Restore the preferred options.  This is called after a song ends, before
 * setting new course options, so options from one song don't carry into the
 * next and we default back to the preferred options.  This is also called
 * at the end of gameplay to restore options. */
void GameState::RestoreSelectedOptions()
{
	for( int p=0; p<NUM_PLAYERS; p++ )
		GAMESTATE->m_PlayerOptions[p] = GAMESTATE->m_StoredPlayerOptions[p];
	m_SongOptions = m_StoredSongOptions;
}

void GameState::ResetNoteSkins()
{
	for( int pn = 0; pn < NUM_PLAYERS; ++pn )
	{
		m_BeatToNoteSkin[pn].clear();
		m_BeatToNoteSkin[pn][-1000] = GAMESTATE->m_PlayerOptions[pn].m_sNoteSkin;
	}

	m_BeatToNoteSkinRev = 0;
}

void GameState::GetAllUsedNoteSkins( vector<CString> &out ) const
{
	for( int pn=0; pn<NUM_PLAYERS; ++pn )
	{
		out.push_back( GAMESTATE->m_PlayerOptions[pn].m_sNoteSkin );

		switch( GAMESTATE->m_PlayMode )
		{
		case PLAY_MODE_BATTLE:
		case PLAY_MODE_RAVE:
			for( int al=0; al<NUM_ATTACK_LEVELS; al++ )
			{
				const Character *ch = GAMESTATE->m_pCurCharacters[pn];
				ASSERT( ch );
				const CString* asAttacks = ch->m_sAttacks[al];
				for( int att = 0; att < NUM_ATTACKS_PER_LEVEL; ++att )
				{
					PlayerOptions po;
					po.FromString( asAttacks[att] );
					if( po.m_sNoteSkin != "" )
						out.push_back( po.m_sNoteSkin );
				}
			}
		}

		for( map<float,CString>::const_iterator it = m_BeatToNoteSkin[pn].begin(); 
			it != m_BeatToNoteSkin[pn].end(); ++it )
			out.push_back( it->second );
	}
}

/* From NoteField: */

void GameState::GetUndisplayedBeats( PlayerNumber pn, float TotalSeconds, float &StartBeat, float &EndBeat )
{
	/* If reasonable, push the attack forward so notes on screen don't change suddenly. */
	StartBeat = min( this->m_fSongBeat+BEATS_PER_MEASURE*2, m_fLastDrawnBeat[pn] );
	StartBeat = truncf(StartBeat)+1;

	const float StartSecond = this->m_pCurSong->GetElapsedTimeFromBeat( StartBeat );
	const float EndSecond = StartSecond + TotalSeconds;
	EndBeat = this->m_pCurSong->GetBeatFromElapsedTime( EndSecond );
	EndBeat = truncf(EndBeat)+1;
}

void GameState::ActivateAttack( PlayerNumber target, int slot, bool ActivingDelayedAttack )
{
	const Attack &a = m_ActiveAttacks[target][slot];

	enum { QUEUE_FOR_LATER, ACTIVATE_QUEUED_ATTACK, START_IMMEDIATELY } type;
	if( ActivingDelayedAttack )
	{
		ASSERT( a.fStartSecond < 0 );
		type = ACTIVATE_QUEUED_ATTACK;
	}
	else if( a.fStartSecond >= 0 )
		type = QUEUE_FOR_LATER;
	else
		type = START_IMMEDIATELY;

	//
	// Peek at the effect being applied.  If it's a transform, add it to 
	// a list of transforms that should be applied by the Player on its 
	// next update.
	//
	PlayerOptions po;
	po.FromString( a.sModifier );
	if( type != QUEUE_FOR_LATER && po.m_Transform != PlayerOptions::TRANSFORM_NONE )
	{
		m_TransformsToApply[target].push_back( po.m_Transform );
	}

	if( po.m_sNoteSkin != "" )
	{
		if( type == QUEUE_FOR_LATER )
		{
			float StartBeat = this->m_pCurSong->GetBeatFromElapsedTime( a.fStartSecond );
			float EndBeat = this->m_pCurSong->GetBeatFromElapsedTime( a.fStartSecond+a.fSecsRemaining );
			LOG->Trace("... x queue at %f..%f", StartBeat, EndBeat );
			SetNoteSkinForBeatRange( target, po.m_sNoteSkin, StartBeat, EndBeat );
		}
		else if( type == START_IMMEDIATELY )
		{
			/* We're changing note skins on the fly.  Add it in the future, past what's
			 * currently on screen, so new arrows will scroll on screen with this skin. */
			float StartBeat, EndBeat;
			GetUndisplayedBeats( target, a.fSecsRemaining, StartBeat, EndBeat );
			SetNoteSkinForBeatRange( target, po.m_sNoteSkin, StartBeat, EndBeat );
		}
	}
}

void GameState::SetNoteSkinForBeatRange( PlayerNumber pn, CString sNoteSkin, float StartBeat, float EndBeat )
{
	map<float,CString> &BeatToNoteSkin = m_BeatToNoteSkin[pn];

	/* Erase any other note skin settings in this range. */
	map<float,CString>::iterator it = BeatToNoteSkin.lower_bound( StartBeat );
	map<float,CString>::iterator end = BeatToNoteSkin.upper_bound( EndBeat );
	while( it != end )
	{
		map<float,CString>::iterator next = it;
		++next;

		BeatToNoteSkin.erase( it );

		it = next;
	}

	/* Add the skin to m_BeatToNoteSkin.  */
	BeatToNoteSkin[StartBeat] = sNoteSkin;

	/* Return to the default note skin after the duration. */
	BeatToNoteSkin[EndBeat] = m_StoredPlayerOptions[pn].m_sNoteSkin;

	++m_BeatToNoteSkinRev;
}

/* This is called to launch an attack, or to queue an attack if a.fStartSecond
 * is set.  This is also called by GameState::Update when activating a queued attack. */
void GameState::LaunchAttack( PlayerNumber target, Attack a )
{
	LOG->Trace( "Launch attack '%s' against P%d at %f", a.sModifier.c_str(), target+1, a.fStartSecond );

	// search for an open slot
	for( int s=0; s<MAX_SIMULTANEOUS_ATTACKS; s++ )
	{
		if( m_ActiveAttacks[target][s].fSecsRemaining <= 0 )
		{
			m_ActiveAttacks[target][s] = a;
			ActivateAttack( target, s, false );
			GAMESTATE->RebuildPlayerOptionsFromActiveAttacks( target );
			return;
		}
	}

	LOG->Warn("Couldn't launch attack '%s' against p%i: no empty attack slots",
		a.sModifier.c_str(), target );
}

void GameState::RemoveActiveAttacksForPlayer( PlayerNumber pn, AttackLevel al )
{
	for( int s=0; s<MAX_SIMULTANEOUS_ATTACKS; s++ )
	{
		if( al != NUM_ATTACK_LEVELS && al != m_ActiveAttacks[pn][s].level )
			continue;
		m_ActiveAttacks[pn][s].fSecsRemaining = 0;
		m_ActiveAttacks[pn][s].sModifier = "";
	}
	RebuildPlayerOptionsFromActiveAttacks( (PlayerNumber)pn );
}

void GameState::RemoveAllInventory()
{
	for( int p=0; p<NUM_PLAYERS; p++ )
		for( int s=0; s<NUM_INVENTORY_SLOTS; s++ )
		{
			m_Inventory[p][s].fSecsRemaining = 0;
			m_Inventory[p][s].sModifier = "";
		}
}

void GameState::RebuildPlayerOptionsFromActiveAttacks( PlayerNumber pn )
{
	// rebuild player options
	PlayerOptions po = m_StoredPlayerOptions[pn];
	for( int s=0; s<MAX_SIMULTANEOUS_ATTACKS; s++ )
	{
		if( m_ActiveAttacks[pn][s].fStartSecond >= 0 )
			continue; /* hasn't started yet */
		po.FromString( m_ActiveAttacks[pn][s].sModifier );
	}
	m_PlayerOptions[pn] = po;
}

int GameState::GetSumOfActiveAttackLevels( PlayerNumber pn )
{
	int iSum = 0;

	for( int s=0; s<MAX_SIMULTANEOUS_ATTACKS; s++ )
		if( m_ActiveAttacks[pn][s].fSecsRemaining > 0 && m_ActiveAttacks[pn][s].level != NUM_ATTACK_LEVELS )
			iSum += m_ActiveAttacks[pn][s].level;

	return iSum;
}

template<class T>
void setmin( T &a, const T &b )
{
	a = min(a, b);
}

template<class T>
void setmax( T &a, const T &b )
{
	a = max(a, b);
}

/* Adjust the fail mode based on the chosen difficulty.  This must be called
 * after the difficulty has been finalized (usually in ScreenSelectMusic or
 * ScreenPlayerOptions), and before the fail mode is displayed or used (usually
 * in ScreenSongOptions). */
void GameState::AdjustFailType()
{
	/* Single song mode only. */
	if( this->IsCourseMode() )
		return;

	/* If the player changed the fail mode explicitly, leave it alone. */
	if( GAMESTATE->m_bChangedFailType )
		return;

	/* Find the easiest difficulty notes selected by either player. */
	Difficulty dc = DIFFICULTY_INVALID;
	for( int p=0; p<NUM_PLAYERS; p++ )
	{
		if( !GAMESTATE->IsHumanPlayer(p) )
			continue;	// skip

		dc = min(dc, GAMESTATE->m_pCurNotes[p]->GetDifficulty());
	}

	/* Reset the fail type to the default. */
	SongOptions so;
	so.FromString( PREFSMAN->m_sDefaultModifiers );
	GAMESTATE->m_SongOptions.m_FailType = so.m_FailType;

    /* Easy and beginner are never harder than FAIL_END_OF_SONG. */
	if(dc <= DIFFICULTY_EASY)
		setmax(GAMESTATE->m_SongOptions.m_FailType, SongOptions::FAIL_END_OF_SONG);

	/* If beginner's steps were chosen, and this is the first stage,
		* turn off failure completely--always give a second try. */
	if(dc == DIFFICULTY_BEGINNER &&
		!PREFSMAN->m_bEventMode && /* stage index is meaningless in event mode */
		GAMESTATE->m_iCurrentStageIndex == 0)
		setmax(GAMESTATE->m_SongOptions.m_FailType, SongOptions::FAIL_OFF);
}

bool GameState::ShowMarvelous() const
{
	if (PREFSMAN->m_iMarvelousTiming == 2)
		return true;

	if (PREFSMAN->m_iMarvelousTiming == 1)
		if (IsCourseMode())
			return true;

	return false;
}

void GameState::GetCharacters( vector<Character*> &apCharactersOut )
{
	for( unsigned i=0; i<m_pCharacters.size(); i++ )
		if( m_pCharacters[i]->m_sName.CompareNoCase("default")!=0 )
			apCharactersOut.push_back( m_pCharacters[i] );
}

Character* GameState::GetRandomCharacter()
{
	vector<Character*> apCharacters;
	GetCharacters( apCharacters );
	if( apCharacters.size() )
		return apCharacters[rand()%apCharacters.size()];
	else
		return GetDefaultCharacter();
}

Character* GameState::GetDefaultCharacter()
{
	for( unsigned i=0; i<m_pCharacters.size(); i++ )
	{
		if( m_pCharacters[i]->m_sName.CompareNoCase("default")==0 )
			return m_pCharacters[i];
	}

	/* We always have the default character. */
	ASSERT(0);
	return NULL;
}

struct SongAndSteps
{
	Song* pSong;
	Steps* pSteps;
	bool operator==( const SongAndSteps& other ) const { return pSong==other.pSong && pSteps==other.pSteps; }
	bool operator<( const SongAndSteps& other ) const { return pSong<=other.pSong && pSteps<=other.pSteps; }
};

void GameState::GetRankingFeats( PlayerNumber pn, vector<RankingFeats> &asFeatsOut )
{
	if( !IsHumanPlayer(pn) )
		return;

	switch( GAMESTATE->m_PlayMode )
	{
	case PLAY_MODE_ARCADE:
		{
			unsigned i, j;

			StepsType nt = GAMESTATE->GetCurrentStyleDef()->m_StepsType;

			//
			// Find unique Song and Steps combinations that were played.
			// We must keep only the unique combination or else we'll double-count
			// high score markers.
			//
			vector<SongAndSteps> vSongAndSteps;

			for( i=0; i<m_vPlayedStageStats.size(); i++ )
			{
				SongAndSteps sas;
				sas.pSong = m_vPlayedStageStats[i].pSong;
				ASSERT( sas.pSong );
				sas.pSteps = m_vPlayedStageStats[i].pSteps[pn];
				ASSERT( sas.pSteps );
				vSongAndSteps.push_back( sas );
			}

			sort( vSongAndSteps.begin(), vSongAndSteps.end() );

			vector<SongAndSteps>::iterator toDelete = unique( vSongAndSteps.begin(), vSongAndSteps.end() );
			vSongAndSteps.erase(toDelete, vSongAndSteps.end());

			for( i=0; i<vSongAndSteps.size(); i++ )
			{
				Song* pSong = vSongAndSteps[i].pSong;
				Steps* pSteps = vSongAndSteps[i].pSteps;

				// Find Machine Records
				vector<Steps::MemCardData::HighScore> &vMachineHighScores = pSteps->m_MemCardDatas[MEMORY_CARD_MACHINE].vHighScores;
				for( j=0; j<vMachineHighScores.size(); j++ )
				{
					if( vMachineHighScores[j].sName != RANKING_TO_FILL_IN_MARKER[pn] )
						continue;

					RankingFeats feat;
					feat.Type = RankingFeats::SONG;
					feat.Feat = ssprintf("MR #%d in %s %s", j+1, pSong->GetTranslitMainTitle().c_str(), DifficultyToString(pSteps->GetDifficulty()).c_str() );
					feat.pStringToFill = &vMachineHighScores[j].sName;
					feat.g = vMachineHighScores[j].grade;
					if( PREFSMAN->m_bPercentageScoring )
						feat.Score = vMachineHighScores[j].fPercentDP;
					else
						feat.Score = (float) vMachineHighScores[j].iScore;

					// XXX: temporary hack
					if( pSong->HasBackground() )
						feat.Banner = pSong->GetBackgroundPath();
//					if( pSong->HasBanner() )
//						feat.Banner = pSong->GetBannerPath();

					asFeatsOut.push_back( feat );
				}
		
				// Find Personal Records
				vector<Steps::MemCardData::HighScore> &vPersonalHighScores = pSteps->m_MemCardDatas[pn].vHighScores;
				for( j=0; j<vPersonalHighScores.size(); j++ )
				{
					if( vPersonalHighScores[j].sName != RANKING_TO_FILL_IN_MARKER[pn] )
						continue;

					RankingFeats feat;
					feat.Type = RankingFeats::SONG;
					feat.Feat = ssprintf("PR #%d in %s %s", j+1, pSong->GetTranslitMainTitle().c_str(), DifficultyToString(pSteps->GetDifficulty()).c_str() );
					feat.pStringToFill = &vPersonalHighScores[j].sName;
					feat.g = vPersonalHighScores[j].grade;
					feat.Score = (float) vPersonalHighScores[j].iScore;

					// XXX: temporary hack
					if( pSong->HasBackground() )
						feat.Banner = pSong->GetBackgroundPath();
	//					if( pSong->HasBanner() )
	//						feat.Banner = pSong->GetBannerPath();

					asFeatsOut.push_back( feat );
				}
			}

			StageStats stats;
			vector<Song*> vSongs;
			GetFinalEvalStatsAndSongs( stats, vSongs );


			for( i=0; i<NUM_RANKING_CATEGORIES; i++ )
			{
				vector<SongManager::CategoryData::HighScore> &vHighScores = SONGMAN->m_CategoryDatas[nt][i].vHighScores;
				for( unsigned j=0; j<vHighScores.size(); j++ )
				{
					if( vHighScores[j].sName != RANKING_TO_FILL_IN_MARKER[pn] )
						continue;

					RankingFeats feat;
					feat.Type = RankingFeats::RANKING;
					feat.Feat = ssprintf("#%d in Type %c (%d)", j+1, 'A'+i, stats.iMeter[pn] );
					feat.pStringToFill = &vHighScores[j].sName;
					feat.g = GRADE_NO_DATA;
					feat.Score = (float) vHighScores[j].iScore;
					asFeatsOut.push_back( feat );
				}
			}
		}
		break;
	case PLAY_MODE_BATTLE:
	case PLAY_MODE_RAVE:
		break;
	case PLAY_MODE_NONSTOP:
	case PLAY_MODE_ONI:
	case PLAY_MODE_ENDLESS:
		{
			StepsType nt = GAMESTATE->GetCurrentStyleDef()->m_StepsType;
			Course* pCourse = GAMESTATE->m_pCurCourse;
			vector<Course::MemCardData::HighScore> &vHighScores = pCourse->m_MemCardDatas[nt][MEMORY_CARD_MACHINE].vHighScores;
			for( unsigned i=0; i<vHighScores.size(); i++ )
			{
				if( vHighScores[i].sName != RANKING_TO_FILL_IN_MARKER[pn] )
						continue;

				RankingFeats feat;
				feat.Type = RankingFeats::COURSE;
				feat.Feat = ssprintf("No. %d in %s", i+1, pCourse->m_sName.c_str() );
				feat.pStringToFill = &vHighScores[i].sName;
				feat.g = GRADE_NO_DATA;
				feat.Score = (float) vHighScores[i].iScore;
				if( pCourse->HasBanner() )
					feat.Banner = pCourse->m_sBannerPath;
				asFeatsOut.push_back( feat );
			}
		}
		break;
	default:
		ASSERT(0);
	}
}

void GameState::StoreRankingName( PlayerNumber pn, CString name )
{
	//
	// Filter swear words from name
	//
	name.MakeUpper();
	ifstream f;
	f.open( NAMES_BLACKLIST_FILE );
	if( f.good() )
	{
		CString sLine;
		while( getline(f,sLine) )
		{
			sLine.MakeUpper();
			if( name.Find(sLine) != -1 )
				name = "";
		}
	}

	vector<RankingFeats> aFeats;
	GetRankingFeats( pn, aFeats );

	for( unsigned i=0; i<aFeats.size(); i++ )
	{
		*aFeats[i].pStringToFill = name;
	}
}

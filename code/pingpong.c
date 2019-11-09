#include "pingpong.h"
#include "MAX72S19.h"
#include "stdbool.h"

#define PINGPONG_PLAYER_NONE 0
#define PINGPONG_PLAYER_1    1
#define PINGPONG_PLAYER_2    2

#define PINGPONG_DISPMODE_GAME 0
#define PINGPONG_DISPMODE_SET  1
#define PINGPONG_DISPMODE_ALL  2

#define PINGPONG_STATE_IDLE     0
#define PINGPONG_STATE_GAME     1
#define PINGPONG_STATE_GAME_END 2

#define PINGPONG_POINTS_TO_WIN 11
#define PINGPONG_MIN_POINT_DIFF_TO_WIN 2
#define PINGPONG_POINTS_TO_CHANGE_SERVE 2

#define LED_PLAYER1     (6)
#define LED_PLAYER2     (5)
#define LED_ROW_PLAYERS (4)

static void _swapSides();
static void _writeScore(uint8_t player, uint8_t score);
static void _refreshDisplay();
static uint8_t _getCurrentPlayer();
static void _indicatePlayerTurn(uint8_t player);
static void _addPoint(uint8_t player);
static void _removePoint(uint8_t player);
static void _toggleMode();
static bool _isGameOver();
static uint8_t _getWinningPlayer();
static void _endOfGame();

static uint8_t _startingPlayer = PINGPONG_PLAYER_NONE;
static uint8_t _currentPlayer = PINGPONG_PLAYER_NONE;

static uint8_t _state = PINGPONG_STATE_IDLE;
static uint8_t _dispMode = PINGPONG_DISPMODE_GAME;
static uint8_t _gameScores[] =    { 0, 0 };
static uint8_t _setScores[] =     { 0, 0 };
static uint8_t _allTimeScores[] = { 0, 0 };
static bool _pButtonDown[] = { false, false };
static bool _mButtonDown = false;

void pingpongInit() {
  // TODO load allTimeScores from eeprom
  _refreshDisplay();
}

void pingpongGameLoop() {
  while (1) {
  }
}

void pingpongPlayerButtonDown(uint8_t player) {
  _pButtonDown[player - 1] = true;
  // TODO: debounce
  
  switch (_state) {
    case PINGPONG_STATE_IDLE:
      if (_startingPlayer == PINGPONG_PLAYER_NONE) {
        _startingPlayer = player;
        _currentPlayer = _startingPlayer;
        _indicatePlayerTurn(_currentPlayer);
        _state = PINGPONG_STATE_GAME;
        return;
      }

      _state = PINGPONG_STATE_GAME;
      _indicatePlayerTurn(_currentPlayer);
      break;

    case PINGPONG_STATE_GAME:
      _addPoint(player);
      break;

    case PINGPONG_STATE_GAME_END:
      // TODO: start new game
      break;
  }
}

void pingpongPlayerButtonUp(uint8_t player) {
  _pButtonDown[player - 1] = false;
}

void pingpongModeButtonDown() {
  _mButtonDown = true;
  _toggleMode();
}

void pingpongModeButtonUp() {
  _mButtonDown = false;
}

static void _swapSides() {
  uint8_t sw = _gameScores[0];
  _gameScores[0] = _gameScores[1];
  _gameScores[1] = sw;

  sw = _setScores[0];
  _setScores[0] = _setScores[1];
  _setScores[1] = sw;

  sw = _allTimeScores[0];
  _allTimeScores[0] = _allTimeScores[1];
  _allTimeScores[1] = sw;

  _refreshDisplay();
}

static void _writeScore(uint8_t player, uint8_t score) {
  uint8_t digitIndex;

  // score assumed to be at most 99
  if (player == PINGPONG_PLAYER_NONE) return;

  digitIndex = player == PINGPONG_PLAYER_1 ? 3 : 1;

  if (score < 10) {
    displayWriteChar(digitIndex, ' ', false);
  } else {
    displayWriteChar(digitIndex, '0' + (score / 10), false);
  }

  displayWriteChar(
      digitIndex - 1,
      '0' + score % 10,
      player == PINGPONG_PLAYER_1);
}

static void _refreshDisplay() {
  uint8_t p1Score;
  uint8_t p2Score;

  switch (_dispMode) {
    case PINGPONG_DISPMODE_GAME:
      p1Score = _gameScores[0];
      p2Score = _gameScores[1];
      break;

    case PINGPONG_DISPMODE_SET:
      p1Score = _setScores[0];
      p2Score = _setScores[1];
      break;

    case PINGPONG_DISPMODE_ALL:
      p1Score = _allTimeScores[0];
      p2Score = _allTimeScores[1];
      break;
  }

  _writeScore(PINGPONG_PLAYER_1, p1Score);
  _writeScore(PINGPONG_PLAYER_2, p2Score);
}

static uint8_t _getCurrentPlayer() {
  uint8_t combinedScore = _gameScores[0] + _gameScores[1];
  uint8_t swpPoints = PINGPONG_POINTS_TO_CHANGE_SERVE;
  uint8_t nonStartingPlayer = _startingPlayer == PINGPONG_PLAYER_1
    ? PINGPONG_PLAYER_2
    : PINGPONG_PLAYER_1;

  return combinedScore % (swpPoints * 2) < swpPoints 
    ? _startingPlayer 
    : nonStartingPlayer;
}

static void _indicatePlayerTurn(uint8_t player) {
  uint8_t ledOutput = (player == PINGPONG_PLAYER_NONE)
    ? 0x00
    : (player == PINGPONG_PLAYER_1)
      ? (1 << LED_PLAYER1)
      : (1 << LED_PLAYER2);

  displaySetRow(LED_ROW_PLAYERS, ledOutput);
}

static void _addPoint(uint8_t player) {
  if (player == PINGPONG_PLAYER_NONE) return;

  _gameScores[player - 1]++;
  _writeScore(player, _gameScores[player - 1]);
  _currentPlayer = _getCurrentPlayer();
  _indicatePlayerTurn(_currentPlayer);

  if (_isGameOver()) _endOfGame();
}

static void _removePoint(uint8_t player) {
  // TODO
}

static void _toggleMode() {
  _dispMode++;
  if (_dispMode > PINGPONG_DISPMODE_ALL) _dispMode = PINGPONG_DISPMODE_GAME;
  _refreshDisplay();
  // TODO: add some LEDs to show what the current mode is, with another digit
}

static bool _isGameOver() {
  int8_t p1Score = _gameScores[0];
  int8_t p2Score = _gameScores[1];
  int8_t scoreDiff = (p1Score > p2Score) > 0
    ? (p1Score - p2Score)
    : (p2Score - p1Score);

  if (p1Score < PINGPONG_POINTS_TO_WIN && p2Score < PINGPONG_POINTS_TO_WIN) {
    return false;
  }

  if (scoreDiff < PINGPONG_MIN_POINT_DIFF_TO_WIN) return false;

  return true;
}

// Assumes the game is indeed over.
static uint8_t _getWinningPlayer() {
  return _gameScores[0] > _gameScores[1]
    ? PINGPONG_PLAYER_1
    : PINGPONG_PLAYER_2;
}

static void _endOfGame() {
  uint8_t winner = _getWinningPlayer();

  _setScores[winner - 1]++;
  _allTimeScores[winner - 1]++;
  _state = PINGPONG_STATE_GAME_END;

  // TODO: blink current game scores a few times
  // Then show set score for a while, then reset to game scores that are now 0-0

  // TODO tempoarily switch mode to show set scores
}

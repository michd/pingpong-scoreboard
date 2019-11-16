#include <avr/eeprom.h>
#include "pingpong.h"
#include "animation.h"
#include "MAX72S19.h"
#include "button.h"
#include "stdbool.h"

#define PINGPONG_STATE_IDLE     0
#define PINGPONG_STATE_GAME     1
#define PINGPONG_STATE_GAME_END 2

#define PINGPONG_POINTS_TO_WIN 11
#define PINGPONG_MIN_POINT_DIFF_TO_WIN 2
#define PINGPONG_POINTS_TO_CHANGE_SERVE 2

#define LED_PLAYER1     (6)
#define LED_PLAYER2     (5)
#define LED_ROW_PLAYERS (4)

#define LED_ROW_DISPMODE  (5)
#define LED_DISPMODE_GAME (6)
#define LED_DISPMODE_SET  (5)
#define LED_DISPMODE_ALL  (4)

#define OTHER_PLAYER(p) (p == PINGPONG_PLAYER_1\
    ? PINGPONG_PLAYER_2 \
    : PINGPONG_PLAYER_1)

#define SAVE_DELAY_TICKS (500 * 30) //30 seconds

static void _modeButtonPress();
static void _modeButtonLongPress();
static void _playerButtonPress(uint8_t);
static void _playerButtonLongPress(uint8_t);
static void _swapSides();
static void _writeScore(uint8_t player, uint8_t score);
static void _refreshDisplay();
static uint8_t _getCurrentPlayer();
static void _indicatePlayerTurn(uint8_t player);
static void _addPoint(uint8_t player);
static void _removePoint(uint8_t player);
static void _toggleMode();
static void _setMode(uint8_t);
static void _resetScore();
static bool _isGameOver();
static uint8_t _getWinningPlayer();
static void _endOfGame();
static void _newGame();
static void _indicateIfScoresSaved();

static uint16_t eepromAddrPlayer1;
static uint16_t eepromAddrPlayer2;

static uint8_t _startingPlayer = PINGPONG_PLAYER_NONE;
static uint8_t _currentPlayer = PINGPONG_PLAYER_NONE;
static uint16_t _ticks;
static uint8_t _state = PINGPONG_STATE_IDLE;
static uint8_t _dispMode = PINGPONG_DISPMODE_NONE;
static uint8_t _gameScores[] =    { 0, 0 };
static uint8_t _setScores[] =     { 0, 0 };
static uint8_t _allTimeScores[] = { 0, 0 };
static uint8_t _cachedAllTimeScores[] = { 0, 0 };
static uint32_t _scoresLastSaved;
static void _saveScores();
static Button * _playerButtons[2];
static Button * _modeButton;

void pingpongInit(
  Button * p1Button, Button * p2Button, Button * modeButton,
  uint16_t eepromP1, uint16_t eepromP2) {
  _playerButtons[0] = p1Button;
  _playerButtons[1] = p2Button;
  _modeButton = modeButton;

  eepromAddrPlayer1 = eepromP1;
  eepromAddrPlayer2 = eepromP2;

  _allTimeScores[0] = eeprom_read_word((uint16_t*)eepromAddrPlayer1);
  _allTimeScores[1] = eeprom_read_word((uint16_t*)eepromAddrPlayer2);
  _cachedAllTimeScores[0] = _allTimeScores[0];
  _cachedAllTimeScores[1] = _allTimeScores[1];

  animationTrigger(Startup);
}

void pingpongGameTick() {
  _ticks++;
  _saveScores();
}

void pingpongButtonPress(Button * button) {
  animationClear();

  if (button == _modeButton) {
    _modeButtonPress();
    return;
  }

  _playerButtonPress(
      button == _playerButtons[0] ? PINGPONG_PLAYER_1 : PINGPONG_PLAYER_2);
}

void pingpongButtonLongPress(Button * button) {
  animationClear();

  if (button == _modeButton) {
    _modeButtonLongPress();
    return;
  }

  _playerButtonLongPress(
      button == _playerButtons[0] ? PINGPONG_PLAYER_1 : PINGPONG_PLAYER_2);
}

void pingpongSetMode(uint8_t newMode) {
  _setMode(newMode);
}

void pingpongIndicatePlayerTurn(uint8_t player) {
  _indicatePlayerTurn(player);
}

static void _modeButtonPress() {
  _toggleMode();
}

static void _modeButtonLongPress() {
  _resetScore();
}

static void _playerButtonPress(uint8_t player) {
  switch (_state) {
    case PINGPONG_STATE_IDLE:
      if (_startingPlayer == PINGPONG_PLAYER_NONE) {
        _startingPlayer = _currentPlayer = player;
        _indicatePlayerTurn(_currentPlayer);
        _state = PINGPONG_STATE_GAME;
        break;
      }

      _currentPlayer = _startingPlayer;
      _indicatePlayerTurn(_currentPlayer);
      _state = PINGPONG_STATE_GAME;
      break;

    case PINGPONG_STATE_GAME:
       _addPoint(player);
      break;

    case PINGPONG_STATE_GAME_END:
      _newGame();
      break;
  }
}

static void _playerButtonLongPress(uint8_t player) {
  if (_playerButtons[0]->held && _playerButtons[1]->held) {
    // Undo point removal of player who first reached long press
    // Gosh this whole thing could really do with unit tests
    if (_state == PINGPONG_STATE_GAME) {
      _addPoint(OTHER_PLAYER(player));
    }

    _swapSides();
    return;
  }

  switch (_state) {
    case PINGPONG_STATE_IDLE:
      // Nothing to do in this case?
      break;

    case PINGPONG_STATE_GAME:
    case PINGPONG_STATE_GAME_END: // Fallthrough intentional
      _removePoint(player);
      break;
  }
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
    case PINGPONG_DISPMODE_SET:
      p1Score = _setScores[0];
      p2Score = _setScores[1];
      break;

    case PINGPONG_DISPMODE_ALL:
      p1Score = _allTimeScores[0];
      p2Score = _allTimeScores[1];
      break;

    case PINGPONG_DISPMODE_GAME:
    default:
      p1Score = _gameScores[0];
      p2Score = _gameScores[1];
      break;
  }

  _writeScore(PINGPONG_PLAYER_1, p1Score);
  _writeScore(PINGPONG_PLAYER_2, p2Score);
}

static uint8_t _getCurrentPlayer() {
  uint8_t combinedScore = _gameScores[0] + _gameScores[1];
  uint8_t swpPoints = PINGPONG_POINTS_TO_CHANGE_SERVE;

  return combinedScore % (swpPoints * 2) < swpPoints 
    ? _startingPlayer 
    : OTHER_PLAYER(_startingPlayer);
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
  _refreshDisplay();

  if (_isGameOver()) {
    _endOfGame();
  } else {
    _currentPlayer = _getCurrentPlayer();
    _indicatePlayerTurn(_currentPlayer);
  }
}

static void _removePoint(uint8_t player) {
  if (_state == PINGPONG_STATE_IDLE) return;
  if (_gameScores[player - 1] == 0) return;

  bool wasGameOver = _state == PINGPONG_STATE_GAME_END;
  uint8_t prevWinner = PINGPONG_PLAYER_NONE;

  if (wasGameOver) prevWinner = _getWinningPlayer();

  _gameScores[player - 1]--;

  if (_isGameOver() && wasGameOver) {
    // This doesn't change anything, so don't bother.
    _gameScores[player - 1]++;
    return;
  }

  _setMode(PINGPONG_DISPMODE_GAME);
  _currentPlayer = _getCurrentPlayer();
  _indicatePlayerTurn(_currentPlayer);

  if (_state == PINGPONG_STATE_GAME_END && !_isGameOver()) {
    // If game over is undone, revert set / all time score changes
    _setScores[prevWinner - 1]--;
    _allTimeScores[prevWinner - 1]--;
    _state = PINGPONG_STATE_GAME;
  }
}

static void _toggleMode() {
  uint8_t newDispMode = _dispMode + 1;
  if (newDispMode > PINGPONG_DISPMODE_ALL) newDispMode = PINGPONG_DISPMODE_GAME;
  _setMode(newDispMode);
}

static void _setMode(uint8_t newMode) {
  if (_dispMode == newMode) {
    _refreshDisplay();
    return;
  }

  _dispMode = newMode;

  uint8_t leds;

  switch (_dispMode) {
    case PINGPONG_DISPMODE_NONE: leds = 0x00; break;
    case PINGPONG_DISPMODE_SET:  leds = (1 << LED_DISPMODE_SET); break;
    case PINGPONG_DISPMODE_ALL:  leds = (1 << LED_DISPMODE_ALL); break;
    case PINGPONG_DISPMODE_GAME: // Fallthrough intentional
    default:                    leds = (1 << LED_DISPMODE_GAME); break;
  }

  displaySetRow(LED_ROW_DISPMODE, leds);
  _refreshDisplay();
  _indicateIfScoresSaved();
}

static void _resetScore() {
  switch (_dispMode) {
    case PINGPONG_DISPMODE_NONE:
      return;

    case PINGPONG_DISPMODE_GAME:
      switch (_state) {
        case PINGPONG_STATE_IDLE:
          return;

        case PINGPONG_STATE_GAME:
          // Reset score and current player
          _gameScores[0] = _gameScores[1] = 0;
          _currentPlayer = _startingPlayer;
          _indicatePlayerTurn(_currentPlayer);
          _refreshDisplay();
          return;

        case PINGPONG_STATE_GAME_END:
          _newGame();
          return;
      }

      return;

    case PINGPONG_DISPMODE_SET:
      _setScores[0] = _setScores[1] = 0;
      _refreshDisplay();
      return;

    case PINGPONG_DISPMODE_ALL:
      _allTimeScores[0] = _allTimeScores[1] = 0;
      _setScores[0] = _setScores[1] = 0;
      _scoresLastSaved = _ticks;
      _refreshDisplay();
      return;
  }
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
  animationTrigger(winner == PINGPONG_PLAYER_1 ? Player1Win : Player2Win);
}

static void _newGame() {
  _gameScores[0] = _gameScores[1] = 0;
  _setMode(PINGPONG_DISPMODE_GAME);

  if (_startingPlayer == PINGPONG_PLAYER_NONE) {
    _state = PINGPONG_STATE_IDLE;
    _currentPlayer = PINGPONG_PLAYER_NONE;
    _indicatePlayerTurn(_currentPlayer);
    return;
  }

  _startingPlayer = OTHER_PLAYER(_startingPlayer);
  _currentPlayer = _startingPlayer;
  _indicatePlayerTurn(_currentPlayer);
  _state = PINGPONG_STATE_GAME;
}

static void _saveScores() {
  if (_ticks - _scoresLastSaved < SAVE_DELAY_TICKS) return;

  if (_cachedAllTimeScores[0] != _allTimeScores[0]) {
    eeprom_write_word((uint16_t*)eepromAddrPlayer1, _allTimeScores[0]);
    _cachedAllTimeScores[0] = _allTimeScores[0];
  }

  if (_cachedAllTimeScores[1] != _allTimeScores[1]) {
    eeprom_write_word((uint16_t*)eepromAddrPlayer2, _allTimeScores[1]);
    _cachedAllTimeScores[1] = _allTimeScores[1];
  }

  _scoresLastSaved = _ticks;
  _indicateIfScoresSaved();
}

static void _indicateIfScoresSaved() {
  if (_dispMode != PINGPONG_DISPMODE_ALL) return;

  if (_cachedAllTimeScores[0] != _allTimeScores[0] ||
      _cachedAllTimeScores[1] != _allTimeScores[1]) {
    return;
  }

  displaySetLED(0, 7, true);
}

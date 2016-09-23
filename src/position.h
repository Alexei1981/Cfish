/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2016 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef POSITION_H
#define POSITION_H

#include <assert.h>
#ifndef __WIN32__
#include <pthread.h>
#endif
#include <stdatomic.h>
#include <stddef.h>  // For offsetof()
#include <string.h>

#include "bitboard.h"
#include "types.h"

struct Zob {
  Key psq[16][64];
  Key enpassant[8];
  Key castling[16];
  Key side;
};

extern struct Zob zob;

void psqt_init(void);
void zob_init(void);

// Stack struct stores information needed to restore a Pos struct to
// its previous state when we retract a move.

struct Stack {
  // Copied when making a move
  Key pawnKey;
  Key materialKey;
  union {
    struct {
      Score psq;
      union {
        uint16_t nonPawnMaterial[2];
        uint32_t nonPawn;
      };
    };
    uint64_t psqnpm;
  };
  uint8_t castlingRights;
  uint8_t pliesFromNull;
  uint8_t rule50;

  // Not copied when making a move
  uint8_t capturedPiece;
  uint8_t epSquare;
  Key key;
  Bitboard checkersBB;
  struct Stack *previous;

  // Original search stack data
  Move* pv;
  int ply;
  Move currentMove;
  Move excludedMove;
  Move killers[2];
  Value staticEval;
  int skipEarlyPruning;
  int moveCount;
  CounterMoveStats *counterMoves;

  // MovePicker data
  Move countermove;
  Depth depth;
  Move ttMove;
  uint8_t recaptureSquare;
  Value threshold;
  int stage;
  ExtMove *cur, *endMoves, *endBadCaptures;

  // CheckInfo data
  union {
    struct {
      Bitboard blockersForKing[2];
      Bitboard pinnersForKing[2];
    };
    struct {
      Bitboard dummy[3]; // blockersForKing[2], pinnersForKing[WHITE]
      Bitboard checkSquares[7]; // element 0 is pinnersForKing[BLACK]
    };
  };
  Square ksq;
};

typedef struct Stack Stack;

#define StateCopySize offsetof(Stack, capturedPiece)
#define StateSize offsetof(Stack, pv)
#define SStackBegin(st) (&st.pv)
#define SStackSize (offsetof(Stack, countermove) - offsetof(Stack, pv))


// Pos struct stores information regarding the board representation as
// pieces, side to move, hash keys, castling info, etc. The search uses
// the functions do_move() and undo_move() on a Pos struct to traverse
// the search tree.

struct Pos {
  // Board / game representation.
  uint8_t board[64];
  Bitboard byTypeBB[7]; // no reason to allocate 8 here
  Bitboard byColorBB[2];
#ifdef PEDANTIC
  uint8_t pieceCount[16];
  uint8_t pieceList[256];
  uint8_t index[64];
  uint8_t castlingRightsMask[64];
  uint8_t castlingRookSquare[16];
  Bitboard castlingPath[16];
#endif
  uint8_t sideToMove;
  uint8_t chess960;
  uint16_t gamePly;

  Stack *st;
  ExtMove *moveList;

  // Relevant mainly to the search of the root position.
  RootMoves *rootMoves;
  Stack *stack;
  uint64_t nodes;
  uint64_t tb_hits;
  int PVIdx;
  int maxPly;
  Depth rootDepth;
  Depth completedDepth;

  // Pointers to thread-specific tables.
  HistoryStats *history;
  MoveStats *counterMoves;
  FromToStats *fromTo;
  PawnEntry *pawnTable;
  MaterialEntry *materialTable;
  CounterMoveHistoryStats *counterMoveHistory;

  // Thread-control data.
  atomic_bool resetCalls;
  int callsCnt;
  int exit, searching;
  int thread_idx;
#ifndef __WIN32__
  pthread_t nativeThread;
  pthread_mutex_t mutex;
  pthread_cond_t sleepCondition;
#else
  HANDLE nativeThread;
  HANDLE startEvent, stopEvent;
#endif
};

// FEN string input/output
void pos_set(Pos *pos, char *fen, int isChess960);
void pos_fen(Pos *pos, char *fen);
void print_pos(Pos *pos);

FAST PURE Bitboard pos_attackers_to_occ(Pos *pos, Square s, Bitboard occupied);
FAST PURE Bitboard slider_blockers(Pos *pos, Bitboard sliders, Square s,
                              Bitboard *pinners);
//Bitboard slider_blockers(Pos *pos, Bitboard sliders, Square s);

FAST PURE int is_legal(Pos *pos, Move m);
FAST PURE int is_pseudo_legal(Pos *pos, Move m);
FAST PURE int gives_check_special(Pos *pos, Stack *st, Move m);

// Doing and undoing moves
FAST void do_move(Pos *pos, Move m, int givesCheck);
FAST void undo_move(Pos *pos, Move m);
FAST void do_null_move(Pos *pos);
FAST void undo_null_move(Pos *pos);

// Static exchange evaluation
FAST PURE Value see(Pos *pos, Move m);
FAST PURE Value see_sign(Pos *pos, Move m);
FAST PURE Value see_test(Pos *pos, Move m, int value);

FAST PURE Key key_after(Pos *pos, Move m);
FAST PURE int game_phase(Pos *pos);
FAST PURE int is_draw(Pos *pos);

// Position representation
#define pieces() (pos->byTypeBB[0])
#define pieces_p(p) (pos->byTypeBB[p])
#define pieces_pp(p1,p2) (pos->byTypeBB[p1] | pos->byTypeBB[p2])
#define pieces_c(c) (pos->byColorBB[c])
#define pieces_cp(c,p) (pieces_p(p) & pieces_c(c))
#define pieces_cpp(c,p1,p2) (pieces_pp(p1,p2) & pieces_c(c))
#define piece_on(s) (pos->board[s])
#define ep_square() (pos->st->epSquare)
#define is_empty(s) (!piece_on(s))
#ifdef PEDANTIC
#define piece_count(c,p) (pos->pieceCount[8 * (c) + (p)] - (8*(c)+(p)) * 16)
#define piece_list(c,p) (&pos->pieceList[16 * (8 * (c) + (p))])
#define square_of(c,p) (pos->pieceList[16 * (8 * (c) + (p))])
#define loop_through_pieces(c,p,s) \
  uint8_t *pl = piece_list(c,p); \
  while ((s = *pl++) != SQ_NONE)
#else
#define piece_count(c,p) (popcount(pieces_cp(c, p)))
#define square_of(c,p) (lsb(pieces_cp(c,p)))
#define loop_through_pieces(c,p,s) \
  Bitboard pcs = pieces_cp(c,p); \
  while (pcs && (s = pop_lsb(&pcs), 1))
#endif
#define piece_count_mk(c, p) (((pos_material_key()) >> (20 * (c) + 4 * (p) + 4)) & 15)

// Castling
#define can_castle_cr(cr) (pos->st->castlingRights & (cr))
#define can_castle_c(c) can_castle_cr((WHITE_OO | WHITE_OOO) << (2 * (c)))
#ifdef PEDANTIC
#define castling_impeded(cr) (pieces() & pos->castlingPath[cr])
#define castling_rook_square(cr) (pos->castlingRookSquare[cr])
#else
#define castling_impeded(cr) (pieces() & CastlingPath[cr])
#define castling_rook_square(cr) (CastlingRookSquare[cr])
#endif

// Checking
#define pos_checkers() (pos->st->checkersBB)

// Attacks to/from a given square
#define attackers_to_occ(s,occ) pos_attackers_to_occ(pos,s,occ)
#define attackers_to(s) attackers_to_occ(s,pieces())
#define attacks_from_pawn(s,c) (StepAttacksBB[make_piece(c,PAWN)][s])
#define attacks_from_knight(s) (StepAttacksBB[KNIGHT][s])
#define attacks_from_bishop(s) attacks_bb_bishop(s, pieces())
#define attacks_from_rook(s) attacks_bb_rook(s, pieces())
#define attacks_from_queen(s) (attacks_from_bishop(s)|attacks_from_rook(s))
#define attacks_from_king(s) (StepAttacksBB[KING][s])
#define attacks_from(pc,s) attacks_bb(pc,s,pieces())

// Properties of moves
#define moved_piece(m) (piece_on(from_sq(m)))
#define captured_piece() (pos->st->capturedPiece)

// Accessing hash keys
#define pos_key() (pos->st->key)
#define pos_material_key() (pos->st->materialKey)
#define pos_pawn_key() (pos->st->pawnKey)

// Other properties of the position
#define pos_stm() (pos->sideToMove)
#define pos_game_ply() (pos->gamePly)
#define is_chess960() (pos->chess960)
#define pos_nodes_searched() (pos->nodes)
#define pos_rule50_count() (pos->st->rule50)
#define pos_psq_score() (pos->st->psq)
#define pos_non_pawn_material(c) (pos->st->nonPawnMaterial[c])

INLINE Bitboard discovered_check_candidates(Pos *pos)
{
  return pos->st->blockersForKing[pos_stm() ^ 1] & pieces_c(pos_stm());
}

INLINE Bitboard blockers_for_king(Pos *pos, int c)
{
  return pos->st->blockersForKing[c];
}

INLINE Bitboard pinned_pieces(Pos *pos, int c)
{
  return pos->st->blockersForKing[c] & pieces_c(c);
}

INLINE int pawn_passed(Pos *pos, int c, Square s)
{
  return !(pieces_cp(c ^ 1, PAWN) & passed_pawn_mask(c, s));
}

INLINE int advanced_pawn_push(Pos *pos, Move m)
{
  return   type_of_p(moved_piece(m)) == PAWN
        && relative_rank_s(pos_stm(), from_sq(m)) > RANK_4;
}

INLINE int opposite_bishops(Pos *pos)
{
#if 1
  return   piece_count(WHITE, BISHOP) == 1
        && piece_count(BLACK, BISHOP) == 1
        && opposite_colors(square_of(WHITE, BISHOP), square_of(BLACK, BISHOP));
#else
  return   (pos_material_key() & 0xf0000f0000) == 0x1000010000
        && (pieces_p(BISHOP) & DarkSquares)
        && (pieces_p(BISHOP) & DarkSquares) != pieces_p(BISHOP);
#endif
}

INLINE int is_capture_or_promotion(const Pos *pos, Move m)
{
  assert(move_is_ok(m));
  return type_of_m(m) != NORMAL ? type_of_m(m) != CASTLING : !is_empty(to_sq(m));
}

INLINE int is_capture(Pos *pos, Move m)
{
  // Castling is encoded as "king captures the rook"
  assert(move_is_ok(m));
  return (!is_empty(to_sq(m)) && type_of_m(m) != CASTLING) || type_of_m(m) == ENPASSANT;
}

INLINE int gives_check(Pos *pos, Stack *st, Move m)
{
  return  type_of_m(m) == NORMAL && !discovered_check_candidates(pos)
        ? !!(st->checkSquares[type_of_p(moved_piece(m))] & sq_bb(to_sq(m)))
        : gives_check_special(pos, st, m);
}

void pos_copy(Pos *dest, Pos *src);

#endif


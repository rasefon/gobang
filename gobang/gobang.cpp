// gobang.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <map>
#include <set>
#include <stack>
#include <string>
#include <vector>
#include <time.h>
#include <ppl.h>
#include <concurrent_vector.h>
#include "Board2.h"

using namespace std;
using namespace concurrency;

//#define DYM_EVAL
#define PATTERN_NUM 19

class Board;

struct Step
{
   Step(){}
   Step(int ii, int jj):i(ii), j(jj) {}
   Step(const Step& step):i(step.i), j(step.j) {}
   int i;
   int j;
};

struct TwoSteps
{
   Step step1;
   Step step2;

   TwoSteps(){}

   TwoSteps(int s1_i, int s1_j, int s2_i, int s2_j)
   {
      step1.i = s1_i;
      step1.j = s1_j;
      step2.i = s2_i;
      step2.j = s2_j;
   }
};

struct BoardContext
{
   Board *board;
   TwoSteps *steps;
   int depth;
   int alpha;
   int beta;
   char grid;
   bool is_black_turn;

   BoardContext(Board *bd, TwoSteps *s, int d, int a, int b, char g, bool black):
      board(bd), steps(s), depth(d), alpha(a), beta(b), grid(g), is_black_turn(black)
   {}
};

struct StepsWithValue
{
   TwoSteps *steps;
   int value;
   StepsWithValue(TwoSteps *ts, int v): steps(ts), value(v) {}
};

class Board
{
private:
   char m_bb[15][15];
   // available for certain grid?
   map<int, bool> m_grid_available;

   int m_left;
   int m_right;
   int m_top;
   int m_buttom;

   // current round, black 'x',  white 'o'.
   char m_pre_round;

   //each side can play 2 steps, this index is to show currently in which step. 1 or 2
   //int m_step_index;
   //Step m_pre_step;

   TwoSteps m_next_best_steps;

   stack<Board*> m_backup_board;

   char m_pattern_buffer[16];
public:
   bool m_black_win, m_white_win;

#ifdef DYM_EVAL
   //cache previous step score.
   int m_pre_horizontal_score[15];
   int m_pre_vertical_score[15];
   int m_pre_diagonal_slash[21];
   int m_pre_diagonal_backslash[21];
#endif

   // steps, computed from STEP_INDEX macro.  
   set<int> m_step_list;
public:
   Board();
   Board(const Board& other);
   Board& operator= (const Board& other);
   ~Board();
   // '-' empty, 'o' white, 'x' black
   void update_grid_status(int i, int j, char grid);
   void get_next_steps(set<int>& steps, char grid);

   // positive score for black and negative score for white.
   int eval_board(bool is_black_turn);
   int eval_horizontal(int row, bool is_black_turn);
   int eval_vertical(int col, bool is_black_turn);
   int eval_diagonal_slash(int i, int j, bool is_black_turn);
   int eval_diagonal_backslash(int i, int j, bool is_black_turn);
   void compute_horizonal_target(int row);
   void compute_vertical_target(int col);
   int compute_diagonal_slash_target(int i, int j);
   int compute_diagonal_backslash_target(int i, int j);
   bool is_game_over();
   bool game_over_calculator_helper(int len);

   bool is_avaliable_grid(int i, int j);
   bool is_valid_next_step(int i, int j, char grid);
   bool is_sibling(int i, int j, int ii, int jj);
   int parallel_alpha_beta_max(int depth, int alpha, int beta, bool is_black_turn);
   int alpha_beta_max(int depth, int alpha, int beta, bool is_black_turn);
   int alpha_beta_min(int depth, int alpha, int beta, bool is_black_turn);

   void pre_compute_steps(char grid, vector<TwoSteps*>& ssVector);

   void print_board();

   void self_gamming();
   void game();

   TwoSteps& best_steps() { return m_next_best_steps; }
private:
   void get_steps_from_center_step(int i, int j, char grid, vector<int>& steps);
   int eval_partial_board(int target_len, bool is_black_turn);
   void copy_from_other(const Board& other);
};

//////////////global variables/////////////////////////
///////////////////////////////////////////////////////

//x: black, o:white
static char* s_black_patterns[] = {
   // win
   "xxxxx", 
   // win in next step
   "-xxxx", "x-xxx", "xx-xx", "xxx-x", "xxxx-", "-xxx-", "x--xx", "-x-xx", "-xx-x", "x-xx-", "xx-x-", "x-x-x",
   "-xx-", "x---x", "-x-x-", "x--x-", "-x--x", 
   "-x-"
};

static char* s_white_patterns[] = {
   // win
   "ooooo", 
   // win in next step
   "-oooo", "o-ooo", "oo-oo", "ooo-o", "oooo-", "-ooo-", "o--oo", "-o-oo", "-oo-o", "o-oo-", "oo-o-", "o-o-o",
   "-oo-", "o---o", "-o-o-", "o--o-", "-o--o",
   "-o-"
};

static int s_positive_score[] = {
   WIN_SCORE,
   2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 
   700, 700, 700, 700, 700, 
   100 
};

// kmp string compare.
static int *s_failure_function[PATTERN_NUM];
static int s_pattern_len[PATTERN_NUM];

static void compute_failure_function()
{
   //first allocate failure function array.
   for (int i = 0; i < PATTERN_NUM; i++) {
      int len = strlen(s_black_patterns[i]);
      s_pattern_len[i] = len;
      s_failure_function[i] = new int[len];
      s_failure_function[i][0] = 0;
      int k = 0;

      for (int q = 1; q < len; q++) {
         while (k>0 && s_black_patterns[i][k]!=s_black_patterns[i][q]) {
            if (s_failure_function[i][k] == 0) 
               k = 0;
            else 
               k = s_failure_function[i][k]-1;
         }

         if (s_black_patterns[i][k] == s_black_patterns[i][q]) 
            k += 1;

         s_failure_function[i][q] = k;
      }
   }
}

// return number of pattern matched.
static int kmp_matcher(char *target, int target_len, char *pattern, int pattern_index, int pattern_len)
{
   if (target_len < pattern_len) 
      return 0;

   int pattern_matched = 0;
   int q = 0; // pattern index;
   int p = 0; // target index;
   while (p < target_len) {
      while (target[p] != pattern[q] && p < target_len)
         p++;

      if (p == target_len)
         return pattern_matched;

      while (target[p] == pattern[q] && q != pattern_len-1) {
         p++;
         q++;
      }

      if (target[p] == pattern[q] && q == pattern_len-1) {
         pattern_matched++;
      }

      q = s_failure_function[pattern_index][q-1];
   }

   return pattern_matched;
}

static int ranged_rand(int range_min, int range_max)
{
   // Generate random numbers in the half-closed interval
   // [range_min, range_max). In other words,
   // range_min <= random number < range_max
   int u = (double)rand() / (RAND_MAX + 1) * (range_max - range_min) + range_min;
   return u;
}


Board::~Board()
{
   m_grid_available.clear();
}

Board::Board(): m_black_win(false), m_white_win(false)
{ 
   m_left = m_top = 16;
   m_right = m_buttom = 0;
   m_pre_round = '-';
   //m_step_index = 0;

   for (int i = 0; i < 15; i++)  {
      for (int j = 0; j < 15; j++) {
         m_bb[i][j] = '-';
         int grid_index_key = STEP_INDEX(i,j);
         m_grid_available[grid_index_key] = true;
      }
   }

#ifdef DYM_EVAL
   //initialize previous score as zero
   for (int i = 0; i < 15; i++) {
      m_pre_horizontal_score[i] = m_pre_vertical_score[i] = 0;
   }

   for (int i = 0; i < 21; i++) {
      m_pre_diagonal_slash[i] = m_pre_diagonal_backslash[i] = 0;
   }
#endif
}

void Board::copy_from_other(const Board& other)
{
   if (this != &other) {
      for (int i = 0; i < 15; i++) {
         for (int j = 0; j < 15; j++) {
            m_bb[i][j] = other.m_bb[i][j];
         }
      }

      m_grid_available.clear();
      map<int,bool>::const_iterator it = other.m_grid_available.begin();
      for (;it != other.m_grid_available.end(); it++) 
         m_grid_available[it->first] = it->second;

      m_left = other.m_left;
      m_right = other.m_right;
      m_top = other.m_top;
      m_buttom = other.m_buttom;
      
      m_pre_round = other.m_pre_round;

      //m_step_index = other.m_step_index;
      //m_pre_step = other.m_pre_step;

      m_black_win = other.m_black_win;
      m_white_win = other.m_white_win;

#ifdef DYM_EVAL
      for (int i = 0; i < 15; i++) {
         m_pre_horizontal_score[i] = other.m_pre_horizontal_score[i];
         m_pre_vertical_score[i] = other.m_pre_vertical_score[i];
      }

      for (int i = 0; i < 21; i++) {
         m_pre_diagonal_slash[i] = other.m_pre_diagonal_slash[i];
         m_pre_diagonal_backslash[i] = other.m_pre_diagonal_backslash[i];
      }
#endif

      m_step_list.clear();
      set<int>::const_iterator it2 = other.m_step_list.begin();
      for (;it2 != other.m_step_list.end(); it2++)
         m_step_list.insert(*it2);
   } 
}

Board::Board(const Board& other)
{
   copy_from_other(other);
}

Board& Board::operator= (const Board& other)
{
   copy_from_other(other);
   return *this;
}

void Board::update_grid_status(int i, int j, char grid)
{
   m_bb[i][j] = grid;

   //m_pre_step.i = i, m_pre_step.j = j;

   //if (grid != m_pre_round) 
   //   m_step_index = 1;
   //else
   //   m_step_index++;

   m_pre_round = grid;

   if ('-' != grid) {
      int index = STEP_INDEX(i,j);
      m_grid_available[index] = false;
      m_step_list.insert(index);

      if (m_left > i) 
         m_left = i;

      if (m_right < i) 
         m_right = i;

      if (m_top > j) 
         m_top = j;

      if (m_buttom < j) 
         m_buttom = j;
   }

#ifdef DYM_EVAL
   m_pre_horizontal_score[i] = eval_horizontal(i);
   m_pre_vertical_score[j] = eval_vertical(j);

   int slash_index = i+j-4;
   if (slash_index>=0 && slash_index <=20) 
      m_pre_diagonal_slash[i+j-4] = eval_diagonal_slash(i, j);

   int backslash_index = i-j+10;
   if (backslash_index>=0 && backslash_index <=20) 
      m_pre_diagonal_backslash[backslash_index] = eval_diagonal_backslash(i, j);
#endif
}

void Board::get_next_steps(set<int>& steps, char grid)
{
   vector<int> tmp_steps;
   set<int>::const_iterator it = m_step_list.begin();
   for (; it != m_step_list.end(); it++)
   {
      int step = *it;
      int i = I_FROM_INDEX(step);
      int j = J_FROM_INDEX(step);
      tmp_steps.clear();
      get_steps_from_center_step(i, j, grid, tmp_steps);
      vector<int>::const_iterator it2 = tmp_steps.begin();
      for (; it2 != tmp_steps.end(); it2++) {
         int potential_step = *it2;
         if (m_step_list.find(potential_step) == m_step_list.end()) {
            steps.insert(potential_step);
         }
      }
   }

   //Add one random step
}

void Board::get_steps_from_center_step(int i, int j, char grid, vector<int>& steps)
{
   int left = i-POTENTIAL_GEP;
   if (left < 0)
      left = 0;

   int right = i+POTENTIAL_GEP;
   if (right >14)
      right = 14;

   int top = j-POTENTIAL_GEP;
   if (top < 0)
      top = 0;

   int buttom = j+POTENTIAL_GEP;
   if (buttom > 14)
      buttom = 14;

   for (int x = left; x <= right; x++) {
      for (int y = top; y <= buttom; y++) {
         int index = STEP_INDEX(x,y);
         if (m_grid_available[index] /*&& is_valid_next_step(x, y, grid)*/) {
            steps.push_back(index);
         }
      }
   }
}

//bool Board::is_valid_next_step(int i, int j, char grid)
//{
//   if (grid == m_pre_round && m_step_index == 1) 
//   {
//      if (i >= m_pre_step.i-1 && i <= m_pre_step.i+1 && j >= m_pre_step.j-1 && j <= m_pre_step.j+1)
//         return false;
//   }
//
//   return true;
//}

int Board::eval_board(bool is_black_turn)
{
   int score = 0;

#ifndef DYM_EVAL
   for (int count = 0; count < 15; count++) {
      score += eval_horizontal(count, is_black_turn);
      score += eval_vertical(count, is_black_turn);
      score += eval_diagonal_slash(0, count, is_black_turn);
      score += eval_diagonal_backslash(0, count, is_black_turn);
   }

   for (int count = 1; count < 15; count++) {
      score += eval_diagonal_slash(14, count, is_black_turn);
      score += eval_diagonal_backslash(14, count, is_black_turn);
   }
#endif

#ifdef DYM_EVAL
   for (int i = 0; i < 15; i++) 
   {
      score += m_pre_horizontal_score[i];
      score += m_pre_vertical_score[i];
   }

   for (int i = 0; i < 21; i++) 
   {
      score += m_pre_diagonal_slash[i];
      score += m_pre_diagonal_backslash[i];
   }
#endif

   return score;
}

int Board::eval_partial_board(int target_len, bool is_black_turn)
{
   int score = 0;

   for (int i = 0; i < PATTERN_NUM; i++)
   {
      int black_turn_matched = kmp_matcher(m_pattern_buffer, target_len, s_black_patterns[i], i, s_pattern_len[i]);
      int tmp_score = s_positive_score[i]*black_turn_matched;
      if (is_black_turn) {
         score += tmp_score;
      }
      else {
         score -= tmp_score;
      }

      int white_turn_matched = kmp_matcher(m_pattern_buffer, target_len, s_white_patterns[i], i, s_pattern_len[i]);
      tmp_score = s_positive_score[i]*white_turn_matched;
      if (is_black_turn) {
         score -= tmp_score;
      }
      else {
         score += tmp_score;
      }
   }

   return score;
}

int Board::eval_horizontal(int row, bool is_black_turn)
{
   int score = 0;
  
   compute_horizonal_target(row);
   score = eval_partial_board(15, is_black_turn);

   return score;
}

void Board::compute_horizonal_target(int row)
{
   for (int i = 0; i < 15; i++)
      m_pattern_buffer[i] = m_bb[row][i];
}

int Board::eval_vertical(int col, bool is_black_turn)
{
   int score = 0;

   compute_vertical_target(col);
   score = eval_partial_board(15, is_black_turn);

   return score;
}

void Board::compute_vertical_target(int col)
{
   for (int i = 0; i < 15; i++)
      m_pattern_buffer[i] = m_bb[i][col];
}

int Board::eval_diagonal_slash(int i, int j, bool is_black_turn)
{
   int score = 0;
   
   if ((i<4 && j<4) || (i>10) && (j>10)) 
      return score;

   int len = compute_diagonal_slash_target(i, j);
   score = eval_partial_board(len, is_black_turn);

   return score;
}

int Board::compute_diagonal_slash_target(int i, int j)
{
   int len = 0;
   if (i+j < 15) {
      len = i+j+1;
      int row = 0, col = i+j;
      for (; row < len; row++, col--)
         m_pattern_buffer[row] = m_bb[row][col];

      m_pattern_buffer[len] = 0;
   }
   else {
      int row = i+j-14, col = 14, count = 0;
      len = col-row+1;
      for (; count < len; row++, count++, col--) 
         m_pattern_buffer[count] = m_bb[row][col];

      m_pattern_buffer[len] = 0;
   }

   return len;
}

int Board::eval_diagonal_backslash(int i, int j, bool is_black_turn)
{
   int score = 0;

   if ((i<4 && j >10) || (i>10 && j<4)) 
      return score;

   int len = compute_diagonal_backslash_target(i, j);
   score = eval_partial_board(len, is_black_turn);

   return score;
}

int Board::compute_diagonal_backslash_target(int i, int j)
{
   int len = 0;
   if (i<j) {
      len = i-j+15;
      int row = 0, col = j-i;
      for (; row < len; row++, col++) 
         m_pattern_buffer[row] = m_bb[row][col];

      m_pattern_buffer[len] = 0;
   }
   else {
      len = j-i+15;
      int row = i-j, col = 0;
      for ( ;row < 15; row++, col++) 
         m_pattern_buffer[col] = m_bb[row][col];

      m_pattern_buffer[len] = 0;
   }

   return len;
}

bool Board::game_over_calculator_helper(int len)
{
   int matched = kmp_matcher(m_pattern_buffer, 15, s_black_patterns[0], 0, s_pattern_len[0]);
   if (matched >= 1) {
      m_black_win = true;
      return true;
   }

   matched = kmp_matcher(m_pattern_buffer, 15, s_white_patterns[0], 0, s_pattern_len[0]);
   if (matched >= 1) {
      m_white_win = true;
      return true;
   }

   return false;
}

bool Board::is_game_over()
{
   int matched = 0;
   int len;
   for (int count = 0; count < 15; count++) {
      compute_horizonal_target(count);
      if (game_over_calculator_helper(15)) {
         return true;
      }

      compute_vertical_target(count);
      if (game_over_calculator_helper(15)) {
         return true;
      } 

      len = compute_diagonal_slash_target(0, count);
      if (game_over_calculator_helper(len)) {
         return true;
      }

      len = compute_diagonal_backslash_target(0, count);
      if (game_over_calculator_helper(len)) {
         return true;
      } 
   }

   for (int count = 1; count < 15; count++) {
      len = compute_diagonal_slash_target(14, count);
      if (game_over_calculator_helper(len)) {
         return true;
      }

      len = compute_diagonal_backslash_target(14, count);
      if (game_over_calculator_helper(len)) {
         return true;
      }
   }

   return false;
}


int Board::parallel_alpha_beta_max(int depth, int alpha, int beta, bool is_black_turn)
{
   vector<TwoSteps*> two_sptes;
   char grid = is_black_turn?'x':'o';
   pre_compute_steps(grid, two_sptes);
   concurrent_vector<BoardContext*> bc_buff;
   for (size_t i = 0; i< two_sptes.size(); i++) {
      Board *board = new Board();
      *board = *this;
      BoardContext *bs = new BoardContext(board, two_sptes[i], depth, alpha, beta, grid, is_black_turn);
      bc_buff.push_back(bs);
   }

   concurrent_vector<StepsWithValue*> svs;
   parallel_for_each(begin(bc_buff), end(bc_buff), [&](BoardContext* bc) {
      TwoSteps *ss = bc->steps;
      Board *board = bc->board;
      int depth = bc->depth-1; 
      int alpha = bc->alpha;
      int beta = bc->beta;
      char grid = bc->grid;
      bool is_black_turn = bc->is_black_turn;
      board->update_grid_status(ss->step1.i, ss->step1.j, grid);
      board->update_grid_status(ss->step2.i, ss->step2.j, grid);
      int score = board->alpha_beta_min(depth-1, alpha, beta, is_black_turn);
      StepsWithValue *sv = new StepsWithValue(ss, score);
      svs.push_back(sv);
   });

   int best_score = -_INFINITE_;
   for (size_t i = 0; i < svs.size(); i++) {
      if (best_score < svs[i]->value) {
         best_score = svs[i]->value;
         m_next_best_steps = *(svs[i]->steps);
      }
   }

   return best_score;
}

int Board::alpha_beta_max(int depth, int alpha, int beta, bool is_black_turn)
{
   if (depth ==0 || is_game_over()) {
      return eval_board(is_black_turn);
   }

   vector<TwoSteps*> two_sptes;
   char grid = is_black_turn?'x':'o';
   pre_compute_steps(grid, two_sptes);
   for (size_t i = 0; i < two_sptes.size(); i++) {
      TwoSteps *ss = two_sptes[i];
      Board *tmp_board = new Board();
      *tmp_board = *this;
      m_backup_board.push(tmp_board);
      update_grid_status(ss->step1.i, ss->step1.j, grid);
      update_grid_status(ss->step2.i, ss->step2.j, grid);
      int score = alpha_beta_min(depth-1, alpha, beta, is_black_turn);
      *this = *m_backup_board.top();
      delete m_backup_board.top();
      m_backup_board.pop();

      if (score >= beta) {
         alpha = score;
         goto Exit;
      }

      if (score > alpha) {
         alpha = score;
         //if (depth == DEPTH) {
         //   m_next_best_steps = *ss;
         //}
      }
   }

   //release memory
Exit:
   vector<TwoSteps*>::iterator it = two_sptes.begin();
   for (; it != two_sptes.end(); it++) {
      delete *it;
   }
   two_sptes.clear();

   return alpha;
}

int Board::alpha_beta_min(int depth, int alpha, int beta, bool is_black_turn)
{
   if (depth ==0 || is_game_over()) {
      return eval_board(is_black_turn);
   }

   vector<TwoSteps*> two_sptes;
   // attention!!!
   char grid = is_black_turn?'o':'x';
   pre_compute_steps(grid, two_sptes);
   for (size_t i = 0; i < two_sptes.size(); i++) {
      TwoSteps *ss = two_sptes[i];
      Board *tmp_board = new Board();
      *tmp_board = *this;
      m_backup_board.push(tmp_board);;
      update_grid_status(ss->step1.i, ss->step1.j, grid);
      update_grid_status(ss->step2.i, ss->step2.j, grid);
      int score = alpha_beta_max(depth-1, alpha, beta, is_black_turn);
      *this = *m_backup_board.top();
      delete m_backup_board.top();
      m_backup_board.pop();;

      if (score <= alpha) {
         beta = score;
         goto Exit;
      }

      if (score < beta) {
         beta = score;
      }
   }

   //release memory
Exit:
   vector<TwoSteps*>::iterator it = two_sptes.begin();
   for (; it != two_sptes.end(); it++) {
      delete *it;
   }
   two_sptes.clear();

   return beta;
}

void Board::pre_compute_steps(char grid, vector<TwoSteps*>& ss_vector)
{
   if (m_step_list.empty()) {
      TwoSteps *ss = new TwoSteps(7,7,9,9);
      ss_vector.push_back(ss);
      return;
   }

   set<int> first_steps;
   get_next_steps(first_steps, grid);
   if (first_steps.find(707) == first_steps.end() && is_avaliable_grid(7,7)) {
      first_steps.insert(707);
   }
   if (first_steps.find(709) == first_steps.end() && is_avaliable_grid(7,9)) {
      first_steps.insert(709);
   }

   set<int> ts_buffer;
   set<int>::const_iterator fs_it = first_steps.begin();
   for (; fs_it != first_steps.end(); fs_it++) {
      int fs_index = *fs_it;
      int i = I_FROM_INDEX(fs_index);
      int j = J_FROM_INDEX(fs_index);
      set<int>::const_iterator ss_it = first_steps.begin();
      for (; ss_it != first_steps.end(); ss_it++) {
         int ss_index = *ss_it;
         if (ss_index != fs_index) {
            int ii = I_FROM_INDEX(ss_index);
            int jj = J_FROM_INDEX(ss_index);
            if (!is_sibling(i, j, ii, jj)) {
               int ts_index;
               if (fs_index < ss_index) {
                  ts_index = ss_index*10000+fs_index;
               }
               else {
                  ts_index = fs_index*10000+ss_index;
               }
               ts_buffer.insert(ts_index);
            }
         }
      }
   }

   for (set<int>::const_iterator ts_it = ts_buffer.begin(); ts_it != ts_buffer.end(); ts_it++) {
      int fs_index = (*ts_it)/10000;
      int ss_index = (*ts_it)%10000;
      int i = I_FROM_INDEX(fs_index);
      int j = J_FROM_INDEX(fs_index);
      int ii = I_FROM_INDEX(ss_index);
      int jj = J_FROM_INDEX(ss_index);
      TwoSteps *ts = new TwoSteps(i, j, ii, jj);
      ss_vector.push_back(ts);
   }
}

bool Board::is_sibling(int i, int j, int ii, int jj)
{
   // if two steps are same, return true;
   if (i == ii && j == jj) {
      return true;
   }

   if ( ii>=i-1 && ii <= i+1 && jj >= j-1 && jj <= j+1) {
      return true;
   }

   return false;
}

void Board::print_board()
{
   cout<<"   ";
   for (int i = 0; i < 15; i++) {
      if (i<10) {
         cout<<i<<"  ";
      }
      else {
         cout<<i<<" ";
      }
   }
   cout<<endl;

   for (int i = 0; i < 15; i++) {
      if (i < 10) 
         cout<<i<<"  ";
      else
         cout<<i<<" ";

      for (int j = 0; j < 15; j++) {
         cout<<m_bb[i][j]<<"  ";
      }
      cout<<endl;
   }
}

void Board::self_gamming()
{
   bool black_turn = true;
   char grid;
   while (!is_game_over()) {
      grid = black_turn?'x':'o';
      //nega_alpha_beta(DEPTH, -_INFINITE_, _INFINITE_, black_turn);
      TwoSteps& ts = best_steps();
      if(black_turn) {
         cout <<"black";
      }
      else {
         cout << "white";
      }
      cout << " next step: (" << ts.step1.i <<", " << ts.step1.j 
         << "), (" << ts.step2.i << ", " << ts.step2.j << ")" << endl;
      update_grid_status(ts.step1.i, ts.step1.j, grid);
      update_grid_status(ts.step2.i, ts.step2.j, grid);
      print_board();
      int score = eval_board(true);
      cout << "black score: " << score << endl;
      score = eval_board(false);
      cout << "white score: " << score << endl;
      cout << endl;
      black_turn = !black_turn;
   }

   if (m_black_win) {
      cout << "black wins" << endl;
   }
   else if (m_white_win) {
      cout << "white wins" << endl;
   }
   else {
      cout << "bug!" << endl;
   }
}

bool Board::is_avaliable_grid(int i, int j)
{
   int index_key = STEP_INDEX(i,j);
   return m_grid_available[index_key];
}

void Board::game()
{
   char buf[6];
   //b1.self_gamming();
   print_board();
   while (!is_game_over())
   {
      Step s1, s2;
      for (int i=0; i<2; i++) {
         cout << "enter 2 (i,j), for example 2,3" << endl;
         memset(buf, 0, 6);
         cin >> buf;
         if (i == 0) {
            s1.i = atoi(&buf[0]);

            int next = 1;
            while(buf[next++]!=',');

            s1.j = atoi(&buf[next]);

            while(!is_avaliable_grid(s1.i, s1.j)) {
               cout << "invalid input!" <<endl;
               memset(buf, 0, 6);
               cin >> buf;

               next = 1;
               while(buf[next++]!=',');

               s1.i = atoi(&buf[0]);
               s1.j = atoi(&buf[next]);
            }
         }
         else {
            int i = atoi(&buf[0]);

            int next = 1;
            while(buf[next++]!=',');

            int j = atoi(&buf[next]);

            while(is_sibling(s1.i, s1.j, i, j) || !is_avaliable_grid(i, j)) {
               cout << "invalid input!" <<endl;
               memset(buf, 0, 6);
               cin >> buf;

               next = 1;
               while(buf[next++]!=',');

               i = atoi(&buf[0]);
               j = atoi(&buf[next]);
            }
            s2.i = i;
            s2.j = j;
         }
      }
      update_grid_status(s1.i, s1.j, 'x');
      update_grid_status(s2.i, s2.j, 'x');
      if (is_game_over()) {
         print_board();
         break;
      }
      print_board();
      int score = eval_board(true);
      cout << "black score: " << score << endl;
      score = eval_board(false);
      cout << "white score: " << score << endl;
      cout << endl;

      //int potential_score = alpha_beta_max(DEPTH, -_INFINITE_, _INFINITE_, false);
      int potential_score = parallel_alpha_beta_max(DEPTH, -_INFINITE_, _INFINITE_, false);
      TwoSteps& ts = best_steps();
      update_grid_status(ts.step1.i, ts.step1.j, 'o');
      update_grid_status(ts.step2.i, ts.step2.j, 'o');
      print_board();
      cout << "white steps: (" << ts.step1.i <<","<<ts.step1.j<<") ("<<ts.step2.i<<","<<ts.step2.j<<")" <<endl;
      score = eval_board(true);
      cout << "white potential score: " << potential_score << endl;
      cout << "black score: " << score << endl;
      score = eval_board(false);
      cout << "white score: " << score << endl;
      cout << endl;
   }

   if (m_black_win) {
      cout << "black wins" << endl;
   }
   else if (m_white_win) {
      cout << "white wins" << endl;
   }
   else {
      cout << "bug!" << endl;
   }
}

// some unit test functions
void test_kmp_matcher();

int _tmain(int argc, _TCHAR* argv[])
{
   clock_t begin = clock();

   compute_failure_function();

   Board b1;
   b1.game();

   //testing
   //BetterBoard::Board2::test_board2();

   //BetterBoard::Board2 board;
   //board.set_computer_as_white();
   //board.game();
   
   clock_t end = clock();
   cout << end-begin << endl;
   return 0;
}

void test_kmp_matcher()
{
   //test kmp
   int pattern_index = 11; //0xxx--
   char *target = "--oxoxoxxx--ooxxx----";
   int matched = kmp_matcher(target, strlen(target), s_black_patterns[pattern_index], pattern_index, s_pattern_len[pattern_index]);

   int pattern_index2 = 25; //-x--x-
   char *target2 = "o-x-x--x--x-xx-";
   int matched2 = kmp_matcher(target2, strlen(target2), s_black_patterns[pattern_index2], pattern_index2, s_pattern_len[pattern_index2]);

   int pattern_index3 = 21; //-xxx-o
   char *target3 = "-xx-xxooxxoo--xxoo";
   int matched3 = kmp_matcher(target3, strlen(target3), s_black_patterns[pattern_index3], pattern_index3, s_pattern_len[pattern_index3]);

   cout << matched3 << endl;
}

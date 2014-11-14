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

using namespace std;

//#define DYM_EVAL

#define POTENTIAL_GEP 1
#define DEPTH 3

#define INDEX_KEY(i,j) (i*100+j)
#define I_FROM_INDEX(index) (index/100)
#define J_FROM_INDEX(index) (index%100)

#define _INFINITE_ 2147483646
#define _IMPOSSIBLE_ 2147483647
#define WIN_SCORE 500000

#define PATTERN_NUM 35

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
   int m_step_index;
   Step m_pre_step;

   TwoSteps m_next_best_steps;

public:
   bool m_black_win, m_white_win;

#ifdef DYM_EVAL
   //cache previous step score.
   int m_pre_horizontal_score[15];
   int m_pre_vertical_score[15];
   int m_pre_diagonal_slash[21];
   int m_pre_diagonal_backslash[21];
#endif

   // steps, computed from INDEX_KEY macro.  
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
   int alpha_beta(int depth, int alpha, int beta, bool bOrW);

   void pre_compute_steps(char grid, vector<TwoSteps*>& ssVector);

   void print_board();

   void self_gamming();

   TwoSteps& best_steps() { return m_next_best_steps; }
private:
   void get_steps_from_center_step(int i, int j, char grid, vector<int>& steps);
   int eval_partial_board(int target_len, bool is_black_turn);
   void copy_from_other(const Board& other);
};

//////////////global variables/////////////////////////
char g_pattern_buffer[16];
stack<Board> g_backup_board;
///////////////////////////////////////////////////////

//x: black, o:white
static char* s_black_patterns[] = {
   "xxxxx", //0
   "-xxxx-", //1
   "oxxxx-", "-xxxxo", "x-xxx", "xx-xx", "xxx-x", "xxxx-", "-xxxx",
   "--xxx-", "-xxx--", "-xx-x-", "-x-xx-", 
   "oxxx--", "oxx-x-", "ox-xx-", "--xxxo", "-x-xxo", "-xx-xo", "xx--x", "x--xx", "x-x-x", "o-xxx-", "-xxx-o", //11~21
   "--xx--", "--x-x-", "-x-x--", "-x--x-",
   "oxx---", "ox-x--", "ox--x-", "---xxo", "--x-xo", "-x--xo", "x---x"
};

static char* s_white_patterns[] = {
   "ooooo",
   "-oooo-", 
   "xoooo-", "-oooox", "o-ooo", "oo-oo", "ooo-o", "oooo-", "-oooo"
   "--ooo-", "-ooo--", "-oo-o-", "-o-oo-",
   "xooo--", "xoo-o-", "xo-oo-", "--ooox", "-o-oox", "-oo-ox", "oo--o", "o--oo", "o-o-o", "x-ooo-", "-ooo-x",
   "--oo--", "--o-o-", "-o-o--", "-o--o-",
   "xoo---", "xo-o--", "xo--o-", "---oox", "--o-ox", "-o--ox", "o---o"
};

static int s_positive_score[] = {
   WIN_SCORE,
   10000,
   5000, 5000, 5000, 5000, 5000, 5000, 5000,
   1000, 1000, 1000, 1000,
   500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
   50, 50, 50, 50,
   10, 10, 10, 10, 10, 10, 10
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

//static int ranged_rand(int range_min, int range_max)
//{
//   // Generate random numbers in the half-closed interval
//   // [range_min, range_max). In other words,
//   // range_min <= random number < range_max
//   int u = (double)rand() / (RAND_MAX + 1) * (range_max - range_min) + range_min;
//   return u;
//}


Board::~Board()
{
   m_grid_available.clear();
}

Board::Board(): m_black_win(false), m_white_win(false)
{ 
   m_left = m_top = 16;
   m_right = m_buttom = 0;
   m_pre_round = '-';
   m_step_index = 0;

   for (int i = 0; i < 15; i++)  {
      for (int j = 0; j < 15; j++) {
         m_bb[i][j] = '-';
         int grid_index_key = INDEX_KEY(i,j);
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

      m_step_index = other.m_step_index;
      m_pre_step = other.m_pre_step;

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

   m_pre_step.i = i, m_pre_step.j = j;

   if (grid != m_pre_round) 
      m_step_index = 1;
   else
      m_step_index++;

   m_pre_round = grid;

   if ('-' != grid) {
      int index = INDEX_KEY(i,j);
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
         int index = INDEX_KEY(x,y);
         if (m_grid_available[index] && is_valid_next_step(x, y, grid)) {
            steps.push_back(index);
         }
      }
   }
}

bool Board::is_valid_next_step(int i, int j, char grid)
{
   if (grid == m_pre_round && m_step_index == 1) 
   {
      if (i >= m_pre_step.i-1 && i <= m_pre_step.i+1 && j >= m_pre_step.j-1 && j <= m_pre_step.j+1)
         return false;
   }

   return true;
}

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
      int black_turn_matched = kmp_matcher(g_pattern_buffer, target_len, s_black_patterns[i], i, s_pattern_len[i]);
      int tmp_score = s_positive_score[i]*black_turn_matched;
      if (is_black_turn) {
         score += tmp_score;
      }
      else {
         score -= tmp_score;
      }

      int white_turn_matched = kmp_matcher(g_pattern_buffer, target_len, s_white_patterns[i], i, s_pattern_len[i]);
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
      g_pattern_buffer[i] = m_bb[row][i];
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
      g_pattern_buffer[i] = m_bb[i][col];
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
         g_pattern_buffer[row] = m_bb[row][col];

      g_pattern_buffer[len] = 0;
   }
   else {
      int row = i+j-14, col = 14, count = 0;
      len = col-row+1;
      for (; count < len; row++, count++, col--) 
         g_pattern_buffer[count] = m_bb[row][col];

      g_pattern_buffer[len] = 0;
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
         g_pattern_buffer[row] = m_bb[row][col];

      g_pattern_buffer[len] = 0;
   }
   else {
      len = j-i+15;
      int row = i-j, col = 0;
      for ( ;row < 15; row++, col++) 
         g_pattern_buffer[col] = m_bb[row][col];

      g_pattern_buffer[len] = 0;
   }

   return len;
}

bool Board::game_over_calculator_helper(int len)
{
   int matched = kmp_matcher(g_pattern_buffer, 15, s_black_patterns[0], 0, s_pattern_len[0]);
   if (matched >= 1) {
      m_black_win = true;
      return true;
   }

   matched = kmp_matcher(g_pattern_buffer, 15, s_white_patterns[0], 0, s_pattern_len[0]);
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

int Board::alpha_beta(int depth, int alpha, int beta, bool is_black_turn) {
   if (depth == 0  || is_game_over()) {
      return eval_board(is_black_turn);
   }

   vector<TwoSteps*> two_sptes;
   char grid = is_black_turn?'x':'o';
   pre_compute_steps(grid, two_sptes);
   for (size_t i = 0; i < two_sptes.size(); i++) {
      TwoSteps *ss = two_sptes[i];
      g_backup_board.push(*this);
      update_grid_status(ss->step1.i, ss->step1.j, grid);
      update_grid_status(ss->step2.i, ss->step2.j, grid);
      int val = -alpha_beta(depth-1, -beta, -alpha, !is_black_turn);
      *this = g_backup_board.top();
      g_backup_board.pop();

      if (val >= beta) {
         return val;
      }

      if (val >= alpha) {
         alpha = val;
         m_next_best_steps = *ss;
      }
   }

   return alpha;
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
               TwoSteps *ts = new TwoSteps(i, j, ii, jj);
               ss_vector.push_back(ts);
            }
         }
      }
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
      cout<<i<<"  ";
   }
   cout<<endl;

   for (int i = 0; i < 15; i++) {
      if (i < 10) 
         cout<<i<<"  ";
      else
         cout<<i<<" ";

      for (int j = 0; j < 15; j++) {
         if (j < 10) 
            cout<<m_bb[i][j]<<"  ";
         else
            cout<<m_bb[i][j]<<"   ";
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
      alpha_beta(DEPTH, -_INFINITE_, _INFINITE_, black_turn);
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
   int index_key = INDEX_KEY(i,j);
   return m_grid_available[index_key];
}

// some unit test functions
void test_kmp_matcher();

int _tmain(int argc, _TCHAR* argv[])
{
   clock_t begin = clock();

   compute_failure_function();
   Board b1;
   char buf[4];
   //b1.self_gamming();
   b1.print_board();
   while (!b1.is_game_over())
   {
      Step s1, s2;
      for (int i=0; i<2; i++) {
         cout << "enter 2 (i,j), for example 2,3" << endl;
         memset(buf, 0, 4);
         cin >> buf;
         if (i == 0) {
            s1.i = atoi(&buf[0]);
            s1.j = atoi(&buf[2]);
         }
         else {
            int i = atoi(&buf[0]);
            int j = atoi(&buf[2]);
            while(b1.is_sibling(s1.i, s1.j, i, j) || !b1.is_avaliable_grid(i, j)) {
               cout << "invalid input!" <<endl;
               memset(buf, 0, 4);
               cin >> buf;
               i = atoi(&buf[0]);
               j = atoi(&buf[2]);
            }
            s2.i = i;
            s2.j = j;
         }
      }
      b1.update_grid_status(s1.i, s1.j, 'x');
      b1.update_grid_status(s2.i, s2.j, 'x');
      if (b1.is_game_over()) {
         break;
      }
      b1.print_board();
      int score = b1.eval_board(true);
      cout << "black score: " << score << endl;
      score = b1.eval_board(false);
      cout << "white score: " << score << endl;
      cout << endl;

      b1.alpha_beta(DEPTH, -_INFINITE_, _INFINITE_, false);
      TwoSteps& ts = b1.best_steps();
      b1.update_grid_status(ts.step1.i, ts.step1.j, 'o');
      b1.update_grid_status(ts.step2.i, ts.step2.j, 'o');
      b1.print_board();
      score = b1.eval_board(true);
      cout << "black score: " << score << endl;
      score = b1.eval_board(false);
      cout << "white score: " << score << endl;
      cout << endl;
   }

   if (b1.m_black_win) {
      cout << "black wins" << endl;
   }
   else if (b1.m_white_win) {
      cout << "white wins" << endl;
   }
   else {
      cout << "bug!" << endl;
   }

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

#pragma once
#include <iostream>
#include <map>
#include <set>
#include <stack>
#include <string>
#include <vector>
#include <time.h>
#include <ppl.h>
#include <concurrent_vector.h>

using namespace std;
using namespace concurrency;


//#define DYM_EVAL
#define POTENTIAL_GEP 1
// must >= 3
#ifdef _DEBUG
#define DEPTH 2
#else
#define DEPTH 3
#endif

#define _INFINITE_ 2147483646
#define _IMPOSSIBLE_ 2147483647
#define WIN_SCORE 100000

#define STEP_INDEX(i,j) ((i*100)+j)
#define I_FROM_INDEX(index) (index/100)
#define J_FROM_INDEX(index) (index%100)
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

   //x: black, o:white
   static char* s_black_patterns[];
   static char* s_white_patterns[];
   static int s_positive_score[];

   // kmp string compare.
   static int *s_failure_function[PATTERN_NUM];
   static int s_pattern_len[PATTERN_NUM];

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
   void test();
   TwoSteps& best_steps() { return m_next_best_steps; }

   static void compute_failure_function();
private:
   void get_steps_from_center_step(int i, int j, char grid, vector<int>& steps);
   int eval_partial_board(int target_len, bool is_black_turn);
   void copy_from_other(const Board& other);

   static int kmp_matcher(char *target, int target_len, char *pattern, int pattern_index, int pattern_len);
};

static int ranged_rand(int range_min, int range_max)
{
   // Generate random numbers in the half-closed interval
   // [range_min, range_max). In other words,
   // range_min <= random number < range_max
   int u = static_cast<int>((double)rand()/(RAND_MAX+1)*(range_max-range_min)+range_min);
   return u;
}
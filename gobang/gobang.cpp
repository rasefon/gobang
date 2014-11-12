// gobang.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <map>
#include <string>
#include <vector>
#include <iostream>

using namespace std;

#define POTENTIAL_GEP 2
#define DEPTH = 5
#define INDEX_KEY(i,j) (i*100+j)
#define _INFINITE_ 2147483646
#define _IMPOSSIBLE_ 2147483647
#define PATTERN_NUM 33

class Step
{
public:
   Step(int ii, int jj):i(ii), j(jj) {}
   int i;
   int j;
};

char g_pattern_buffer[15];

//x: black, o:white
static char* s_black_patterns[] = {
   "xxxxx", //0
   "-xxxx-", //1
   "oxxxx-", "-xxxxo", "x-xxx", "xx-xx", "xxx-x", //2~6
   "--xxx-", "-xxx--", "-xx-x-", "-x-xx-", //7~10
   "oxxx--", "oxx-x-", "ox-xx-", "--xxxo", "-x-xxo", "-xx-xo", "xx--x", "x--xx", "x-x-x", "o-xxx-", "-xxx-o", //11~21
   "--xx--", "--x-x-", "-x-x--", "-x--x-",//22~25
   "oxx---", "ox-x--", "ox--x-", "---xxo", "--x-xo", "-x--xo", "x---x"//26~32
};

static char* s_white_patterns[] = {
   "ooooo",
   "-oooo-", 
   "xoooo-", "-oooox", "o-ooo", "oo-oo", "ooo-o",
   "--ooo-", "-ooo--", "-oo-o-", "-o-oo-",
   "xooo--", "xoo-o-", "xo-oo-", "--ooox", "-o-oox", "-oo-ox", "oo--o", "o--oo", "o-o-o", "x-ooo-", "-ooo-x",
   "--oo--", "--o-o-", "-o-o--", "-o--o-",
   "xoo---", "xo-o--", "xo--o-", "---oox", "--o-ox", "-o--ox", "o---o"
};

static int s_positive_score[] = {
   _INFINITE_,
   50000,
   10000, 10000, 10000, 10000, 10000,
   5000, 5000, 5000, 5000,
   1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
   500, 500, 500, 500,
   100, 100, 100, 100, 100, 100, 100
};

// kmp string compare.
static int *s_failure_function[PATTERN_NUM];
static int s_pattern_len[PATTERN_NUM];

static void compute_failure_function()
{
   //first allocate failure function array.
   for (int i = 0; i < PATTERN_NUM; i++) 
   {
      int len = strlen(s_black_patterns[i]);
      s_pattern_len[i] = len;
      s_failure_function[i] = new int[len];
      s_failure_function[i][0] = 0;
      int k = 0;

      for (int q = 1; q < len; q++) 
      {
         while (k>0 && s_black_patterns[i][k]!=s_black_patterns[i][q]) 
         {
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
   while (p < target_len)
   {
      while (target[p] != pattern[q] && p < target_len)
         p++;

      if (p == target_len)
         return pattern_matched;

      while (target[p] == pattern[q] && q != pattern_len-1)
      {
         p++;
         q++;
      }

      if (target[p] == pattern[q] && q == pattern_len-1)
      {
         pattern_matched++;
      }

      q = s_failure_function[pattern_index][q-1];
   }

   return pattern_matched;
}

class Board
{
private:
   char m_bb[15][15];
   // available for certain grid?
   map<int, bool> m_grid_status;

   int m_max_right;
   int m_min_left;
   int m_min_top;
   int m_max_buttom;

   int m_left;
   int m_right;
   int m_top;
   int m_buttom;

   // current round, black 'x',  white 'o'.
   char m_pre_round;

   //each side can play 2 steps, this index is to show currently in which step. 1 or 2
   int m_step_index;
   Step *m_pre_step;

   //cache previous step score.
   int m_pre_horizontal_score[15];
   int m_pre_vertical_score[15];
   int m_pre_diagonal_slash[21];
   int m_pre_diagonal_backslash[21];
public:
   Board();
   ~Board();
   // '-' empty, 'o' white, 'x' black
   void update_grid_status(int i, int j, char grid);
   void get_next_steps(vector<Step*>& steps, char grid);

   // positive score for black and negative score for white.
   int eval_board();
   int eval_horizontal(int row);
   int eval_vertical(int col);
   int eval_diagonal_slash(int index, bool up);
   int eval_diagonal_backslash(int index, bool up);

   bool is_valid_next_step(int i, int j, char grid);
   int alpha_beta(int depth, int alpha, int beta, bool bOrW);

   void print_board();
private:
   void update_next_step_range();
};

Board::~Board()
{
   m_grid_status.clear();
}

Board::Board()
{ 
   m_min_left = m_min_top = m_left = m_top = 16;
   m_max_right = m_max_buttom = m_right = m_buttom = 0;
   m_pre_round = '-';
   m_step_index = 0;

   for (int i = 0; i < 15; i++) 
   {
      for (int j = 0; j < 15; j++) 
      {
         m_bb[i][j] = '-';
         int grid_index_key = INDEX_KEY(i,j);
         m_grid_status[grid_index_key] = true;
      }
   }

   //initialize previous score as zero
   for (int i = 0; i < 15; i++) 
   {
      m_pre_horizontal_score[i] = m_pre_vertical_score[i] = 0;
   }

   for (int i = 0; i < 21; i++) 
   {
      m_pre_diagonal_slash[i] = m_pre_diagonal_backslash[i] = 0;
   }
}

void Board::update_grid_status(int i, int j, char grid)
{
   m_bb[i][j] = grid;

   Step *s = new Step(i,j);

   if (m_pre_step) 
   {
      delete m_pre_step;
   }
   m_pre_step = s;

   if (grid != m_pre_round) 
   {
      m_step_index = 1;
   }
   else
   {
      m_step_index++;
   }
   m_pre_round = grid;


   if ('-' != grid) 
   {
      int index = INDEX_KEY(i,j);
      m_grid_status[index] = false;

      if (m_left > i) 
      {
         m_left = i;
      }

      if (m_right < i) 
      {
         m_right = i;
      }

      if (m_top > j) 
      {
         m_top = j;
      }

      if (m_buttom < j) 
      {
         m_buttom = j;
      }
   }
}

void Board::update_next_step_range()
{
   m_min_left= (m_left-POTENTIAL_GEP)<0 ? 0 : (m_left-POTENTIAL_GEP);
   m_max_right = (m_right+POTENTIAL_GEP)>15 ? 15 : (m_right+POTENTIAL_GEP);
   m_min_top = (m_top-POTENTIAL_GEP)<0 ? 0 : (m_top-POTENTIAL_GEP);
   m_max_buttom = (m_buttom-POTENTIAL_GEP)>15 ? 0 : (m_buttom+POTENTIAL_GEP);
}

void Board::get_next_steps(vector<Step*>& steps, char grid)
{
   update_next_step_range();

   for (int i = m_min_left; i <= m_max_right; i++) 
   {
      for (int j = m_min_top; j <= m_max_buttom; j++) 
      {
         int key = INDEX_KEY(i,j);
         if (m_grid_status[key]) 
         {
            bool valid = is_valid_next_step(i, j, grid);
            if (valid)
            {
               Step *s = new Step(i,j);
               steps.push_back(s);
            }
         }
      }
   }
}

bool Board::is_valid_next_step(int i, int j, char grid)
{
   if (grid == m_pre_round && m_step_index == 1) 
   {
      if (i >= m_pre_step->i-1 && i <= m_pre_step->i+1 && j >= m_pre_step->j-1 && j <= m_pre_step->j+1)
         return false;
   }

   return true;
}

int Board::eval_board()
{
   int score = 0;

   for (int row = m_top; row <= m_buttom; row++)
      score += eval_horizontal(row);

   for (int col = m_left; col <= m_right; col++)
      score += eval_vertical(col);

   return score;
}

int Board::eval_horizontal(int row)
{
   int score = 0;
   
   for (int i = 0; i < 15; i++)
      g_pattern_buffer[i] = m_bb[row][i];
   
   for (int i = 0; i < PATTERN_NUM; i++)
   {
      int black_turn_matched = kmp_matcher(g_pattern_buffer, 15, s_black_patterns[i], i, s_pattern_len[i]);
      score += s_positive_score[i]*black_turn_matched;
      int white_turn_matched = kmp_matcher(g_pattern_buffer, 15, s_white_patterns[i], i, s_pattern_len[i]);
      score -= s_positive_score[i]*white_turn_matched;
   }

   return score;
}

int Board::eval_vertical(int col)
{
   int score = 0;

   for (int i = 0; i < 15; i++)
      g_pattern_buffer[i] = m_bb[i][col];

   for (int i = 0; i < PATTERN_NUM; i++)
   {
      int black_turn_matched = kmp_matcher(g_pattern_buffer, 15, s_black_patterns[i], i, s_pattern_len[i]);
      score += s_positive_score[i]*black_turn_matched;
      int white_turn_matched = kmp_matcher(g_pattern_buffer, 15, s_white_patterns[i], i, s_pattern_len[i]);
      score -= s_positive_score[i]*white_turn_matched;
   }

   return 0;
}

int Board::eval_diagonal_slash(int index, bool up)
{
   return 0;
}

int Board::eval_diagonal_backslash(int index, bool up)
{
   return 0;
}

int Board::alpha_beta(int depth, int alpha, int beta, bool bOrW) {
   static int pre_value = _IMPOSSIBLE_;
   int value;
   char grid = bOrW ? 'x' : 'o';

   if (depth == 0) {
      return eval_board();
   }

   vector<Step*> ss1;
   get_next_steps(ss1, grid);
   for (size_t ii = 0; ii < ss1.size(); ii++) 
   {
      Step *s1 = ss1[ii];
      update_grid_status(s1->i, s1->j, grid);
      vector<Step*> ss2;
      get_next_steps(ss2, grid);
      for (size_t jj = 0; jj < ss2.size(); jj++) 
      {
         Step *s2 = ss2[jj];
         update_grid_status(s2->i, s2->j, grid);
         value = -alpha_beta(depth - 1, -beta, -alpha, !bOrW);
         update_grid_status(s2->i, s2->j, '-');
         if (pre_value == _IMPOSSIBLE_) 
         {
            pre_value = value;
         }

         if (pre_value < value) 
         {
            value = pre_value;
         }
      }
      update_grid_status(s1->i, s1->j, '-');
      if (value >= beta) {
         return beta;
      }
      if (value > alpha) {
         alpha = value;
      }
   }
   return alpha;
}

void Board::print_board()
{
   cout<<"   ";
   for (int i = 0; i < 15; i++) 
   {
      cout<<i<<"  ";
   }
   cout<<endl;

   for (int i = 0; i < 15; i++) 
   {
      if (i < 10) 
      {
         cout<<i<<"  ";
      }
      else
      {
         cout<<i<<" ";
      }

      for (int j = 0; j < 15; j++) 
      {
         if (j < 10) 
         {
            cout<<m_bb[i][j]<<"  ";
         }
         else
         {
            cout<<m_bb[i][j]<<"   ";
         }
      }
      cout<<endl;
   }
}

// some unit test functions
void test_kmp_matcher();

int _tmain(int argc, _TCHAR* argv[])
{
   compute_failure_function();
   Board b1;
   vector<Step*> steps;
   b1.update_grid_status(8,8,'x');
   b1.update_grid_status(6,8,'x');
   b1.update_grid_status(1,1,'o');
   b1.update_grid_status(3,1,'o');
   b1.update_grid_status(8,9,'x');
   b1.update_grid_status(6,7,'x');
   b1.update_grid_status(2,1,'o');
   b1.update_grid_status(2,3,'o');
   b1.print_board();

   int score = b1.eval_board();
   //testing
   //test_kmp_matcher();

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

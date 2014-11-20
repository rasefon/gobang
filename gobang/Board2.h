#pragma once

#include <stdlib.h>
#include <stack>
#include <set>
#include <vector>

using namespace std;

#define POTENTIAL_GEP 1
#define DEPTH 3

#define _INFINITE_ 2147483646
#define _IMPOSSIBLE_ 2147483647
#define WIN_SCORE 100000

#define STEP_INDEX(i,j) (i*100+j)
#define I_FROM_INDEX(index) (index/100)
#define J_FROM_INDEX(index) (index%100)

namespace BetterBoard
{
   typedef unsigned long long _uint64_;

   static _uint64_ s_patterns[] = {
      // ccccc
      0x1f,
      // -cccc, c-ccc, cc-cc, ccc-c, cccc-, -ccc-, c--cc, -c-cc, -cc-c, c-cc-, cc-c-, c-c-c
      0xf, 0x17, 0x1b, 0x1d, 0x1e, 0x0e, 0x13, 0x0b, 0x0d, 0x16, 0x1a, 0x15,
      // -cc-, c---c, -c-c-, c--c-, -c--c
      0x06, 0x11, 0x0a, 0x12, 0x09,
      // -c-
      0x02
   };
   static const int s_patterns_num = 19;

   static int s_lshift_count[] = {
      10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      11, 10, 10, 10, 10,
      12
   };

   static int s_score[] = {
      WIN_SCORE,
      2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 
      800, 800, 800, 800, 800, 
      100 
   };

   enum PieceType
   {
      empty,
      white,
      black 
   };

   struct Step
   {
      Step(){}
      Step(int ii, int jj):i(ii), j(jj) {}
      Step(const Step& step):i(step.i), j(step.j) {}
      int i;
      int j;
   };

   struct StepsPair
   {
      Step step1;
      Step step2;

      StepsPair(){}

      StepsPair(int s1_i, int s1_j, int s2_i, int s2_j)
      {
         step1.i = s1_i;
         step1.j = s1_j;
         step2.i = s2_i;
         step2.j = s2_j;
      }

      StepsPair(const StepsPair& other)
      {
         if (this != &other) {
            step1.i = other.step1.i;
            step1.j = other.step1.j;
            step2.i = other.step2.i;
            step2.j = other.step2.j;
         }
      }
   };

   // Board, from (0,0) ~ (14,14)
   class Board2
   {
      static _uint64_ set_16bit_helper(_uint64_ org_val, int i);

   private:
      // bitmap as board, use 2 long long array to represent black and white pieces.
      _uint64_ m_white_piece[4];
      _uint64_ m_black_piece[4];
      // global mask bitmap
      _uint64_ m_mask[4][64];
      bool m_is_computer_black;
      stack<StepsPair> m_steps_stack;
      StepsPair m_next_best_steps;

   private:
      inline _uint64_ get_mask(int i, int j, int& block_index) {
         block_index = i/4;
         // i%4 == i&0x03
         int mask_second_index = (i&0x03)*16+j;
         return m_mask[block_index][mask_second_index];
      }

   public:
      Board2();
      void set_computer_as_white() { m_is_computer_black = false;}
      void set_computer_as_black() { m_is_computer_black = true;}
      static void test_board2();

   private:
      void init();
      // check if the (i,j) is occupied.
      bool is_occupied(int block_index, _uint64_ mask);
      bool is_occupied(int block_index, _uint64_ mask, PieceType pt);
      bool is_sibling(int i, int j, int ii, int jj);
      void update_grid_status(int i, int j, PieceType pt);
      void generate_next_step_pair(vector<StepsPair*>& sp_vector);
      void get_steps_from_center_step(int i, int j, set<int>& steps);
      bool is_empty_board();
      
      _uint64_ get_hor_row(int row, PieceType pt);
      _uint64_ get_ver_col(int col, PieceType pt);
      _uint64_ get_slash_bits(int row, bool up, PieceType pt);
      _uint64_ get_backslash_bits(int row, bool up, PieceType pt);

      int eval_partial_board(_uint64_ partial_board);
      int eval_board();
      int eval_hor_board_helper(PieceType pt);
      int eval_ver_board_helper(PieceType pt);
      int eval_slash_board_helper(PieceType pt);
      int eval_backslash_board_helper(PieceType pt);

      int alpha_beta_max(int depth, int alpha, int beta);
      int alpha_beta_min(int depth, int alpha, int beta);

      void save_board(const StepsPair& next_steps);
      inline void restore_board(PieceType pt);
   };
}

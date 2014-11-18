#pragma once

#include <stdlib.h>
#include <stack>
#include <set>
#include <vector>

using namespace std;

#define POTENTIAL_GEP 1
#define DEPTH 2

#define _INFINITE_ 2147483646
#define _IMPOSSIBLE_ 2147483647
#define WIN_SCORE 100000

#define STEP_INDEX(i,j) (i*100+j)
#define I_FROM_INDEX(index) (index/100)
#define J_FROM_INDEX(index) (index%100)

namespace BetterBoard
{
   typedef unsigned long long _uint64_;

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
   };

   // Board, from (0,0) ~ (14,14)
   class Board2
   {
      // set (1,j) bit to 1.
      static _uint64_ set_bit_helper(_uint64_ org_value, int i, int j);
      // ***** 0x1f pattern
      static _uint64_ s_1f_hor_patterns[44];
      static void init_1f_hor_patterns();
      static _uint64_ s_1f_ver_patterns[4][64];
      static void init_1f_ver_patterns();

   private:
      // bitmap as board, use 2 long long array to represent black and white pieces.
      _uint64_ m_white_piece[4];
      _uint64_ m_black_piece[4];
      // global mask bitmap
      _uint64_ m_mask[4][64];

   private:
      inline _uint64_ get_mask(int i, int j, int& piece_index) {
         piece_index = i/4;
         int mask_second_index = (i%4)*16+j;
         return m_mask[piece_index][mask_second_index];
      }

   public:
      Board2();
      static void test_board2();

   private:
      void init();
      // check if the (i,j) is occupied.
      bool is_occupied(int piece_index, _uint64_ mask);
      bool is_sibling(int i, int j, int ii, int jj);
      void update_grid_status(int i, int j, PieceType pt);
      void generate_next_step_pair(vector<StepsPair*>& sp_vector);
      void get_steps_from_center_step(int i, int j, set<int>& steps);
      bool is_empty_board();
   };
}

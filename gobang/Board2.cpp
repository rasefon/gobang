#include "stdafx.h"
#include "Board2.h"
#include <iostream>

using namespace BetterBoard;

Board2::Board2()
{
   init();
}

void Board2::init()
{
   // initialize the mask array
   _uint64_ start_i = 1;
   for (size_t i = 0; i < 4; i++) {
      for (size_t k = 0; k < 64; k++) {
         m_mask[i][k] = start_i << k;
      }
   }

   // initialize white and black piece bitmap
   for (size_t i = 0; i < 4; i++) {
      m_white_piece[i] = 0;
      m_black_piece[i] = 0;
   }
}

void Board2::update_grid_status(int i, int j, PieceType pt)
{
   int block_index;
   _uint64_ mask = get_mask(i, j, block_index);
   if (pt == PieceType::empty) {
      mask = ~mask;
      m_black_piece[block_index] &= mask;
      m_white_piece[block_index] &= mask;
   }
   else if (pt == PieceType::white) {
      m_white_piece[block_index] |= mask;
   }
   else {
      m_black_piece[block_index] |= mask;
   }
}

bool Board2::is_occupied(int block_index, _uint64_ mask)
{
   if ((m_black_piece[block_index] | m_white_piece[block_index]) & mask) {
      return true;
   }

   return false;
}

bool Board2::is_occupied(int block_index, _uint64_ mask, PieceType pt)
{
   if (pt == PieceType::white) {
      if (m_white_piece[block_index] & mask) {
         return true;
      }
   } 
   else if (pt == PieceType::black){
      if (m_black_piece[block_index] & mask) {
         return true;
      }
   }

   return false;
}

bool Board2::is_sibling(int i, int j, int ii, int jj)
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

void Board2::generate_next_step_pair(vector<StepsPair*>& sp_vector)
{
   if (is_empty_board()) {
      StepsPair *sp = new StepsPair(7,7,9,9);
      sp_vector.push_back(sp);
      return;
   }

   set<int> first_steps;
   int block_index;
   for (int i = 0; i < 15; i++) {
      for (int j = 0; j < 15; j++) {
         _uint64_ mask = get_mask(i, j, block_index);
         if (is_occupied(block_index, mask)) {
            get_steps_from_center_step(i, j, first_steps);
         }
      }
   }

   set<int> step_pair_buffer;
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
               step_pair_buffer.insert(ts_index);
            }
         }
      }
   }

   set<int>::const_iterator ts_it = step_pair_buffer.begin();
   for (; ts_it != step_pair_buffer.end(); ts_it++) {
      int fs_index = (*ts_it)/10000;
      int ss_index = (*ts_it)%10000;
      int i = I_FROM_INDEX(fs_index);
      int j = J_FROM_INDEX(fs_index);
      int ii = I_FROM_INDEX(ss_index);
      int jj = J_FROM_INDEX(ss_index);
      StepsPair *sp = new StepsPair(i, j, ii, jj);
      sp_vector.push_back(sp);
   }
}

bool Board2::is_empty_board()
{
   for (int i = 0; i < 4; i++) {
      if ((m_white_piece[i]|m_black_piece[i]) != 0) {
         return false;
      }
   }

   return true;
}

void Board2::get_steps_from_center_step(int i, int j, set<int>& steps)
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
         int sibling_piece_index;
         _uint64_ sibling_mask = get_mask(x, y, sibling_piece_index);
         if (!is_occupied(sibling_piece_index, sibling_mask)) {
            int step_index = STEP_INDEX(x,y);
            steps.insert(step_index);
         }
      }
   }
}

void Board2::test_board2()
{
   Board2 b2;
   b2.update_grid_status(13,13,PieceType::white);
   b2.update_grid_status(13,14,PieceType::white);
   b2.update_grid_status(2,11,PieceType::black);
   b2.update_grid_status(4,12,PieceType::black);
   b2.update_grid_status(11,8,PieceType::white);
   b2.update_grid_status(7,4,PieceType::white);
   vector<StepsPair*> sp_vector;
   b2.generate_next_step_pair(sp_vector);

   _uint64_ r1 = b2.get_hor_row(13, PieceType::white);
   _uint64_ r2 = b2.get_hor_row(2, PieceType::black);
   _uint64_ r3 = b2.get_hor_row(5, PieceType::white);
   _uint64_ c1 = b2.get_ver_col(13, PieceType::white);
   _uint64_ c2 = b2.get_ver_col(11, PieceType::black);
   _uint64_ c3 = b2.get_ver_col(5, PieceType::white);
   _uint64_ s1 = b2.get_slash_bits(13, true, PieceType::black);
   _uint64_ s2 = b2.get_slash_bits(5, false, PieceType::white);
   _uint64_ bs1 = b2.get_backslash_bits(3, false, PieceType::white);
   _uint64_ bs2 = b2.get_backslash_bits(6, true, PieceType::black);

   cout<<"placeholder"<<endl;
}

_uint64_ Board2::set_16bit_helper(_uint64_ org_val, int i)
{
   _uint64_ mask = 1<<i;
   return (org_val|mask);
}

_uint64_ Board2::get_hor_row(int row, PieceType pt)
{
   int block_index = row/4;
   int sub_index = row&0x03;
   _uint64_ block;
   if (pt == PieceType::white) {
      block = m_white_piece[block_index];
   }
   else {
      block = m_black_piece[block_index];
   }
   block = block >> (16*sub_index);
   _uint64_ mask = 0xffff;
   return (block & mask);
}

_uint64_ Board2::get_ver_col(int col, PieceType pt)
{
   _uint64_ col_val = 0;
   for (int i = 0; i < 15; i++) {
      int block_index = i/4;
      _uint64_ mask = get_mask(i, col, block_index);
      if (is_occupied(block_index, mask, pt)) {
         col_val = set_16bit_helper(col_val, i);
      }
   }
   return col_val;
}

_uint64_ Board2::get_slash_bits(int row, bool up, PieceType pt)
{
   if ((up && row<4) || (!up && row>10)) {
      return 0;
   }

   _uint64_ slash_val = 0;
   if (up) {
      for (int i = row, j = 14; i >= 0; i--, j--) {
         int block_index = i/4;
         _uint64_ mask = get_mask(i, j, block_index);
         if (is_occupied(block_index, mask, pt)) {
            slash_val = set_16bit_helper(slash_val, row-i);
         }
      }
   }
   else {
      for (int i = row, j=0; i <= 14; i++, j++) {
         int block_index = i/4;
         _uint64_ mask = get_mask(i, j, block_index);
         if (is_occupied(block_index, mask, pt)) {
            slash_val = set_16bit_helper(slash_val, i-row);
         }
      }
   }

   return slash_val;
}

_uint64_ Board2::get_backslash_bits(int row, bool up, PieceType pt)
{
   if ((up && row<4) || (!up && row>10)) {
      return 0;
   }

   _uint64_ backslash_val = 0;
   if (up) {
      for (int i = row, j = 14; i >=0; i--, j--) {
         int block_index = i/4;
         _uint64_ mask = get_mask(i, j, block_index);
         if (is_occupied(block_index, mask, pt)) {
            backslash_val = set_16bit_helper(backslash_val, row-i);
         }
      }
   } 
   else {
      for (int i = row, j = 0; i <=14; i++, j++) {
         int block_index = i/4;
         _uint64_ mask = get_mask(i, j, block_index);
         if (is_occupied(block_index, mask, pt)) {
            backslash_val = set_16bit_helper(backslash_val, i-row);
         }
      }
   }

   return backslash_val;
}
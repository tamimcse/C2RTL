/*
* TabTree based packet classification
*/
#include <stdint.h>

uint8_t tabtree(uint32_t ip_src, uint32_t ip_dst,
  uint32_t small_small_tree[64], int ssbit1, int ssbit2, int ssbit3, int ssbit4, int ssbit5, int ssbit6,  /*subtree along with bit position*/
  uint32_t big_small_tree[64], int bsbit1, int bsbit2, int bsbit3, int bsbit4, int bsbit5, int bsbit6,
  uint32_t small_big_tree[64], int sbbit1, int sbbit2, int sbbit3, int sbbit4, int sbbit5, int sbbit6) 
{
  //Find leaf for each tree
  int ss_idx =  ((ip_src >> ssbit1) & 1) | (((ip_src >> ssbit2) & 1) << 1) | (((ip_src >> ssbit3) & 1) << 2) | (((ip_dst >> ssbit4) & 1) << 3) |
                (((ip_dst >> ssbit5) & 1) << 4) | (((ip_dst >> ssbit6) & 1) << 5);
  uint32_t ss_leaf = small_small_tree[ss_idx];

  int bs_idx =  ((ip_dst >> bsbit1) & 1) | (((ip_dst >> bsbit2) & 1) << 1) | (((ip_dst >> bsbit3) & 1) << 2) | (((ip_dst >> bsbit4) & 1) << 3) |
                (((ip_dst >> bsbit5) & 1) << 4) | (((ip_dst >> bsbit6) & 1) << 5);
  uint32_t bs_leaf = big_small_tree[bs_idx];

  int sb_idx =  ((ip_src >> sbbit1) & 1) | (((ip_src >> sbbit2) & 1) << 1) | (((ip_src >> sbbit3) & 1) << 2) | (((ip_src >> sbbit4) & 1) << 3) |
                (((ip_src >> sbbit5) & 1) << 4) | (((ip_src >> sbbit6) & 1) << 5);
  uint32_t sb_leaf = small_big_tree[sb_idx];

  //Merge the result
  uint16_t ss_priority = ss_leaf && 0XFFFF;
  uint16_t ss_matchid = ss_leaf >> 16;
  uint16_t bs_priority = bs_leaf && 0XFFFF;
  uint16_t bs_matchid = bs_leaf >> 16;
  uint16_t sb_priority = sb_leaf && 0XFFFF;
  uint16_t sb_matchid = sb_leaf >> 16;
  
  if (ss_priority > bs_priority) {
    if (ss_priority > sb_priority)
      return ss_matchid;
    else
      return sb_matchid;
  } else {
    if (bs_priority > sb_priority)
      return bs_matchid;
    else
      return sb_matchid;
  }
}


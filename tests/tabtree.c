/*
CutSplit based packet classification. 

Step 1. TabTree removes (big,big) rules if they exist (less likely, and should be very few). Those rules should be classified by HyperSplit or TCAM

Step 2. TabTree partition rules into 3 subsets: (small,small), (big, small) and (small, big) subsets (based on source and destination IP addresses). 

Step 3. Each subset is represented as a bit-selecting tree. If a leaf has more rules that the threachthod, it appplies TSS on the leaf. But, here we will select more bits such that leaves always reach threashold (so we don't need TSS). 
*/

#include <stdint.h>

uint16_t tabtree(uint32_t ip_src, uint32_t ip_dst,
  uint32_t ss_subset[64], uint8_t ssbit1, uint8_t ssbit2, uint8_t ssbit3, uint8_t ssbit4, uint8_t ssbit5, uint8_t ssbit6,  /*(small, small) subset along with bit position*/
  uint32_t bs_subset[64], uint8_t bsbit1, uint8_t bsbit2, uint8_t bsbit3, uint8_t bsbit4, uint8_t bsbit5, uint8_t bsbit6,  /*(big, small) subset along with bit position*/
  uint32_t sb_subset[64], uint8_t sbbit1, uint8_t sbbit2, uint8_t sbbit3, uint8_t sbbit4, uint8_t sbbit5, uint8_t sbbit6)  /*(small, big) subset along with bit position*/
{
  uint16_t ss_idx, bs_idx, sb_idx;
  uint32_t ss_leaf, bs_leaf, sb_leaf;
  uint16_t ss_priority, ss_matchid, bs_priority, bs_matchid, sb_priority, sb_matchid;

  //Find leaf in (small,small) subtree
  ss_idx =  ((ip_src >> ssbit1) & 1) | (((ip_src >> ssbit2) & 1) << 1) | (((ip_src >> ssbit3) & 1) << 2) | (((ip_dst >> ssbit4) & 1) << 3) |
                (((ip_dst >> ssbit5) & 1) << 4) | (((ip_dst >> ssbit6) & 1) << 5);
  ss_leaf = ss_subset[ss_idx];

  //Find leaf in (big,small) subtree
  bs_idx =  ((ip_dst >> bsbit1) & 1) | (((ip_dst >> bsbit2) & 1) << 1) | (((ip_dst >> bsbit3) & 1) << 2) | (((ip_dst >> bsbit4) & 1) << 3) |
                (((ip_dst >> bsbit5) & 1) << 4) | (((ip_dst >> bsbit6) & 1) << 5);
  bs_leaf = bs_subset[bs_idx];

  //Find leaf in (small,big) subtree
  sb_idx =  ((ip_src >> sbbit1) & 1) | (((ip_src >> sbbit2) & 1) << 1) | (((ip_src >> sbbit3) & 1) << 2) | (((ip_src >> sbbit4) & 1) << 3) |
                (((ip_src >> sbbit5) & 1) << 4) | (((ip_src >> sbbit6) & 1) << 5);
  sb_leaf = sb_subset[sb_idx];

  //Merge the result based on pririty
  ss_priority = ss_leaf & 0XFFFF;
  ss_matchid = ss_leaf >> 16;
  bs_priority = bs_leaf & 0XFFFF;
  bs_matchid = bs_leaf >> 16;
  sb_priority = sb_leaf & 0XFFFF;
  sb_matchid = sb_leaf >> 16;
  
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


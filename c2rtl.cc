/*
 * Copyright (c) 2020-2021 MD Iftakharul Islam (Tamim) <tamim@csebuet.org>
 * All rights reserved.
 */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <sys/queue.h>
#include <stdbool.h>

#include "queue.h"

// This is the first gcc header to be included
#include "gcc-plugin.h"
#include "plugin-version.h"

#include "tree-pass.h"
#include "context.h"
#include "function.h"
#include "tree.h"
#include "tree-ssa-alias.h"
#include "internal-fn.h"
#include "is-a.h"
#include "predict.h"
#include "basic-block.h"
#include "gimple-expr.h"
#include "gimple.h"
#include "gimple-pretty-print.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"

// We must assert that this plugin is GPL compatible
int plugin_is_GPL_compatible;

//Latency of the register. This can vary based on the cell library
#define REGISTER_LATENCY .04

static struct plugin_info my_gcc_plugin_info = { "1.0", "This is a very simple plugin" };

//It can be input/output variable names or constant
struct name {
  char name[100];
  int bitsize;
  SLIST_ENTRY(name) nextptr;//single linked list next pointer
} *np;

//Verilog input, output and wires produced by this program 
SLIST_HEAD(, name) input = SLIST_HEAD_INITIALIZER(input);
SLIST_HEAD(, name) output = SLIST_HEAD_INITIALIZER(output);
SLIST_HEAD(, name) wires = SLIST_HEAD_INITIALIZER(wires);

//There can be at-most 3 inputs in GIMPLE_ASSIGN. MUX2 also need 3 inputs
#define MAX_INPUT_SIZE 3
struct operation {
  //Operation
  enum tree_code op;
  char op_name [1000];
  struct name output;
  struct name inputs[MAX_INPUT_SIZE];
  uint64_t num_inputs;
  int bb_idx;
};

//We stores all the operations in the program in an array. We use the array index
//in the CDFG to reference the operation
#define MAX_NUM_OPERATIONS 20000
struct operation ops[MAX_NUM_OPERATIONS];
int ops_cnt = 0;
//index of the return operation.
int ret_ops_idx = 0;

//Returns next available operation from the array
struct operation* get_next_empty_op ()
{
  if (ops_cnt >= MAX_NUM_OPERATIONS) {
    printf ("Exceeded the maximum number of operations. Please increase the array size !!!!\n");
    exit (1);
  }
  return &ops[ops_cnt];
}

//Holds latency of each kind of operation. The latency vary based on the cell library.
//Here index indicates the tree_code and value is the latency of that kind of operation
float latency[200];

//forward declaration
struct bb_vertex;
  
//This is used for implementing topological sorting using DFS. This is also used
//for implementing BFS and DFS.
enum mark
{
  UNMARKED = 0,
  TEMP_MARKED = 1,
  PERM_MARKED = 2
};
  
//Operation vertex of CDFG
struct op_vertex {
  int op_idx;
  int in_degree;
  float latenccy;
  //start time in the schedule
  float start_time;
  //Edges representing data dependencies (Dynamic array of op_vertex*)
  struct op_vertex **data_edges;
  int num_data_edges;
  //Edges representing control dependencies
  struct bb_vertex **control_edges;
  int num_control_edges;
  enum mark visit;//used for implementing topological sorting using DFS
  SLIST_ENTRY(op_vertex) nextptr;//next pointer
  TAILQ_ENTRY(op_vertex) nextptr_sched;//next pointer
} *ovp, *ovp1;

//topologically sorted operation vertices
SLIST_HEAD(, op_vertex) sorted_op_vertices = SLIST_HEAD_INITIALIZER(sorted_op_vertices);

//number of clock cycles needed by scheduling
int num_clock_cycles = 0;
//maximum number of clock cycles in scheduling
#define MAX_CLOCK_CYCLE 300
//It contains the result of the scheduling. Each element of the array is a
//linked list. The linked list contains the operations that are to be
//scheduled at i-th clock cycle where i is the array index 
TAILQ_HEAD(, op_vertex) sched_res[MAX_CLOCK_CYCLE];

//Predicate of BB (Basic Block) Vertex
struct predicate {
  struct name *name;
  bool pred_value;
  STAILQ_ENTRY(predicate) nextptr;//next pointer
} *pp, *pp1;

STAILQ_HEAD(pred_list_head, predicate);
//Basic block (BB) vertex of CDFG
struct bb_vertex {
  int bb_idx;
  int in_degree;
  int num_preds;
  //Dynamic array of operations in the BB
  struct op_vertex **operations;
  int num_operations;
  //Edges representing control dependencies (dynamic array of bb_vertex *)
  struct bb_vertex **control_edges;
  int num_control_edges;
  //dynamic array of predicate lists
  struct pred_list_head *pred_list;
//  struct predicate **preds;
  bool visited;//Used for implementing BFS and DFS
  STAILQ_ENTRY(bb_vertex) nextptr;//next pointer used BB list
  STAILQ_ENTRY(bb_vertex) phi_pred_nextptr;//next pointer used by PHI pred list
} *bvp;

//This is the root of CDFG
struct bb_vertex *cdfg_root;

//List of all the BBs
STAILQ_HEAD(, bb_vertex) bb_list = STAILQ_HEAD_INITIALIZER(bb_list);

enum gimple_tree_code {
  RSHIFT_TREE_CODE = 89,
  NOP_TREE_CODE = 121,
  MULT_TREE_CODE = 69,
  POINTER_PLUS_TREE_CODE = 70,
  MEM_REF_TREE_CODE = 158,
  COMPONENT_REF_TREE_CODE = 45,
  SSA_TREE_CODE = 146,
  NE_EXPR_TREE_CODE = 107,
  GT_EXPR_TREE_CODE = 104,
  LSHIFT_TREE_CODE = 88,
  BIT_AND_TREE_CODE = 94,
  PLUS_TREE_CODE = 67,
  MINUS_TREE_CODE = 68,
  PHI_TREE_CODE = 33,/*PHI*/
  /*This isn't present in GCC. We use this when relating PHI ops with muxs*/
  MUX_TREE_CODE = 889, 
  INT_CST_TREE_CODE = 26, /*assigning an integer constant*/
};

bool is_cond_op (struct operation *op)
{
  return (op->op == NE_EXPR_TREE_CODE) || (op->op == GT_EXPR_TREE_CODE);
}

bool is_phi_op (struct operation *op)
{
  return op->op == PHI_TREE_CODE;
}

bool is_mux_op (struct operation *op)
{
  return op->op == MUX_TREE_CODE;
}

bool is_ssa_op (struct operation *op)
{
  return op->op == SSA_TREE_CODE;
}

bool is_mult_op (struct operation *op)
{
  return op->op == MULT_TREE_CODE;
}

bool is_reg_op (struct operation *op)
{
  return op->op == SAVE_EXPR;
}

static bool is_ret_op (struct op_vertex *op)
{
  return op->op_idx == ret_ops_idx;
}

//Based TSMC 90nm
void init_latency ()
{
  latency[RSHIFT_TREE_CODE] = 0.35000000000000003;//Taken from Babbu
  latency[NOP_TREE_CODE] = .19;//Taken from Bambu
  latency[MULT_TREE_CODE] = .93;
  latency[POINTER_PLUS_TREE_CODE] = 0.64000000000000001;//Taken from Bambu
  latency[MEM_REF_TREE_CODE] = .95;
  latency[NE_EXPR_TREE_CODE] = .06;
  latency[GT_EXPR_TREE_CODE] = .1;
  latency[LSHIFT_TREE_CODE] = 0.65000000000000002;//Taken from Bambu
  latency[BIT_AND_TREE_CODE] = .05;
  latency[PLUS_TREE_CODE] = .35;
  latency[MINUS_TREE_CODE] = .35;
  latency[MUX_TREE_CODE] = .115;
  latency[INT_CST_TREE_CODE] = .05;
  latency[SSA_TREE_CODE] = .05;
  latency[COMPONENT_REF_TREE_CODE] = .95;
}

//Maximum number of selectors
#define SELECTOR_COUNT 50
//Macximum number of inputs
#define INPUT_COUNT 50
//Maximum number of characters in selector or input name
#define NAME_SIZE 50

struct mux {
  char selectors[SELECTOR_COUNT][NAME_SIZE];
  //actual number of selectors
  int num_selectors;
  char inputs[INPUT_COUNT][NAME_SIZE];
  //actual number of inputs
  int num_inputs;
  //contains selector bitstring for each input
  char input_selectors[INPUT_COUNT][SELECTOR_COUNT];
  int input_bitsize;
  int bb_idx;
};

void print_mux (struct mux *m) {
  int i;
  
  printf ("Selectors =");
  for (i = 0; i < m->num_selectors; i++)
    printf (" %s", &m->selectors[i][0]);
  
  printf ("\nInputs = \n");
  for (i = 0; i < m->num_inputs; i++) {
    printf (" %s %s\n", &m->input_selectors[i][0], &m->inputs[i][0]);
  }
}

//This variable is used to keep track of the number of MUXes. We also
//use it to create MUX output
int num_mux = 0;

//Maximum number of muxes in a tree
#define MUX_TREE_SIZE 100

//Generates MUX tree of 2-input muxes from a many-input MUX
void mux_tree_generation (struct mux *mux, struct operation op_arr[], int *mux_cnt) {
  int sel_idx, input_idx, i, j, p, q, in0, in1;
  char output[100];
  struct operation mux_arr0[MUX_TREE_SIZE];
  struct operation mux_arr1[MUX_TREE_SIZE];
  int mux_cnt0, mux_cnt1;
  
  printf ("printing MUX....");
  print_mux(mux);
  
  if (mux->num_inputs == 2) {
    //find a selector for which both input are 0/1 not x.
    sel_idx = -1;
    for (i = 0; i < mux->num_selectors; i++) {
      //i-th selector can alone select the input
      if ((mux->input_selectors[0][i] == '0' && mux->input_selectors[1][i] == '1') ||
        (mux->input_selectors[0][i] == '1' && mux->input_selectors[1][i] == '0')) {
        sel_idx = i;
        //find out which input will go to which port
        in0 = mux->input_selectors[0][i] == '0' ? 0 : 1;
        in1 = mux->input_selectors[1][i] == '1' ? 1 : 0;
        break;
      }
    }
    assert (sel_idx >= 0);
    struct operation op;
    op.bb_idx = mux->bb_idx;
    op.op = (enum tree_code)MUX_TREE_CODE;
    strcpy(op.op_name, "MUX");
    op.num_inputs = 3;
    strcpy (op.inputs[0].name, &mux->inputs[in0][0]);
    strcpy (op.inputs[1].name, &mux->inputs[in1][0]);
    strcpy(op.inputs[2].name, &mux->selectors[sel_idx][0]);
    op.inputs[0].bitsize = mux->input_bitsize;
    op.inputs[1].bitsize = mux->input_bitsize;
    op.inputs[2].bitsize = 1;
    sprintf(output, "mux%d", num_mux++);
    strcpy (op.output.name, output);
    op.output.bitsize = mux->input_bitsize;
    op_arr[0] = op;
    *mux_cnt = 1;
    return;
  }
  
  //Find a selector for which only one of the input is 1/0 and the other inputs
  //are opposite (none of are X). To do that, we divide the input into 0 and 1
  //group
  for (sel_idx = 0; sel_idx < mux->num_selectors; sel_idx++) {
    int group0_idx[INPUT_COUNT] = {0};
    int group0_cnt = 0;
    int group1_idx[INPUT_COUNT] = {0};
    int group1_cnt = 0;
    bool dont_care_found = false;
    for (input_idx = 0; input_idx < mux->num_inputs; input_idx++) {
      if (mux->input_selectors[input_idx][sel_idx] == 'x') {
        dont_care_found = true;
        break;
      } else if (mux->input_selectors[input_idx][sel_idx] == '0') {
        group0_idx[group0_cnt++] = input_idx;
      } else if (mux->input_selectors[input_idx][sel_idx] == '1') {
        group1_idx[group1_cnt++] = input_idx;
      }
    }
    if (dont_care_found)
      continue;
    
//    printf ("sel_idx = %d group0_cnt = %d group1_cnt = %d !!\n", sel_idx, group0_cnt, group1_cnt);
    
    //The selector can select an input with just one MUX
    if (group0_cnt == 1 || group1_cnt == 1) {
      //create a new big MUX without the selector and without the input that
      //has been deduced
      struct mux new_mux;
      memset(&new_mux, NULL, sizeof(new_mux));
      for (i = 0, j = 0; i < mux->num_selectors; i++) {
        if (i != sel_idx)
          strcpy(&new_mux.selectors[j++][0], &mux->selectors[i][0]);
      }
      int input_idx_to_skip = (group0_cnt == 1) ? group0_idx[0] : group1_idx[0];
      for (i = 0, j = 0; i < mux->num_inputs; i++) {
        if (i != input_idx_to_skip)
          strcpy(&new_mux.inputs[j++][0], &mux->inputs[i][0]);
      }

      for (i = 0, p = 0; i < mux->num_inputs; i++) {
        if (i == input_idx_to_skip)
          continue;
        for (j = 0, q = 0; j < mux->num_selectors; j++) {
          if (j == sel_idx)
            continue;
          new_mux.input_selectors[p][q++] = mux->input_selectors[i][j];
        }
        p++;
      }
      new_mux.num_inputs = mux->num_inputs - 1;
      new_mux.num_selectors = mux->num_selectors - 1;
      new_mux.bb_idx = mux->bb_idx;
      new_mux.input_bitsize = mux->input_bitsize;
      mux_tree_generation (&new_mux, op_arr, mux_cnt);
      
      struct operation op;
      op.bb_idx = mux->bb_idx;
      op.op = (enum tree_code)MUX_TREE_CODE;
      strcpy(op.op_name, "MUX");
      op.num_inputs = 3;
      //The single input will go to port 0, otherwise input will go to port 1
      if (group0_cnt == 1) {
        strcpy (op.inputs[0].name, &mux->inputs[input_idx_to_skip][0]);
        op.inputs[1] = op_arr[*mux_cnt - 1].output;
      } else {
        op.inputs[0] = op_arr[*mux_cnt - 1].output;
        strcpy (op.inputs[1].name, &mux->inputs[input_idx_to_skip][0]);
      }
      //Third input of the MUX is the selector
      strcpy(op.inputs[2].name, &mux->selectors[sel_idx][0]);
      //set the bitsize of the three inputs
      op.inputs[0].bitsize = mux->input_bitsize;
      op.inputs[1].bitsize = mux->input_bitsize;
      op.inputs[2].bitsize = 1;
      sprintf(output, "mux%d", num_mux++);
      strcpy (op.output.name, output);
      op.output.bitsize = mux->input_bitsize;
      op_arr[*mux_cnt] = op;
      *mux_cnt += 1;
      return;
    }
  }
  
  //Find a selector that divides the input into two groups.
  for (sel_idx = 0; sel_idx < mux->num_selectors; sel_idx++) {
    int group0_idx[INPUT_COUNT] = {0};
    int group0_cnt = 0;
    int group1_idx[INPUT_COUNT] = {0};
    int group1_cnt = 0;
    bool dont_care_found = false;
    for (input_idx = 0; input_idx < mux->num_inputs; input_idx++) {
      if (mux->input_selectors[input_idx][sel_idx] == 'x') {
        dont_care_found = true;
        break;
      } else if (mux->input_selectors[input_idx][sel_idx] == '0') {
        group0_idx[group0_cnt++] = input_idx;
      } else if (mux->input_selectors[input_idx][sel_idx] == '1') {
        group1_idx[group1_cnt++] = input_idx;
      }
    }
    if (dont_care_found)
      continue;
    
//    printf ("sel_idx = %d group0_cnt = %d group1_cnt = %d !!\n", sel_idx, group0_cnt, group1_cnt);
    
    if (group0_cnt >= 2 && group1_cnt >= 2) {
      //The selector divides the inputs into two groups of inputs
      struct mux new_mux0, new_mux1;
      memset (&new_mux0, NULL, sizeof (new_mux0));
      memset (&new_mux1, NULL, sizeof (new_mux1));
      
      for (i = 0, j = 0; i < mux->num_selectors; i++) {
        if (i != sel_idx) {
          strcpy(&new_mux0.selectors[j][0], &mux->selectors[i][0]);
          strcpy(&new_mux1.selectors[j][0], &mux->selectors[i][0]);
          j++;
        }
      }
      
      for (i = 0; i < group0_cnt; i++) {
        strcpy(&new_mux0.inputs[i][0], &mux->inputs[group0_idx[i]][0]);
      }
      for (i = 0; i < group1_cnt; i++) {
        strcpy(&new_mux1.inputs[i][0], &mux->inputs[group1_idx[i]][0]);
      }
      
      for (i = 0; i < group0_cnt; i++) {
        for (j = 0, q = 0; j < mux->num_selectors; j++) {
          if (j == sel_idx)
            continue;
          new_mux0.input_selectors[i][q++] = mux->input_selectors[group0_idx[i]][j];
        }
      }
      for (i = 0; i < group1_cnt; i++) {
        for (j = 0, q = 0; j < mux->num_selectors; j++) {
          if (j == sel_idx)
            continue;
          new_mux1.input_selectors[i][q++] = mux->input_selectors[group1_idx[i]][j];
        }
      }

      new_mux0.num_inputs = group0_cnt;      
      new_mux0.num_selectors = mux->num_selectors - 1;
      new_mux1.num_inputs = group1_cnt;      
      new_mux1.num_selectors = mux->num_selectors - 1;
      new_mux0.bb_idx = mux->bb_idx;
      new_mux0.input_bitsize = mux->input_bitsize;
      new_mux1.bb_idx = mux->bb_idx;
      new_mux1.input_bitsize = mux->input_bitsize;
      mux_tree_generation (&new_mux0, mux_arr0, &mux_cnt0);
      mux_tree_generation (&new_mux1, mux_arr1, &mux_cnt1);
      
      struct operation op;
      op.bb_idx = mux->bb_idx;
      op.op = (enum tree_code)MUX_TREE_CODE;
      strcpy(op.op_name, "MUX");
      op.num_inputs = 3;
      op.inputs[0] = mux_arr0[mux_cnt0 - 1].output;
      op.inputs[1] = mux_arr1[mux_cnt1 - 1].output;
      strcpy(op.inputs[2].name, &mux->selectors[sel_idx][0]);
      op.inputs[2].bitsize = 1;
      sprintf(output, "mux%d", num_mux++);
      strcpy (op.output.name, output);
      op.output.bitsize = mux->input_bitsize;
      
      j = 0;
      for (i = 0; i < mux_cnt0; i++) {
        op_arr[j++] = mux_arr0[i];
      }
      for (i = 0; i < mux_cnt1; i++) {
        op_arr[j++] = mux_arr1[i];
      }
      op_arr[j++] = op;
      *mux_cnt = j;
      return;
    }
  }

  printf ("The code shouldn't come here !!!!!\n");
  strcpy (output, "no output found");
  exit (1);
}

//Compares two predicate based on their names
bool cmp_predicate(struct predicate *pred1, struct predicate *pred2)
{
  return !strcmp(pred1->name->name, pred2->name->name);
}

//Creates a new predicate based on existing predicate
struct predicate * clone_predicate (struct predicate *p)
{
  struct predicate *new_pred = (struct predicate *)xmalloc(sizeof (*new_pred));
  new_pred->name = p->name;
  new_pred->pred_value = p->pred_value;
  return new_pred;
}
  
//Inserts a data edge in CDFG
void insert_data_edge(struct op_vertex *src, struct op_vertex *dst)
{
  struct op_vertex **new_arr;
  int i;

  new_arr = (struct op_vertex **)xmalloc((src->num_data_edges + 1) *
             sizeof (struct op_vertex *));
  for (i = 0; i < src->num_data_edges; i++) {
    new_arr[i] = src->data_edges[i];
  }
  new_arr[src->num_data_edges++] = dst;
  //xfree (src->data_edges);
  src->data_edges = new_arr;
}

//Inserts an operation to a BB (basic block)
void insert_op_to_bbvertex (struct bb_vertex *bbv, int op_idx)
{
  int i;
  struct op_vertex **new_arr;
  struct op_vertex *new_vertex;

  if (!bbv)
    return;
      
  new_arr = (struct op_vertex **)xmalloc((bbv->num_operations + 1) *
            sizeof (*new_arr));
//      printf ("Current num of operations %d !!!!!\n", bbv->num_operations);
  for (i = 0; i < bbv->num_operations; i++) {
    new_arr[i] = bbv->operations[i];
  }

  new_vertex = (struct op_vertex *)xmalloc(sizeof (*new_vertex));
  new_vertex->op_idx = op_idx;
  new_vertex->latenccy = latency[ops[op_idx].op];
  new_vertex->start_time = 0;
  new_vertex->in_degree = 0;
  new_vertex->num_data_edges = 0;
  new_vertex->num_control_edges = 0;
  new_vertex->visit = UNMARKED;
  new_arr[bbv->num_operations] = new_vertex;
//xfree (bbv->operations);
  bbv->operations = new_arr;
  bbv->num_operations++;
}

//Remove an operation to a BB (basic block)
void remove_op_vertex (struct bb_vertex *bbv, int op_idx)
{
  int i;
  int to_remove = -1;

  if (!bbv)
    return;

  for (i = 0; i < bbv->num_operations; i++) {
    if (bbv->operations[i]->op_idx == op_idx) {
      to_remove = i;
      break;
    }
  }
  
  assert (to_remove >= 0);
  //TODO: dellocate bbv->operations[to_remove]  
  for (i = to_remove + 1; i < bbv->num_operations; i++) {
    bbv->operations[i - 1] = bbv->operations[i];
  }

  bbv->num_operations--;
}

//Creates BB list by visiting CDFG in BFS order
static void create_bb_list_bfs ()
{
  struct bb_vertex *bb;
  int i;
  struct queue q;
  
  init_queue(&q);
  enqueue(&q, cdfg_root);
  while (!queue_empty(&q)) {
    bb = (struct bb_vertex *)dequeue(&q);
    assert(bb);
    if (bb->visited)
      continue;
    bb->visited = true;
    //Add to the BB list
    STAILQ_INSERT_TAIL(&bb_list, bb, nextptr);
    for (i = 0; i < bb->num_control_edges; i++) {
      if (!bb->control_edges[i]->visited) {
        enqueue(&q, bb->control_edges[i]);
      }
    }
  }
}

static void print_bb_list ()
{
  printf ("Print BB Vertex List = ");
  STAILQ_FOREACH(bvp, &bb_list, nextptr)
    printf ("BB%d, ", bvp->bb_idx);
  printf ("\b\b\n");
}

const pass_data my_first_pass_data = 
{
  GIMPLE_PASS,
  "my_first_pass",        /* name */
  OPTGROUP_NONE,          /* optinfo_flags */
  TV_NONE,                /* tv_id */
  PROP_gimple_any,        /* properties_required */
  0,                      /* properties_provided */
  0,                      /* properties_destroyed */
  0,                      /* todo_flags_start */
  0                       /* todo_flags_finish */
};

const int get_bitsize(tree node)
{
  tree size_tree;
  
  size_tree = size_in_bytes (TREE_TYPE (node));
  return TREE_INT_CST_LOW(size_tree) * 8;
}

/*This function is implemented based on gcc's dump_generic_node()*/
const char * get_name1(tree node)
{
  enum tree_code code;
  tree op0, op1;
  tree decl_name;
  //static allocation will not work as we will return the string
  char *name =  (char*) xmalloc(100 * sizeof(char));
  const char *tmp1 = "", *tmp2 = "";

  if (node == NULL_TREE)
    return "empty_node";
  code = TREE_CODE (node);
  switch (code)
  {
    case SSA_NAME:
      if (SSA_NAME_IDENTIFIER (node))
	tmp1 = IDENTIFIER_POINTER(SSA_NAME_IDENTIFIER(node));      
      if (SSA_NAME_IS_DEFAULT_DEF (node))
	tmp2 = "_D";//"(D)"
      else if (SSA_NAME_OCCURS_IN_ABNORMAL_PHI (node))
        tmp2 = "_ab";//"(ab)"
      sprintf (name, "%s_%d%s", tmp1, SSA_NAME_VERSION (node), tmp2);
        return name;

    case PARM_DECL:
    case FIELD_DECL:
    case DEBUG_EXPR_DECL:
    case NAMESPACE_DECL:
    case NAMELIST_DECL:
    case VAR_DECL:
      //based on dump_decl_name () in gcc
      decl_name = DECL_NAME(node);
      if (decl_name != NULL_TREE) {
        return IDENTIFIER_POINTER(decl_name);
      } else {
        if (TREE_CODE (node) == LABEL_DECL && LABEL_DECL_UID (node) != -1)
          sprintf (name, "L.%d", (int) LABEL_DECL_UID (node));
        else if (TREE_CODE (node) == DEBUG_EXPR_DECL)
          sprintf (name, "D.#%i", DEBUG_TEMP_UID (node));
        else
          sprintf (name, "%c.%u", TREE_CODE (node) == CONST_DECL ? 'C' : 'D',
                    DECL_UID (node));
        return name;
      }
      
    case VOID_CST:
      return "VOID_CST";

    case INTEGER_CST:
//      sprintf (name, "%d", TREE_INT_CST_LOW (node));
      if (tree_fits_shwi_p (node))
        sprintf (name, "%" PRId64, tree_to_shwi (node));
      else if (tree_fits_uhwi_p (node))
        sprintf (name, "%" PRIu64, tree_to_uhwi (node));

      return name;
      
    case MEM_REF:
      if (TREE_CODE (TREE_OPERAND (node, 0)) != ADDR_EXPR)
        return get_name1(TREE_OPERAND (node, 0));
      else
        return get_name1(TREE_OPERAND (TREE_OPERAND (node, 0), 0));
      
    case RETURN_EXPR:
      op0 = TREE_OPERAND (node, 0);
      if (op0) {
	if (TREE_CODE (op0) == MODIFY_EXPR)
	  return get_name1(TREE_OPERAND (op0, 1));
	else
	  return get_name1(op0);
      } else {
        return "empty_return";
      }
      break;
    case COMPONENT_REF:
      op0 = TREE_OPERAND (node, 0);
      op1 = TREE_OPERAND (node, 1);
      sprintf (name, "%s->%s", get_name1(op0), get_name1(op1));
      return name;
  }
  return "name not found !!";
}

static void print_op (struct operation *op)
{
  printf ("bb= %d op = %s op code = %d output = %s(%d) Inputs: ", op->bb_idx, op->op_name, op->op,
          op->output.name, op->output.bitsize);
  for (int i = 0; i < (int)op->num_inputs; i++) {
    printf (" %s(%d)", op->inputs[i].name, op->inputs[i].bitsize);
  }
  printf ("\n");
}
  
static void print_ops ()
{
  int i;

  for (i = 0; i < ops_cnt; i++) {
    printf ("%d. ", i);
    print_op(&ops[i]);
  }
}

//Lookup a BB in CDFG
static struct bb_vertex* lookup_bb_vertex (struct bb_vertex *cdfg, int bb_idx)
{
  int i;
  struct bb_vertex *succ;

  if (!cdfg)
    return 0;
  if (cdfg->bb_idx == bb_idx) {
    return cdfg;
  } else {
    for (i = 0; i < cdfg->num_control_edges; i++) {
      succ = lookup_bb_vertex(cdfg->control_edges[i], bb_idx);
      if (succ)
        return succ;
    }
    return 0;
  }
}

//Looks up BB Vextex for a BB index
static struct bb_vertex* lookup_bb_vertex (int bb_idx)
{
  return lookup_bb_vertex(cdfg_root, bb_idx);
}

static struct bb_vertex* create_bb_vertex (int bb_idx)
{
  struct bb_vertex *new_bb_vertex;

  new_bb_vertex = (struct bb_vertex *)xmalloc(sizeof (*new_bb_vertex));
  memset(new_bb_vertex, 0, sizeof(*new_bb_vertex));
  new_bb_vertex->bb_idx = bb_idx;
  return new_bb_vertex;
}

static void insert_control_edges_to_last_op (struct bb_vertex *bbv,
        struct bb_vertex *new_bb_vertex)
{
  int i;
  struct bb_vertex **new_arr;

  if (!bbv)
    return;
  
  //Insert a control edge to the bb_vextex
  new_arr = (struct bb_vertex **)xmalloc((bbv->num_control_edges + 1) *
              sizeof (*new_arr));
  for (i = 0; i < bbv->num_control_edges; i++) {
    new_arr[i] = bbv->control_edges[i];
  }
  new_arr[bbv->num_control_edges] = new_bb_vertex;
//xfree (bbv->operations);
  bbv->control_edges = new_arr;
  bbv->num_control_edges++;
}

static struct bb_vertex *get_nonempty_succ (struct bb_vertex *bb)
{
  //Empty BB should have exactly one successor
  if (bb->num_control_edges != 1) {
    printf ("Empty BB should have exactly one successor\n");
    exit(1);
  }
  
  //found a non-empty succesor
  if (bb->control_edges[0]->num_operations) {
    return bb->control_edges[0];
  } else {
    return get_nonempty_succ(bb->control_edges[0]);
  }
}

static void remove_empty_bb (struct bb_vertex *bb)
{
  int i;
  
  for (i = 0; i < bb->num_control_edges; i++) {
    //successor is an empty BB
    if (!bb->control_edges[i]->num_operations) {
      bb->control_edges[i] = get_nonempty_succ(bb->control_edges[i]);
      printf ("Pointing BB %d to BB %d!!!!!!!\n", bb->bb_idx, bb->control_edges[i]->bb_idx);
    }
  }
  
  //do the same for the child BB
  for (i = 0; i < bb->num_control_edges; i++) {
    remove_empty_bb(bb->control_edges[i]);
  }
}

//GCC creates empty BB. Remove them
static void remove_empty_bb ()
{
  remove_empty_bb(cdfg_root);
}

static void calculate_bb_indegree()
{
  int i;
  
  STAILQ_FOREACH(bvp, &bb_list, nextptr) {
    for (i = 0; i < bvp->num_control_edges; i++)
      bvp->control_edges[i]->in_degree++;
  }
}

//This function iterates over the operations. For the last operation in a BB,
//it adds control edge
static void insert_control_edges_to_last_op_for_bb (struct bb_vertex *bb_v)
{
  int i, j;
  struct op_vertex *last_op;

  //control edge exists    
  if (bb_v->control_edges) {
    if (!bb_v->num_operations) {
      printf ("No operation in the BB %d !!!!!!\n", bb_v->bb_idx);
      return;
    }
    //Pick the last operation of the BB
    last_op = bb_v->operations[bb_v->num_operations - 1];
    //If the last operation is a conditional operation, move the control edges
    //to the operation
    if (is_cond_op(&ops[last_op->op_idx])) {
      //Set the control edges to the control edges of BB
      last_op->control_edges = bb_v->control_edges;
      last_op->num_control_edges = bb_v->num_control_edges;
      //update the indegree of the destination nodes
      for (i = 0; i < last_op->num_control_edges; i++) {
        for (j = 0; j < last_op->control_edges[i]->num_operations; j++) {
          last_op->control_edges[i]->operations[j]->in_degree++;    
        }
      }
    
      if (last_op->num_control_edges > 2) {
        printf ("This BB has more than two control edges!!!!!!!\n");
        exit (1);
      }
    }
  }
}

//This function iterates over the operations. For the last operation in a BB,
//it adds control edge
static void set_predicates_to_child_bb (struct bb_vertex *bb_v)
{
  struct op_vertex *last_op;
  struct predicate *new_pred, *pred;
  struct bb_vertex *chield_bb;

  //BB has no control edge or no operation
  if (!bb_v->control_edges || !bb_v->num_operations) {
    return;
  }

  //If the last operation is not a conditional node, no need to do it
  last_op = bb_v->operations[bb_v->num_operations - 1];
  if (!is_cond_op(&ops[last_op->op_idx])) {
    return;
  }
    
  if (last_op->control_edges[0]->num_preds == last_op->control_edges[0]->in_degree ||
       last_op->control_edges[1]->num_preds == last_op->control_edges[1]->in_degree) {
    printf ("All the predicates are already set. The code should not come here !!!!!!!\n");
    exit (1);
  }
    
  //add the predicates to the first child BB
  new_pred = (struct predicate *)xmalloc(sizeof (*new_pred));
  new_pred->name = &ops[last_op->op_idx].output;
  new_pred->pred_value = true;
  chield_bb = last_op->control_edges[0];
  STAILQ_INSERT_HEAD(&chield_bb->pred_list[chield_bb->num_preds], new_pred, nextptr);
  //predicates of a BB should be added to the successor BBs
  STAILQ_FOREACH(pp, &bb_v->pred_list[0], nextptr) {
    //Don't make function call in STAILQ_INSERT_HEAD()
    pred = clone_predicate(pp);
    STAILQ_INSERT_HEAD(&chield_bb->pred_list[chield_bb->num_preds], pred, nextptr);
  }
  chield_bb->num_preds++;
    
  //add the predicates to the second child BB
  new_pred = (struct predicate *)xmalloc(sizeof (*new_pred));
  new_pred->name = &ops[last_op->op_idx].output;
  new_pred->pred_value = false;
  chield_bb = last_op->control_edges[1];
  STAILQ_INSERT_HEAD(&chield_bb->pred_list[chield_bb->num_preds], new_pred, nextptr);
  //predicates of a BB should be added to the successor BBs
  STAILQ_FOREACH(pp, &bb_v->pred_list[0], nextptr) {
    //Don't make function call in STAILQ_INSERT_HEAD()
    pred = clone_predicate(pp);
    STAILQ_INSERT_HEAD(&chield_bb->pred_list[chield_bb->num_preds], pred, nextptr);
  }
  chield_bb->num_preds++;
}

static void insert_control_edges_to_last_op ()
{
  STAILQ_FOREACH(bvp, &bb_list, nextptr)
    insert_control_edges_to_last_op_for_bb(bvp);
}

static void allocate_predicate (struct bb_vertex *bb)
{
  int i;

  bb->pred_list = (struct pred_list_head *) xmalloc(bb->in_degree * sizeof (*bb->pred_list));
  for (i = 0; i < bb->in_degree; i++) {
    STAILQ_INIT(&bb->pred_list[i]);
  }
}

static void allocate_predicates ()
{
  STAILQ_FOREACH(bvp, &bb_list, nextptr)
    allocate_predicate(bvp);
}

//Sets predicates to each BB
static void set_predicates ()
{
  STAILQ_FOREACH(bvp, &bb_list, nextptr)
    set_predicates_to_child_bb(bvp);
}
  
static void insert_bb_vertices (basic_block bb)
{
  struct bb_vertex *src, *dst;

  //start and end BB
  if (bb->index == 0 || bb->index == 1)
    return;
  //First BB has not been created
  if (bb->index == 2 && !cdfg_root) {
    cdfg_root = (struct bb_vertex *)xmalloc(sizeof (*cdfg_root));
    memset(cdfg_root, 0, sizeof (*cdfg_root));
    cdfg_root->bb_idx = 2;
  }

  src = lookup_bb_vertex(bb->index);
  if (!src) {
    printf ("Shouldn't happen!!!!!!\n");
  }

  edge e;
  edge_iterator ei;

  FOR_EACH_EDGE (e, ei, bb->succs)
  {
    basic_block dest = e->dest;
    //If the destination node is the end node, then do nothing
    if (dest->index == 1)
      continue;
    //check if a bb-vertex is created for the destination
    dst = lookup_bb_vertex(dest->index);
    //If it's not created, create one
    if (!dst)
      dst = create_bb_vertex(dest->index);
    //insert a control edge
    insert_control_edges_to_last_op(src, dst);
    printf("processing bb%d -> bb%d \n", bb->index, dest->index);
  }
}
  
//This function iterates over the operations and insert them to bb_vertex
static void insert_ops_to_bb_vertex ()
{
  int i;
  int last_bb_idx = 0;
  struct bb_vertex *ret;

  for (i = 0; i < ops_cnt; i++) {
    if (ops[i].bb_idx != last_bb_idx) {
      ret = lookup_bb_vertex (ops[i].bb_idx);
      last_bb_idx = ops[i].bb_idx;
    }
    insert_op_to_bbvertex(ret, i);
  }
}
  
static struct op_vertex* find_op_vertex (struct bb_vertex *cdfg, int op_idx)
{
  int i;
  struct op_vertex *ret;

  if (!cdfg)
    return 0;
  for (i = 0; i < cdfg->num_operations; i++) {
    if (cdfg->operations[i]->op_idx == op_idx)
      return cdfg->operations[i];
  }
    
  for (i = 0; i < cdfg->num_control_edges; i++) {
    ret = find_op_vertex(cdfg->control_edges[i], op_idx);
    if (ret)
      return ret;
  }
  return 0;
}
  
//Lookup an op_vertex in CDFG based on op idx
static struct op_vertex* find_op_vertex (int op_idx)
{
  return find_op_vertex(cdfg_root, op_idx);
}
  
static void insert_data_edge (int from, int to)
{
  struct op_vertex *src, *dst;

  src = find_op_vertex(from);
  dst = find_op_vertex(to);
  insert_data_edge (src, dst);
  dst->in_degree++;
}
  
//This function iterates over the operations and data edges
static void insert_data_edges ()
{
  int i, j, k;
  char *output;

  for (i = 0; i < ops_cnt; i++) {
    output = ops[i].output.name;
    for (j = i+1; j < ops_cnt; j++) {
      for (k = 0; k < (int)ops[j].num_inputs; k++) {
        //match found
        if (!strcmp(output, ops[j].inputs[k].name)) {
          insert_data_edge (i, j);
        }
      }
    }
  }
}

//Print vertex in format: 
//op_idx(latency, in_degree)->[comma separated op_idx of vertices with outgoing edges]
static void print_op_vertex (struct op_vertex *op_v)
{
  int i, j;
  struct bb_vertex *bbv;
  
  printf ("%d(%f, %d)", op_v->op_idx, op_v->latenccy, op_v->in_degree);
  if (!op_v->num_data_edges && !op_v->num_control_edges)
    return;
  printf ("->["); 
  for (i = 0; i < op_v->num_data_edges; i++) {
    printf ("%d,", op_v->data_edges[i]->op_idx);
  }
  for (i = 0; i < op_v->num_control_edges; i++) {
    bbv = op_v->control_edges[i];
    if (!bbv->num_operations) {
      printf("]");
      return;
    }
    for (j = 0; j < bbv->num_operations; j++) {
      printf ("%d,", bbv->operations[j]->op_idx);
    }
  }
  //to remove trailing comma
  printf ("\b]");
}

static void print_bb (struct bb_vertex *bb)
{
  int i;

  printf ("BB %d ", bb->bb_idx);
  printf ("[pred= ");
  for (i = 0; i < bb->in_degree; i++) {
    printf ("{");
    STAILQ_FOREACH(pp, &bb->pred_list[i], nextptr)
      printf (" %s", pp->name->name);
    printf ("}, ");
  }
  printf ("]:");
  
  printf ("{indegree=%d}:", bb->in_degree);
  
  for (i = 0; i < bb->num_operations; i++) {
    printf (" ");
    print_op_vertex(bb->operations[i]);
  }
}

static void print_cdfg ()
{
  STAILQ_FOREACH(bvp, &bb_list, nextptr) {
    print_bb(bvp);
    printf ("\n");
  }
}

static struct op_vertex* find_unmarked_node (struct bb_vertex *cdfg_start)
{
  int i;
  struct op_vertex* ret;
    
  for (i = 0; i < cdfg_start->num_operations; i++) {
    if (cdfg_start->operations[i]->visit == UNMARKED)
      return cdfg_start->operations[i];
  }
  for (i = 0; i < cdfg_start->num_control_edges; i++) {
    ret = find_unmarked_node(cdfg_start->control_edges[i]);
    if (ret)
      return ret;
  }
  return 0;
}
  
static void visit (struct op_vertex* op_vertex)
{
  int i, j;
      
  if (op_vertex->visit == PERM_MARKED)
    return;

  if (op_vertex->visit == TEMP_MARKED) {
    printf ("This is not a DAG !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    exit (1);
  }
    
  op_vertex->visit = TEMP_MARKED;

  for (i = 0; i < op_vertex->num_data_edges; i++) {
    visit(op_vertex->data_edges[i]);
  }
    
  for (i = 0; i < op_vertex->num_control_edges; i++) {
    struct bb_vertex *control_edge = op_vertex->control_edges[i];
    for (j = 0; j < control_edge->num_operations; j++) {
      visit(control_edge->operations[j]);
    }
  }
  
  op_vertex->visit = PERM_MARKED;
  SLIST_INSERT_HEAD(&sorted_op_vertices, op_vertex, nextptr);
}

static void print_sorted_list ()
{
  printf("sorted_list =");
  SLIST_FOREACH (ovp, &sorted_op_vertices, nextptr)
    printf(" %d", ovp->op_idx);
  printf ("\n");    
}

//Topological sorting using DFS
static void topological_sort()
{
  //Select an unmarked node
  while (true) {
    struct op_vertex* unmarked = find_unmarked_node(cdfg_root);
    if (!unmarked)
      return;
    visit (unmarked);
  }
}

static void print_path_latency ()
{
  printf("Path latency of nodes =");
  SLIST_FOREACH (ovp, &sorted_op_vertices, nextptr)
    printf(" %d(%f)", ovp->op_idx, ovp->start_time);
  printf ("\n");
}

static void print_schedule_by_clock_cycle ()
{
  int i;
  
  for (i = 0; i <= num_clock_cycles; i++) {
    printf ("clock cycle %d. ", i);
    TAILQ_FOREACH (ovp, &sched_res[i], nextptr_sched)
      printf(" %d(%f)", ovp->op_idx, ovp->start_time);
    printf ("\n");
  }
}

static void populate_schedule_by_clock_cycle ()
{
  int i;
  
  //Initialize the array
  for (i = 0; i < MAX_CLOCK_CYCLE; i++) {
    TAILQ_INIT (&sched_res[i]);
  }
  //Iterate over the operations to be added
  SLIST_FOREACH (ovp, &sorted_op_vertices, nextptr) {
    //Take the floor of the path latency
    int clock_cycle = ovp->start_time;
    assert (clock_cycle < MAX_CLOCK_CYCLE);
    //find the operation after which this operation should be added
    bool added = false;
    TAILQ_FOREACH (ovp1, &sched_res[clock_cycle], nextptr_sched) {
      //operations are being added in the middle of the list
      if (ovp1->start_time > ovp->start_time) {
        TAILQ_INSERT_BEFORE (ovp1, ovp, nextptr_sched);
        added = true;
        break;
      }
    }
    //operations are being added in at the beginning or end of the list
    if (!added) {
      TAILQ_INSERT_TAIL (&sched_res[clock_cycle], ovp, nextptr_sched);
    }
  }
}

static float calc_start_time (float start_time, float latency)
{
  //It does not cross clock cycle boundary. So they can be scheduled in the same
  //clock cycle. otherwise schedule it in the next clock cycle
  return ((int)(start_time + latency + REGISTER_LATENCY) == (int)start_time) ? 
      start_time : ceil(start_time);
}

//ASAP is implemented using longest-path problem of DAG
//https://www.geeksforgeeks.org/find-longest-path-directed-acyclic-graph/
static void asap_schedule ()
{
  int i, j;
  struct op_vertex *op;
  
  //Initialization
  SLIST_FOREACH (ovp, &sorted_op_vertices, nextptr) {
    //if in_degree is 0, then initialize it to 0, otherwise -INFINITE
    ovp->start_time = !ovp->in_degree ? 0: -1000;      
  }
  
  while (true) {
    bool updated = false;
    SLIST_FOREACH (op, &sorted_op_vertices, nextptr) {
      for (i = 0; i < op->num_data_edges; i++) {
        if (op->data_edges[i]->start_time < (op->start_time + op->latenccy)) {
          op->data_edges[i]->start_time = 
            calc_start_time (op->start_time + op->latenccy, op->data_edges[i]->latenccy);
          updated = true;
        }
      }
      for (i = 0; i < op->num_control_edges; i++) {
        struct bb_vertex *dest_bb = op->control_edges[i];
        for (j = 0; j < dest_bb->num_operations; j++) {
          if (dest_bb->operations[j]->start_time < (op->start_time + op->latenccy)) {
            dest_bb->operations[j]->start_time = 
              calc_start_time (op->start_time + op->latenccy, dest_bb->operations[j]->latenccy);
            updated = true;
          }
        }
      }    
    }

    //No change is made, so we longest path is calculated
    if (!updated)
      return;
  }
}

static void set_num_clock_cycles ()
{
  SLIST_FOREACH (ovp, &sorted_op_vertices, nextptr) {
    //set num_clock_cycles based on the last node
    if (!SLIST_NEXT(ovp, nextptr))
      num_clock_cycles = ovp->start_time;      
  }
}

#define NEG_INF -1000
#define POS_INF 100000

//ALAP is implemented based on the result of ASAP. 
//http://cas.ee.ic.ac.uk/people/gac1/Synthesis/Lecture9.pdf
static void alap_schedule ()
{
  int i, j;
  struct op_vertex *op;
  float earliest_successor_latency;
  
  //Initialization
  SLIST_FOREACH (ovp, &sorted_op_vertices, nextptr) {
    //if it is not the last node, set path latency to -INFINITE
    if (SLIST_NEXT(ovp, nextptr))
      ovp->start_time = NEG_INF;
  }
  
  while (true) {
    bool updated = false;
    SLIST_FOREACH (op, &sorted_op_vertices, nextptr) {
      earliest_successor_latency = POS_INF;
      //ALAP time is already populated
      if (op->start_time > NEG_INF) {
        continue;
      }
      //traverse all the data edge to find the earliest successor 
      for (i = 0; i < op->num_data_edges; i++) {
        if (op->data_edges[i]->start_time < earliest_successor_latency) {
          earliest_successor_latency = op->data_edges[i]->start_time;
        }
      }
      //traverse all the control edge to find the earliest successor
      for (i = 0; i < op->num_control_edges; i++) {
        struct bb_vertex *dest_bb = op->control_edges[i];
        for (j = 0; j < dest_bb->num_operations; j++) {
          if (dest_bb->operations[j]->start_time < earliest_successor_latency) {
            earliest_successor_latency = dest_bb->operations[j]->start_time;
          }
        }
      }
      
      //all the following node has ALAP time populated
      if (earliest_successor_latency > NEG_INF) {
        //They belong to same cycle  
        if ((int)(earliest_successor_latency - op->latenccy) == (int)earliest_successor_latency)
          op->start_time = earliest_successor_latency - op->latenccy;
        else//It belongs to the previous cycle
          op->start_time = (int)earliest_successor_latency - op->latenccy - REGISTER_LATENCY;  
        updated = true;
      }
    }
    
    //No change is made, so we longest path is calculated
    if (!updated)
      return;
  }
}

static void set_name (struct name *n, tree t)
{
  strcpy(n->name, get_name1(t));
  n->bitsize = get_bitsize(t);
}

static void set_name (struct name *n, char *na, int bitsize)
{
  strcpy(n->name, na);
  n->bitsize = bitsize;
}

static int create_register (struct op_vertex *op)
{
  struct operation *new_op;
  char output_name[1000];
  int bitsize = ops[op->op_idx].output.bitsize;
  int op_idx;
  
  //Take an empty operation
  new_op = get_next_empty_op();
  new_op->op = SAVE_EXPR;
  strcpy(new_op->op_name, "reg");
  sprintf(output_name, "R%d", ops_cnt);
  set_name(&new_op->output, output_name, bitsize);
  new_op->num_inputs = 1;
  new_op->bb_idx = ops[op->op_idx].bb_idx;
  set_name(&new_op->inputs[0], ops[op->op_idx].output.name, bitsize);
  op_idx = ops_cnt++;
  print_op(new_op);
  return op_idx;
}

//Add a register with this vertex
static struct op_vertex *add_register (struct op_vertex *op, float start_time)
{
  struct bb_vertex *bb;
  int reg_op_idx;
  int i, j, k;
  struct op_vertex **new_arr;
  int data_edge = 0, control_edge = 0;
  struct op_vertex **pushed_data_edges, **unpushed_data_edges;
  struct bb_vertex **pushed_control_edges, **unpushed_control_edges;
  struct op_vertex *new_reg;
  char *output_name = ops[op->op_idx].output.name;
  
  reg_op_idx = create_register(op);
  
  //create the new op_vertex for the register
  new_reg = (struct op_vertex *)xmalloc(sizeof (*new_reg));
  new_reg->op_idx = reg_op_idx;
  new_reg->latenccy = REGISTER_LATENCY;
  new_reg->start_time = start_time;
  new_reg->in_degree = 1;
  new_reg->visit = UNMARKED;

  //lookup the BB to which this register should be added
  bb = lookup_bb_vertex (ops[reg_op_idx].bb_idx);
  if (!bb)
    return 0;
  
  //add the register to the BB
  new_arr = (struct op_vertex **)xmalloc((bb->num_operations + 1) *
          sizeof (*new_arr));
  for (i = 0; i < bb->num_operations; i++)
    new_arr[i] = bb->operations[i];
  new_arr[bb->num_operations] = new_reg;
//xfree (bb->operations);
  bb->operations = new_arr;
  bb->num_operations++;
  
  //count how many of the data edge should be pushed to the register
  for (i = 0; i < op->num_data_edges; i++) {
    if ((int)op->data_edges[i]->start_time > (int)start_time)
      data_edge++;
  }
  
  //All the data edge should be moved to the register
  if (op->num_data_edges == data_edge) {
    //move all the data edges to register
    new_reg->num_data_edges = op->num_data_edges;
    new_reg->data_edges = op->data_edges;
    //add an edge from the current operation to new register
    op->num_data_edges = 1;
    op->data_edges = (struct op_vertex **)xmalloc(1 * sizeof (*op->data_edges));
    op->data_edges[0] = new_reg;
  } else {
    //Only some data edges should be pushed to the newly inserted register.
    //Divide the current edges into list of pushed and un pushed edges
    pushed_data_edges = (struct op_vertex **)xmalloc(data_edge * sizeof(*pushed_data_edges));
    unpushed_data_edges = (struct op_vertex **)xmalloc((op->num_data_edges - data_edge + 1) *
            sizeof(*unpushed_data_edges));
    for (i = 0, j = 0, k = 0; i < op->num_data_edges; i++) {
      if ((int)op->data_edges[i]->start_time > (int)start_time)
        pushed_data_edges [j++] = op->data_edges[i];
      else
        unpushed_data_edges [k++] = op->data_edges[i];
    }
    //place the edges to current operation and the new register
    op->num_data_edges = op->num_data_edges - data_edge + 1;
    //xfree(op->data_edges);
    op->data_edges = unpushed_data_edges;
    op->data_edges[op->num_data_edges - 1] = new_reg;
    new_reg->num_data_edges = data_edge;
    new_reg->data_edges = pushed_data_edges;
  }
  
  //update input of the successor operations based on the register output
  struct operation *succ_op;
  struct name *reg_output;
  reg_output = &ops[reg_op_idx].output;
  for (i = 0; i < new_reg->num_data_edges; i++) {
    succ_op = &ops[new_reg->data_edges[i]->op_idx];
    for (j = 0; j < succ_op->num_inputs; j++) {
      if (!strcmp(succ_op->inputs[j].name, output_name)) {
        set_name(&succ_op->inputs[j], reg_output->name, reg_output->bitsize); 
      }
    }
  }
  
  //count how many of the control edge should be pushed to the register
  int idx_to_move [100] = {0};
  for (i = 0; i < op->num_control_edges; i++) {
    float bb_start_time = MAX_CLOCK_CYCLE;
    for (j = 0; j < op->control_edges[i]->num_operations; j++) {
      if (op->control_edges[i]->operations[j]->start_time < bb_start_time) {
        bb_start_time = op->control_edges[i]->operations[j]->start_time;
      }
    }
    //First op of the BB starts after the next cycle
    if ((int)bb_start_time > (int)start_time) {
      control_edge++;
      idx_to_move[i] = 1;//1 indicates that it needs to be moved
    }
  }

  //Only some control edges should be pushed to the newly inserted register
  if (op->num_control_edges != control_edge) {
    pushed_control_edges = (struct bb_vertex **)xmalloc(control_edge * sizeof(*pushed_control_edges));
    unpushed_control_edges = (struct bb_vertex **)xmalloc((op->num_control_edges - control_edge) *
            sizeof(*unpushed_control_edges));
    for (i = 0, j = 0, k = 0; i < op->num_control_edges; i++) {
      if (idx_to_move[i])
        pushed_control_edges [j++] = op->control_edges[i];
      else
        unpushed_control_edges [k++] = op->control_edges[i];
    }
  }
  
  //All the control edge has to be moved to new register
  if (op->num_control_edges == control_edge) {
    new_reg->num_control_edges = op->num_control_edges;
    new_reg->control_edges = op->control_edges;
    op->num_control_edges = 0;
    op->control_edges = 0;
  } else {
    op->num_control_edges = op->num_control_edges - control_edge;
    //xfree(op->control_edges);
    op->control_edges = unpushed_control_edges;
    //made the edges of operation edges of the register
    new_reg->num_control_edges = control_edge;
    new_reg->control_edges = pushed_control_edges;
  }

  return new_reg;
}

static void register_insertion_inner (struct op_vertex *op,
        int start_cycle, int end_cycle)
{
  struct op_vertex *new_reg;
  float start_time;
  int i;
  printf ("Register needed for op %d (%d, %d)\n", op->op_idx, start_cycle, end_cycle);
  
  struct op_vertex *pred_op = op;  
  for (i = start_cycle; i <= end_cycle; i++) {
    //first register should be added after the predeccessor. The following register
    //should be added at the beginning for the cycle
    start_time = (i == start_cycle) ? (pred_op->start_time + pred_op->latenccy) : i;
    new_reg = add_register(pred_op, start_time);
    TAILQ_INSERT_TAIL (&sched_res[i], new_reg, nextptr_sched);
    //the newly inserted register becomes the predeccessor
    pred_op = new_reg;
  }
}

static void register_insertion_after_ret (struct op_vertex *op, int start_cycle)
{
  struct op_vertex *new_reg;
  float start_time;

  printf ("Register needed for op %d (%d, %d)\n", op->op_idx, start_cycle, start_cycle);
    
  start_time = op->start_time + op->latenccy;
  new_reg = add_register(op, start_time);
  TAILQ_INSERT_TAIL (&sched_res[start_cycle], new_reg, nextptr_sched);
}

//returns the clock cycle of the last successor
static int clock_cycle_of_last_succ (struct op_vertex *curr_op)
{
  int end_clock_cycle;
  struct op_vertex *succ_op;
  struct bb_vertex *control_edge;
  int j, k;
  
  end_clock_cycle = curr_op->start_time;
  for (j = 0; j < curr_op->num_data_edges; j++) {
    succ_op = curr_op->data_edges[j];
    if (end_clock_cycle < succ_op->start_time)
      end_clock_cycle = succ_op->start_time;
  }
  for (j = 0; j < curr_op->num_control_edges; j++) {
    control_edge = curr_op->control_edges[j];
    for (k = 0; k < control_edge->num_operations; k++) {
      succ_op = control_edge->operations[k];
      if (end_clock_cycle < succ_op->start_time)
        end_clock_cycle = succ_op->start_time;
    }
  }
  return end_clock_cycle;
}

static void register_insertion ()
{
  struct op_vertex *curr_op;
  int end_clock_cycle;
  int i;
  
  for (i = 0; i < MAX_CLOCK_CYCLE; i++) {
    TAILQ_FOREACH (curr_op, &sched_res[i], nextptr_sched) {
      //If current operation is a register, simply skip
      if (is_reg_op(&ops[curr_op->op_idx]))
        continue;
      end_clock_cycle = clock_cycle_of_last_succ(curr_op);
      //current and successor are not scheduled in the same clock cycle,
      //registers need to be inserted 
      if (i != end_clock_cycle) {
        register_insertion_inner(curr_op, i, end_clock_cycle - 1);
      } else if (is_ret_op(curr_op)) {
        //We need to add a register after return operation
        register_insertion_after_ret(curr_op, i);
      }
    }
  }
}

//If the string ends with _D, then it's a function input
bool is_func_input (char *str)
{
  str = strrchr(str, '_');

  if( str != NULL )
    return strcmp(str, "_D") == 0;

  return false;
}

static void dump_gimple_return (const greturn *gs)
{
  tree t;

  t = gimple_return_retval (gs);
  
  if (ops_cnt <= 0) {
    printf ("Return should be preceeded by other operations!!!!\n");
    return;
  }
  //Pick last operation
  struct operation *last_op = &ops[ops_cnt - 1];
  set_name(&last_op->output, t);
  print_op(last_op);
  
  //store the index of the return statement
  ret_ops_idx = ops_cnt - 1;
}

static void generate_mux (struct operation *op, int op_idx)
{
  int i, j;
  int num_selectors = 0;
  bool found;
  struct predicate *new_pred;
  struct operation *last_op = NULL;
  char bitstr[100];
  struct bb_vertex* bb;
  
  //MUX can only be generated from a PHI operation
  assert(is_phi_op(op));

  //List of BBs that has control edge from the the BB to the the PHI node
  STAILQ_HEAD(, bb_vertex) input_bb_list = STAILQ_HEAD_INITIALIZER(input_bb_list);
  
  //Create a list of BBs that are predeccessor to the PHI operation
  STAILQ_FOREACH(bvp, &bb_list, nextptr) {
    for (i = 0; i < bvp->num_control_edges; i++) {
      if (bvp->control_edges[i]->bb_idx == op->bb_idx) {
        STAILQ_INSERT_TAIL(&input_bb_list, bvp, phi_pred_nextptr);
        break;
      }
    }
  }

  //create a list of selectors
  STAILQ_HEAD(, predicate) sel_list = STAILQ_HEAD_INITIALIZER(sel_list);
  STAILQ_FOREACH(bvp, &input_bb_list, phi_pred_nextptr) {
    for (j = 0; j < bvp->num_preds; j++) {
      STAILQ_FOREACH(pp, &bvp->pred_list[j], nextptr) {
        found = false;
        //check if the predicate already been added to the selector list
        STAILQ_FOREACH(pp1, &sel_list, nextptr) {
          if (!strcmp(pp->name->name, pp1->name->name)) {
            found = true;
            break;
          }
        }
        if (!found) {
          //Don't make function call in STAILQ_INSERT_HEAD()
          new_pred = clone_predicate(pp);
          STAILQ_INSERT_HEAD(&sel_list, new_pred, nextptr);
          num_selectors++;
        }
      }
    }
  }
  
  //print selector list
  printf ("Selectors (first one is MSB and the last one is LSB): ");
  STAILQ_FOREACH(pp, &sel_list, nextptr)
    printf (" %s", pp->name->name);
  printf ("\n");
  
  struct mux big_mux;
  big_mux.num_inputs = 0;
  big_mux.num_selectors = num_selectors;
  big_mux.bb_idx = op->bb_idx;
  big_mux.input_bitsize = 0;
  //Check if bit size differ for different inputs.
  STAILQ_FOREACH(bvp, &input_bb_list, phi_pred_nextptr) {
    last_op = &ops[bvp->operations[bvp->num_operations - 1]->op_idx];
    if (big_mux.input_bitsize == 0)
      big_mux.input_bitsize = last_op->output.bitsize;
    else
      assert (last_op->output.bitsize == big_mux.input_bitsize);
  }

  i = 0;
  STAILQ_FOREACH(pp, &sel_list, nextptr) {
    strcpy(&big_mux.selectors[i++][0], pp->name->name);
  }

  STAILQ_FOREACH(bvp, &input_bb_list, phi_pred_nextptr) {
    for (j = 0; j < bvp->num_preds; j++) {
      memset(bitstr, NULL, sizeof (bitstr));
      STAILQ_FOREACH(pp, &sel_list, nextptr) {
        found = false;
        STAILQ_FOREACH(pp1, &bvp->pred_list[j], nextptr) {
          if (!strcmp(pp->name->name, pp1->name->name)) {
            found = true;
            break;
          }
        }
        if(!found)
          sprintf (bitstr, "%sx", bitstr);
        else
          sprintf (bitstr, "%s%d", bitstr, (int)pp1->pred_value);
      }
      printf ("BB%d Bitstr = %s !!!\n", bvp->bb_idx, bitstr);
      assert(big_mux.num_inputs < INPUT_COUNT);
      strcpy(&big_mux.input_selectors[big_mux.num_inputs][0], bitstr);
      last_op = &ops[bvp->operations[bvp->num_operations - 1]->op_idx];
      strcpy(&big_mux.inputs[big_mux.num_inputs][0], last_op->output.name);
      big_mux.num_inputs++;
    }
  }
  
  struct operation muxs[MUX_TREE_SIZE];
  int mux_cnt = 0;
  mux_tree_generation (&big_mux, muxs, &mux_cnt);
  assert(mux_cnt < MUX_TREE_SIZE);
  
  printf ("printing MUXs.......................\n");
  for (i = 0; i < mux_cnt; i++) {
    print_op(&muxs[i]);
  }

  //Move all the operations after the PHI operation to make space for MUX operations
  for (i = ops_cnt - 1; i > op_idx; i--) {
    assert (i < MAX_NUM_OPERATIONS);
    ops[i + mux_cnt - 1] = ops[i];
  }
  //Replace PHI operation with new MUX operations
  for (i = 0, j = op_idx; i < mux_cnt; i++) {
    ops[j++] = muxs[i];
  }
  ops_cnt += (mux_cnt - 1);
  
  //Add the MUXs to the BB
  bb = lookup_bb_vertex (op->bb_idx);
  remove_op_vertex (bb, op_idx);
  for (i = 0, j = op_idx; i < mux_cnt; i++) {
    insert_op_to_bbvertex(bb, j++);
  }
  //TODO: dellocate selectors list and PHI predecccessor list
}

static void generate_muxs ()
{
  int i;
  
  for (i = 0; i < ops_cnt; i++) {
    if (is_phi_op(&ops[i])) {
      generate_mux(&ops[i], i);
    }
  }
}

/*This function is developed based on GCC's dump_gimple_cond()*/
static void dump_gimple_label (const glabel *gs)
{
  tree label = gimple_label_label (gs);
  //Take an empty operation
  struct operation *new_op = get_next_empty_op();
  new_op->op = TREE_CODE(label);
  //Add a PHI operation (SELECT operation) for label
  strcpy(new_op->op_name, "PHI");
  new_op->bb_idx = gs->bb->index;
  //Output will be set by the following return statement
  //Input will also be set later
    
  ops_cnt++;
  print_op(new_op);
}
  
/*This function is developed based on GCC's dump_gimple_cond()*/
static void dump_gimple_cond (const gcond *gs)
{
  char output[100];
  //Take an empty operation
  struct operation *new_op = get_next_empty_op();
  new_op->op = gimple_cond_code (gs);
  strcpy(new_op->op_name, get_tree_code_name (gimple_cond_code (gs)));
  sprintf(output, "ifout%d", ops_cnt);
  strcpy(new_op->output.name, output);
  new_op->output.bitsize = 1; 
  new_op->num_inputs = 2;
  new_op->bb_idx = gs->bb->index;
            
  tree arg1 = gimple_cond_lhs (gs);
  tree arg2 = gimple_cond_rhs (gs);
  set_name(&new_op->inputs[0], arg1);
  set_name(&new_op->inputs[1], arg2);
    
  ops_cnt++;
  print_op(new_op);
}

/*This function is developed based on GCC's dump_gimple_assign()*/
static void dump_gimple_assign (const gassign *gs)
{
  //Take an empty operation
  struct operation *new_op = get_next_empty_op();
  new_op->op = gimple_assign_rhs_code (gs);
  strcpy(new_op->op_name, get_tree_code_name (gimple_assign_rhs_code (gs)));
  set_name(&new_op->output, gimple_assign_lhs (gs));
  new_op->num_inputs = gimple_num_ops (gs) - 1;
  assert (new_op->num_inputs <= MAX_INPUT_SIZE);
  new_op->bb_idx = gs->bb->index;    
            
  tree arg1 = NULL;
  tree arg2 = NULL;
  tree arg3 = NULL;
  switch (gimple_num_ops (gs))
  {
    case 4:
      arg3 = gimple_assign_rhs3 (gs);
      arg2 = gimple_assign_rhs2 (gs);
      arg1 = gimple_assign_rhs1 (gs);
      set_name(&new_op->inputs[0], arg1);
      set_name(&new_op->inputs[1], arg2);
      set_name(&new_op->inputs[2], arg3);          
      break;
    case 3:
      arg2 = gimple_assign_rhs2 (gs);
      arg1 = gimple_assign_rhs1 (gs);
      set_name(&new_op->inputs[0], arg1);
      set_name(&new_op->inputs[1], arg2);
      break;
    case 2:
      arg1 = gimple_assign_rhs1 (gs);
      set_name(&new_op->inputs[0], arg1);
      break;
    default:
      printf("invalid gimple_assign !!!\n");
      exit (1);
  }
  ops_cnt++;
  print_op(new_op);
}

static bool is_power_of_two(int x)
{
  return (x != 0) && ((x & (x - 1)) == 0);
}

//Replace mult op with left shift
static void optimize_mult_op ()
{
  int i, j;
  int val;
  bool found;
  char snum[5];
  
  for (i = 0; i < ops_cnt; i++) {
    if (is_mult_op(&ops[i])) {
      found = false;
      for (j = 0; j < ops[i].num_inputs; j++) {
        val = atoi(ops[i].inputs[j].name);
        //the input is a valid integer and it's power of two
        if (val && is_power_of_two(val)) {
          sprintf(snum, "%d", (int)log2(val));
          strcpy(ops[i].inputs[j].name, snum);
          found = true;
        }
      }
      if (found) {
        ops[i].op = (enum tree_code)LSHIFT_TREE_CODE;
        strcpy (ops[i].op_name, "lshift_expr");
      }
    }
  }
}

//convert unsized constants to sized constant
static void convert_sized_const ()
{
  int i, j;
  long long int val;
  bool found;
  char snum[100];
  bool is_negative;
  
  for (i = 0; i < ops_cnt; i++) {
    for (j = 0; j < ops[i].num_inputs; j++) {
      is_negative = false;
      val = atoll(ops[i].inputs[j].name);
      //the input is a constant
      if (val) {
        if (val < 0) {
          val = abs (val);
          is_negative = true;
        }
        ops[i].inputs[j].bitsize = (int)log2(val) + 1;
        if (!is_negative)
          sprintf(snum, "%d 'd %s", ops[i].inputs[j].bitsize, ops[i].inputs[j].name);
        else
          sprintf(snum, "-%d 'd %d", ops[i].inputs[j].bitsize, val);
        strcpy(ops[i].inputs[j].name, snum);
        found = true;
      } else if (!strcmp(ops[i].inputs[j].name, "0")) {
        ops[i].inputs[j].bitsize = 1;
        strcpy(ops[i].inputs[j].name, "1 'd 0");
        found = true;
      }
    }
  }
}

//SSA often creates SSA operation which does a simple assignement. Eliminate those
static void remove_ssa_op ()
{
  int i, j;
  bool found;
  
  for (i = 0; i < ops_cnt; ) {
    if (is_ssa_op(&ops[i])) {
      found = false;
      //find the previous operation to which the SSA op should be added to
      for (j = i - 1; j >= 0; j++) {        
        if (!strcmp (ops[i].inputs[0].name, ops[j].output.name)) {
          //update the output of previous operation with the output of SSA operation
          strcpy(ops[j].output.name, ops[i].output.name);
          found = true;
          break;
        }
      }
      //Just to check if everything is going well or not
      if (!found) {
        printf ("SSA operation couldn't handled properly !!!!\n");
        exit (1);
      }
      //remove the SSA operation
      for (j = i + 1; j < ops_cnt; j++) {
        ops[j - 1] = ops[j];  
      }
      ops_cnt--;
    } else {
      i++;
    }
  }
}

static void extract_operations (basic_block bb)
{
  fprintf(stderr, "Basic Block %d\n", bb->index);
  gimple_bb_info *bb_info = &bb->il.gimple;
  gimple_seq seq = bb_info->seq;
  gimple_stmt_iterator i;

  for (i = gsi_start (seq); !gsi_end_p (i); gsi_next (&i))
  {
    gimple *gs = gsi_stmt (i);
    switch (gimple_code (gs))
    {
      case GIMPLE_ASM:
        printf("found ASM !!!! \n");
        //dump_gimple_asm (buffer, as_a <const gasm *> (gs), spc, flags);
        break;

      case GIMPLE_ASSIGN:
//        printf("found ASSIGN !!!! \n");
        dump_gimple_assign (as_a <const gassign *> (gs));
        break;

      case GIMPLE_BIND:
        printf("found BIND !!!! \n");
        //dump_gimple_bind (buffer, as_a <const gbind *> (gs), spc, flags);
        break;

      case GIMPLE_CALL:
        printf("found CALL !!!! \n");
        //dump_gimple_call (buffer, as_a <const gcall *> (gs), spc, flags);
        break;

      case GIMPLE_COND:
//      printf("found COND !!!! \n");
        dump_gimple_cond (as_a <const gcond *> (gs));
        break;

      case GIMPLE_LABEL:
//        printf("found LABEL !!!! \n");
        dump_gimple_label (as_a <const glabel *> (gs));
        break;

      case GIMPLE_GOTO:
        printf("found GOTO !!!! \n");
        //dump_gimple_goto (buffer, as_a <const ggoto *> (gs), spc, flags);
        break;

      case GIMPLE_NOP:
        printf("found NOP !!!! \n");
        //pp_string (buffer, "GIMPLE_NOP");
        break;

      case GIMPLE_RETURN:
//        printf("found RETURN !!!! \n");
        dump_gimple_return (as_a <const greturn *> (gs));
        break;

      case GIMPLE_SWITCH:
        printf("found SWITCH !!!! \n");
        //dump_gimple_switch (buffer, as_a <const gswitch *> (gs), spc, flags);
        break;

      default:
        printf("found DEFAULT !!!! \n");
    }
  }
}

void add_module_decl (FILE *out)
{
  //Time unit = 1ns clock precesion = 1ps
  fprintf (out, "`timescale 1ns / 1ps\n");
  fprintf (out, "module top(clock");
  SLIST_FOREACH(np, &input, nextptr)
    fprintf (out, ", %s", np->name);
  SLIST_FOREACH(np, &output, nextptr)
    fprintf (out, ", %s", np->name);
  fprintf (out, ");\n");
  fprintf (out, "  //IN\n");
  fprintf (out, "  input clock;\n");
  SLIST_FOREACH(np, &input, nextptr)
    fprintf (out, "  input [%d:0] %s;\n", np->bitsize - 1, np->name);
  fprintf (out, "  //OUT\n");
  SLIST_FOREACH(np, &output, nextptr)
    fprintf (out, "  output [%d:0] %s;\n", np->bitsize - 1, np->name);
  fprintf (out, "  //WIRES\n");
  SLIST_FOREACH(np, &wires, nextptr)
    fprintf (out, "  wire [%d:0] %s;\n", np->bitsize - 1, np->name);
}

void add_include_ip (FILE *output)
{
  fprintf (output, "`include \"ip.v\"\n");
  fprintf (output, "`include \"sram.v\"\n\n");
}

void add_module_end (FILE *output) 
{
  fprintf (output, "endmodule");
}

void add_operations (FILE *output, struct op_vertex *op)
{
  struct operation *o = &ops[op->op_idx];
  
  //Add two spaces before operation for indentation
  fprintf (output, "  ");
  
  switch (o->op) {
    case POINTER_PLUS_TREE_CODE:
    case PLUS_TREE_CODE:
      fprintf(output, "ADD_GATE #(.BITSIZE_in1(%d), .BITSIZE_in2(%d), "
              ".BITSIZE_out1(%d)) op%d (.out1(%s), .in1(%s), .in2(%s));\n",
              o->inputs[0].bitsize, o->inputs[1].bitsize, o->output.bitsize,
              op->op_idx, o->output.name, o->inputs[0].name, o->inputs[1].name);
      break;
      
    case MINUS_TREE_CODE:
      fprintf(output, "SUB_GATE #(.BITSIZE_in1(%d), .BITSIZE_in2(%d), "
              ".BITSIZE_out1(%d)) op%d (.out1(%s), .in1(%s), .in2(%s));\n",
              o->inputs[0].bitsize, o->inputs[1].bitsize, o->output.bitsize,
              op->op_idx, o->output.name, o->inputs[0].name, o->inputs[1].name);
      break;
      
    case MULT_TREE_CODE:
      fprintf(output, "MUL_GATE #(.BITSIZE_in1(%d), .BITSIZE_in2(%d), "
              ".BITSIZE_out1(%d)) op%d (.out1(%s), .in1(%s), .in2(%s));\n",
              o->inputs[0].bitsize, o->inputs[1].bitsize, o->output.bitsize,
              op->op_idx, o->output.name, o->inputs[0].name, o->inputs[1].name);
      break;
            
    case RSHIFT_TREE_CODE:
      fprintf(output, "RSHIFT_GATE #(.BITSIZE_in1(%d), .BITSIZE_in2(%d), "
              ".BITSIZE_out1(%d), .PRECISION(%d)) op%d (.out1(%s), .in1(%s), .in2(%s));\n",
              o->inputs[0].bitsize, o->inputs[1].bitsize, o->output.bitsize, o->output.bitsize,
              op->op_idx, o->output.name, o->inputs[0].name, o->inputs[1].name);
      break;
      
    case LSHIFT_TREE_CODE:
      fprintf(output, "LSHIFT_GATE #(.BITSIZE_in1(%d), .BITSIZE_in2(%d), "
              ".BITSIZE_out1(%d), .PRECISION(%d)) op%d (.out1(%s), .in1(%s), .in2(%s));\n",
              o->inputs[0].bitsize, o->inputs[1].bitsize, o->output.bitsize, o->output.bitsize,
              op->op_idx, o->output.name, o->inputs[0].name, o->inputs[1].name);
      break;
      
    case NOP_TREE_CODE:
      fprintf(output, "UUdata_converter_FU #(.BITSIZE_in1(%d), "
              ".BITSIZE_out1(%d)) op%d (.out1(%s), .in1(%s));\n",
              o->inputs[0].bitsize, o->output.bitsize,
              op->op_idx, o->output.name, o->inputs[0].name);
      break;
      
    case INT_CST_TREE_CODE:
      fprintf(output, "UUdata_converter_FU #(.BITSIZE_in1(%d), "
              ".BITSIZE_out1(%d)) op%d (.out1(%s), .in1(%s));\n",
              o->inputs[0].bitsize, o->output.bitsize,
              op->op_idx, o->output.name, o->inputs[0].name);
      break;
      
    case SSA_TREE_CODE:
      fprintf(output, "SSA_OP #(.BITSIZE_in1(%d), "
              ".BITSIZE_out1(%d)) op%d (.out1(%s), .in1(%s));\n",
              o->inputs[0].bitsize, o->output.bitsize,
              op->op_idx, o->output.name, o->inputs[0].name);
      break;
      
    case BIT_AND_TREE_CODE:
      fprintf(output, "bit_and #(.BITSIZE_in1(%d), .BITSIZE_in2(%d), "
              ".BITSIZE_out1(%d)) op%d (.out1(%s), .in1(%s), .in2(%s));\n",
              o->inputs[0].bitsize, o->inputs[1].bitsize, o->output.bitsize,
              op->op_idx, o->output.name, o->inputs[0].name, o->inputs[1].name);
      break;
      
    case NE_EXPR_TREE_CODE:
      fprintf(output, "NE_EXPR #(.BITSIZE_in1(%d), .BITSIZE_in2(%d),"
              ".BITSIZE_out1(%d)) op%d (.out1(%s), .in1(%s), .in2(%s));\n",
              o->inputs[0].bitsize, o->inputs[1].bitsize, o->output.bitsize,
              op->op_idx, o->output.name, o->inputs[0].name, o->inputs[1].name);
      break;
      
    case GT_EXPR_TREE_CODE:
      fprintf(output, "GT_EXPR #(.BITSIZE_in1(%d), .BITSIZE_in2(%d),"
              ".BITSIZE_out1(%d)) op%d (.out1(%s), .in1(%s), .in2(%s));\n",
              o->inputs[0].bitsize, o->inputs[1].bitsize, o->output.bitsize,
              op->op_idx, o->output.name, o->inputs[0].name, o->inputs[1].name);
      break;
      
    case SAVE_EXPR:
      fprintf(output, "REG_STD #(.BITSIZE_in1(%d), "
              ".BITSIZE_out1(%d)) op%d (.out1(%s), .clock(clock), .in1(%s));\n",
              o->inputs[0].bitsize, o->output.bitsize,
              op->op_idx, o->output.name, o->inputs[0].name);
      break;

    case MUX_TREE_CODE:
      fprintf(output, "MUX_GATE #(.BITSIZE_in1(%d), .BITSIZE_in2(%d), "
              ".BITSIZE_out1(%d)) op%d (.out1(%s), .in1(%s), .in2(%s), .sel(%s));\n",
              o->inputs[0].bitsize, o->inputs[1].bitsize, o->output.bitsize,
              op->op_idx, o->output.name, o->inputs[0].name, o->inputs[1].name,  o->inputs[2].name);
      break;

    case MEM_REF_TREE_CODE:
      fprintf(output, "syncRAM #(.ADR(16), .ADR(16), .DPTH(10)) op%d ("
              ".CS(1), .Clk(clock), .RD(1), .WE(0), .dataIn(0), .dataOut(%s), .Addr(%s));\n",
              op->op_idx, o->output.name, o->inputs[0].name);
      break;
      
    default:
     fprintf(output, "instruction missing %d\n", (int)o->op);
     break;
  }  
}

void populate_wires ()
{
  int i;
  
  for (i = 0; i < ops_cnt; i++) {
    bool added = false;
    //check if the wire already added
    SLIST_FOREACH(np, &wires, nextptr) {
      if (!strcmp(np->name, ops[i].output.name)) {
        added = true;
        break;
      }
    }
    if (!added)
      SLIST_INSERT_HEAD(&wires, &ops[i].output, nextptr);
  }
}

void populate_output ()
{
  //Note that same output will also be used by wires, so need to use a separate
  //copy before using it
  struct name *new_output = (struct name *) xmalloc(sizeof (*new_output));
  *new_output = ops[ops_cnt - 1].output;
  SLIST_INSERT_HEAD(&output, new_output, nextptr);
}

void populate_inputs () 
{
  int i, j;
  
  for (i = 0; i < ops_cnt; i++) {
    for (j = 0; j < ops[i].num_inputs; j++) {
      if (is_func_input(ops[i].inputs[j].name)) {
        bool added = false;
        //check if the input already added
        SLIST_FOREACH(np, &input, nextptr) {
          if (!strcmp(np->name, ops[i].inputs[j].name)) {
            added = true;
            break;
          }
        }
        if (!added)
          SLIST_INSERT_HEAD(&input, &ops[i].inputs[j], nextptr);
      }
    }
  }
}

void gen_verilog ()
{
  FILE *output;
  int i;

  output = fopen("top.v", "w");

  add_include_ip(output);

  add_module_decl(output);
  
  for (i = 0; i < MAX_CLOCK_CYCLE; i++) {
    TAILQ_FOREACH (ovp, &sched_res[i], nextptr_sched) {
      add_operations(output, ovp);
    }
  }
  add_module_end(output);
  fclose(output);
}

void gen_verilog_tb ()
{
  FILE *out;

  out = fopen("top_tb.v", "w");

  //include top module
  fprintf (out, "`include \"top.v\"\n");
  
  //Test bench, input and output declaration
  fprintf(out, "\n");
  //Time unit = 1ns clock precesion = 1ps
  fprintf (out, "`timescale 1ns / 1ps\n");
  //Test bench should only take a clock
  fprintf (out, "module top_tb;\n");
  fprintf (out, "  //IN\n");
  fprintf (out, "  reg clk, rst;\n");
  //in test_bench, inputs have to be register
  SLIST_FOREACH(np, &input, nextptr)
    fprintf (out, "  reg [%d:0] %s;\n", np->bitsize - 1, np->name);
  fprintf (out, "  //OUT\n");
  //in test_bench, outouts have to be wire
  SLIST_FOREACH(np, &output, nextptr)
    fprintf (out, "  wire [%d:0] %s;\n", np->bitsize - 1, np->name);
  
  //top module instantiation
  fprintf (out, "\n  //MODULE INSTANTIATION\n");
  fprintf(out, "  top tb (.clock(clk)");
  SLIST_FOREACH(np, &input, nextptr)
    fprintf (out, ", .%s(%s)", np->name, np->name);
  SLIST_FOREACH(np, &output, nextptr)
    fprintf (out, ", .%s(%s)", np->name, np->name);
  fprintf(out, ");\n");
  
  //First initialization
  fprintf(out, "\n  //INITIALIZATION\n");
  fprintf (out, "  initial\n");
  fprintf (out, "  begin\n");
  fprintf (out, "    $display($time, \"  Starting the Simulation \");\n");
  
  //adding a monitor
  fprintf (out, "    $monitor(\"%cd, %b, %b", '%');
  SLIST_FOREACH(np, &input, nextptr)
    fprintf (out, ", %sd", "%0");
  SLIST_FOREACH(np, &output, nextptr)
    fprintf (out, ", %sd", "%0");
  fprintf(out, "\", $time, clk, rst");
  SLIST_FOREACH(np, &input, nextptr)
    fprintf (out, ", %s", np->name);
  SLIST_FOREACH(np, &output, nextptr)
    fprintf (out, ", %s", np->name);
  fprintf(out, ");\n");
  
  SLIST_FOREACH(np, &input, nextptr)
    fprintf (out, "    %s = 64'hFFFFFFFFFFFFFFFF;\n", np->name);
  fprintf(out, "  end\n");
  
  //second initialization obtained from https://alchitry.com/blogs/tutorials/writing-test-benches
  fprintf(out, "\n  //INITIALIZATION\n");
  fprintf (out, "  initial\n");
  fprintf (out, "  begin\n");
  fprintf (out, "    clk = 1'b0;\n");
  fprintf (out, "    rst = 1'b1;\n");
  fprintf (out, "    repeat(4) #10 clk = ~clk;\n");
  fprintf (out, "    rst = 1'b0;\n");
  fprintf (out, "    repeat(1000) #10 clk = ~clk; // generate a clock\n");
  fprintf(out, "  end\n");
  
  fprintf(out, "\n  //Update input signals\n");
  fprintf (out, "endmodule");
  fclose(out);
}

struct my_first_pass : gimple_opt_pass
{
  my_first_pass(gcc::context *ctx) : gimple_opt_pass(my_first_pass_data, ctx)
  {
  }

  virtual unsigned int execute(function *fun) override
  {
    int status;
      
    init_latency();
    
    basic_block bb;
    printf ("\n");
    printf("------------------------------------------------------------\n");
    printf("          Reading SSA Operations\n");
    printf("------------------------------------------------------------\n");
    FOR_EACH_BB_FN(bb, fun)
    {
      extract_operations(bb);
    }
    
    remove_ssa_op ();
    optimize_mult_op();
    convert_sized_const();

    printf ("\n");
    printf("------------------------------------------------------------\n");
    printf("          Printing SSA Operations \n");
    printf("------------------------------------------------------------\n");
    print_ops();
      
    printf ("\n");
    printf("------------------------------------------------------------\n");
    printf("          Generating CDFG \n");
    printf("------------------------------------------------------------\n");
    //Iterates over the basic blocks and add bb_vertex and control edges to CDFG
    FOR_EACH_BB_FN(bb, fun)
    {
      insert_bb_vertices(bb);
    }
      
    //Now insert operations to bb_vertex..
    insert_ops_to_bb_vertex();
    
    //GCC creates empty BB. Remove them
    remove_empty_bb();
    
    //Create a BB list. In a CDFG, a BB may be visited more than once. 
    //To avoid this problem, we maintain this list.
    create_bb_list_bfs();
    
    //Make sure that BB list has been created. Rest of the program will use it internally 
    assert(!STAILQ_EMPTY(&bb_list));
    
    print_bb_list();
    
    //calculates indegree of each BB
    calculate_bb_indegree();
    
    //This function has to call after calculating indegree of BB Vertex because it uses it
    allocate_predicates();

    //Now add control edge to the last op_vertex of a BB
    insert_control_edges_to_last_op();
    
    //Sets predicates to each BB
    set_predicates();

    //Generate MUXs from PHI operation. This should be done after inserting control edges
    generate_muxs ();
    
    //Now iterate over operations and add data edges
    insert_data_edges();
    
    printf ("\n");
    printf("------------------------------------------------------------\n");
    printf("          Printing SSA Operations \n");
    printf("------------------------------------------------------------\n");
    print_ops();

    printf ("\n");
    printf("------------------------------------------------------------\n");
    printf("          Printing CDFG \n");
    printf("------------------------------------------------------------\n");
    print_cdfg();
            
    printf ("\n");
    printf("------------------------------------------------------------\n");
    printf("          Topological sorting CDFG \n");
    printf("------------------------------------------------------------\n");
    topological_sort();
    print_sorted_list();
      
    printf ("\n");
    printf("------------------------------------------------------------\n");
    printf("          ASAP Scheduling \n");
    printf("------------------------------------------------------------\n");
    asap_schedule();
    print_path_latency();
    
    printf ("\n");
    printf("------------------------------------------------------------\n");
    printf("          ALAP Scheduling \n");
    printf("------------------------------------------------------------\n");
    set_num_clock_cycles();
    alap_schedule();
    print_path_latency();

    printf ("\n");
    printf("------------------------------------------------------------\n");
    printf("          Scheduling by clock cycle\n");
    printf("------------------------------------------------------------\n");
    populate_schedule_by_clock_cycle();
    print_schedule_by_clock_cycle();

    printf ("\n");
    printf("------------------------------------------------------------\n");
    printf("          Register Insertion\n");
    printf("------------------------------------------------------------\n");
    register_insertion();

    printf ("\n");
    printf("------------------------------------------------------------\n");
    printf("          Printing SSA Operations \n");
    printf("------------------------------------------------------------\n");
    print_ops();

    printf ("\n");
    printf("------------------------------------------------------------\n");
    printf("          Printing CDFG \n");
    printf("------------------------------------------------------------\n");
    print_cdfg();
    
    printf ("\n");
    printf("------------------------------------------------------------\n");
    printf("          Printing Schedule by clock cycle\n");
    printf("------------------------------------------------------------\n");
    print_schedule_by_clock_cycle();
    
    printf ("\n");
    printf("------------------------------------------------------------\n");
    printf("          Verilog generation\n");
    printf("------------------------------------------------------------\n");
    populate_wires();
    populate_inputs();
    populate_output();
    gen_verilog();
    gen_verilog_tb();
    //compile verilog file
    status = system("iverilog top_tb.v && ./a.out");
    if (!status) {
      printf ("could not find top.v \n");
      return 1;
    }
    //open the verilog file
    status = system("gedit top.v &");
    if (status) {
      printf ("could not find top.v \n");
      return 1;
    }
    return 0;
  }

  virtual my_first_pass* clone() override
  {
    // We do not clone ourselves
    return this;
  }
};

int plugin_init (struct plugin_name_args *plugin_info,
		struct plugin_gcc_version *version)
{
  // We check the current gcc loading this plugin against the gcc we used to
  // created this plugin
  if (!plugin_default_version_check (version, &gcc_version))
  {
    printf("This GCC plugin is for version %d.%d\n", GCCPLUGIN_VERSION_MAJOR,
            GCCPLUGIN_VERSION_MINOR);
    return 1;
  }

  register_callback(plugin_info->base_name,
            /* event */ PLUGIN_INFO,
            /* callback */ NULL, /* user_data */ &my_gcc_plugin_info);

  // Register the phase right after cfg
  struct register_pass_info pass_info;

  pass_info.pass = new my_first_pass(g);
  pass_info.reference_pass_name = "ssa";
  pass_info.ref_pass_instance_number = 1;
  pass_info.pos_op = PASS_POS_INSERT_AFTER;

  register_callback (plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass_info);

  return 0;
}

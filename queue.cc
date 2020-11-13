#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "queue.h"

void init_queue (struct queue *q)
{
  q->start = 0;
  q->end = 0;
  memset(q->arr, NULL, sizeof (q->arr));  
}

void enqueue (struct queue *q, void* elem)
{
  assert (q->end < QUEUE_SIZE);
  q->arr[q->end++] = elem;
}

void* dequeue (struct queue *q)
{
  assert (q->end > q->start);
  return q->arr[q->start++];
}

bool queue_empty (struct queue *q)
{
  return q->end == q->start;
}

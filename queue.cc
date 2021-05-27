/*
 *   Copyright (c) 2020-2021 MD Iftakharul Islam (Tamim) <tamim@csebuet.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
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

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
#ifndef QUEUE_H_
#define QUEUE_H_

//We implement our own queue.
//Note that, STAILQ cannot add an item to queue if the item is already in the
//queue. Although we can create a separate struct, that will require dynamic memory
//allocation and deallocation everytime we do enqueue or dequeue. So, we implement
//our own array based queue
#define QUEUE_SIZE 2000
struct queue {
  int start;
  int end;
  void* arr[QUEUE_SIZE];
};

void init_queue (struct queue *q);
void enqueue (struct queue *q, void* elem);
void* dequeue (struct queue *q);
bool queue_empty (struct queue *q);

#endif /* QUEUE_H_ */

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


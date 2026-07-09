/*
 * simples_queue.h
 *
 *  Created on: Jun 25, 2026
 *      Author: Karol
 */

#ifndef INC_SIMPLE_QUEUE_H_
#define INC_SIMPLE_QUEUE_H_

#include <stdint.h>

typedef struct
{
    uint32_t head_ptr;
    uint32_t tail_ptr;
    uint32_t current_size;
    uint32_t max_size;
    uint8_t *buffer;
}SimpleQueue_t;

typedef enum
{
    QUEUE_FULL,
    QUEUE_EMPTY,
    QUEUE_OK,
    QUEUE_ERROR,
}QueueStatus_t;

QueueStatus_t SimpleQueueInit(SimpleQueue_t *my_queue, uint8_t *buffer, uint32_t max_size);
QueueStatus_t SimpleQueuePut(SimpleQueue_t *my_queue, void* element_in, uint8_t size);
QueueStatus_t SimpleQueueGet(SimpleQueue_t *my_queue, void *element_out, uint8_t size);

#endif /* INC_SIMPLE_QUEUE_H_ */

/*
 * simple_queue.c
 *
 *  Created on: Jun 25, 2026
 *      Author: Karol
 */
#include "simple_queue.h"
#include <stdlib.h>

QueueStatus_t SimpleQueueInit(SimpleQueue_t *my_queue, uint8_t *buffer, uint32_t max_size)
{
	QueueStatus_t ret_val = QUEUE_ERROR;
	if((my_queue != NULL) && (buffer != NULL))
	{
		my_queue->buffer = buffer;
		my_queue->current_size = 0;
		my_queue->max_size = max_size;
		my_queue->head_ptr = 0;
		my_queue->tail_ptr = 0;
		ret_val = QUEUE_OK;
	}
	return ret_val;
}

// size in bytes
QueueStatus_t SimpleQueuePut(SimpleQueue_t *my_queue, void* element_in, uint8_t size)
{
    uint8_t *helper = (uint8_t*)element_in;
    QueueStatus_t ret_val = QUEUE_ERROR;
    if((my_queue != NULL) && (element_in != NULL))
    {
        if((my_queue->current_size + size) < my_queue->max_size)
        {
            for(int i = 0; i < size; i++)
            {
                my_queue->buffer[my_queue->tail_ptr] = helper[i];
                my_queue->tail_ptr = (my_queue->tail_ptr + 1) % my_queue->max_size;
            }
            ret_val = QUEUE_OK;
            my_queue->current_size += size;
        }
        else
        {
            ret_val = QUEUE_FULL;
        }
    }
    return ret_val;
}

QueueStatus_t SimpleQueueGet(SimpleQueue_t *my_queue, void *element_out, uint8_t size)
{
    uint8_t *helper = (uint8_t*)element_out;
    QueueStatus_t ret_val = QUEUE_ERROR;
    if((my_queue != NULL) && (element_out != NULL))
    {
        if(my_queue->current_size >= size)
        {
            for(int i = 0; i < size; i++)
            {
                helper[i] = my_queue->buffer[my_queue->head_ptr];
                my_queue->head_ptr = (my_queue->head_ptr + 1) % my_queue->max_size;
            }
            my_queue->current_size-=size;
            ret_val = QUEUE_OK;
        }
        else
        {
            *helper = 0xFF;
            ret_val = QUEUE_EMPTY;
        }
    }
    return ret_val;
}

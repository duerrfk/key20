/**
 * This file is part of Key20.
 *
 * Copyright 2016 Frank Duerr
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "app_event_queue.h"
#include <app_util_platform.h>

void app_event_queue_init(struct app_event_queue *queue)
{
     queue->size = APP_EVENT_QUEUE_SIZE;
     queue->free = queue->size;
     queue->head = 0;
     queue->tail = 0;
}

int app_event_queue_add(struct app_event_queue *queue, struct app_event event)
{
     int retval;

     // Add and get functions might be accessed from interrupt context and 
     // application context concurrently -> need to protect critical sections
     // by avoiding context switches (interrupts) in critical section.
     CRITICAL_REGION_ENTER();
     if (queue->free == 0) {
	  retval = -1;
     } else { 
	  queue->events[queue->head] = event;
	  queue->head = (queue->head+1)%queue->size;
	  queue->free--;
	  retval = 0;
     }
     CRITICAL_REGION_EXIT();

     return retval;
}

int app_event_queue_get(struct app_event_queue *queue, struct app_event *event)
{
     int retval;
     
     // Add and get functions might be accessed from interrupt context and 
     // application context concurrently -> need to protect critical sections
     // by avoiding context switches (interrupts) in critical section.
     CRITICAL_REGION_ENTER();
     if (queue->free == queue->size) {
	  retval = -1;
     } else {
	  *event = queue->events[queue->tail];
	  queue->tail = (queue->tail+1)%queue->size;
	  queue->free++;
	  retval = 0;
     }
     CRITICAL_REGION_EXIT();

     return retval;
}


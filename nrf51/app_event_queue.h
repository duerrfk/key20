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

#ifndef APP_EVENT_QUEUE_H
#define APP_EVENT_QUEUE_H

#include <stdint.h>

#ifndef APP_EVENT_QUEUE_SIZE
#define APP_EVENT_QUEUE_SIZE 16
#endif

struct app_event {
     uint8_t event_type;
};

struct app_event_queue {
     struct app_event events[APP_EVENT_QUEUE_SIZE];
     unsigned int size;
     unsigned int free;
     unsigned int head;
     unsigned int tail;
};

void app_event_queue_init(struct app_event_queue *queue);

int app_event_queue_add(struct app_event_queue *queue, struct app_event event);

int app_event_queue_get(struct app_event_queue *queue, struct app_event *event);

#endif

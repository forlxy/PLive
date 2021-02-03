//
// Created by MarshallShuai on 2018/10/30.
//

#include "awesome_cache_interrupt_cb_c.h"

bool AwesomeCacheInterruptCB_is_interrupted(AwesomeCacheInterruptCB* self) {
    if (!self) {
        return false;
    }

    return self->callback
           && self->opaque
           && (self->callback(self->opaque) != 0);
}


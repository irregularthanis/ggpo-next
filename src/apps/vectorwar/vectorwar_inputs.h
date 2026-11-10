#ifndef _VECTORWAR_INPUTS_H_
#define _VECTORWAR_INPUTS_H_

/* Input bits consumed by the deterministic Vector War simulation. */
enum VectorWarInputs {
   INPUT_THRUST            = (1 << 0),
   INPUT_BREAK             = (1 << 1),
   INPUT_ROTATE_LEFT       = (1 << 2),
   INPUT_ROTATE_RIGHT      = (1 << 3),
   INPUT_FIRE              = (1 << 4),
   INPUT_BOMB              = (1 << 5),
};

#endif

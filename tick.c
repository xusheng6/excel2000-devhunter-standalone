/* game_tick (sub_3c7dcb73) — TODO: real translation (physics, entities, road
 * scroll + dev-name painting, collision).  For now: render the (static) loaded
 * state so the standalone shows the real road + text.  shade arg = 0 works for
 * the road (verified against the real framebuffer). */
#include "dh.h"
void game_tick(void){ game_render(0); }

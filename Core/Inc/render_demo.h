#ifndef RENDER_DEMO_H
#define RENDER_DEMO_H

typedef enum
{
  RENDER_DEMO_MODE_IDLE = 0,
  RENDER_DEMO_MODE_SINGLE = 1,
  RENDER_DEMO_MODE_RUN = 2
} render_demo_mode_t;

#ifdef __cplusplus
extern "C" {
#endif

void render_demo_set_mode(render_demo_mode_t mode);
render_demo_mode_t render_demo_get_mode(void);
void render_demo_toggle_background(void);
void render_demo_toggle_cube(void);
void render_demo_draw(void);
void render_demo_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* RENDER_DEMO_H */

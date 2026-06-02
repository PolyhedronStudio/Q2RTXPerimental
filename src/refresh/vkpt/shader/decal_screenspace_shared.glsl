#ifndef VKPT_DECAL_SCREENSPACE_SHARED_GLSL
#define VKPT_DECAL_SCREENSPACE_SHARED_GLSL

struct VkptDecalScreenspaceItem {
    vec4 pos_radius;
    vec4 normal_depth;
    vec4 color_misc;
};

#endif

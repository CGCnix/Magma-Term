#pragma once

#include <magma/backend/backend.h>

typedef struct magma_vk_renderer magma_vk_renderer_t;


magma_vk_renderer_t *magma_vk_renderer_init(magma_backend_t *backend);
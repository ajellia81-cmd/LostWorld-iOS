// iOS port: MoltenVK is linked statically, so volk is handed its
// vkGetInstanceProcAddr instead of loading a Vulkan library. This file is
// compiled with the Vulkan prototypes (no volk), and without volk_prefix.h's
// renaming of that one name, so it is MoltenVK's own function.
#undef VK_NO_PROTOTYPES
#undef vkGetInstanceProcAddr
#include <vulkan/vulkan.h>

PFN_vkGetInstanceProcAddr lw_moltenvk_get_instance_proc_addr(void) { return vkGetInstanceProcAddr; }

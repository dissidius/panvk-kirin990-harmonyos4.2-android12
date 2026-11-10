/* placed: VK_EXT_map_memory_placed below 4 GiB, as Wine does for 32-bit
 * games. Maps host-visible memory at a reserved low address, has the GPU fill
 * the buffer, then checks the result through the placed mapping.
 * With "opaque", does what Winlator's wrapper does with
 * WRAPPER_RESOURCE_TYPE=opaque instead: exports the memory as an opaque fd and
 * mmaps that fd at the low address itself.
 * Usage: placed <icd .so> [MiB] [opaque] */
#include <unistd.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define CHECK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) { \
   printf("FAIL %s = %d (line %d)\n", #x, r_, __LINE__); return 1; } } while (0)

int main(int argc, char **argv)
{
   if (argc < 2) { printf("usage: placed icd.so [MiB]\n"); return 2; }
   const uint64_t size = (argc > 2 ? atoi(argv[2]) : 4) << 20;
   const int opaque = argc > 3 && !strcmp(argv[3], "opaque");
   void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
   if (!lib) { printf("FAIL dlopen: %s\n", dlerror()); return 1; }
   PFN_vkGetInstanceProcAddr gipa =
      (PFN_vkGetInstanceProcAddr)dlsym(lib, "vk_icdGetInstanceProcAddr");
#define I(n) PFN_vk##n n = (PFN_vk##n)gipa(inst, "vk" #n)
   VkInstance inst = NULL;
   I(CreateInstance);
   const VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3 };
   CHECK(CreateInstance(&(VkInstanceCreateInfo){ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
         .pApplicationInfo = &app }, NULL, &inst));
   I(EnumeratePhysicalDevices); I(GetPhysicalDeviceMemoryProperties); I(CreateDevice);
   I(GetDeviceProcAddr); I(GetPhysicalDeviceProperties2);
   uint32_t n = 1; VkPhysicalDevice pd;
   CHECK(EnumeratePhysicalDevices(inst, &n, &pd));

   VkPhysicalDeviceMapMemoryPlacedPropertiesEXT pp = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAP_MEMORY_PLACED_PROPERTIES_EXT };
   VkPhysicalDeviceProperties2 props = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &pp };
   GetPhysicalDeviceProperties2(pd, &props);
   printf("device %s, placed alignment %llu\n", props.properties.deviceName,
          (unsigned long long)pp.minPlacedMemoryMapAlignment);

   const float prio = 1;
   VkPhysicalDeviceMapMemoryPlacedFeaturesEXT feat = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAP_MEMORY_PLACED_FEATURES_EXT,
      .memoryMapPlaced = VK_TRUE };
   const char *exts[] = { "VK_EXT_map_memory_placed", "VK_KHR_map_memory2",
                          "VK_KHR_external_memory_fd" };
   VkDevice dev;
   CHECK(CreateDevice(pd, &(VkDeviceCreateInfo){ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &feat,
         .queueCreateInfoCount = 1,
         .pQueueCreateInfos = &(VkDeviceQueueCreateInfo){ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueCount = 1, .pQueuePriorities = &prio },
         .enabledExtensionCount = 3, .ppEnabledExtensionNames = exts }, NULL, &dev));
#define D(n) PFN_vk##n n = (PFN_vk##n)GetDeviceProcAddr(dev, "vk" #n)
   D(GetDeviceQueue); D(CreateBuffer); D(GetBufferMemoryRequirements); D(AllocateMemory);
   D(BindBufferMemory); D(MapMemory2KHR); D(CreateCommandPool); D(AllocateCommandBuffers);
   D(GetMemoryFdKHR); D(BeginCommandBuffer); D(CmdFillBuffer); D(EndCommandBuffer); D(QueueSubmit); D(QueueWaitIdle);
   VkQueue q; GetDeviceQueue(dev, 0, 0, &q);

   VkBuffer buf;
   CHECK(CreateBuffer(dev, &(VkBufferCreateInfo){ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size,
         .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT }, NULL, &buf));
   VkMemoryRequirements mr; GetBufferMemoryRequirements(dev, buf, &mr);
   VkPhysicalDeviceMemoryProperties mp; GetPhysicalDeviceMemoryProperties(pd, &mp);
   uint32_t type = UINT32_MAX;
   for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
      if ((mr.memoryTypeBits & (1u << i)) &&
          (mp.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) { type = i; break; }
   VkDeviceMemory mem;
   const VkExportMemoryAllocateInfo export_info = { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
      .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT };
   CHECK(AllocateMemory(dev, &(VkMemoryAllocateInfo){ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
         opaque ? &export_info : NULL, .allocationSize = mr.size, .memoryTypeIndex = type }, NULL, &mem));
   CHECK(BindBufferMemory(dev, buf, mem, 0));

   /* Reserve an address below 4 GiB, as Wine's 32-bit address space does. */
   void *want = NULL;
   for (uintptr_t a = 0x40000000; a < 0xf0000000 && !want; a += 0x10000000) {
      void *p = mmap((void *)a, mr.size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
      if (p != MAP_FAILED) want = p;
   }
   if (!want) { printf("FAIL no free address below 4 GiB\n"); return 1; }

   void *ptr = NULL;
   if (opaque) {
      int fd = -1;
      CHECK(GetMemoryFdKHR(dev, &(VkMemoryGetFdInfoKHR){ VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
            .memory = mem, .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT }, &fd));
      off_t end = lseek(fd, 0, SEEK_END);
      if (end < 0) { printf("FAIL lseek on exported fd %d\n", fd); return 1; }
      ptr = mmap(want, mr.size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
      if (ptr == MAP_FAILED) { printf("FAIL mmap of exported fd at %p\n", want); return 1; }
      printf("opaque fd %d, size %lld\n", fd, (long long)end);
   } else
   CHECK(MapMemory2KHR(dev, &(VkMemoryMapInfoKHR){ VK_STRUCTURE_TYPE_MEMORY_MAP_INFO_KHR,
         &(VkMemoryMapPlacedInfoEXT){ VK_STRUCTURE_TYPE_MEMORY_MAP_PLACED_INFO_EXT, .pPlacedAddress = want },
         .flags = VK_MEMORY_MAP_PLACED_BIT_EXT, .memory = mem, .size = VK_WHOLE_SIZE }, &ptr));
   if (ptr != want) { printf("FAIL mapped at %p, asked %p\n", ptr, want); return 1; }
   memset(ptr, 0, size);

   VkCommandPool pool; VkCommandBuffer cb;
   CHECK(CreateCommandPool(dev, &(VkCommandPoolCreateInfo){ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO }, NULL, &pool));
   CHECK(AllocateCommandBuffers(dev, &(VkCommandBufferAllocateInfo){ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
         .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 }, &cb));
   CHECK(BeginCommandBuffer(cb, &(VkCommandBufferBeginInfo){ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO }));
   CmdFillBuffer(cb, buf, 0, size, 0x5a17c0de);
   CHECK(EndCommandBuffer(cb));
   CHECK(QueueSubmit(q, 1, &(VkSubmitInfo){ VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1,
         .pCommandBuffers = &cb }, VK_NULL_HANDLE));
   CHECK(QueueWaitIdle(q));

   const uint32_t *w = ptr; uint64_t bad = 0;
   for (uint64_t i = 0; i < size / 4; i++) bad += w[i] != 0x5a17c0de;
   printf("%s placed map at %p, %llu MiB, %llu of %llu words wrong\n", bad ? "FAIL" : "PASS", ptr,
          (unsigned long long)(size >> 20), (unsigned long long)bad, (unsigned long long)(size / 4));
   return bad != 0;
}

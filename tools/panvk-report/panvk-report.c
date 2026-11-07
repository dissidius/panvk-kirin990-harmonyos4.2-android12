/* panvk-report: collects what we need to look into a PanVK problem in
 * Winlator or GameNative, into one zip in Android's Download folder.
 *
 * Run it in the container (and settings) where the problem happens. It
 * gathers: Wine/Windows/host versions, RAM and CPUs, the container's
 * graphics-related environment (DXVK, Box64/FEX, Winlator wrapper, PanVK
 * options), the Vulkan device as Wine sees it (driver version, features,
 * extensions, limits, memory), what D3D11 (DXVK) creates, two GPU
 * self-tests, and the files panvk_env.txt, panvk_errors.txt (written by
 * the driver on GPU faults), Winlator's or GameNative's newest logs (lines
 * that may hold account data removed) and *.log files next
 * to the tool. Nothing leaves the phone: the user attaches the zip to an
 * issue at https://github.com/soulexit/panvk-kirin990/issues.
 *
 * Build: tools/panvk-report/build.sh (mingw-w64, x86-64: runs in x86-64
 * and ARM64EC containers).
 */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "shaders.h"

#define TOOL_VERSION "1.3"
#define ISSUES_URL "https://github.com/soulexit/panvk-kirin990/issues/new/choose"

/* Growing text buffer. */
struct buf {
   char *p;
   size_t len, cap;
};

static void
buf_add(struct buf *b, const void *data, size_t n)
{
   if (b->len + n + 1 > b->cap) {
      size_t cap = b->cap ? b->cap : 4096;
      while (cap < b->len + n + 1)
         cap *= 2;
      char *p = realloc(b->p, cap);
      if (!p)
         return;
      b->p = p;
      b->cap = cap;
   }
   memcpy(b->p + b->len, data, n);
   b->len += n;
   b->p[b->len] = 0;
}

static void
bprintf(struct buf *b, const char *fmt, ...)
{
   char tmp[4096];
   va_list ap;
   va_start(ap, fmt);
   int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
   va_end(ap);
   if (n > 0)
      buf_add(b, tmp, n < (int)sizeof(tmp) ? (size_t)n : sizeof(tmp) - 1);
}

static struct buf report; /* report.txt */
static struct buf hints;  /* "Hints" at the top of report.txt */

/* Set while collecting, read by collect_hints(). */
static char g_driver_name[64], g_driver_info[256];
static unsigned g_tests_failed;

#define R(...) bprintf(&report, __VA_ARGS__)

/* ---- Minimal zip writer (stored, no compression) ---- */

static uint32_t crc_table[256];

static void
crc_init(void)
{
   for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (int k = 0; k < 8; k++)
         c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
      crc_table[i] = c;
   }
}

static uint32_t
crc32(const uint8_t *p, size_t n)
{
   uint32_t c = 0xffffffffu;
   for (size_t i = 0; i < n; i++)
      c = crc_table[(c ^ p[i]) & 0xff] ^ (c >> 8);
   return c ^ 0xffffffffu;
}

struct zentry {
   char name[260];
   uint32_t crc, size, offset;
};

static struct zentry zentries[64];
static unsigned nzentries;
static struct buf zip;
static uint16_t zip_time, zip_date;

static void
put16(struct buf *b, uint16_t v)
{
   uint8_t x[2] = {v & 0xff, v >> 8};
   buf_add(b, x, 2);
}

static void
put32(struct buf *b, uint32_t v)
{
   uint8_t x[4] = {v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, v >> 24};
   buf_add(b, x, 4);
}

static void
zip_add(const char *name, const void *data, size_t size)
{
   if (nzentries == sizeof(zentries) / sizeof(zentries[0]))
      return;
   struct zentry *e = &zentries[nzentries++];
   snprintf(e->name, sizeof(e->name), "%s", name);
   e->crc = crc32(data, size);
   e->size = (uint32_t)size;
   e->offset = (uint32_t)zip.len;
   const uint16_t nlen = (uint16_t)strlen(e->name);
   put32(&zip, 0x04034b50);
   put16(&zip, 20);
   put16(&zip, 0x0800); /* UTF-8 names */
   put16(&zip, 0);      /* stored */
   put16(&zip, zip_time);
   put16(&zip, zip_date);
   put32(&zip, e->crc);
   put32(&zip, e->size);
   put32(&zip, e->size);
   put16(&zip, nlen);
   put16(&zip, 0);
   buf_add(&zip, e->name, nlen);
   buf_add(&zip, data, size);
}

static void
zip_finish(void)
{
   const uint32_t cd = (uint32_t)zip.len;
   for (unsigned i = 0; i < nzentries; i++) {
      struct zentry *e = &zentries[i];
      const uint16_t nlen = (uint16_t)strlen(e->name);
      put32(&zip, 0x02014b50);
      put16(&zip, 20);
      put16(&zip, 20);
      put16(&zip, 0x0800);
      put16(&zip, 0);
      put16(&zip, zip_time);
      put16(&zip, zip_date);
      put32(&zip, e->crc);
      put32(&zip, e->size);
      put32(&zip, e->size);
      put16(&zip, nlen);
      put16(&zip, 0);
      put16(&zip, 0);
      put16(&zip, 0);
      put16(&zip, 0);
      put32(&zip, 0);
      put32(&zip, e->offset);
      buf_add(&zip, e->name, nlen);
   }
   const uint32_t cdsize = (uint32_t)zip.len - cd;
   put32(&zip, 0x06054b50);
   put16(&zip, 0);
   put16(&zip, 0);
   put16(&zip, (uint16_t)nzentries);
   put16(&zip, (uint16_t)nzentries);
   put32(&zip, cdsize);
   put32(&zip, cd);
   put16(&zip, 0);
}

/* Reads up to `cap` bytes from the end of a file (logs: the end matters). */
static char *
read_file_tail(const char *path, size_t cap, size_t *out_len)
{
   FILE *f = fopen(path, "rb");
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   long size = ftell(f);
   if (size < 0) {
      fclose(f);
      return NULL;
   }
   size_t n = (size_t)size > cap ? cap : (size_t)size;
   fseek(f, size - (long)n, SEEK_SET);
   char *p = malloc(n + 1);
   if (p && fread(p, 1, n, f) != n) {
      free(p);
      p = NULL;
   }
   fclose(f);
   if (p) {
      p[n] = 0;
      *out_len = n;
   }
   return p;
}

/* Words that mark a log line as possibly holding account data (GameNative
 * logs Steam sign-in; any app's logcat can hold tokens). Lowercase. */
static const char *const secret_words[] = {
   "password", "passwd", "token", "login_key", "loginkey", "accountname",
   "account_name", "steamguard", "auth_code", "authcode", "cookie", "bearer",
   "secret",
};

static bool
line_has_secret(const char *s, size_t n)
{
   char low[512];
   for (size_t off = 0; off < n; off += sizeof(low) - 32) {
      /* Overlapping windows so a word split by a window edge is still seen. */
      size_t m = n - off < sizeof(low) - 1 ? n - off : sizeof(low) - 1;
      for (size_t i = 0; i < m; i++) {
         char c = s[off + i];
         low[i] = c >= 'A' && c <= 'Z' ? c + 32 : c ? c : ' ';
      }
      low[m] = 0;
      for (unsigned w = 0; w < sizeof(secret_words) / sizeof(secret_words[0]); w++)
         if (strstr(low, secret_words[w]))
            return true;
      if (m < sizeof(low) - 1)
         break;
   }
   return false;
}

/* Copy of p without lines that may hold account data, with SteamID64s (17
 * digits starting 7656119) masked. */
static struct buf
scrub(const char *p, size_t n, unsigned *removed)
{
   static const char cut[] = "[line removed by panvk-report: may hold account data]\n";
   struct buf out = {0};
   size_t i = 0;
   while (i < n) {
      const char *nl = memchr(p + i, '\n', n - i);
      size_t len = nl ? (size_t)(nl - (p + i)) + 1 : n - i;
      if (line_has_secret(p + i, len)) {
         buf_add(&out, cut, sizeof(cut) - 1);
         (*removed)++;
      } else {
         for (size_t k = 0; k < len; k++) {
            const char *q = p + i + k;
            if (len - k >= 17 && !memcmp(q, "7656119", 7) &&
                (k == 0 || q[-1] < '0' || q[-1] > '9')) {
               size_t d = 7;
               while (d < len - k && q[d] >= '0' && q[d] <= '9')
                  d++;
               if (d == 17) {
                  buf_add(&out, "<steamid>", 9);
                  k += 16;
                  continue;
               }
            }
            buf_add(&out, q, 1);
         }
      }
      i += len;
   }
   return out;
}

static bool
add_file(const char *zipname, const char *path, size_t cap)
{
   size_t n;
   char *p = read_file_tail(path, cap, &n);
   if (!p)
      return false;
   unsigned removed = 0;
   struct buf clean = scrub(p, n, &removed);
   free(p);
   zip_add(zipname, clean.p ? clean.p : "", clean.len);
   free(clean.p);
   R("  added %s (%zu bytes) from %s", zipname, clean.len, path);
   if (removed)
      R(", %u line(s) removed as possible account data", removed);
   R("\n");
   return true;
}

/* ---- Wine helpers ---- */

typedef const char *(CDECL *wine_get_version_t)(void);
typedef void(CDECL *wine_get_host_version_t)(const char **, const char **);
typedef WCHAR *(CDECL *wine_get_dos_file_name_t)(const char *);

static wine_get_dos_file_name_t dos_name_fn;

/* DOS path of a Linux/Android path, "" if Wine doesn't map it. */
static void
dos_path(const char *unix_path, char *out, size_t size)
{
   out[0] = 0;
   if (!dos_name_fn)
      return;
   WCHAR *w = dos_name_fn(unix_path);
   if (!w)
      return;
   WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)size, NULL, NULL);
   HeapFree(GetProcessHeap(), 0, w);
}

static bool
dir_exists(const char *path)
{
   DWORD a = GetFileAttributesA(path);
   return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

/* A folder in Android's shared storage (relative to /sdcard) as a DOS path. */
static bool
find_android_dir(const char *rel, char *out, size_t size)
{
   static const char *const roots[] = {"/storage/emulated/0", "/sdcard"};
   for (unsigned i = 0; i < 2; i++) {
      char u[MAX_PATH];
      snprintf(u, sizeof(u), "%s/%s", roots[i], rel);
      dos_path(u, out, size);
      if (out[0] && dir_exists(out))
         return true;
   }
   out[0] = 0;
   return false;
}

/* Android's Download folder as a DOS path. */
static bool
find_download(char *out, size_t size)
{
   static const char *const cands[] = {"/storage/emulated/0/Download",
                                       "/sdcard/Download"};
   for (unsigned i = 0; i < 2; i++) {
      dos_path(cands[i], out, size);
      if (out[0] && dir_exists(out))
         return true;
   }
   /* Winlator maps D: to Download. */
   snprintf(out, size, "D:\\");
   return dir_exists(out);
}

static void
section(const char *title)
{
   R("\n=== %s ===\n", title);
}

enum host_app { APP_UNKNOWN, APP_WINLATOR, APP_GAMENATIVE };

/* The Android app the container runs in, from its data paths in the
 * environment (/data/user/0/<package>/...). */
static enum host_app
host_app(void)
{
   static const char *const vars[] = {"WINE_HOST_PATH", "WINEPREFIX", "WINE_HOST_HOME"};
   for (unsigned i = 0; i < sizeof(vars) / sizeof(vars[0]); i++) {
      const char *v = getenv(vars[i]);
      if (!v)
         continue;
      if (strstr(v, "/app.gamenative/"))
         return APP_GAMENATIVE;
      if (strstr(v, "/com.winlator"))
         return APP_WINLATOR;
   }
   return APP_UNKNOWN;
}

static void
collect_system(void)
{
   section("System");
   HMODULE ntdll = GetModuleHandleA("ntdll.dll");
   wine_get_version_t wv =
      (wine_get_version_t)GetProcAddress(ntdll, "wine_get_version");
   wine_get_host_version_t hv =
      (wine_get_host_version_t)GetProcAddress(ntdll, "wine_get_host_version");
   wine_get_version_t wb =
      (wine_get_version_t)GetProcAddress(ntdll, "wine_get_build_id");
   R("Wine: %s\n", wv ? wv() : "(not Wine)");
   if (wb)
      R("Wine build: %s\n", wb());
   if (hv) {
      const char *sys = NULL, *rel = NULL;
      hv(&sys, &rel);
      R("Host: %s %s\n", sys ? sys : "?", rel ? rel : "?");
   }
   OSVERSIONINFOA ov = {sizeof(ov)};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
   if (GetVersionExA(&ov))
      R("Windows version reported: %lu.%lu build %lu\n", ov.dwMajorVersion,
        ov.dwMinorVersion, ov.dwBuildNumber);
#pragma GCC diagnostic pop
   SYSTEM_INFO si;
   GetNativeSystemInfo(&si);
   R("CPUs: %lu, architecture %u (9 = x86-64, 12 = ARM64)\n",
     si.dwNumberOfProcessors, si.wProcessorArchitecture);
   MEMORYSTATUSEX ms = {sizeof(ms)};
   if (GlobalMemoryStatusEx(&ms))
      R("RAM: %llu MiB total, %llu MiB free\n", ms.ullTotalPhys >> 20,
        ms.ullAvailPhys >> 20);
   char exe[MAX_PATH];
   GetModuleFileNameA(NULL, exe, sizeof(exe));
   static const char *const app_names[] = {"unknown", "Winlator", "GameNative"};
   R("App: %s\n", app_names[host_app()]);
   R("Tool: panvk-report %s at %s\n", TOOL_VERSION, exe);
}

/* Environment variables that describe the graphics setup. */
static void
collect_env(void)
{
   section("Container environment (graphics-related)");
   static const char *const prefixes[] = {
      "DXVK_",  "VKD3D_", "WINE",   "BOX64_",  "BOX86_",   "FEX",
      "MESA_",  "PANVK_", "PAN_",   "BIFROST_", "WRAPPER_", "GALLIUM_",
      "ZINK_",  "LIBGL_", "MANGOHUD", "VK_",    "ENABLE_",  "DISABLE_",
      "LD_PRELOAD", "ADRENOTOOLS", "heap_memory_percent",
   };
   char *env = GetEnvironmentStringsA();
   unsigned n = 0;
   for (char *e = env; e && *e; e += strlen(e) + 1) {
      bool want = false;
      for (unsigned i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++)
         want = want || !_strnicmp(e, prefixes[i], strlen(prefixes[i]));
      /* Never anything that looks like a secret. */
      if (strstr(e, "TOKEN") || strstr(e, "PASSWORD") || strstr(e, "SECRET"))
         want = false;
      if (want) {
         R("%s\n", e);
         n++;
      }
   }
   if (!n)
      R("(none set)\n");
   FreeEnvironmentStringsA(env);
}

/* ---- Vulkan ---- */

static PFN_vkGetInstanceProcAddr gipa;
#define VKI(inst, name) ((PFN_##name)gipa((inst), #name))

static const char *
vk_result_str(VkResult r)
{
   switch (r) {
   case VK_SUCCESS: return "VK_SUCCESS";
   case VK_TIMEOUT: return "VK_TIMEOUT";
   case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
   case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
   case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
   case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
   case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
   case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
   case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
   default: return "other";
   }
}

/* Runs one compute shader writing `count` uints and checks the result with
 * `expect`. A 5 s fence timeout turns a GPU hang into a result. */
static void
gpu_test(VkInstance inst, VkPhysicalDevice pd, uint32_t qfam, const char *name,
         const uint32_t *spv, size_t spv_size, uint32_t groups, uint32_t count,
         uint32_t (*expect)(uint32_t))
{
   PFN_vkCreateDevice createDevice = VKI(inst, vkCreateDevice);
   PFN_vkGetDeviceProcAddr gdpa = VKI(inst, vkGetDeviceProcAddr);
   PFN_vkGetPhysicalDeviceMemoryProperties memProps =
      VKI(inst, vkGetPhysicalDeviceMemoryProperties);

   float prio = 1.0f;
   VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                  .queueFamilyIndex = qfam, .queueCount = 1,
                                  .pQueuePriorities = &prio};
   VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                             .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci};
   VkDevice dev;
   VkResult r = createDevice(pd, &dci, NULL, &dev);
   if (r != VK_SUCCESS) {
      R("GPU test %s: vkCreateDevice failed: %s\n", name, vk_result_str(r));
      return;
   }
#define VKD(n) PFN_##n n = (PFN_##n)gdpa(dev, #n)
   VKD(vkGetDeviceQueue); VKD(vkCreateBuffer); VKD(vkGetBufferMemoryRequirements);
   VKD(vkAllocateMemory); VKD(vkBindBufferMemory); VKD(vkMapMemory);
   VKD(vkCreateShaderModule); VKD(vkCreateDescriptorSetLayout);
   VKD(vkCreatePipelineLayout); VKD(vkCreateComputePipelines);
   VKD(vkCreateDescriptorPool); VKD(vkAllocateDescriptorSets);
   VKD(vkUpdateDescriptorSets); VKD(vkCreateCommandPool);
   VKD(vkAllocateCommandBuffers); VKD(vkBeginCommandBuffer);
   VKD(vkCmdBindPipeline); VKD(vkCmdBindDescriptorSets); VKD(vkCmdDispatch);
   VKD(vkEndCommandBuffer); VKD(vkCreateFence); VKD(vkQueueSubmit);
   VKD(vkWaitForFences); VKD(vkDestroyDevice); VKD(vkDeviceWaitIdle);
#undef VKD
   VkQueue q;
   vkGetDeviceQueue(dev, qfam, 0, &q);

   const VkDeviceSize size = (VkDeviceSize)count * 4;
   VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size,
                             .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT};
   VkBuffer buf;
   vkCreateBuffer(dev, &bci, NULL, &buf);
   VkMemoryRequirements mr;
   vkGetBufferMemoryRequirements(dev, buf, &mr);
   VkPhysicalDeviceMemoryProperties mp;
   memProps(pd, &mp);
   uint32_t mt = UINT32_MAX;
   for (uint32_t i = 0; i < mp.memoryTypeCount && mt == UINT32_MAX; i++)
      if ((mr.memoryTypeBits & (1u << i)) &&
          (mp.memoryTypes[i].propertyFlags &
           (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
         mt = i;
   VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                               .allocationSize = mr.size, .memoryTypeIndex = mt};
   VkDeviceMemory mem;
   if (mt == UINT32_MAX || vkAllocateMemory(dev, &mai, NULL, &mem) != VK_SUCCESS) {
      R("GPU test %s: no host-visible coherent memory\n", name);
      vkDestroyDevice(dev, NULL);
      return;
   }
   vkBindBufferMemory(dev, buf, mem, 0);
   uint32_t *ptr;
   vkMapMemory(dev, mem, 0, size, 0, (void **)&ptr);
   memset(ptr, 0xff, size);

   VkShaderModuleCreateInfo smci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                    .codeSize = spv_size, .pCode = spv};
   VkShaderModule sm;
   vkCreateShaderModule(dev, &smci, NULL, &sm);
   VkDescriptorSetLayoutBinding b = {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                     VK_SHADER_STAGE_COMPUTE_BIT, NULL};
   VkDescriptorSetLayoutCreateInfo dl = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1,
      .pBindings = &b};
   VkDescriptorSetLayout sl;
   vkCreateDescriptorSetLayout(dev, &dl, NULL, &sl);
   VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                      .setLayoutCount = 1, .pSetLayouts = &sl};
   VkPipelineLayout pl;
   vkCreatePipelineLayout(dev, &plci, NULL, &pl);
   VkComputePipelineCreateInfo cpci = {
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = sm, .pName = "main"},
      .layout = pl};
   VkPipeline pipe;
   r = vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, NULL, &pipe);
   if (r != VK_SUCCESS) {
      R("GPU test %s: pipeline creation failed: %s\n", name, vk_result_str(r));
      vkDestroyDevice(dev, NULL);
      return;
   }
   VkDescriptorPoolSize ps = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
   VkDescriptorPoolCreateInfo dpci = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                      .maxSets = 1, .poolSizeCount = 1,
                                      .pPoolSizes = &ps};
   VkDescriptorPool dp;
   vkCreateDescriptorPool(dev, &dpci, NULL, &dp);
   VkDescriptorSetAllocateInfo dsai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                       .descriptorPool = dp, .descriptorSetCount = 1,
                                       .pSetLayouts = &sl};
   VkDescriptorSet set;
   vkAllocateDescriptorSets(dev, &dsai, &set);
   VkDescriptorBufferInfo dbi = {buf, 0, size};
   VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set,
                             .descriptorCount = 1,
                             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                             .pBufferInfo = &dbi};
   vkUpdateDescriptorSets(dev, 1, &w, 0, NULL);
   VkCommandPoolCreateInfo cpi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                  .queueFamilyIndex = qfam};
   VkCommandPool cp;
   vkCreateCommandPool(dev, &cpi, NULL, &cp);
   VkCommandBufferAllocateInfo cbai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                       .commandPool = cp,
                                       .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                       .commandBufferCount = 1};
   VkCommandBuffer cb;
   vkAllocateCommandBuffers(dev, &cbai, &cb);
   VkCommandBufferBeginInfo cbbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
   vkBeginCommandBuffer(cb, &cbbi);
   vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
   vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &set, 0,
                           NULL);
   vkCmdDispatch(cb, groups, 1, 1);
   vkEndCommandBuffer(cb);
   VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
   VkFence fence;
   vkCreateFence(dev, &fci, NULL, &fence);
   VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1,
                      .pCommandBuffers = &cb};
   const DWORD t0 = GetTickCount();
   r = vkQueueSubmit(q, 1, &si, fence);
   if (r == VK_SUCCESS)
      r = vkWaitForFences(dev, 1, &fence, VK_TRUE, 5000000000ull);
   const DWORD ms = GetTickCount() - t0;
   if (r != VK_SUCCESS) {
      R("GPU test %s: FAIL, %s after %lu ms (GPU hang or lost device)\n", name,
        vk_result_str(r), ms);
      g_tests_failed++;
   } else {
      uint32_t bad = 0, first = UINT32_MAX;
      for (uint32_t i = 0; i < count; i++)
         if (ptr[i] != expect(i)) {
            if (first == UINT32_MAX)
               first = i;
            bad++;
         }
      if (bad) {
         R("GPU test %s: FAIL, %u of %u results wrong (first at %u: %u, "
           "expected %u), %lu ms\n",
           name, bad, count, first, ptr[first], expect(first), ms);
         g_tests_failed++;
      }
      else
         R("GPU test %s: PASS (%u results, %lu ms)\n", name, count, ms);
      vkDeviceWaitIdle(dev);
   }
   vkDestroyDevice(dev, NULL);
}

static uint32_t expect_simple(uint32_t i) { return i * 3u + 1u; }
static uint32_t expect_largewg(uint32_t i)
{
   const uint32_t lid = i % 512u, wg = i / 512u;
   return lid * 7u + wg + (511u - lid);
}

static void
collect_vulkan(struct buf *exts_out)
{
   section("Vulkan (as Wine sees it)");
   HMODULE vk = LoadLibraryA("vulkan-1.dll");
   if (!vk) {
      R("vulkan-1.dll not found: the container has no Vulkan.\n");
      return;
   }
   gipa = (PFN_vkGetInstanceProcAddr)GetProcAddress(vk, "vkGetInstanceProcAddr");
   if (!gipa) {
      R("vkGetInstanceProcAddr missing in vulkan-1.dll\n");
      return;
   }
   PFN_vkCreateInstance createInstance = VKI(NULL, vkCreateInstance);
   PFN_vkEnumerateInstanceVersion instVersion = VKI(NULL, vkEnumerateInstanceVersion);
   uint32_t iv = VK_API_VERSION_1_0;
   if (instVersion)
      instVersion(&iv);
   R("Instance version: %u.%u.%u\n", VK_API_VERSION_MAJOR(iv),
     VK_API_VERSION_MINOR(iv), VK_API_VERSION_PATCH(iv));
   VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO,
                            .pApplicationName = "panvk-report",
                            .apiVersion = iv >= VK_API_VERSION_1_1 ? VK_API_VERSION_1_1
                                                                   : VK_API_VERSION_1_0};
   VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                               .pApplicationInfo = &app};
   VkInstance inst;
   VkResult r = createInstance(&ici, NULL, &inst);
   if (r != VK_SUCCESS) {
      R("vkCreateInstance failed: %s\n", vk_result_str(r));
      return;
   }
   PFN_vkEnumeratePhysicalDevices enumPd = VKI(inst, vkEnumeratePhysicalDevices);
   PFN_vkGetPhysicalDeviceProperties2 props2 = VKI(inst, vkGetPhysicalDeviceProperties2);
   PFN_vkGetPhysicalDeviceProperties props1 = VKI(inst, vkGetPhysicalDeviceProperties);
   PFN_vkGetPhysicalDeviceFeatures feats = VKI(inst, vkGetPhysicalDeviceFeatures);
   PFN_vkEnumerateDeviceExtensionProperties devExts =
      VKI(inst, vkEnumerateDeviceExtensionProperties);
   PFN_vkGetPhysicalDeviceMemoryProperties memProps =
      VKI(inst, vkGetPhysicalDeviceMemoryProperties);
   PFN_vkGetPhysicalDeviceQueueFamilyProperties qfp =
      VKI(inst, vkGetPhysicalDeviceQueueFamilyProperties);

   uint32_t n = 0;
   enumPd(inst, &n, NULL);
   R("Physical devices: %u\n", n);
   VkPhysicalDevice pds[8];
   if (n > 8)
      n = 8;
   enumPd(inst, &n, pds);
   for (uint32_t d = 0; d < n; d++) {
      VkPhysicalDeviceProperties p;
      VkPhysicalDeviceDriverProperties drv = {
         VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
      if (props2) {
         VkPhysicalDeviceProperties2 p2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                                           .pNext = &drv};
         props2(pds[d], &p2);
         p = p2.properties;
      } else {
         props1(pds[d], &p);
      }
      R("\n[device %u] %s\n", d, p.deviceName);
      R("  API %u.%u.%u, driverVersion 0x%x, vendor 0x%04x, device 0x%04x, type %d\n",
        VK_API_VERSION_MAJOR(p.apiVersion), VK_API_VERSION_MINOR(p.apiVersion),
        VK_API_VERSION_PATCH(p.apiVersion), p.driverVersion, p.vendorID,
        p.deviceID, p.deviceType);
      R("  driverName: %s\n  driverInfo: %s\n", drv.driverName, drv.driverInfo);
      if (!g_driver_name[0]) {
         snprintf(g_driver_name, sizeof(g_driver_name), "%s", drv.driverName);
         snprintf(g_driver_info, sizeof(g_driver_info), "%s", drv.driverInfo);
      }
      R("  conformance: %u.%u.%u.%u\n", drv.conformanceVersion.major,
        drv.conformanceVersion.minor, drv.conformanceVersion.subminor,
        drv.conformanceVersion.patch);
      const VkPhysicalDeviceLimits *l = &p.limits;
      R("  limits: maxImageDimension2D %u, maxComputeWorkGroupInvocations %u, "
        "maxComputeSharedMemorySize %u, maxBoundDescriptorSets %u, "
        "maxPushConstantsSize %u, maxUniformBufferRange %u, "
        "maxStorageBufferRange %u, maxViewports %u, maxColorAttachments %u\n",
        l->maxImageDimension2D, l->maxComputeWorkGroupInvocations,
        l->maxComputeSharedMemorySize, l->maxBoundDescriptorSets,
        l->maxPushConstantsSize, l->maxUniformBufferRange,
        l->maxStorageBufferRange, l->maxViewports, l->maxColorAttachments);

      VkPhysicalDeviceFeatures f;
      feats(pds[d], &f);
      R("  features: geometryShader %u tessellationShader %u multiDrawIndirect %u "
        "drawIndirectFirstInstance %u depthClamp %u fillModeNonSolid %u "
        "samplerAnisotropy %u textureCompressionBC %u textureCompressionETC2 %u "
        "shaderFloat64 %u shaderInt64 %u robustBufferAccess %u "
        "fragmentStoresAndAtomics %u vertexPipelineStoresAndAtomics %u "
        "dualSrcBlend %u logicOp %u independentBlend %u\n",
        f.geometryShader, f.tessellationShader, f.multiDrawIndirect,
        f.drawIndirectFirstInstance, f.depthClamp, f.fillModeNonSolid,
        f.samplerAnisotropy, f.textureCompressionBC, f.textureCompressionETC2,
        f.shaderFloat64, f.shaderInt64, f.robustBufferAccess,
        f.fragmentStoresAndAtomics, f.vertexPipelineStoresAndAtomics,
        f.dualSrcBlend, f.logicOp, f.independentBlend);

      VkPhysicalDeviceMemoryProperties mp;
      memProps(pds[d], &mp);
      for (uint32_t h = 0; h < mp.memoryHeapCount; h++)
         R("  heap %u: %llu MiB, flags 0x%x\n", h,
           (unsigned long long)(mp.memoryHeaps[h].size >> 20),
           mp.memoryHeaps[h].flags);
      for (uint32_t t = 0; t < mp.memoryTypeCount; t++)
         R("  memory type %u: heap %u, flags 0x%x\n", t,
           mp.memoryTypes[t].heapIndex, mp.memoryTypes[t].propertyFlags);

      uint32_t ne = 0;
      devExts(pds[d], NULL, &ne, NULL);
      VkExtensionProperties *ep = calloc(ne ? ne : 1, sizeof(*ep));
      devExts(pds[d], NULL, &ne, ep);
      R("  device extensions: %u (listed in vulkan-extensions.txt)\n", ne);
      bprintf(exts_out, "[device %u] %s, %u extensions\n", d, p.deviceName, ne);
      for (uint32_t e = 0; e < ne; e++)
         bprintf(exts_out, "%s %u\n", ep[e].extensionName, ep[e].specVersion);
      free(ep);

      uint32_t nq = 0;
      qfp(pds[d], &nq, NULL);
      VkQueueFamilyProperties qp[16];
      if (nq > 16)
         nq = 16;
      qfp(pds[d], &nq, qp);
      uint32_t compute_q = UINT32_MAX;
      for (uint32_t i = 0; i < nq; i++) {
         R("  queue family %u: flags 0x%x, %u queues\n", i, qp[i].queueFlags,
           qp[i].queueCount);
         if (compute_q == UINT32_MAX && (qp[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
            compute_q = i;
      }

      if (compute_q != UINT32_MAX) {
         gpu_test(inst, pds[d], compute_q, "compute", simple_spv, sizeof(simple_spv_bytes),
                  256, 256 * 64, expect_simple);
         gpu_test(inst, pds[d], compute_q, "large-workgroup", largewg_spv,
                  sizeof(largewg_spv_bytes), 32, 32 * 512, expect_largewg);
      }
   }
   PFN_vkDestroyInstance destroyInstance = VKI(inst, vkDestroyInstance);
   destroyInstance(inst, NULL);
}

/* ---- D3D11 / DXGI (DXVK) ---- */

typedef HRESULT(WINAPI *d3d11_create_t)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE,
                                        UINT, const D3D_FEATURE_LEVEL *, UINT, UINT,
                                        ID3D11Device **, D3D_FEATURE_LEVEL *,
                                        ID3D11DeviceContext **);

static void
collect_d3d11(const char *logdir)
{
   section("Direct3D 11 (DXVK or WineD3D)");
   /* DXVK writes its log (version, adapter, extensions) here. */
   SetEnvironmentVariableA("DXVK_LOG_LEVEL", "info");
   SetEnvironmentVariableA("DXVK_LOG_PATH", logdir);
   HMODULE d3d = LoadLibraryA("d3d11.dll");
   if (!d3d) {
      R("d3d11.dll not found\n");
      return;
   }
   char path[MAX_PATH];
   GetModuleFileNameA(d3d, path, sizeof(path));
   R("d3d11.dll: %s\n", path);
   d3d11_create_t create = (d3d11_create_t)GetProcAddress(d3d, "D3D11CreateDevice");
   if (!create) {
      R("D3D11CreateDevice missing\n");
      return;
   }
   ID3D11Device *dev = NULL;
   ID3D11DeviceContext *ctx = NULL;
   D3D_FEATURE_LEVEL fl = 0;
   HRESULT hr = create(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
                       D3D11_SDK_VERSION, &dev, &fl, &ctx);
   if (FAILED(hr)) {
      R("D3D11CreateDevice failed: 0x%08lx\n", (unsigned long)hr);
      return;
   }
   R("D3D11 device created, feature level %x.%x\n", (fl >> 12) & 0xf, (fl >> 8) & 0xf);
   IDXGIDevice *xdev = NULL;
   if (SUCCEEDED(ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void **)&xdev))) {
      IDXGIAdapter *ad = NULL;
      if (SUCCEEDED(IDXGIDevice_GetAdapter(xdev, &ad))) {
         DXGI_ADAPTER_DESC desc;
         if (SUCCEEDED(IDXGIAdapter_GetDesc(ad, &desc))) {
            char name[256];
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name,
                                sizeof(name), NULL, NULL);
            R("Adapter: %s, vendor 0x%04x, device 0x%04x, video memory %llu MiB, "
              "shared %llu MiB\n",
              name, desc.VendorId, desc.DeviceId,
              (unsigned long long)(desc.DedicatedVideoMemory >> 20),
              (unsigned long long)(desc.SharedSystemMemory >> 20));
         }
         IDXGIAdapter_Release(ad);
      }
      IDXGIDevice_Release(xdev);
   }
   ID3D11DeviceContext_Release(ctx);
   ID3D11Device_Release(dev);

   /* DXVK names its version at the top of its dxgi log (the d3d11 log in
    * some versions). */
   static const char *const logs[] = {"panvk-report_dxgi.log",
                                      "panvk-report_d3d11.log"};
   bool found = false, any = false;
   for (unsigned i = 0; i < 2 && !found; i++) {
      char logp[MAX_PATH];
      snprintf(logp, sizeof(logp), "%s\\%s", logdir, logs[i]);
      size_t n;
      char *log = read_file_tail(logp, 1 << 20, &n);
      if (!log)
         continue;
      any = true;
      char *v = strstr(log, "DXVK: v");
      if (v) {
         char *e = strpbrk(v, "\r\n");
         if (e)
            *e = 0;
         R("%s\n", v);
         found = true;
      }
      free(log);
   }
   if (!any)
      R("No DXVK log: d3d11.dll is probably WineD3D, not DXVK\n");
}

/* ---- Files ---- */

struct found {
   char path[MAX_PATH];
   FILETIME t;
};

static int
newest_first(const void *a, const void *b)
{
   return -CompareFileTime(&((const struct found *)a)->t,
                           &((const struct found *)b)->t);
}

/* Adds the `max` newest files matching dir\pattern under zipdir/. */
static unsigned
add_newest(const char *dir, const char *pattern, const char *zipdir, unsigned max,
           size_t cap)
{
   /* No trailing backslash in the joins below ("D:\\" -> "D:"). */
   char d[MAX_PATH];
   snprintf(d, sizeof(d), "%s", dir);
   size_t dl = strlen(d);
   if (dl && d[dl - 1] == '\\')
      d[dl - 1] = 0;
   dir = d;
   char glob[MAX_PATH];
   snprintf(glob, sizeof(glob), "%s\\%s", dir, pattern);
   WIN32_FIND_DATAA fd;
   HANDLE h = FindFirstFileA(glob, &fd);
   if (h == INVALID_HANDLE_VALUE)
      return 0;
   static struct found list[256];
   unsigned n = 0;
   do {
      if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && n < 256) {
         snprintf(list[n].path, MAX_PATH, "%s\\%s", dir, fd.cFileName);
         list[n].t = fd.ftLastWriteTime;
         n++;
      }
   } while (FindNextFileA(h, &fd));
   FindClose(h);
   qsort(list, n, sizeof(list[0]), newest_first);
   unsigned added = 0;
   for (unsigned i = 0; i < n && added < max; i++) {
      const char *base = strrchr(list[i].path, '\\');
      char zn[300];
      snprintf(zn, sizeof(zn), "%s/%s", zipdir, base ? base + 1 : list[i].path);
      added += add_file(zn, list[i].path, cap);
   }
   return added;
}

static void
collect_files(const char *download, const char *logdir)
{
   section("Files");
   char p[MAX_PATH];
   snprintf(p, sizeof(p), "%s\\panvk_env.txt", download);
   if (!add_file("panvk_env.txt", p, 64 << 10))
      R("  no panvk_env.txt in Download\n");
   snprintf(p, sizeof(p), "%s\\panvk_errors.txt", download);
   if (!add_file("panvk_errors.txt", p, 1 << 20))
      R("  no panvk_errors.txt in Download (no GPU faults recorded, or a "
        "driver older than 1.2.9)\n");

   /* Only the logs of the app the tool runs in (a phone can have both). */
   const enum host_app app = host_app();
   char wl[MAX_PATH];
   const bool winlator = app != APP_GAMENATIVE &&
                         find_android_dir("Winlator/logs", wl, sizeof(wl));
   if (winlator && !add_newest(wl, "*.txt", "winlator-logs", 3, 2 << 20))
      R("  no Winlator logs (turn on logging in Winlator's settings)\n");

   /* GameNative: Wine's log, crash logs, and the app log it saves to
    * Download ("Save logs"). */
   char gn[MAX_PATH];
   const bool gamenative =
      app != APP_WINLATOR &&
      find_android_dir("Android/data/app.gamenative/files", gn, sizeof(gn));
   if (gamenative) {
      snprintf(p, sizeof(p), "%s\\wine_logs", gn);
      unsigned n = add_newest(p, "*.log", "gamenative-logs/wine", 2, 2 << 20);
      snprintf(p, sizeof(p), "%s\\crash_logs", gn);
      n += add_newest(p, "*.txt", "gamenative-logs/crash", 2, 1 << 20);
      n += add_newest(download, "app_logs_*.txt", "gamenative-logs/app", 1, 2 << 20);
      if (!n)
         R("  no GameNative logs (turn on logging in GameNative's settings)\n");
   }
   if (!winlator && !gamenative)
      R("  no Winlator or GameNative log folder reachable\n");

   char cwd[MAX_PATH];
   GetCurrentDirectoryA(sizeof(cwd), cwd);
   add_newest(cwd, "*.log", "local-logs", 5, 2 << 20);
   add_newest(logdir, "*.log", "dxvk-logs", 3, 1 << 20);
}

static const char *
env_or(const char *name, const char *def)
{
   const char *v = getenv(name);
   return v && v[0] ? v : def;
}

/* Plain-language notes on what in this setup is known to matter. */
static void
collect_hints(bool have_errors)
{
#define H(...) bprintf(&hints, __VA_ARGS__)
   H("\n=== Hints ===\n");
   unsigned n = 0;
   if (!g_driver_name[0]) {
      H("- No Vulkan driver answered: the container has no working Vulkan.\n");
      n++;
   } else if (strcmp(g_driver_name, "panvk")) {
      H("- This container does not use PanVK (driver: %s, %s). Select the PanVK "
        "driver in the container's Graphics Driver setting.\n",
        g_driver_name, g_driver_info);
      n++;
   } else {
      H("- Driver in use: %s. Is it the version you meant to test?\n", g_driver_info);
   }
   if (g_tests_failed) {
      H("- %u GPU self-test(s) FAILED: the driver is not working correctly in this "
        "container, before any game is involved. Make sure the newest driver "
        "is selected, and mention this in the issue.\n", g_tests_failed);
      n++;
   }
   const char *bcn = env_or("WRAPPER_EMULATE_BCN", "0");
   if (strcmp(bcn, "0")) {
      H("- BCn emulation is on (WRAPPER_EMULATE_BCN=%s). PanVK decodes BC "
        "textures itself: set Winlator's BCn emulation to None (on, textures "
        "can be wrong and loading is slower).\n", bcn);
      n++;
   }
   const char *strong = env_or("BOX64_DYNAREC_STRONGMEM", "0");
   if (strcmp(strong, "0")) {
      H("- Box64 compatibility mode is on (BOX64_DYNAREC_STRONGMEM=%s): much "
        "slower on the CPU. Use the Performance preset unless a game needs it.\n",
        strong);
      n++;
   }
   const char *wsi = env_or("MESA_VK_WSI_DEBUG", "");
   if (strstr(wsi, "forcesync") || strstr(wsi, "sw")) {
      H("- Present is synchronous (MESA_VK_WSI_DEBUG=%s, Winlator's \"Sync "
        "frame\" on or \"Use DRI3\" off): every frame waits for the GPU.\n", wsi);
      n++;
   }
   if (have_errors) {
      H("- panvk_errors.txt is included: the driver recorded GPU errors (see "
        "its header lines for when and in which program).\n");
      n++;
   }
   if (!n)
      H("- Nothing unusual found in the settings.\n");
#undef H
}

int
main(void)
{
   crc_init();
   time_t now = time(NULL);
   struct tm *tm = localtime(&now);
   zip_time = (uint16_t)((tm->tm_hour << 11) | (tm->tm_min << 5) | (tm->tm_sec / 2));
   zip_date = (uint16_t)(((tm->tm_year - 80) << 9) | ((tm->tm_mon + 1) << 5) |
                         tm->tm_mday);
   char stamp[32];
   strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", tm);

   HMODULE k32 = GetModuleHandleA("kernel32.dll");
   dos_name_fn = (wine_get_dos_file_name_t)GetProcAddress(k32, "wine_get_dos_file_name");

   printf("panvk-report %s: collecting, this takes a few seconds...\n", TOOL_VERSION);
   R("panvk-report %s, %s\n", TOOL_VERSION, stamp);
   R("Attach this zip to an issue: %s\n", ISSUES_URL);

   char download[MAX_PATH];
   const bool have_download = find_download(download, sizeof(download));
   /* Joined as "%s\\name" below: no trailing backslash ("D:" stays valid). */
   size_t dl = strlen(download);
   if (dl && download[dl - 1] == '\\')
      download[dl - 1] = 0;

   char logdir[MAX_PATH];
   GetTempPathA(sizeof(logdir), logdir);
   strncat(logdir, "panvk-report-logs", sizeof(logdir) - strlen(logdir) - 1);
   CreateDirectoryA(logdir, NULL);

   collect_system();
   collect_env();
   struct buf exts = {0};
   collect_vulkan(&exts);
   collect_d3d11(logdir);
   collect_files(have_download ? download : ".", logdir);

   bool have_errors = false;
   for (unsigned i = 0; i < nzentries; i++)
      have_errors = have_errors || !strcmp(zentries[i].name, "panvk_errors.txt");
   collect_hints(have_errors);
   /* Hints go right after the first two lines of report.txt. */
   struct buf full = {0};
   const char *after = report.p;
   for (int line = 0; line < 2 && after && *after; line++) {
      const char *nl = strchr(after, '\n');
      after = nl ? nl + 1 : after + strlen(after);
   }
   buf_add(&full, report.p, after - report.p);
   if (hints.len)
      buf_add(&full, hints.p, hints.len);
   buf_add(&full, after, report.len - (after - report.p));
   zip_add("report.txt", full.p, full.len);
   if (exts.len)
      zip_add("vulkan-extensions.txt", exts.p, exts.len);
   zip_finish();

   char out[MAX_PATH];
   if (have_download)
      snprintf(out, sizeof(out), "%s\\panvk-report-%s.zip", download, stamp);
   else
      snprintf(out, sizeof(out), "panvk-report-%s.zip", stamp);
   FILE *f = fopen(out, "wb");
   if (!f) {
      snprintf(out, sizeof(out), "panvk-report-%s.zip", stamp);
      f = fopen(out, "wb");
   }
   bool ok = f && fwrite(zip.p, 1, zip.len, f) == zip.len;
   if (f)
      fclose(f);

   char msg[1024];
   if (ok)
      snprintf(msg, sizeof(msg),
               "Saved %s\n(%s)\n\nPlease attach this zip to a new issue:\n%s\n\n"
               "Say which game, what goes wrong and when.",
               strrchr(out, '\\') ? strrchr(out, '\\') + 1 : out,
               have_download ? "in Android's Download folder" : "next to this tool",
               ISSUES_URL);
   else
      snprintf(msg, sizeof(msg), "Could not write the report zip.");
   printf("%s\n", msg);
   MessageBoxA(NULL, msg, "panvk-report", ok ? MB_ICONINFORMATION : MB_ICONERROR);
   return ok ? 0 : 1;
}

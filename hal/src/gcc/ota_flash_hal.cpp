#include <filesystem>
#include <fstream>
#include <algorithm>
#include <memory>
#include <random>
#include <cstring>

#include <boost/algorithm/hex.hpp>
#include <boost/endian/conversion.hpp>

#include "ota_flash_hal.h"
#include "device_config.h"
#include "service_debug.h"
#include "core_hal.h"
#include "filesystem.h"
#include "bytes2hexbuf.h"
#include "module_info.h"
#include "../../../system/inc/system_info.h" // FIXME

using namespace particle;
namespace fs = std::filesystem;
namespace endian = boost::endian;

bool moduleUpdatePending = false;

namespace {

const size_t MODULE_PREFIX_SIZE = 24; // sizeof(module_info_t) on a 32-bit platform
const size_t MODULE_SUFFIX_SIZE = 36; // sizeof(module_info_suffix_t) on a 32-bit platform

struct ParsedModuleInfo {
    uint32_t startAddress;
    uint32_t endAddress;
    uint16_t version;
    uint16_t platformId;
    uint8_t function;
    uint8_t index;
    struct {
        uint8_t function;
        uint8_t index;
        uint16_t version;
    } dependency1;
    struct {
        uint8_t function;
        uint8_t index;
        uint16_t version;
    } dependency2;
    uint8_t hash[32];
};

ParsedModuleInfo parseModule(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (in.fail()) {
        throw std::runtime_error("Failed to open file");
    }
    in.seekg(0, std::ios::end);
    if (in.tellg() == (std::ifstream::pos_type)-1) {
        throw std::runtime_error("Failed to read file");
    }
    size_t fileSize = in.tellg();
    if (fileSize <= MODULE_PREFIX_SIZE + MODULE_SUFFIX_SIZE + 4 /* CRC-32 */) {
        throw std::runtime_error("Invalid module size");
    }
    // Scan the first few KBs of the module binary for something that looks like a valid module header
    auto prefixSize = std::min<size_t>(16 * 1024, fileSize - MODULE_SUFFIX_SIZE - 4 /* CRC-32 */);
    std::string prefix(prefixSize, '\0');
    in.seekg(0);
    in.read(prefix.data(), prefixSize);
    if (in.fail()) {
        throw std::runtime_error("Failed to read file");
    }
    ParsedModuleInfo info = {};
    bool foundHeader = false;
    size_t offs = 0;
    while (offs < prefixSize) {
        // Start address
        memcpy(&info.startAddress, prefix.data() + offs, sizeof(info.startAddress));
        info.startAddress = endian::little_to_native(info.startAddress);
        // End address
        memcpy(&info.endAddress, prefix.data() + offs + 4, sizeof(info.endAddress));
        info.endAddress = endian::little_to_native(info.endAddress);
        // Version
        memcpy(&info.version, prefix.data() + offs + 10, sizeof(info.version));
        info.version = endian::little_to_native(info.version);
        // Platform ID
        memcpy(&info.platformId, prefix.data() + offs + 12, sizeof(info.platformId));
        info.platformId = endian::little_to_native(info.platformId);
        // Function
        memcpy(&info.function, prefix.data() + offs + 14, sizeof(info.function));
        // Index
        memcpy(&info.index, prefix.data() + offs + 15, sizeof(info.index));
        // Dependency 1
        memcpy(&info.dependency1.function, prefix.data() + offs + 16, sizeof(info.dependency1.function));
        memcpy(&info.dependency1.index, prefix.data() + offs + 17, sizeof(info.dependency1.index));
        memcpy(&info.dependency1.version, prefix.data() + offs + 18, sizeof(info.dependency1.version));
        info.dependency1.version = endian::little_to_native(info.dependency1.version);
        // Dependency 2
        memcpy(&info.dependency2.function, prefix.data() + offs + 20, sizeof(info.dependency2.function));
        memcpy(&info.dependency2.index, prefix.data() + offs + 21, sizeof(info.dependency2.index));
        memcpy(&info.dependency2.version, prefix.data() + offs + 22, sizeof(info.dependency2.version));
        info.dependency2.version = endian::little_to_native(info.dependency2.version);
        if (info.endAddress <= info.startAddress ||
                info.endAddress - info.startAddress + 4 /* CRC-32 */ != fileSize ||
                info.platformId != deviceConfig.platform_id ||
                !system::is_module_function_valid((module_function_t)info.function) ||
                (info.dependency1.function != MODULE_FUNCTION_NONE && !system::is_module_function_valid((module_function_t)info.dependency1.function)) ||
                (info.dependency2.function != MODULE_FUNCTION_NONE && !system::is_module_function_valid((module_function_t)info.dependency2.function)) ||
                (info.dependency1.function == MODULE_FUNCTION_NONE && (info.dependency1.index != 0 || info.dependency1.version != 0)) ||
                (info.dependency2.function == MODULE_FUNCTION_NONE && (info.dependency2.index != 0 || info.dependency2.version != 0))) {
            continue;
        }
        foundHeader = true;
        break;
    }
    if (!foundHeader) {
        throw std::runtime_error("Failed to parse module header");
    }
    in.seekg(fileSize - 4 /* CRC-32 */ - 2 /* size */ - sizeof(info.hash));
    in.read((char*)info.hash, sizeof(info.hash));
    if (in.fail()) {
        throw std::runtime_error("Failed to read file");
    }
    return info;
}

module_dependency_t halModuleDependency(const config::ModuleDependencyInfo& info) {
    return {
        .module_function = info.function(),
        .module_index = info.index(),
        .module_version = info.version()
    };
}

module_bounds_location_t moduleLocation(const config::ModuleInfo& module) {
    if (module.function() == MODULE_FUNCTION_NCP_FIRMWARE) {
        return MODULE_BOUNDS_LOC_NCP_FLASH;
    }
    return MODULE_BOUNDS_LOC_INTERNAL_FLASH;
}

config::Describe defaultDescribe() {
    auto desc = config::Describe::fromString("{\"p\": 0,\"m\":[{\"f\":\"m\",\"n\":\"0\",\"v\":0,\"d\":[]}]}");
    desc.platformId(deviceConfig.platform_id);
    auto modules = desc.modules();
    for (auto& module: modules) {
        module.version(MODULE_VERSION);
    }
    desc.modules(modules);
    return desc;
}

std::string genUpdateFileName() {
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 255);
    std::string suff(16, '\0');
    for (size_t i = 0; i < suff.size(); ++i) {
        suff[i] = dist(rd);
    }
    return "update_" + boost::algorithm::hex(suff) + ".bin";
}

fs::path g_updateFile;
std::ofstream g_updateStream;

} // namespace

int HAL_System_Info(hal_system_info_t* info, bool create, void* reserved)
{
    if (!create) {
        delete[] info->modules;
        return 0;
    }
    const auto& desc = deviceConfig.describe.isValid() ? deviceConfig.describe : defaultDescribe();
    std::unique_ptr<hal_module_t[]> halModules(new(std::nothrow) hal_module_t[desc.modules().size()]);
    if (!halModules) {
        return SYSTEM_ERROR_NO_MEMORY;
    }
    uint32_t moduleAddr = 0;
    for (size_t i = 0; i < desc.modules().size(); ++i) {
        const auto& module = desc.modules().at(i);
        auto& halModule = halModules[i];
        halModule = {
            .bounds = {
                .maximum_size = module.maximumSize(),
                .start_address = moduleAddr,
                .end_address = moduleAddr + module.maximumSize(),
                .module_function = module.function(),
                .module_index = module.index(),
                .store = module.store(),
                .mcu_identifier = 0,
                .location = moduleLocation(module)
            },
            .info = {
                .module_start_address = (char*)0 + moduleAddr,
                .module_end_address = (char*)0 + moduleAddr + module.maximumSize(),
                .reserved = 0,
                .flags = 0,
                .module_version = module.version(),
                .platform_id = deviceConfig.platform_id,
                .module_function = module.function(),
                .module_index = module.index(),
                .dependency = {
                    .module_function = MODULE_FUNCTION_NONE,
                    .module_index = 0,
                    .module_version = 0
                },
                .dependency2 = {
                    .module_function = MODULE_FUNCTION_NONE,
                    .module_index = 0,
                    .module_version = 0
                }
            },
            .crc = {
                .crc32 = 0
            },
            .suffix = {
                .reserved = 0,
                .sha = {},
                .size = 36
            },
            .validity_checked = module.validityChecked(),
            .validity_result = module.validityResult(),
            .module_info_offset = 0
        };
        if (module.dependencies().size() >= 1) {
            halModule.info.dependency = halModuleDependency(module.dependencies().at(0));
        }
        if (module.dependencies().size() >= 2) {
            halModule.info.dependency2 = halModuleDependency(module.dependencies().at(1));
        }
        memcpy(halModule.suffix.sha, module.hash().data(), std::min(sizeof(halModule.suffix.sha), module.hash().size()));
        moduleAddr += module.maximumSize();
    }
    info->platform_id = deviceConfig.platform_id;
    info->module_count = desc.modules().size();
    info->modules = halModules.release();
    return 0;
}

uint32_t HAL_OTA_FlashAddress()
{
    return 0;
}

uint32_t HAL_OTA_FlashLength()
{
    return 10 * 1024 * 1024;
}

uint16_t HAL_OTA_ChunkSize()
{
    return 512;
}

bool HAL_FLASH_Begin(uint32_t sFLASH_Address, uint32_t fileSize, void* reserved)
{
    try {
        g_updateFile = fs::temp_directory_path().append(genUpdateFileName());
        g_updateStream.open(g_updateFile, std::ios::binary);
        if (g_updateStream.fail()) {
            throw std::runtime_error("Failed to create file");
        }
        return true;
    } catch (const std::exception& e) {
        LOG(ERROR, "%s", e.what());
        g_updateStream.close();
        fs::remove(g_updateFile);
        return false;
    }
}

int HAL_FLASH_Update(const uint8_t *pBuffer, uint32_t address, uint32_t length, void* reserved)
{
    try {
        if (!g_updateStream.is_open()) {
            throw std::runtime_error("File is not open");
        }
        g_updateStream.seekp(address);
        g_updateStream.write((const char*)pBuffer, length);
        if (g_updateStream.fail()) {
            throw std::runtime_error("Failed to write to file");
        }
        return 0;
    } catch (const std::exception& e) {
        LOG(ERROR, "%s", e.what());
        g_updateStream.close();
        fs::remove(g_updateFile);
        return SYSTEM_ERROR_IO;
    }
}

int HAL_FLASH_OTA_Validate(bool userDepsOptional, module_validation_flags_t flags, void* reserved)
{
    return 0; // FIXME
}

int HAL_FLASH_End(void* reserved)
{
    try {
        if (!g_updateStream.is_open()) {
            throw std::runtime_error("File is not open");
        }
        g_updateStream.close();
        auto updatedModule = parseModule(g_updateFile);
        auto desc = deviceConfig.describe.isValid() ? deviceConfig.describe : defaultDescribe();
        auto modules = desc.modules();
        bool foundModule = false;
        for (auto& module: modules) {
            if (module.function() == updatedModule.function && module.index() == updatedModule.index) {
                // Update module info
                module.version(updatedModule.version);
                config::ModuleInfo::Dependencies deps;
                if (updatedModule.dependency1.function != MODULE_FUNCTION_NONE) {
                    config::ModuleDependencyInfo dep;
                    dep.function((module_function_t)updatedModule.dependency1.function);
                    dep.index(updatedModule.dependency1.index);
                    dep.version(updatedModule.dependency1.version);
                    deps.push_back(dep);
                }
                if (updatedModule.dependency2.function != MODULE_FUNCTION_NONE) {
                    config::ModuleDependencyInfo dep;
                    dep.function((module_function_t)updatedModule.dependency2.function);
                    dep.index(updatedModule.dependency2.index);
                    dep.version(updatedModule.dependency2.version);
                    deps.push_back(dep);
                }
                module.dependencies(deps);
                module.hash(std::string((const char*)updatedModule.hash, sizeof(updatedModule.hash)));
                desc.modules(modules);
                deviceConfig.describe = desc;
                foundModule = true;
                break;
            }
        }
        if (foundModule) {
            LOG(INFO, "Applying module update: function: %d, index: %d, version: %d", (int)updatedModule.function,
                    (int)updatedModule.index, (int)updatedModule.version);
            moduleUpdatePending = true;
        } else {
            LOG(INFO, "Unsupported module: function: %d, index: %d, version: %d", (int)updatedModule.function,
                    (int)updatedModule.index, (int)updatedModule.version);
        }
        fs::remove(g_updateFile);
        return HAL_UPDATE_APPLIED_PENDING_RESTART;
    } catch (const std::exception& e) {
        LOG(ERROR, "%s", e.what());
        g_updateStream.close();
        fs::remove(g_updateFile);
        return SYSTEM_ERROR_IO;
    }
}

int HAL_FLASH_ApplyPendingUpdate(bool dryRun, void* reserved)
{
    return SYSTEM_ERROR_UNKNOWN;
}

/**
 * Set the claim code for this device.
 * @param code  The claim code to set. If null, clears the claim code.
 * @return 0 on success.
 */
uint16_t HAL_Set_Claim_Code(const char* code)
{
    return 0;
}

/**
 * Retrieves the claim code for this device.
 * @param buffer    The buffer to recieve the claim code.
 * @param len       The maximum size of the code to copy to the buffer, including the null terminator.
 * @return          0 on success.
 */
uint16_t HAL_Get_Claim_Code(char* buffer, unsigned len)
{
    *buffer = 0;
    return 0;
}

bool HAL_IsDeviceClaimed(void* reserved)
{
	return false;
}


#define EXTERNAL_FLASH_SERVER_DOMAIN_LENGTH 128

// todo - duplicate from core, factor this down into a common area
void parseServerAddressData(ServerAddress* server_addr, uint8_t* buf)
{
  // Internet address stored on external flash may be
  // either a domain name or an IP address.
  // It's stored in a type-length-value encoding.
  // First byte is type, second byte is length, the rest is value.

  switch (buf[0])
  {
    case IP_ADDRESS:
      server_addr->addr_type = IP_ADDRESS;
      server_addr->ip = (buf[2] << 24) | (buf[3] << 16) |
                        (buf[4] << 8)  |  buf[5];
      break;

    case DOMAIN_NAME:
      if (buf[1] <= EXTERNAL_FLASH_SERVER_DOMAIN_LENGTH - 2)
      {
        server_addr->addr_type = DOMAIN_NAME;
        memcpy(server_addr->domain, buf + 2, buf[1]);

        // null terminate string
        char *p = server_addr->domain + buf[1];
        *p = 0;
        break;
      }
      // else fall through

    default:
      server_addr->addr_type = INVALID_INTERNET_ADDRESS;
  }

}


#define MAXIMUM_CLOUD_KEY_LEN (512)
#define SERVER_ADDRESS_OFFSET (384)
#define SERVER_ADDRESS_OFFSET_EC (192)
#define SERVER_ADDRESS_SIZE (128)
#define SERVER_PUBLIC_KEY_SIZE (294)
#define SERVER_PUBLIC_KEY_EC_SIZE (320)


void HAL_FLASH_Read_ServerAddress(ServerAddress* server_addr)
{
	memset(server_addr, 0, sizeof(ServerAddress));
	int offset = HAL_Feature_Get(FEATURE_CLOUD_UDP) ? SERVER_ADDRESS_OFFSET_EC : SERVER_ADDRESS_OFFSET;
    parseServerAddressData(server_addr, deviceConfig.server_key+offset);
}

void HAL_FLASH_Write_ServerAddress(const uint8_t *buf, bool udp)
{
    int offset = (udp) ? SERVER_ADDRESS_OFFSET_EC : SERVER_ADDRESS_OFFSET;
    memcpy(deviceConfig.server_key+offset, buf, SERVER_ADDRESS_SIZE);
}

bool HAL_OTA_Flashed_GetStatus(void)
{
    return false;
}

void HAL_OTA_Flashed_ResetStatus(void)
{
}

#define PUBLIC_KEY_LEN 294
#define PRIVATE_KEY_LEN 612

void HAL_FLASH_Read_ServerPublicKey(uint8_t *keyBuffer)
{
    memcpy(keyBuffer, deviceConfig.server_key, PUBLIC_KEY_LEN);
    char buf[PUBLIC_KEY_LEN*2];
    bytes2hexbuf(keyBuffer, PUBLIC_KEY_LEN, buf);
    INFO("server key: %s", buf);
}

void HAL_FLASH_Write_ServerPublicKey(const uint8_t *keyBuffer, bool udp)
{
    if (udp) {
        memcpy(&deviceConfig.server_key, keyBuffer, SERVER_PUBLIC_KEY_EC_SIZE);
    } else {
        memcpy(&deviceConfig.server_key, keyBuffer, SERVER_PUBLIC_KEY_SIZE);
    }
}

int HAL_FLASH_Read_CorePrivateKey(uint8_t *keyBuffer, private_key_generation_t* generation)
{
    memcpy(keyBuffer, deviceConfig.device_key, PRIVATE_KEY_LEN);
    char buf[PRIVATE_KEY_LEN*2];
    bytes2hexbuf(keyBuffer, PRIVATE_KEY_LEN, buf);
    INFO("device key: %s", buf);
    return 0;
}

extern "C" void random_seed_from_cloud(unsigned int value)
{
}

void HAL_OTA_Add_System_Info(hal_system_info_t* info, bool create, void* reserved)
{
}

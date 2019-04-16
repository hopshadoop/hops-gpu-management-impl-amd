#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <inttypes.h>
#include <glob.h> // glob(), globfree()
#include <string.h> // memset()
#include <stdexcept>
#include <vector>
#include <bitset>
#include <string>
#include <sstream>
#include <dlfcn.h>

namespace convert
{
    template < typename T > std::string to_string( const T& n )
    {
        std::ostringstream stm ;
        stm << n ;
        return stm.str() ;
    }
}

std::vector<std::string> glob_helper(const std::string& pattern) {
    using namespace std;

    // glob struct resides on the stack
    glob_t glob_result;
    memset(&glob_result, 0, sizeof(glob_result));

    // do the glob operation
    int return_value = glob(pattern.c_str(), GLOB_TILDE, NULL, &glob_result);
    if(return_value != 0) {
        globfree(&glob_result);
        stringstream ss;
        ss << "glob() failed with return_value " << return_value << endl;
        throw std::runtime_error(ss.str());
    }

    // collect all the filenames into a std::list<std::string>
    vector<string> filenames;
    for(size_t i = 0; i < glob_result.gl_pathc; ++i) {
        filenames.push_back(string(glob_result.gl_pathv[i]));
    }

    // cleanupamd_gpus
    globfree(&glob_result);

    // done
    return filenames;
}

std::vector<std::string> get_amd_cards() {
    std::string pattern = "/sys/module/amdgpu/drivers/pci:amdgpu/*/drm";
    std::vector<std::string> amd_gpus = glob_helper(pattern);
    return amd_gpus;
}

std::vector<std::string> get_render_nodes() {
    std::string pattern = "/dev/dri/render*";
    std::vector<std::string> render_nodes = glob_helper(pattern);
    return render_nodes;
}

#include <iostream>

#include "AMDManagementLibrary.h"

static const unsigned int AMD_MAJOR_DEVICE = 226;

JNIEXPORT jboolean JNICALL Java_io_hops_management_amd_AMDManagementLibrary_initialize
  (JNIEnv * env, jobject obj) {
    struct stat kfd;
    if(stat("/dev/kfd", &kfd) == 0) {
      return 1;
    }
    return 0;
  }

JNIEXPORT jboolean JNICALL Java_io_hops_management_amd_AMDManagementLibrary_shutDown
  (JNIEnv * env, jobject obj) {
    return 1;
  }

JNIEXPORT jint JNICALL Java_io_hops_management_amd_AMDManagementLibrary_getNumGPUs
  (JNIEnv * env, jobject obj) {
    return get_amd_cards().size();
  }

JNIEXPORT jstring JNICALL Java_io_hops_management_amd_AMDManagementLibrary_queryMandatoryDevices
  (JNIEnv * env, jobject obj) {

    std::string mandatory_devices = "";

    struct stat kfd;
    if(stat("/dev/kfd", &kfd) == 0) {
      mandatory_devices = convert::to_string(mandatory_devices) + convert::to_string(major(kfd.st_rdev)) + ":" + convert::to_string(minor(kfd.st_rdev)) + " ";
    }
    return env->NewStringUTF(mandatory_devices.c_str());
  }

JNIEXPORT jstring JNICALL Java_io_hops_management_amd_AMDManagementLibrary_queryAvailableDevices
  (JNIEnv * env, jobject obj, jint reportedGPUs) {

    std::string available_devices = "";
    if(reportedGPUs == 0) {
      return env->NewStringUTF(available_devices.c_str());
    }

    struct stat amd_gpu;
    std::vector<std::string> amd_gpus = get_amd_cards();
    int numSchedulableGPUs = 0;
    for(std::vector<int>::size_type i = 0; i != amd_gpus.size(); i++) {

      if(reportedGPUs == numSchedulableGPUs) {
        return env->NewStringUTF(available_devices.c_str());
      }

      std::string amd_card_path = glob_helper(amd_gpus[i] + "/card*")[0];
      std::string amd_card_dri_entry = "/dev/dri/" + amd_card_path.substr(amd_card_path.find_last_of("/") + 1);
      struct stat amd_gpu;
      stat(amd_card_dri_entry.c_str(), &amd_gpu);


      std::string render_node_path = glob_helper(amd_gpus[i] + "/render*")[0];
      std::string render_node_dri_entry = "/dev/dri/" + render_node_path.substr(render_node_path.find_last_of("/") + 1);
      struct stat render_node;
      stat(render_node_dri_entry.c_str(), &render_node);

      available_devices = convert::to_string(available_devices) + convert::to_string(major(amd_gpu.st_rdev)) + ":" + convert::to_string(minor(amd_gpu.st_rdev)) +
       "&" + convert::to_string(major(render_node.st_rdev)) + ":" + convert::to_string(minor(render_node.st_rdev)) + " ";

      numSchedulableGPUs++;
    }
    return env->NewStringUTF(available_devices.c_str());
  }
// Helper functions for DirectIO operations
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

// #define DIRECT_IO true

class DirectIOHelper {
private:
    static size_t getBlockSize() {
        #ifdef DIRECT_IO
            long page_size = sysconf(_SC_PAGESIZE);
            return page_size > 0 ? page_size : 4096;
        #else
            return 1;
        #endif
    }

public:
    // Aligned memory allocation
    static void* alignedMalloc(size_t size) {
        void* ptr = nullptr;
        size_t alignment = getBlockSize();
        if (posix_memalign(&ptr, alignment, size) != 0) {
            return nullptr;
        }
        return ptr;
    }

    // Round up size to block boundary
    static size_t roundToBlockSize(size_t size) {
        size_t block_size = getBlockSize();
        return ((size + block_size - 1) / block_size) * block_size;
    }

    // Safe file open with DirectIO
    static FILE* openFile(const std::string& path, const char* mode, bool write = false) {
        FILE* file = nullptr;
        #ifdef DIRECT_IO
            int flags = 0;
            if (strchr(mode, '+') || strchr(mode, 'w') || strchr(mode, 'a')) {
                flags = O_RDWR | O_CREAT;
                if (strchr(mode, 'a')) flags |= O_APPEND;
            } else {
                flags = O_RDONLY;
            }
            
            if (write) flags |= O_WRONLY | O_CREAT;
            
            flags |= O_DIRECT;
            int fd = open(path.c_str(), flags, 0644);
            if (fd == -1) {
                // Fallback to non-DirectIO
                flags &= ~O_DIRECT;
                fd = open(path.c_str(), flags, 0644);
            }
            if (fd != -1) {
                file = fdopen(fd, mode);
                if (!file) close(fd);
            }
        #else
            file = fopen(path.c_str(), mode);
        #endif
        return file;
    }

    // Safe aligned write
    static bool writeAligned(FILE* file, const void* data, size_t size) {
        #ifdef DIRECT_IO
            size_t aligned_size = roundToBlockSize(size);
            void* aligned_buffer = alignedMalloc(aligned_size);
            if (!aligned_buffer) return false;
            
            memcpy(aligned_buffer, data, size);
            if (size < aligned_size) {
                memset(static_cast<char*>(aligned_buffer) + size, 0, aligned_size - size);
            }
            
            bool success = (fwrite(aligned_buffer, 1, aligned_size, file) == aligned_size);
            free(aligned_buffer);
            return success;
        #else
            return (fwrite(data, 1, size, file) == size);
        #endif
    }

    // Safe aligned read
    static bool readAligned(FILE* file, void* data, size_t size) {
        #ifdef DIRECT_IO
            size_t aligned_size = roundToBlockSize(size);
            void* aligned_buffer = alignedMalloc(aligned_size);
            if (!aligned_buffer) return false;
            
            bool success = (fread(aligned_buffer, 1, aligned_size, file) == aligned_size);
            if (success) {
                memcpy(data, aligned_buffer, size);
            }
            free(aligned_buffer);
            return success;
        #else
            return (fread(data, 1, size, file) == size);
        #endif
    }
};
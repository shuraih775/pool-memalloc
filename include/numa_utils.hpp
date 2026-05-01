#ifndef NUMA_UTILS_HPP
#define NUMA_UTILS_HPP

#include <cstddef>
#include <cstdint>

namespace numa
{

    size_t node_count();

    size_t current_node();

    // Falls back to regular aligned allocation if NUMA API unavailable.
    void *alloc_on_node(size_t size, size_t alignment, size_t node);

    // Free memory allocated by alloc_on_node.
    void free_on_node(void *ptr, size_t size);

} // namespace numa

#endif // NUMA_UTILS_HPP

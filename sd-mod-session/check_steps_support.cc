#include <bits/stl_tree.h>
#include <cstdlib>
#include <new>
extern "C" void check_throw(void*) {throw std::bad_alloc();}
extern "C" void check_erase_node(void *tree,void *node)
{
    auto *header=reinterpret_cast<std::_Rb_tree_node_base*>(static_cast<char*>(tree)+8);
    auto *value=std::_Rb_tree_rebalance_for_erase(static_cast<std::_Rb_tree_node_base*>(node),*header);
    std::free(value);--*reinterpret_cast<unsigned long*>(static_cast<char*>(tree)+40);
}
